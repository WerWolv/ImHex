#pragma once

#include <hex.hpp>

#include <string>
#include <vector>

#if defined(OS_WINDOWS)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>

    #define SOCKET_NONE INVALID_SOCKET
#else
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/ip.h>
    #include <arpa/inet.h>

    #define SOCKET_NONE -1
#endif

namespace hex {

    class Socket {
    public:
        Socket()               = default;
        Socket(const Socket &) = delete;
        Socket(Socket &&other);

        Socket(const std::string &address, u16 port);
        ~Socket();

        void connect(const std::string &address, u16 port);
        void disconnect();

        [[nodiscard]] bool isConnected() const;

        std::string readString(size_t size = 0x1000) const;
        std::vector<u8> readBytes(size_t size = 0x1000) const;

        void writeString(const std::string &string) const;
        void writeBytes(const std::vector<u8> &bytes) const;

    private:
        bool m_connected = false;
#if defined(OS_WINDOWS)
        SOCKET m_socket = SOCKET_NONE;
#else
        int m_socket = SOCKET_NONE;
#endif
    };

}