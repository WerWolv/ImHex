#pragma once

#include <hex/providers/provider.hpp>

#include <wolv/net/socket_client.hpp>

#include <array>
#include <mutex>
#include <string_view>
#include <thread>
#include <fonts/vscode_icons.hpp>
#include <hex/providers/cached_provider.hpp>

namespace hex::plugin::builtin {

    class GDBProvider : public prv::CachedProvider,
                        public prv::IProviderDataDescription,
                        public prv::IProviderLoadInterface {
    public:
        GDBProvider();
        ~GDBProvider() override = default;

        [[nodiscard]] bool isAvailable() const override;
        [[nodiscard]] bool isReadable() const override;
        [[nodiscard]] bool isWritable() const override;
        [[nodiscard]] bool isResizable() const override;
        [[nodiscard]] bool isSavable() const override;

        void readFromSource(u64 offset, void *buffer, size_t size) override;
        void writeToSource(u64 offset, const void *buffer, size_t size) override;
        [[nodiscard]] u64 getSourceSize() const override;

        void save() override;

        [[nodiscard]] std::string getName() const override;
        [[nodiscard]] std::vector<Description> getDataDescription() const override;

        [[nodiscard]] OpenResult open() override;
        void close() override;

        [[nodiscard]] bool isConnected() const;

        bool drawLoadInterface() override;

        void loadSettings(const nlohmann::json &settings) override;
        [[nodiscard]] nlohmann::json storeSettings(nlohmann::json settings) const override;

        [[nodiscard]] UnlocalizedString getTypeName() const override {
            return "hex.builtin.provider.gdb";
        }

        [[nodiscard]] const char* getIcon() const override {
            return ICON_VS_CHIP;
        }

        [[nodiscard]] std::pair<Region, bool> getRegionValidity(u64 address) const override;
        std::variant<std::string, i128> queryInformation(const std::string &category, const std::string &argument) override;

    protected:
        wolv::net::SocketClient m_socket;

        std::string m_ipAddress;
        int m_port = 0;
        std::mutex m_mutex;

        u64 m_size = 0;
    };

}
