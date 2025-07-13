#include <content/helpers/sftp_client.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>
#include <wolv/utils/string.hpp>

#include <array>

#if defined(OS_WINDOWS)
    #include <ws2tcpip.h>
#endif

namespace hex::plugin::remote {

    void SFTPClient::init() {
        libssh2_init(0);
    }

    void SFTPClient::exit() {
        libssh2_exit();
    }

    SFTPClient::SFTPClient(const std::string &host, int port, const std::string &user, const std::string &password) {
        connect(host, port);
        authenticatePassword(user, password);

        m_sftp = libssh2_sftp_init(m_session);
        if (!m_sftp)
            throw std::runtime_error("Failed to initialize SFTP session");
    }

    SFTPClient::SFTPClient(const std::string &host, int port, const std::string &user, const std::string &publicKeyPath, const std::string &passphrase) {
        connect(host, port);
        authenticatePublicKey(user, publicKeyPath, passphrase);

        m_sftp = libssh2_sftp_init(m_session);
        if (!m_sftp)
            throw std::runtime_error("Failed to initialize SFTP session");
    }

    SFTPClient::~SFTPClient() {
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

    SFTPClient::SFTPClient(SFTPClient &&other) noexcept {
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

    SFTPClient& SFTPClient::operator=(SFTPClient &&other) noexcept {
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


    void SFTPClient::connect(const std::string &host, int port) {
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

    void SFTPClient::authenticatePassword(const std::string &user, const std::string &password) {
        if (libssh2_userauth_password(m_session, user.c_str(), password.c_str()))
            throw std::runtime_error("Authentication failed: " + getErrorString(m_session));
    }

    void SFTPClient::authenticatePublicKey(const std::string &user, const std::string &privateKeyPath, const std::string &) {
        auto result = libssh2_userauth_publickey_fromfile(m_session, user.c_str(), nullptr, privateKeyPath.c_str(), nullptr);
        if (result)
            throw std::runtime_error("Authentication failed: " + getErrorString(m_session));
    }


    const std::vector<SFTPClient::FsItem>& SFTPClient::listDirectory(const std::fs::path &path) {
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

    SFTPClient::RemoteFile SFTPClient::openFile(const std::fs::path &remotePath, OpenMode mode) {
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
                return openFile(remotePath, OpenMode::Read);
            } else {
                throw std::runtime_error("Failed to open remote file '" + pathString +
                                         "' - " + getErrorString(m_session) +
                                         " (SFTP error: " + std::to_string(sftpError) + ")");
            }
        }

        return RemoteFile(handle, mode);
    }

    void SFTPClient::disconnect() {
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


    std::string SFTPClient::getErrorString(LIBSSH2_SESSION* session) const {
        char *errorString;
        int length = 0;
        libssh2_session_last_error(session, &errorString, &length, false);

        return hex::format("{} ({})", std::string(errorString, static_cast<size_t>(length)), libssh2_session_last_errno(session));
    }

    SFTPClient::RemoteFile::RemoteFile(LIBSSH2_SFTP_HANDLE* handle, OpenMode mode) : m_handle(handle), m_mode(mode) {}

    SFTPClient::RemoteFile::~RemoteFile() {
        if (m_handle) {
            libssh2_sftp_close(m_handle);
            m_handle = nullptr;
        }
    }

    SFTPClient::RemoteFile::RemoteFile(RemoteFile &&other) noexcept
        : m_handle(other.m_handle), m_atEOF(other.m_atEOF), m_mode(other.m_mode) {
        other.m_handle = nullptr;
    }
    SFTPClient::RemoteFile& SFTPClient::RemoteFile::operator=(RemoteFile &&other) noexcept {
        if (this != &other) {
            if (m_handle) libssh2_sftp_close(m_handle);
            m_handle = other.m_handle;
            m_atEOF = other.m_atEOF;
            m_mode = other.m_mode;
            other.m_handle = nullptr;
        }
        return *this;
    }

    size_t SFTPClient::RemoteFile::read(std::span<u8> buffer) {
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

    size_t SFTPClient::RemoteFile::write(std::span<const u8> buffer) {
        if (buffer.empty())
            return 0;

        ssize_t n = libssh2_sftp_write(m_handle, reinterpret_cast<const char*>(buffer.data()), buffer.size_bytes());
        if (n < 0)
            return 0;

        return static_cast<size_t>(n);
    }

    void SFTPClient::RemoteFile::seek(uint64_t offset) {
        libssh2_sftp_seek64(m_handle, offset);
        m_atEOF = false;
    }

    uint64_t SFTPClient::RemoteFile::tell() const {
        return libssh2_sftp_tell64(m_handle);
    }

    u64 SFTPClient::RemoteFile::size() const {
        LIBSSH2_SFTP_ATTRIBUTES attrs = {};
        if (libssh2_sftp_fstat(m_handle, &attrs) != 0)
            return 0;

        return attrs.filesize;
    }

    bool SFTPClient::RemoteFile::eof() const {
        return m_atEOF;
    }

    void SFTPClient::RemoteFile::flush() {
        libssh2_sftp_fsync(m_handle);
    }

    void SFTPClient::RemoteFile::close() {
        libssh2_sftp_close(m_handle);
        m_handle = nullptr;
    }


}