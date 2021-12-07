#include <hex/helpers/socket.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/logger.hpp>

#include <ws2tcpip.h>

namespace hex {

    Socket::Socket(const std::string &address, u16 port) {
        #if defined(OS_WINDOWS)
            FIRST_TIME {
                WSAData wsa;
                WSAStartup(MAKEWORD(2, 2), &wsa);
            };

            FINAL_CLEANUP {
                WSACleanup();
            };
        #endif

        this->connect(address, port);
    }

    Socket::Socket(Socket &&other) {
        this->m_socket = other.m_socket;
        this->m_connected = other.m_connected;

        other.m_socket = INVALID_SOCKET;
    }

    Socket::~Socket() {
        this->disconnect();
    }

    void Socket::writeBytes(const std::vector<u8> &bytes) const {
        if (!this->isConnected()) return;

        ::send(this->m_socket, reinterpret_cast<const char*>(bytes.data()), bytes.size(), 0);
    }

    void Socket::writeString(const std::string &string) const {
        if (!this->isConnected()) return;

        ::send(this->m_socket, string.c_str(), string.length(), 0);
    }

    std::vector<u8> Socket::readBytes(size_t size) const {
        std::vector<u8> data;
        data.resize(size);

        auto receivedSize = ::recv(this->m_socket, reinterpret_cast<char *>(data.data()), size, 0);

        if (receivedSize < 0)
            return { };

        data.resize(receivedSize);

        return data;
    }

    std::string Socket::readString(size_t size) const {
        auto bytes = readBytes(size);

        std::string result;
        std::copy(bytes.begin(), bytes.end(), std::back_inserter(result));

        return result;
    }

    bool Socket::isConnected() const {
        return this->m_connected;
    }

    void Socket::connect(const std::string &address, u16 port) {
        this->m_socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (this->m_socket == INVALID_SOCKET)
            return;

        sockaddr_in client = { 0 };

        client.sin_family = AF_INET;
        client.sin_addr.S_un.S_addr = ::inet_addr(address.c_str());
        client.sin_port = ::htons(port);

        this->m_connected = ::connect(this->m_socket, reinterpret_cast<SOCKADDR*>(&client), sizeof(client)) != SOCKET_ERROR;
    }

    void Socket::disconnect() {
        if (this->m_socket != INVALID_SOCKET)
            closesocket(this->m_socket);

        this->m_connected = false;
    }

}