#pragma once

#include <asio.hpp>
#include <thread>
#include <memory>
#include <atomic>

namespace hex {
    namespace mcp { class CommandProcessor; } // Forward declaration
    namespace plugins {
        namespace mcp {

            class MCPServer {
            public:
                MCPServer(hex::mcp::CommandProcessor& commandProcessor);
                ~MCPServer();

                bool start(unsigned short port);
                void stop();
                bool is_running() const;
                unsigned short get_port() const; // For test verification

            private:
                void do_accept();
                void handle_accept(const asio::error_code& ec, asio::ip::tcp::socket socket);
                void run_service();

                hex::mcp::CommandProcessor& m_commandProcessor;
                asio::io_context m_io_context;
                asio::ip::tcp::acceptor m_acceptor;
                std::unique_ptr<std::thread> m_io_context_thread;
                std::atomic<bool> m_running{false};
                asio::executor_work_guard<asio::io_context::executor_type> m_work_guard; // Keeps io_context::run() from exiting prematurely
                unsigned short m_port {0}; // Store the port
            };

        }
    }
}
