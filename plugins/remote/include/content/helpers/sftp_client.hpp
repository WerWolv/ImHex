#pragma once

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

    class SFTPClient {
    public:
        SFTPClient() = default;
        SFTPClient(const std::string &host,
                   int port,
                   const std::string &user,
                   const std::string &password);

        SFTPClient(const std::string &host,
                   int port,
                   const std::string &user,
                   const std::string &privateKeyPath,
                   const std::string &passphrase);

        ~SFTPClient();

        SFTPClient(const SFTPClient&) = delete;
        SFTPClient& operator=(const SFTPClient&) = delete;

        SFTPClient(SFTPClient &&other) noexcept;
        SFTPClient& operator=(SFTPClient &&other) noexcept;

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
            RemoteFile(LIBSSH2_SFTP_HANDLE* handle, OpenMode mode);
            ~RemoteFile();

            RemoteFile(const RemoteFile&) = delete;
            RemoteFile& operator=(const RemoteFile&) = delete;
            RemoteFile(RemoteFile &&other) noexcept;
            RemoteFile& operator=(RemoteFile &&other) noexcept;

            [[nodiscard]] bool isOpen() const {
                return m_handle != nullptr;
            }

            [[nodiscard]] size_t read(std::span<u8> buffer);
            [[nodiscard]] size_t write(std::span<const u8> buffer);
            void seek(uint64_t offset);
            [[nodiscard]] u64 tell() const;
            [[nodiscard]] u64 size() const;

            bool eof() const;
            void flush();
            void close();

            [[nodiscard]] OpenMode getOpenMode() const { return m_mode; }

        private:
            LIBSSH2_SFTP_HANDLE* m_handle = nullptr;
            bool m_atEOF = false;
            OpenMode m_mode = OpenMode::Read;
        };

        RemoteFile openFile(const std::fs::path& remotePath, OpenMode mode);
        void disconnect();

        [[nodiscard]] bool isConnected() const {
            return m_sftp != nullptr;
        }

        static void init();
        static void exit();

    private:
        void connect(const std::string &host, int port);
        void authenticatePassword(const std::string &user, const std::string &password);
        void authenticatePublicKey(const std::string &user, const std::string &privateKeyPath, const std::string &passphrase);

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

}
