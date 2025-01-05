#pragma once

#include <hex/providers/provider.hpp>

#include <wolv/net/socket_client.hpp>

#include <array>
#include <mutex>
#include <string_view>
#include <thread>

namespace hex::plugin::builtin {

    class GDBProvider : public hex::prv::Provider {
    public:
        GDBProvider();
        ~GDBProvider() override = default;

        [[nodiscard]] bool isAvailable() const override;
        [[nodiscard]] bool isReadable() const override;
        [[nodiscard]] bool isWritable() const override;
        [[nodiscard]] bool isResizable() const override;
        [[nodiscard]] bool isSavable() const override;

        void readRaw(u64 offset, void *buffer, size_t size) override;
        void writeRaw(u64 offset, const void *buffer, size_t size) override;
        [[nodiscard]] u64 getActualSize() const override;

        void save() override;

        [[nodiscard]] std::string getName() const override;
        [[nodiscard]] std::vector<Description> getDataDescription() const override;

        [[nodiscard]] bool open() override;
        void close() override;

        [[nodiscard]] bool isConnected() const;

        [[nodiscard]] bool hasLoadInterface() const override { return true; }
        bool drawLoadInterface() override;

        void loadSettings(const nlohmann::json &settings) override;
        [[nodiscard]] nlohmann::json storeSettings(nlohmann::json settings) const override;

        [[nodiscard]] UnlocalizedString getTypeName() const override {
            return "hex.builtin.provider.gdb";
        }

        [[nodiscard]] std::pair<Region, bool> getRegionValidity(u64 address) const override;
        std::variant<std::string, i128> queryInformation(const std::string &category, const std::string &argument) override;

    protected:
        wolv::net::SocketClient m_socket;

        std::string m_ipAddress;
        int m_port = 0;

        u64 m_size = 0;

        constexpr static size_t CacheLineSize = 0x10;

        struct CacheLine {
            u64 address;

            std::array<u8, CacheLineSize> data;
        };

        std::list<CacheLine> m_cache;
        std::atomic<bool> m_resetCache = false;

        std::thread m_cacheUpdateThread;
        std::mutex m_cacheLock;
    };

}