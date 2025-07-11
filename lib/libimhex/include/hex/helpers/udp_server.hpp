#pragma once

#include <hex.hpp>

#include <functional>
#include <thread>
#include <atomic>
#include <span>

namespace hex {

    class UDPServer {
    public:
        using Callback = std::function<void(std::span<const u8> data)>;
        UDPServer() = default;
        UDPServer(u16 port, Callback callback);
        ~UDPServer();

        UDPServer(const UDPServer&) = delete;
        UDPServer& operator=(const UDPServer&) = delete;
        UDPServer(UDPServer &&other) noexcept {
            m_port = other.m_port;
            m_callback = std::move(other.m_callback);
            m_thread = std::move(other.m_thread);
            m_running = other.m_running.load();
            other.m_running = false;
            m_socketFd = other.m_socketFd;
            other.m_socketFd = -1;
        }
        UDPServer& operator=(UDPServer &&other) noexcept {
            if (this != &other) {
                m_port = other.m_port;
                m_callback = std::move(other.m_callback);
                m_thread = std::move(other.m_thread);
                m_running = other.m_running.load();
                other.m_running = false;
                m_socketFd = other.m_socketFd;
                other.m_socketFd = -1;
            }

            return *this;
        }

        void start();
        void stop();

        [[nodiscard]] u16 getPort() const { return m_port; }

    private:
        void run();

        u16 m_port = 0;
        Callback m_callback;
        std::thread m_thread;
        std::atomic<bool> m_running;
        int m_socketFd = -1;
    };

}