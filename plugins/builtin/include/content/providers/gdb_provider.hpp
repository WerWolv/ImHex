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

        [[nodiscard]] bool isAvailable() const override;
        [[nodiscard]] bool isReadable() const override;
        [[nodiscard]] bool isWritable() const override;
        [[nodiscard]] bool isResizable() const override;
        [[nodiscard]] bool isSavable() const override;

        void read(u64 offset, void *buffer, size_t size, bool overlays) override;
        void write(u64 offset, const void *buffer, size_t size) override;
        void resize(ssize_t newSize) override;

        void readRaw(u64 offset, void *buffer, size_t size) override;
        void writeRaw(u64 offset, const void *buffer, size_t size) override;
        [[nodiscard]] size_t getActualSize() const override;

        void save() override;
        void saveAs(const fs::path &path) override;

        [[nodiscard]] std::string getName() const override;
        [[nodiscard]] std::vector<std::pair<std::string, std::string>> getDataInformation() const override;

        [[nodiscard]] bool open() override;
        void close() override;

        [[nodiscard]] bool isConnected() const;

        [[nodiscard]] bool hasLoadInterface() const override { return true; }
        void drawLoadInterface() override;

    protected:
        hex::Socket m_socket;

        std::string m_ipAddress;
        int m_port;

        constexpr static size_t CacheLineSize = 0x1000;

        struct CacheLine {
            u64 address;

            std::array<u8, CacheLineSize> data;
        };

        std::list<CacheLine> m_cache;

        std::thread m_cacheUpdateThread;
        std::mutex m_cacheLock;
    };

}