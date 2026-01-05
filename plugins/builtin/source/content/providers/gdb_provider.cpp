#include "content/providers/gdb_provider.hpp"

#include <cstring>
#include <thread>
#include <chrono>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/crypto.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/helpers/logger.hpp>

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
                return fmt::format("${}#{:02x}", data, calculateChecksum(data));
            }

            std::optional<std::string> parsePacket(const std::string &packet) {
                if (packet.length() < 4 || packet[0] != '$')
                    return std::nullopt;

                size_t hashPos = packet.find('#');
                if (hashPos == std::string::npos || hashPos + 2 >= packet.size())
                    return std::nullopt;

                std::string data = packet.substr(1, hashPos - 1);
                std::string checksumStr = packet.substr(hashPos + 1, 2);

                auto decodedChecksum = crypt::decode16(checksumStr);
                if (decodedChecksum.empty() || decodedChecksum[0] != calculateChecksum(data))
                    return std::nullopt;

                return data;
            }

            void sendAck(const wolv::net::SocketClient &socket) {
                socket.writeString("+");
            }

            char readCharacter(const wolv::net::SocketClient &socket) {
                auto data = socket.readBytes(1);
                return data.empty() ? '\0' : static_cast<char>(data[0]);
            }

            std::string sendReceivePackage(const wolv::net::SocketClient &socket, const std::string &packet) {
                socket.writeString(packet);

                i32 retries = 20;

                std::string buffer;
                while (true) {
                    char c = readCharacter(socket);
                    if (c == '+') {
                        // ACK response, continue reading
                        continue;
                    } else if (c == '$') {
                        // Start of response packet
                        buffer += c;

                        // Read until the end of the packet
                        while (true) {
                            c = readCharacter(socket);
                            buffer += c;
                            if (c == '#')
                                break;
                        }

                        // Read the checksum
                        buffer += readCharacter(socket);
                        buffer += readCharacter(socket);
                        break;
                    } else if (c == '-') {
                        // NAK response, retry sending the packet
                        socket.writeString(packet);
                        retries -= 1;
                    } else if (c == 0x00) {
                        // No response, wait a bit and retry receiving data
                        std::this_thread::sleep_for(10ms);
                        retries -= 1;
                    }

                    if (retries <= 0) {
                        log::error("No response from GDB server after multiple retries");
                        return {};
                    }
                }

                if (!buffer.empty())
                    sendAck(socket);

                return buffer;
            }

            std::vector<u8> decodeMemoryResponse(const std::string &response) {
                std::string expanded;
                size_t i = 0;

                while (i < response.size()) {
                    char c = response[i];

                    if (i + 2 < response.size() && response[i + 1] == '*') {
                        char repeatChar = response[i + 2];
                        int repeatCount = static_cast<unsigned char>(repeatChar) - 29;

                        if (repeatCount <= 0)
                            return {};

                        expanded.append(repeatCount, c);
                        i += 3;
                    } else {
                        expanded.push_back(c);
                        i++;
                    }
                }

                if (expanded.size() % 2 != 0)
                    return {};

                std::vector<u8> decoded;
                for (size_t j = 0; j < expanded.size(); j += 2) {
                    std::string byteStr = expanded.substr(j, 2);
                    try {
                        u8 byte = static_cast<u8>(std::stoul(byteStr, nullptr, 16));
                        decoded.push_back(byte);
                    } catch (...) {
                        return { };
                    }
                }

                return decoded;
            }


        }

        std::vector<u8> readMemory(const wolv::net::SocketClient &socket, u64 address, size_t size) {
            std::string packet = createPacket(fmt::format("m{:X},{:X}", address, size));

            std::string receivedPacket = sendReceivePackage(socket, packet);

            auto receivedData = parsePacket(receivedPacket);
            if (!receivedData.has_value())
                return {};

            sendAck(socket);

            if (receivedData->size() == 3 && receivedData->starts_with("E"))
                return {};

            auto data = decodeMemoryResponse(*receivedData);
            data.resize(size);
            return data;
        }

        bool writeMemory(const wolv::net::SocketClient &socket, u64 address, const void *buffer, size_t size) {
            std::vector<u8> bytes(size);
            std::memcpy(bytes.data(), buffer, size);
            std::string byteString = crypt::encode16(bytes);

            std::string packet = createPacket(fmt::format("M{:X},{:X}:{}", address, size, byteString));

            std::string receivedPacket = sendReceivePackage(socket, packet);
            auto receivedData = parsePacket(receivedPacket);
            if (!receivedData.has_value() || *receivedData != "OK")
                return false;

            sendAck(socket);
            return true;
        }

    }

    GDBProvider::GDBProvider() : m_size(0xFFFF'FFFF) {
    }

    bool GDBProvider::isAvailable() const {
        return m_socket.isConnected();
    }

    bool GDBProvider::isReadable() const {
        return m_socket.isConnected();
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

    void GDBProvider::readFromSource(u64 offset, void *buffer, size_t size) {
        std::scoped_lock lock(m_mutex);
        if (!m_socket.isConnected())
            return;

        auto data = gdb::readMemory(m_socket, offset, size);
        if (!data.empty())
            std::memcpy(buffer, data.data(), data.size());
    }

    void GDBProvider::writeToSource(u64 offset, const void *buffer, size_t size) {
        std::scoped_lock lock(m_mutex);
        if (!m_socket.isConnected())
            return;

        gdb::writeMemory(m_socket, offset, buffer, size);
    }

    void GDBProvider::save() {
        Provider::save();
    }

    u64 GDBProvider::getSourceSize() const {
        return m_size;
    }

    std::string GDBProvider::getName() const {
        std::string address = "-";
        std::string port = "-";

        if (this->isConnected()) {
            address = m_ipAddress;
            port    = std::to_string(m_port);
        }

        return fmt::format("hex.builtin.provider.gdb.name"_lang, address, port);
    }

    std::vector<GDBProvider::Description> GDBProvider::getDataDescription() const {
        return {
            {"hex.builtin.provider.gdb.server"_lang, fmt::format("{}:{}", m_ipAddress, m_port)},
        };
    }

    prv::Provider::OpenResult GDBProvider::open() {
        std::scoped_lock lock(m_mutex);

        CachedProvider::open();
        m_socket = wolv::net::SocketClient(wolv::net::SocketClient::Type::TCP, false);
        m_socket.connect(m_ipAddress, m_port);

        gdb::sendReceivePackage(m_socket, gdb::createPacket("!"));
        gdb::sendReceivePackage(m_socket, gdb::createPacket("Hg0"));

        if (!m_socket.isConnected()) {
            return OpenResult::failure("hex.builtin.provider.gdb.server.error.not_connected"_lang);
        }

        return {};
    }

    void GDBProvider::close() {
        std::scoped_lock lock(m_mutex);

        CachedProvider::close();
        m_socket.disconnect();
    }

    bool GDBProvider::isConnected() const {
        return m_socket.isConnected();
    }


    bool GDBProvider::drawLoadInterface() {
        ImGui::InputText("hex.builtin.provider.gdb.ip"_lang, m_ipAddress);
        ImGui::InputInt("hex.builtin.provider.gdb.port"_lang, &m_port, 0, 0);

        ImGui::Separator();

        ImGuiExt::InputHexadecimal("hex.ui.common.size"_lang, &m_size, ImGuiInputTextFlags_CharsHexadecimal);

        if (m_port < 0)
            m_port = 0;
        else if (m_port > 0xFFFF)
            m_port = 0xFFFF;

        return !m_ipAddress.empty() && m_port != 0;
    }

    void GDBProvider::loadSettings(const nlohmann::json &settings) {
        Provider::loadSettings(settings);

        m_ipAddress = settings.at("ip").get<std::string>();
        m_port      = settings.at("port").get<int>();
        m_size      = settings.at("size").get<size_t>();
    }

    nlohmann::json GDBProvider::storeSettings(nlohmann::json settings) const {
        settings["ip"]   = m_ipAddress;
        settings["port"] = m_port;
        settings["size"] = m_size;

        return Provider::storeSettings(settings);
    }

    std::pair<Region, bool> GDBProvider::getRegionValidity(u64 address) const {
        address -= this->getBaseAddress();

        if (address < this->getActualSize())
            return { Region { .address=this->getBaseAddress() + address, .size=this->getActualSize() - address }, true };
        else
            return { Region::Invalid(), false };
    }

    std::variant<std::string, i128> GDBProvider::queryInformation(const std::string &category, const std::string &argument) {
        if (category == "ip")
            return m_ipAddress;
        else if (category == "port")
            return m_port;
        else
            return Provider::queryInformation(category, argument);
    }

}