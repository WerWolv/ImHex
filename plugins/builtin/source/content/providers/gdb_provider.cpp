#include "content/providers/gdb_provider.hpp"

#include <cstring>
#include <thread>
#include <chrono>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/crypto.hpp>

namespace hex::plugin::builtin::prv {

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

                std::string data = packet.substr(1, packet.length() - 4);
                std::string checksum = packet.substr(packet.length() - 2, 2);

                if (checksum.length() != 2 || crypt::decode16(checksum)[0] != calculateChecksum(data))
                    return std::nullopt;

                return data;
            }

        }

        void sendAck(Socket &socket) {
            socket.writeString("+");
        }

        std::vector<u8> readMemory(Socket &socket, u64 address, size_t size) {
            std::string packet = createPacket(hex::format("m{:X},{:X}", address, size));

            socket.writeString(packet);

            auto receivedPacket = socket.readString(size * 2 + 4);

            if (receivedPacket.empty())
                return { };

            auto receivedData = parsePacket(receivedPacket);
            if (!receivedData.has_value())
                return { };

            if (receivedData->size() == 3 && receivedData->starts_with("E"))
                return { };

            auto data = crypt::decode16(receivedData.value());

            data.resize(size);

            return data;
        }

        bool enableNoAckMode(Socket &socket) {
            socket.writeString(createPacket("QStartNoAckMode"));

            auto ack = socket.readString(1);

            if (ack.empty() || ack[0] != '+')
                return {};

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

    GDBProvider::GDBProvider() : Provider() {

    }

    GDBProvider::~GDBProvider() {
        this->disconnect();
    }


    bool GDBProvider::isAvailable() const {
        return true;
    }

    bool GDBProvider::isReadable() const {
        return this->m_socket.isConnected();
    }

    bool GDBProvider::isWritable() const {
        return false;
    }

    bool GDBProvider::isResizable() const {
        return false;
    }

    bool GDBProvider::isSavable() const {
        return false;
    }


    void GDBProvider::read(u64 offset, void *buffer, size_t size, bool overlays) {
        if ((offset - this->getBaseAddress()) > (this->getSize() - size) || buffer == nullptr || size == 0)
            return;

        u64 alignedOffset = offset - (offset & 0xFFF);

        {
            std::scoped_lock lock(this->m_cacheLock);

            const auto &cacheLine = std::find_if(this->m_cache.begin(), this->m_cache.end(), [&](auto &line){
                return line.address == alignedOffset;
            });

            if (cacheLine != this->m_cache.end()) {
                // Cache hit

            } else {
                // Cache miss

                this->m_cache.push_back({ alignedOffset, { 0 } });
            }

            if (cacheLine != this->m_cache.end())
                std::memcpy(buffer, &cacheLine->data[0] + (offset & 0xFFF), size);
        }

        for (u64 i = 0; i < size; i++)
            if (getPatches().contains(offset + i))
                reinterpret_cast<u8*>(buffer)[i] = getPatches()[offset + PageSize * this->m_currPage + i];

        if (overlays)
            this->applyOverlays(offset, buffer, size);
    }

    void GDBProvider::write(u64 offset, const void *buffer, size_t size) {
        if (((offset - this->getBaseAddress()) + size) > this->getSize() || buffer == nullptr || size == 0)
            return;

        addPatch(offset, buffer, size);
    }

    void GDBProvider::readRaw(u64 offset, void *buffer, size_t size) {
        offset -= this->getBaseAddress();

        if ((offset + size) > this->getSize() || buffer == nullptr || size == 0)
            return;

        auto data = gdb::readMemory(this->m_socket, offset, size);
        std::memcpy(buffer, &data[0], data.size());
    }

    void GDBProvider::writeRaw(u64 offset, const void *buffer, size_t size) {
        offset -= this->getBaseAddress();

        if ((offset + size) > this->getSize() || buffer == nullptr || size == 0)
            return;
    }

    void GDBProvider::save() {
        this->applyPatches();
    }

    void GDBProvider::saveAs(const std::string &path) {

    }

    void GDBProvider::resize(ssize_t newSize) {

    }

    size_t GDBProvider::getActualSize() const {
        return 0xFFFF'FFFF;
    }

    std::string GDBProvider::getName() const {
        return hex::format("hex.builtin.provider.gdb.name"_lang, this->m_ipAddress, this->m_port);
    }

    std::vector<std::pair<std::string, std::string>> GDBProvider::getDataInformation() const {
        return { };
    }

    void GDBProvider::connect(const std::string &address, u16 port) {
        this->m_socket.connect(address, port);
        if (!gdb::enableNoAckMode(this->m_socket)) {
            this->m_socket.disconnect();
            return;
        }

        this->m_ipAddress = address;
        this->m_port = port;

        if (this->m_socket.isConnected()) {
            this->m_cacheUpdateThread = std::thread([this]() {
                auto cacheLine = this->m_cache.begin();
                while (this->isConnected()) {
                    {
                        std::scoped_lock lock(this->m_cacheLock);

                        if (cacheLine != this->m_cache.end()) {
                            auto data = gdb::readMemory(this->m_socket, cacheLine->address, 0x1000);

                            while (this->m_cache.size() > 10) {
                                this->m_cache.pop_front();
                                cacheLine = this->m_cache.begin();
                            }

                            std::memcpy(cacheLine->data.data(), data.data(), data.size());
                        }

                        cacheLine++;
                        if (cacheLine == this->m_cache.end())
                            cacheLine = this->m_cache.begin();
                    }
                    std::this_thread::sleep_for(100ms);
                }
            });
        }
    }

    void GDBProvider::disconnect() {
        this->m_socket.disconnect();

        if (this->m_cacheUpdateThread.joinable()) {
            this->m_cacheUpdateThread.join();
        }
    }

    bool GDBProvider::isConnected() {
        return this->m_socket.isConnected();
    }

}