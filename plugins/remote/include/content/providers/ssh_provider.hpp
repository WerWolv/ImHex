#pragma once

#include <content/helpers/sftp_client.hpp>
#include <hex/providers/cached_provider.hpp>

namespace hex::plugin::remote {

    class SSHProvider : public hex::prv::CachedProvider {
    public:
        bool isAvailable() const override { return m_remoteFile.isOpen(); }
        bool isReadable() const override  { return isAvailable(); }
        bool isWritable() const override  { return m_remoteFile.getOpenMode() != SFTPClient::OpenMode::Read; }
        bool isResizable() const override { return false; }
        bool isSavable() const override   { return isWritable(); }

        bool open() override;
        void close() override;
        void save() override;

        void readFromSource(uint64_t offset, void* buffer, size_t size) override;
        void writeToSource(uint64_t offset, const void* buffer, size_t size) override;

        u64 getSourceSize() const override;
        UnlocalizedString getTypeName() const override { return "hex.plugin.remote.ssh_provider"; }
        std::string getName() const override;

        bool drawLoadInterface() override;
        bool hasLoadInterface() const override { return true; }

        void loadSettings(const nlohmann::json &settings) override;
        nlohmann::json storeSettings(nlohmann::json settings) const override;

        enum class AuthMethod {
            Password,
            KeyFile
        };

    private:
        SFTPClient m_sftpClient;
        SFTPClient::RemoteFile m_remoteFile;

        std::string m_host;
        int m_port = 22;
        std::string m_username;
        std::string m_password;
        std::string m_privateKeyPath;
        std::string m_keyPassphrase;
        AuthMethod m_authMethod = AuthMethod::Password;

        bool m_selectedFile = false;
        std::fs::path m_remoteFilePath = { "/", std::fs::path::format::generic_format };
    };

}