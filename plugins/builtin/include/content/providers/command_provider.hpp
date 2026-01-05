#pragma once

#include <hex/providers/provider.hpp>

#include <wolv/net/socket_client.hpp>

#include <fonts/vscode_icons.hpp>
#include <hex/providers/cached_provider.hpp>

namespace hex::plugin::builtin {

    class CommandProvider : public prv::CachedProvider,
                            public prv::IProviderLoadInterface {
    public:
        CommandProvider();
        ~CommandProvider() override = default;

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

        [[nodiscard]] OpenResult open() override;
        void close() override;

        bool drawLoadInterface() override;

        void loadSettings(const nlohmann::json &settings) override;
        [[nodiscard]] nlohmann::json storeSettings(nlohmann::json settings) const override;

        [[nodiscard]] UnlocalizedString getTypeName() const override {
            return "hex.builtin.provider.command";
        }

        [[nodiscard]] const char* getIcon() const override {
            return ICON_VS_TERMINAL_CMD;
        }

    protected:
        std::string m_name;
        std::string m_readCommand, m_writeCommand, m_sizeCommand, m_resizeCommand, m_saveCommand;
        bool m_open = false;
    };

}
