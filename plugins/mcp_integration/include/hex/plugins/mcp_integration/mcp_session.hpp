#pragma once

#include <asio.hpp>
#include <string>
#include <memory>
#include <deque>

namespace hex {
    namespace mcp { class CommandProcessor; } // Forward declaration
    namespace plugins {
        namespace mcp {

            class MCPSession : public std::enable_shared_from_this<MCPSession> {
            public:
                MCPSession(asio::ip::tcp::socket socket, hex::mcp::CommandProcessor& commandProcessor);
                ~MCPSession();

                void start();

            private:
                void do_read();
                void handle_read(const asio::error_code& ec, std::size_t bytes_transferred);

                void do_write(const std::string& message);
                void handle_write(const asio::error_code& ec, std::size_t bytes_transferred);

                void close_socket();

                asio::ip::tcp::socket m_socket;
                hex::mcp::CommandProcessor& m_commandProcessor;
                asio::streambuf m_read_buffer; // Buffer for incoming data

                std::deque<std::string> m_write_msgs_queue; // Queue for outgoing messages
            };

        }
    }
}
