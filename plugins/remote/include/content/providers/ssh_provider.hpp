#pragma once

#include <content/helpers/sftp_client.hpp>
#include <fonts/vscode_icons.hpp>
#include <hex/providers/cached_provider.hpp>

namespace hex::plugin::remote {

    class SSHProvider : public prv::CachedProvider,
                        public prv::IProviderLoadInterface {
    public:
        bool isAvailable() const override { return m_remoteFile != nullptr && m_remoteFile->isOpen(); }
        bool isReadable() const override  { return isAvailable(); }
        bool isWritable() const override  { return m_remoteFile != nullptr && m_remoteFile->getOpenMode() != SSHClient::OpenMode::Read; }
        bool isResizable() const override { return false; }
        bool isSavable() const override   { return isWritable(); }

        OpenResult open() override;
        void close() override;
        void save() override;

        void readFromSource(uint64_t offset, void* buffer, size_t size) override;
        void writeToSource(uint64_t offset, const void* buffer, size_t size) override;

        u64 getSourceSize() const override;
        UnlocalizedString getTypeName() const override { return "hex.plugin.remote.ssh_provider"; }
        std::string getName() const override;

        [[nodiscard]] const char* getIcon() const override {
            return ICON_VS_REMOTE;
        }

        bool drawLoadInterface() override;

        void loadSettings(const nlohmann::json &settings) override;
        nlohmann::json storeSettings(nlohmann::json settings) const override;

        enum class AuthMethod {
            Password,
            KeyFile
        };

    private:
        SSHClient m_sftpClient;
        std::unique_ptr<SSHClient::RemoteFile> m_remoteFile;

        std::string m_host;
        int m_port = 22;
        std::string m_username;
        std::string m_password;
        std::fs::path m_privateKeyPath;
        std::string m_keyPassphrase;
        AuthMethod m_authMethod = AuthMethod::Password;

        bool m_selectedFile = false;
        bool m_accessFileOverSSH = false;
        std::fs::path m_remoteFilePath = { "/", std::fs::path::format::generic_format };
    };

}