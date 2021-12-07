#pragma once

#include <hex/helpers/socket.hpp>
#include <hex/providers/provider.hpp>

#include <mutex>
#include <string_view>
#include <thread>

namespace hex::plugin::builtin::prv {

class GDBProvider : public hex::prv::Provider {
    public:
        explicit GDBProvider();
        ~GDBProvider() override;

        bool isAvailable() const override;
        bool isReadable() const override;
        bool isWritable() const override;
        bool isResizable() const override;
        bool isSavable() const override;

        void read(u64 offset, void *buffer, size_t size, bool overlays) override;
        void write(u64 offset, const void *buffer, size_t size) override;
        void resize(ssize_t newSize) override;

        void readRaw(u64 offset, void *buffer, size_t size) override;
        void writeRaw(u64 offset, const void *buffer, size_t size) override;
        size_t getActualSize() const override;

        void save() override;
        void saveAs(const std::string &path) override;

        [[nodiscard]] std::string getName() const override;
        [[nodiscard]] std::vector<std::pair<std::string, std::string>> getDataInformation() const override;

        void connect(const std::string &address, u16 port);
        void disconnect();
        bool isConnected();

    private:
        hex::Socket m_socket;

        std::string m_ipAddress;
        u16 m_port;

        struct CacheLine {
            u64 address;

            std::array<u8, 0x1000> data;
        };

        std::list<CacheLine> m_cache;

        std::jthread m_cacheUpdateThread;
        std::mutex m_cacheLock;
    };

}