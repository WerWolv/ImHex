#pragma once

#include <mutex>
#include <string>
#include <vector>
#include <span>

#if defined(OS_WINDOWS)
    #include <winsock2.h>
    using SocketType = SOCKET;
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    using SocketType = int;
#endif

#include <libssh2.h>
#include <libssh2_sftp.h>

#include <hex/helpers/fs.hpp>

namespace hex::plugin::remote {

    class SSHClient {
    public:
        SSHClient() = default;
        SSHClient(const std::string &host,
                   int port,
                   const std::string &user,
                   const std::string &password);

        SSHClient(const std::string &host,
                   int port,
                   const std::string &user,
                   const std::fs::path &privateKeyPath,
                   const std::string &passphrase);

        ~SSHClient();

        SSHClient(const SSHClient&) = delete;
        SSHClient& operator=(const SSHClient&) = delete;

        SSHClient(SSHClient &&other) noexcept;
        SSHClient& operator=(SSHClient &&other) noexcept;

        struct FsItem {
            std::string name;
            LIBSSH2_SFTP_ATTRIBUTES attributes;

            bool isDirectory() const {
                return (attributes.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) &&
                       (attributes.permissions & LIBSSH2_SFTP_S_IRWXU) == LIBSSH2_SFTP_S_IRWXU;
            }

            bool isRegularFile() const {
                return (attributes.flags & LIBSSH2_SFTP_ATTR_PERMISSIONS) &&
                       (attributes.permissions & LIBSSH2_SFTP_S_IFREG) == LIBSSH2_SFTP_S_IFREG;
            }
        };

        const std::vector<FsItem>& listDirectory(const std::fs::path& path);

        enum class OpenMode { Read, Write, ReadWrite };

        class RemoteFile {
        public:
            RemoteFile() = default;
            explicit RemoteFile(OpenMode mode) : m_mode(mode) {}
            virtual ~RemoteFile() {}

            RemoteFile(const RemoteFile&) = delete;
            RemoteFile& operator=(const RemoteFile&) = delete;
            RemoteFile(RemoteFile &&other) noexcept = delete;
            RemoteFile& operator=(RemoteFile &&other) noexcept = delete;

            [[nodiscard]] virtual bool isOpen() const = 0;

            [[nodiscard]] virtual size_t read(std::span<u8> buffer) = 0;
            [[nodiscard]] virtual size_t write(std::span<const u8> buffer) = 0;
            virtual void seek(uint64_t offset) = 0;
            [[nodiscard]] virtual u64 tell() const = 0;
            [[nodiscard]] virtual u64 size() const = 0;

            virtual bool eof() const = 0;
            virtual void flush() = 0;
            virtual void close() = 0;

            [[nodiscard]] OpenMode getOpenMode() const { return m_mode; }

        private:
            OpenMode m_mode = OpenMode::Read;
        };

        std::unique_ptr<RemoteFile> openFileSFTP(const std::fs::path& remotePath, OpenMode mode);
        std::unique_ptr<RemoteFile> openFileSSH(const std::fs::path& remotePath, OpenMode mode);
        void disconnect();

        [[nodiscard]] bool isConnected() const {
            return m_sftp != nullptr;
        }

        static void init();
        static void exit();

    private:
        void connect(const std::string &host, int port);
        void authenticatePassword(const std::string &user, const std::string &password);
        void authenticatePublicKey(const std::string &user, const std::fs::path &privateKeyPath, const std::string &passphrase);

        std::string getErrorString(LIBSSH2_SESSION* session) const;

    private:
        #if defined(OS_WINDOWS)
            SocketType m_sock = 0;
        #else
            SocketType m_sock = -1;
        #endif

        LIBSSH2_SESSION* m_session = nullptr;
        LIBSSH2_SFTP* m_sftp = nullptr;

        std::fs::path m_cachedDirectoryPath;
        std::vector<FsItem> m_cachedFsItems;
    };

    class RemoteFileSFTP : public SSHClient::RemoteFile {
    public:
        RemoteFileSFTP() = default;
        RemoteFileSFTP(LIBSSH2_SFTP_HANDLE* handle, SSHClient::OpenMode mode);
        ~RemoteFileSFTP() override;

        [[nodiscard]] bool isOpen() const override {
            return m_handle != nullptr;
        }

        [[nodiscard]] size_t read(std::span<u8> buffer) override;
        [[nodiscard]] size_t write(std::span<const u8> buffer) override;
        void seek(uint64_t offset) override;
        [[nodiscard]] u64 tell() const override;
        [[nodiscard]] u64 size() const override;

        bool eof() const override;
        void flush() override;
        void close() override;

    private:
        LIBSSH2_SFTP_HANDLE* m_handle = nullptr;
        bool m_atEOF = false;
    };

    class RemoteFileSSH : public SSHClient::RemoteFile {
    public:
        RemoteFileSSH() = default;
        RemoteFileSSH(LIBSSH2_SESSION* handle, std::string path, SSHClient::OpenMode mode);
        ~RemoteFileSSH() override;

        [[nodiscard]] bool isOpen() const override {
            return m_handle != nullptr;
        }

        [[nodiscard]] size_t read(std::span<u8> buffer) override;
        [[nodiscard]] size_t write(std::span<const u8> buffer) override;
        void seek(uint64_t offset) override;
        [[nodiscard]] u64 tell() const override;
        [[nodiscard]] u64 size() const override;

        bool eof() const override;
        void flush() override;
        void close() override;

    private:
        std::vector<u8> executeCommand(const std::string &command, std::span<const u8> writeData = {}) const;

    private:
        LIBSSH2_SESSION *m_handle = nullptr;
        bool m_atEOF = false;
        u64 m_seekPosition = 0x00;
        std::string m_readCommand, m_writeCommand, m_sizeCommand;
        mutable std::mutex m_mutex;
    };

}
