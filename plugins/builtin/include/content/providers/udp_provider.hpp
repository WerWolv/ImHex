#pragma once

#include <hex/helpers/fmt.hpp>
#include <hex/providers/provider.hpp>
#include <hex/helpers/udp_server.hpp>
#include <nlohmann/json.hpp>
#include <mutex>
#include <fonts/vscode_icons.hpp>

namespace hex::plugin::builtin {

    class UDPProvider : public hex::prv::Provider,
                        public hex::prv::IProviderSidebarInterface,
                        public hex::prv::IProviderLoadInterface {
    public:
        UDPProvider() = default;
        ~UDPProvider() override = default;

        [[nodiscard]] bool isAvailable() const override { return true; }
        [[nodiscard]] bool isReadable() const override { return true; }
        [[nodiscard]] bool isWritable() const override { return false; }
        [[nodiscard]] bool isResizable() const override { return false; }
        [[nodiscard]] bool isSavable() const override { return true; }

        void readRaw(u64 offset, void *buffer, size_t size) override;
        void writeRaw(u64 offset, const void *buffer, size_t size) override;

        [[nodiscard]] u64 getActualSize() const override;

        [[nodiscard]] bool drawLoadInterface() override;
        void drawSidebarInterface() override;

        [[nodiscard]] OpenResult open() override;
        void close() override;

        void loadSettings(const nlohmann::json &) override;
        [[nodiscard]] nlohmann::json storeSettings(nlohmann::json) const override;

        [[nodiscard]] UnlocalizedString getTypeName() const override {
            return "hex.builtin.provider.udp";
        }

        [[nodiscard]] const char* getIcon() const override {
            return ICON_VS_RSS;
        }

        std::string getName() const override { return fmt::format("hex.builtin.provider.udp.name"_lang, m_port); }

    protected:
        void receive(std::span<const u8> data);

    private:
        UDPServer m_udpServer;
        int m_port = 0;

        struct Message {
            std::vector<u8> data;
            std::chrono::system_clock::time_point timestamp;
        };

        mutable std::mutex m_mutex;
        std::vector<Message> m_messages;
        u64 m_selectedMessage = 0;
    };

}
