#include "content/providers/gdb_provider.hpp"

#include <cstring>
#include <thread>
#include <chrono>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/crypto.hpp>
#include <hex/api/localization_manager.hpp>

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    using namespace std::chrono_literals;

    namespace gdb {

        namespace {

            u8 calculateChecksum(const std::string &data) {
                u64 checksum = 0;

                for (const auto &c : data)
                    checksum += c;

                return checksum & 0xFF;
            }

            std::string createPacket(const std::string &data) {
                return hex::format("${}#{:02x}", data, calculateChecksum(data));
            }

            std::optional<std::string> parsePacket(const std::string &packet) {
                if (packet.length() < 4)
                    return std::nullopt;

                if (!packet.starts_with('$'))
                    return std::nullopt;

                if (packet[packet.length() - 3] != '#')
                    return std::nullopt;

                std::string data     = packet.substr(1, packet.length() - 4);
                std::string checksum = packet.substr(packet.length() - 2, 2);

                auto decodedChecksum = crypt::decode16(checksum);
                if (checksum.length() != 2 || decodedChecksum.empty() || decodedChecksum[0] != calculateChecksum(data))
                    return std::nullopt;

                return data;
            }

        }

        void sendAck(const wolv::net::SocketClient &socket) {
            socket.writeString("+");
        }

        void continueExecution(const wolv::net::SocketClient &socket) {
            socket.writeString(createPacket("vCont;c"));
        }

        std::vector<u8> readMemory(const wolv::net::SocketClient &socket, u64 address, size_t size) {
            std::string packet = createPacket(hex::format("m{:X},{:X}", address, size));

            socket.writeString(packet);

            auto receivedPacket = socket.readString(size * 2 + 4);

            if (receivedPacket.empty())
                return {};

            auto receivedData = parsePacket(receivedPacket);
            if (!receivedData.has_value())
                return {};

            if (receivedData->size() == 3 && receivedData->starts_with("E"))
                return {};

            auto data = crypt::decode16(receivedData.value());

            data.resize(size);

            return data;
        }

        void writeMemory(const wolv::net::SocketClient &socket, u64 address, const void *buffer, size_t size) {
            std::vector<u8> bytes(size);
            std::memcpy(bytes.data(), buffer, size);

            std::string byteString = crypt::encode16(bytes);

            std::string packet = createPacket(hex::format("M{:X},{:X}:{}", address, size, byteString));

            socket.writeString(packet);

            auto receivedPacket = socket.readString(6);
        }

        bool enableNoAckMode(const wolv::net::SocketClient &socket) {
            socket.writeString(createPacket("QStartNoAckMode"));

            auto ack = socket.readString(1);

            if (ack.empty() || ack[0] != '+')
                return false;

            auto receivedPacket = socket.readString(6);

            auto receivedData = parsePacket(receivedPacket);

            if (receivedData && *receivedData == "OK") {
                sendAck(socket);
                return true;
            } else {
                return false;
            }
        }

    }

    GDBProvider::GDBProvider() : m_size(0xFFFF'FFFF) {
    }

    bool GDBProvider::isAvailable() const {
        return this->m_socket.isConnected();
    }

    bool GDBProvider::isReadable() const {
        return this->m_socket.isConnected();
    }

    bool GDBProvider::isWritable() const {
        return true;
    }

    bool GDBProvider::isResizable() const {
        return false;
    }

    bool GDBProvider::isSavable() const {
        return false;
    }

    void GDBProvider::readRaw(u64 offset, void *buffer, size_t size) {
        if ((offset - this->getBaseAddress()) > (this->getActualSize() - size) || buffer == nullptr || size == 0)
            return;

        offset -= this->getBaseAddress();

        u64 alignedOffset = offset - (offset % CacheLineSize);

        if (size <= CacheLineSize) {
            std::scoped_lock lock(this->m_cacheLock);

            const auto &cacheLine = std::find_if(this->m_cache.begin(), this->m_cache.end(), [&](auto &line) {
                return line.address == alignedOffset;
            });

            if (cacheLine != this->m_cache.end()) {
                // Cache hit

            } else {
                // Cache miss

                this->m_cache.push_back({ alignedOffset, { 0 } });
            }

            if (cacheLine != this->m_cache.end())
                std::memcpy(buffer, &cacheLine->data[0] + (offset % CacheLineSize), std::min(size, cacheLine->data.size()));
        } else {
            while (size > 0) {
                size_t readSize = std::min(size, CacheLineSize);

                auto data = gdb::readMemory(this->m_socket, offset, size);
                if (!data.empty())
                    std::memcpy(buffer, &data[0], data.size());

                size -= readSize;
                offset += readSize;
            }
        }
    }

    void GDBProvider::writeRaw(u64 offset, const void *buffer, size_t size) {
        if ((offset - this->getBaseAddress()) > (this->getActualSize() - size) || buffer == nullptr || size == 0)
            return;

        offset -= this->getBaseAddress();

        gdb::writeMemory(this->m_socket, offset, buffer, size);
    }

    void GDBProvider::save() {
        Provider::save();
    }

    size_t GDBProvider::getActualSize() const {
        return this->m_size;
    }

    std::string GDBProvider::getName() const {
        std::string address = "-";
        std::string port = "-";

        if (this->isConnected()) {
            address = this->m_ipAddress;
            port    = std::to_string(this->m_port);
        }

        return hex::format("hex.builtin.provider.gdb.name"_lang, address, port);
    }

    std::vector<GDBProvider::Description> GDBProvider::getDataDescription() const {
        return {
            {"hex.builtin.provider.gdb.server"_lang, hex::format("{}:{}", this->m_ipAddress, this->m_port)},
        };
    }

    bool GDBProvider::open() {
        this->m_socket = wolv::net::SocketClient(wolv::net::SocketClient::Type::TCP);
        this->m_socket.connect(this->m_ipAddress, this->m_port);
        if (!gdb::enableNoAckMode(this->m_socket)) {
            this->m_socket.disconnect();
            return false;
        }

        if (this->m_socket.isConnected()) {
            gdb::continueExecution(this->m_socket);

            this->m_cacheUpdateThread = std::thread([this] {
                auto cacheLine = this->m_cache.begin();
                while (this->isConnected()) {
                    {
                        std::scoped_lock lock(this->m_cacheLock);

                        if (this->m_resetCache) {
                            this->m_cache.clear();
                            this->m_resetCache = false;
                            cacheLine = this->m_cache.begin();
                        }

                        if (cacheLine != this->m_cache.end()) {
                            std::vector<u8> data = gdb::readMemory(this->m_socket, cacheLine->address, CacheLineSize);

                            while (std::count_if(this->m_cache.begin(), this->m_cache.end(), [&](auto &line) { return !line.data.empty(); }) > 100) {
                                this->m_cache.pop_front();
                                cacheLine = this->m_cache.begin();
                            }

                            std::memcpy(cacheLine->data.data(), data.data(), data.size());
                        }

                        if (cacheLine == this->m_cache.end())
                            cacheLine = this->m_cache.begin();
                        else
                            ++cacheLine;
                    }
                    std::this_thread::sleep_for(10ms);
                }
            });

            return true;
        } else {
            return false;
        }
    }

    void GDBProvider::close() {
        this->m_socket.disconnect();

        if (this->m_cacheUpdateThread.joinable()) {
            this->m_cacheUpdateThread.join();
        }
    }

    bool GDBProvider::isConnected() const {
        return this->m_socket.isConnected();
    }


    bool GDBProvider::drawLoadInterface() {
        ImGui::InputText("hex.builtin.provider.gdb.ip"_lang, this->m_ipAddress);
        ImGui::InputInt("hex.builtin.provider.gdb.port"_lang, &this->m_port, 0, 0);

        ImGui::Separator();

        ImGuiExt::InputHexadecimal("hex.builtin.common.size"_lang, &this->m_size, ImGuiInputTextFlags_CharsHexadecimal);

        if (this->m_port < 0)
            this->m_port = 0;
        else if (this->m_port > 0xFFFF)
            this->m_port = 0xFFFF;

        return !this->m_ipAddress.empty() && this->m_port != 0;
    }

    void GDBProvider::loadSettings(const nlohmann::json &settings) {
        Provider::loadSettings(settings);

        this->m_ipAddress = settings.at("ip").get<std::string>();
        this->m_port      = settings.at("port").get<int>();
        this->m_size      = settings.at("size").get<size_t>();
    }

    nlohmann::json GDBProvider::storeSettings(nlohmann::json settings) const {
        settings["ip"]   = this->m_ipAddress;
        settings["port"] = this->m_port;
        settings["size"] = this->m_size;

        return Provider::storeSettings(settings);
    }

    std::pair<Region, bool> GDBProvider::getRegionValidity(u64 address) const {
        address -= this->getBaseAddress();

        if (address < this->getActualSize())
            return { Region { this->getBaseAddress() + address, this->getActualSize() - address }, true };
        else
            return { Region::Invalid(), false };
    }

    std::variant<std::string, i128> GDBProvider::queryInformation(const std::string &category, const std::string &argument) {
        if (category == "ip")
            return this->m_ipAddress;
        else if (category == "port")
            return this->m_port;
        else
            return Provider::queryInformation(category, argument);
    }

}