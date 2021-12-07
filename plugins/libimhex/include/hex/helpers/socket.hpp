#pragma once

#include <hex.hpp>

#include <string>
#include <vector>

#if defined(OS_WINDOWS)
    #include <winsock2.h>
    #include <windows.h>
#else
    #include <unistd.h>
    #include <sys/sock.h>
#endif

namespace hex {

    class Socket {
    public:
        Socket() = default;
        Socket(const Socket&) = delete;
        Socket(Socket &&other);

        Socket(const std::string &address, u16 port);
        ~Socket();

        void connect(const std::string &address, u16 port);
        void disconnect();

        [[nodiscard]]
        bool isConnected() const;

        std::string readString(size_t size = 0x1000) const;
        std::vector<u8> readBytes(size_t size = 0x1000) const;

        void writeString(const std::string &string) const;
        void writeBytes(const std::vector<u8> &bytes) const;

    private:
        bool m_connected = false;
        #if defined(OS_WINDOWS)
            SOCKET m_socket = INVALID_SOCKET;
        #else
            int m_socket = -1;
        #endif
    };

}