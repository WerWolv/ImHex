#include <hex/plugins/mcp_integration/mcp_session.hpp>
#include <hex/mcp_integration/command_processor.hpp> // Actual CommandProcessor
#include <hex/helpers/logger.hpp>
#include <iostream> // For asio::read_until debugging

namespace hex {
    namespace plugins {
        namespace mcp {

            MCPSession::MCPSession(asio::ip::tcp::socket socket, hex::mcp::CommandProcessor& commandProcessor)
                : m_socket(std::move(socket)), m_commandProcessor(commandProcessor) {
                Logger::info("MCP: Session created for %s", m_socket.remote_endpoint().address().to_string().c_str());
            }

            MCPSession::~MCPSession() {
                Logger::info("MCP: Session destroyed for %s", m_socket.is_open() ? m_socket.remote_endpoint().address().to_string().c_str() : "closed socket");
            }

            void MCPSession::start() {
                do_read();
            }

            void MCPSession::close_socket() {
                if (m_socket.is_open()) {
                    asio::error_code ec;
                    m_socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
                    if (ec && ec != asio::error::not_connected) { // Ignore "not connected" error if already closed by remote
                        Logger::warning("MCP: Error shutting down socket: %s", ec.message().c_str());
                    }
                    m_socket.close(ec);
                    if (ec) {
                        Logger::error("MCP: Error closing socket: %s", ec.message().c_str());
                    }
                }
            }

            void MCPSession::do_read() {
                auto self = shared_from_this();
                // Use a larger buffer if m_read_buffer is too small by default
                // m_read_buffer.prepare(1024);
                asio::async_read_until(m_socket, m_read_buffer, '\n',
                    [this, self](const asio::error_code& ec, std::size_t bytes_transferred) {
                        handle_read(ec, bytes_transferred);
                    });
            }

            void MCPSession::handle_read(const asio::error_code& ec, std::size_t bytes_transferred) {
                if (!ec) {
                    // Extract the received data from the buffer
                    std::string message_with_potential_excess;
                    message_with_potential_excess.resize(bytes_transferred);
                    asio::buffer_copy(asio::buffer(message_with_potential_excess), m_read_buffer.data(), bytes_transferred);

                    // Consume only up to the delimiter from the buffer for this processing round
                    m_read_buffer.consume(bytes_transferred);


                    std::string actual_message_to_process;
                    std::size_t newline_pos = message_with_potential_excess.find('\n');
                    if (newline_pos != std::string::npos) {
                         actual_message_to_process = message_with_potential_excess.substr(0, newline_pos);
                    } else {
                        // This case should ideally not be hit if async_read_until guarantees delimiter or error
                        actual_message_to_process = message_with_potential_excess;
                    }

                    if (actual_message_to_process.empty() && bytes_transferred > 0 && message_with_potential_excess[0] == '\n') {
                        // Just a newline received, possibly after other data processed by streambuf internals
                        // Or if client sends an empty line.
                        Logger::debug("MCP: Received empty line or just newline, continuing read.");
                        do_read(); // Continue reading for a valid command
                        return;
                    }
                     if (actual_message_to_process.empty() && bytes_transferred == 0) {
                        Logger::debug("MCP: Received 0 bytes, possibly client closed gracefully before sending data.");
                        do_read(); // Might be an EOF, let next read handle it.
                        return;
                    }


                    Logger::debug("MCP: Received from %s: %s", m_socket.remote_endpoint().address().to_string().c_str(), actual_message_to_process.c_str());

                    std::string response_str = m_commandProcessor.processCommand(actual_message_to_process);
                    do_write(response_str + "\n");
                } else {
                    if (ec == asio::error::eof) {
                        Logger::info("MCP: Client %s disconnected.", m_socket.remote_endpoint().address().to_string().c_str());
                    } else if (ec == asio::error::operation_aborted) {
                        Logger::info("MCP: Read operation aborted for %s (session likely ending).", m_socket.is_open() ? m_socket.remote_endpoint().address().to_string().c_str() : "closed_socket");
                    } else {
                        Logger::error("MCP: Read error from %s: %s", m_socket.is_open() ? m_socket.remote_endpoint().address().to_string().c_str() : "closed_socket", ec.message().c_str());
                    }
                    close_socket();
                }
            }

            void MCPSession::do_write(const std::string& message) {
                auto self = shared_from_this(); // Keep session alive
                bool write_in_progress = !m_write_msgs_queue.empty();
                m_write_msgs_queue.push_back(message);
                if (!write_in_progress) {
                    asio::async_write(m_socket, asio::buffer(m_write_msgs_queue.front()),
                        [this, self](const asio::error_code& ec, std::size_t /*length*/) {
                            handle_write(ec, 0 /* not used for length */);
                        });
                }
            }

            void MCPSession::handle_write(const asio::error_code& ec, std::size_t /*bytes_transferred*/) {
                 auto self = shared_from_this(); // Keep session alive during handler
                if (!ec) {
                    m_write_msgs_queue.pop_front();
                    if (!m_write_msgs_queue.empty()) {
                         asio::async_write(m_socket, asio::buffer(m_write_msgs_queue.front()),
                            [this, self](const asio::error_code& ec_inner, std::size_t /*length*/) {
                                handle_write(ec_inner, 0);
                            });
                    } else {
                        // After the current write queue is empty, start reading the next command
                        do_read();
                    }
                } else {
                     if (ec == asio::error::operation_aborted) {
                        Logger::info("MCP: Write operation aborted for %s (session likely ending).", m_socket.is_open() ? m_socket.remote_endpoint().address().to_string().c_str() : "closed_socket");
                    } else {
                        Logger::error("MCP: Write error to %s: %s", m_socket.is_open() ? m_socket.remote_endpoint().address().to_string().c_str() : "closed_socket", ec.message().c_str());
                    }
                    close_socket(); // Close socket on write error
                }
            }
        }
    }
}
