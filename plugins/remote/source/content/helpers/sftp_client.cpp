#include <content/helpers/sftp_client.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>
#include <wolv/utils/string.hpp>

#include <array>
#include <charconv>

#if defined(OS_WINDOWS)
    #include <ws2tcpip.h>
#endif

namespace hex::plugin::remote {

    void SSHClient::init() {
        libssh2_init(0);
    }

    void SSHClient::exit() {
        libssh2_exit();
    }

    SSHClient::SSHClient(const std::string &host, int port, const std::string &user, const std::string &password) {
        connect(host, port);
        authenticatePassword(user, password);

        m_sftp = libssh2_sftp_init(m_session);
        if (!m_sftp)
            throw std::runtime_error("Failed to initialize SFTP session");
    }

    SSHClient::SSHClient(const std::string &host, int port, const std::string &user, const std::fs::path &publicKeyPath, const std::string &passphrase) {
        connect(host, port);
        authenticatePublicKey(user, publicKeyPath, passphrase);

        m_sftp = libssh2_sftp_init(m_session);
        if (!m_sftp)
            throw std::runtime_error("Failed to initialize SFTP session");
    }

    SSHClient::~SSHClient() {
        if (m_sftp) libssh2_sftp_shutdown(m_sftp);
        if (m_session) {
            libssh2_session_disconnect(m_session, "Normal Shutdown");
            libssh2_session_free(m_session);
        }
        #if defined(OS_WINDOWS)
            if (m_sock != INVALID_SOCKET) closesocket(m_sock);
        #else
            if (m_sock != -1) close(m_sock);
        #endif
    }

    SSHClient::SSHClient(SSHClient &&other) noexcept {
        m_sftp = other.m_sftp;
        other.m_sftp = nullptr;

        m_session = other.m_session;
        other.m_session = nullptr;

        m_sock = other.m_sock;
        #if defined(OS_WINDOWS)
            other.m_sock = INVALID_SOCKET;
        #else
            other.m_sock = -1;
        #endif
    }

    SSHClient& SSHClient::operator=(SSHClient &&other) noexcept {
        if (this != &other) {
            if (m_sftp) libssh2_sftp_shutdown(m_sftp);
            if (m_session) {
                libssh2_session_disconnect(m_session, "Normal Shutdown");
                libssh2_session_free(m_session);
            }
            #if defined(OS_WINDOWS)
                if (m_sock != INVALID_SOCKET) closesocket(m_sock);
            #else
                if (m_sock != -1) close(m_sock);
            #endif

            m_sftp = other.m_sftp;
            other.m_sftp = nullptr;

            m_session = other.m_session;
            other.m_session = nullptr;

            m_sock = other.m_sock;
            #if defined(OS_WINDOWS)
                other.m_sock = INVALID_SOCKET;
            #else
                other.m_sock = -1;
            #endif
        }

        return *this;
    }


    void SSHClient::connect(const std::string &host, int port) {
        addrinfo hints = {}, *res;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        auto service = std::to_string(port);
        if (getaddrinfo(host.c_str(), service.c_str(), &hints, &res) != 0)
            throw std::runtime_error("getaddrinfo failed");

        m_sock = ::socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        #if defined(OS_WINDOWS)
            if (m_sock == INVALID_SOCKET)
        #else
            if (m_sock == -1)
        #endif
                throw std::runtime_error("Socket creation failed");

        if (::connect(m_sock, res->ai_addr, res->ai_addrlen) != 0) {
            #if defined(OS_WINDOWS)
                closesocket(m_sock);
            #else
                close(m_sock);
            #endif

            freeaddrinfo(res);
            throw std::runtime_error("Connection to host failed");
        }

        freeaddrinfo(res);

        m_session = libssh2_session_init();
        if (!m_session)
            throw std::runtime_error("SSH session init failed");

        libssh2_session_set_blocking(m_session, true);

        if (libssh2_session_handshake(m_session, m_sock))
            throw std::runtime_error("SSH handshake failed: " + getErrorString(m_session));
    }

    void SSHClient::authenticatePassword(const std::string &user, const std::string &password) {
        if (libssh2_userauth_password(m_session, user.c_str(), password.c_str()))
            throw std::runtime_error("Authentication failed: " + getErrorString(m_session));
    }

    void SSHClient::authenticatePublicKey(const std::string &user, const std::fs::path &privateKeyPath, const std::string &) {
        auto result = libssh2_userauth_publickey_fromfile(m_session, user.c_str(), nullptr, wolv::util::toUTF8String(privateKeyPath).c_str(), nullptr);
        if (result)
            throw std::runtime_error("Authentication failed: " + getErrorString(m_session));
    }


    const std::vector<SSHClient::FsItem>& SSHClient::listDirectory(const std::fs::path &path) {
        if (m_sftp == nullptr)
            return m_cachedFsItems;

        if (path == m_cachedDirectoryPath)
            return m_cachedFsItems;

        m_cachedFsItems.clear();
        m_cachedDirectoryPath = path;

        auto pathString = wolv::util::toUTF8String(path);
        LIBSSH2_SFTP_HANDLE* dir = libssh2_sftp_opendir(m_sftp, pathString.c_str());
        if (!dir)
            return m_cachedFsItems;

        std::array<char, 512> buffer;
        LIBSSH2_SFTP_ATTRIBUTES attrs;

        while (libssh2_sftp_readdir(dir, buffer.data(), buffer.size(), &attrs) > 0) {
            auto nameString = std::string_view(buffer.data());
            if (nameString == "." || nameString == "..")
                continue;

            m_cachedFsItems.emplace_back(nameString.data(), attrs);
        }

        libssh2_sftp_closedir(dir);

        // Sort the items by name, directories first
        std::sort(m_cachedFsItems.begin(), m_cachedFsItems.end(), [](const FsItem &a, const FsItem &b) {
            if (a.isDirectory() && !b.isDirectory())
                return true;
            if (!a.isDirectory() && b.isDirectory())
                return false;
            return a.name < b.name;
        });


        return m_cachedFsItems;
    }

    std::unique_ptr<SSHClient::RemoteFile> SSHClient::openFileSFTP(const std::fs::path &remotePath, OpenMode mode) {
        int flags = 0;

        switch (mode) {
            case OpenMode::Read:
                flags = LIBSSH2_FXF_READ;
                break;
            case OpenMode::Write:
                flags = LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC;
                break;
            case OpenMode::ReadWrite:
                flags = LIBSSH2_FXF_READ | LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT;
                break;
        }

        auto pathString = wolv::util::toUTF8String(remotePath);
        LIBSSH2_SFTP_HANDLE* handle = libssh2_sftp_open(m_sftp, pathString.c_str(), flags, 0);
        if (!handle) {
            long sftpError = libssh2_sftp_last_error(m_sftp);
            if (mode != OpenMode::Read && sftpError == LIBSSH2_FX_PERMISSION_DENIED) {
                return openFileSFTP(remotePath, OpenMode::Read);
            } else {
                throw std::runtime_error("Failed to open remote file '" + pathString +
                                         "' - " + getErrorString(m_session) +
                                         " (SFTP error: " + std::to_string(sftpError) + ")");
            }
        }

        return std::make_unique<RemoteFileSFTP>(handle, mode);
    }

    std::unique_ptr<SSHClient::RemoteFile> SSHClient::openFileSSH(const std::fs::path &remotePath, OpenMode mode) {
        auto pathString = wolv::util::toUTF8String(remotePath);
        return std::make_unique<RemoteFileSSH>(m_session, pathString, mode);
    }

    void SSHClient::disconnect() {
        if (m_sftp != nullptr) {
            libssh2_sftp_shutdown(m_sftp);
            m_sftp = nullptr;
        }

        if (m_session != nullptr) {
            libssh2_session_disconnect(m_session, "Disconnecting");
            libssh2_session_free(m_session);
            m_session = nullptr;
        }
    }


    std::string SSHClient::getErrorString(LIBSSH2_SESSION* session) const {
        char *errorString;
        int length = 0;
        libssh2_session_last_error(session, &errorString, &length, false);

        return fmt::format("{} ({})", std::string(errorString, static_cast<size_t>(length)), libssh2_session_last_errno(session));
    }

    RemoteFileSFTP::RemoteFileSFTP(LIBSSH2_SFTP_HANDLE* handle, SSHClient::OpenMode mode) : RemoteFile(mode), m_handle(handle) {}

    RemoteFileSFTP::~RemoteFileSFTP() {
        if (m_handle) {
            libssh2_sftp_close(m_handle);
            m_handle = nullptr;
        }
    }

    size_t RemoteFileSFTP::read(std::span<u8> buffer) {
        auto dataSize = this->size();
        auto offset = this->tell();

        if (offset > dataSize || buffer.empty())
            return 0;

        ssize_t n = libssh2_sftp_read(m_handle, reinterpret_cast<char*>(buffer.data()), std::min<u64>(buffer.size_bytes(), dataSize - offset));
        if (n < 0)
            return 0;
        if (n == 0)
            m_atEOF = true;

        return static_cast<size_t>(n);
    }

    size_t RemoteFileSFTP::write(std::span<const u8> buffer) {
        if (buffer.empty())
            return 0;

        ssize_t n = libssh2_sftp_write(m_handle, reinterpret_cast<const char*>(buffer.data()), buffer.size_bytes());
        if (n < 0)
            return 0;

        return static_cast<size_t>(n);
    }

    void RemoteFileSFTP::seek(uint64_t offset) {
        libssh2_sftp_seek64(m_handle, offset);
        m_atEOF = false;
    }

    u64 RemoteFileSFTP::tell() const {
        return libssh2_sftp_tell64(m_handle);
    }

    u64 RemoteFileSFTP::size() const {
        LIBSSH2_SFTP_ATTRIBUTES attrs = {};
        if (libssh2_sftp_fstat(m_handle, &attrs) != 0)
            return 0;

        return attrs.filesize;
    }

    bool RemoteFileSFTP::eof() const {
        return m_atEOF;
    }

    void RemoteFileSFTP::flush() {
        libssh2_sftp_fsync(m_handle);
    }

    void RemoteFileSFTP::close() {
        libssh2_sftp_close(m_handle);
        m_handle = nullptr;
    }


    RemoteFileSSH::RemoteFileSSH(LIBSSH2_SESSION *handle, std::string path, SSHClient::OpenMode mode)
        : RemoteFile(mode), m_handle(handle) {
        m_readCommand  = fmt::format("dd if=\"{0}\" skip={{0}} count={{1}} bs=1", path);
        m_writeCommand = fmt::format("dd of=\"{0}\" seek={{0}} count={{1}} bs=1 conv=notrunc", path);
        m_sizeCommand  = "stat -c%s {0}";
    }

    RemoteFileSSH::~RemoteFileSSH() {
        if (m_handle) {
            m_handle = nullptr;
        }
    }

    size_t RemoteFileSSH::read(std::span<u8> buffer) {
        auto offset = this->tell();

        if (buffer.empty())
            return 0;

        auto result = executeCommand(fmt::format(fmt::runtime(m_readCommand), offset, buffer.size()));

        auto size = std::min(result.size(), buffer.size());
        std::memcpy(buffer.data(), result.data(), size);

        return size;
    }

    size_t RemoteFileSSH::write(std::span<const u8> buffer) {
        auto offset = this->tell();

        if (buffer.empty())
            return 0;

        // Send data via STDIN to dd command remotely
        auto bytesWritten = executeCommand(
            fmt::format(fmt::runtime(m_writeCommand), offset, buffer.size()),
            buffer
        );

        return buffer.size();
    }

    void RemoteFileSSH::seek(uint64_t offset) {
        m_seekPosition = offset;
    }

    u64 RemoteFileSSH::tell() const {
        return m_seekPosition;
    }

    u64 RemoteFileSSH::size() const {
        auto bytes = executeCommand(m_sizeCommand);

        u64 size = 0;
        if (std::from_chars(reinterpret_cast<char*>(bytes.data()), reinterpret_cast<char*>(bytes.data() + bytes.size()), size).ec != std::errc()) {
            return 0;
        }

        return size;
    }

    bool RemoteFileSSH::eof() const {
        return m_atEOF;
    }

    void RemoteFileSSH::flush() {
        // Nothing to do
    }

    void RemoteFileSSH::close() {
        m_handle = nullptr;
    }

    std::vector<u8> RemoteFileSSH::executeCommand(const std::string &command, std::span<const u8> writeData) const {
        std::lock_guard lock(m_mutex);

        LIBSSH2_CHANNEL* channel = libssh2_channel_open_session(m_handle);
        if (!channel) {
            return {};
        }

        ON_SCOPE_EXIT {
            libssh2_channel_close(channel);
            libssh2_channel_free(channel);
        };

        if (libssh2_channel_exec(channel, command.c_str()) != 0) {
            return {};
        }

        if (!writeData.empty()) {
            libssh2_channel_write(channel, reinterpret_cast<const char*>(writeData.data()), writeData.size());
        }

        std::vector<u8> result;
        std::array<char, 1024> buffer;
        while (true) {
            const auto rc = libssh2_channel_read(channel, buffer.data(), buffer.size());
            if (rc > 0) {
                result.insert(result.end(), buffer.data(), buffer.data() + rc);
            } else if (rc == LIBSSH2_ERROR_EAGAIN) {
                continue;
            } else {
                break;
            }
        }

        libssh2_channel_send_eof(channel);

        return result;
    }

}
