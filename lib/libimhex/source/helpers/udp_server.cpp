#include <hex/helpers/udp_server.hpp>

#if defined(OS_WINDOWS)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    using socklen_t = int;
#else
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
#endif

namespace hex {

    UDPServer::UDPServer(u16 port, Callback callback)
        : m_port(port), m_callback(std::move(callback)), m_running(false) {
    }

    UDPServer::~UDPServer() {
        stop();
    }

    void UDPServer::start() {
        m_running = true;
        m_thread = std::thread(&UDPServer::run, this);
    }

    void UDPServer::stop() {
        m_running = false;

        if (m_socketFd >= 0) {
            #if defined(OS_WINDOWS)
                ::closesocket(m_socketFd);
            #else
                ::close(m_socketFd);
            #endif
        }

        if (m_thread.joinable()) m_thread.join();
    }

    void UDPServer::run() {
        m_socketFd = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (m_socketFd < 0) {
            return;
        }

        sockaddr_in addr = { };
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(m_port);

        if (bind(m_socketFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            return;
        }

        std::vector<u8> buffer(64 * 1024, 0x00);
        while (m_running) {
            sockaddr_in client{};
            socklen_t len = sizeof(client);
            const auto bytes = ::recvfrom(m_socketFd, reinterpret_cast<char*>(buffer.data()), buffer.size(), 0, reinterpret_cast<sockaddr*>(&client), &len);

            if (bytes > 0) {
                buffer[bytes] = '\0';
                m_callback({ buffer.data(), buffer.data() + bytes });
            }
        }
    }
}