#include <hex/plugins/mcp_integration/mcp_server.hpp>
#include <hex/plugins/mcp_integration/mcp_session.hpp>
#include <hex/mcp_integration/command_processor.hpp>
#include <hex/helpers/logger.hpp>

namespace hex {
    namespace plugins {
        namespace mcp {

            MCPServer::MCPServer(hex::mcp::CommandProcessor& commandProcessor)
                : m_commandProcessor(commandProcessor),
                  m_io_context(),
                  m_acceptor(m_io_context),
                  m_work_guard(asio::make_work_guard(m_io_context)) {
            }

            MCPServer::~MCPServer() {
                if (is_running()) {
                    stop();
                }
            }

            bool MCPServer::is_running() const {
                return m_running.load();
            }

            unsigned short MCPServer::get_port() const {
                return m_port;
            }

            bool MCPServer::start(unsigned short port) {
                if (is_running()) {
                    Logger::warning("MCP: Server already running.");
                    return true;
                }

                m_port = 0; // Reset port before attempting to start
                m_io_context.reset(); // Reset context if it was stopped before
                m_work_guard.reset(); // Ensure work guard is active before it's assigned
                m_work_guard = asio::make_work_guard(m_io_context);


                asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), port);
                asio::error_code ec;

                m_acceptor.open(endpoint.protocol(), ec);
                if (ec) {
                    Logger::error("MCP: Failed to open acceptor: %s", ec.message().c_str());
                    m_work_guard.reset(); // Release work guard on failure
                    return false;
                }
                m_acceptor.set_option(asio::socket_base::reuse_address(true), ec);
                if (ec) {
                    Logger::warning("MCP: Failed to set reuse_address: %s. Continuing...", ec.message().c_str());
                    // Non-fatal, continue
                }
                m_acceptor.bind(endpoint, ec);
                if (ec) {
                    Logger::error("MCP: Failed to bind to port %u: %s", port, ec.message().c_str());
                    if (m_acceptor.is_open()) m_acceptor.close();
                    m_work_guard.reset();
                    return false;
                }
                m_acceptor.listen(asio::socket_base::max_listen_connections, ec);
                if (ec) {
                    Logger::error("MCP: Failed to listen on port %u: %s", port, ec.message().c_str());
                    if (m_acceptor.is_open()) m_acceptor.close();
                    m_work_guard.reset();
                    return false;
                }
                m_port = m_acceptor.local_endpoint().port(); // Get actual listening port

                Logger::info("MCP: Server starting on port %u...", m_port); // Log actual port

                do_accept();

                m_io_context_thread = std::make_unique<std::thread>([this]() {
                    Logger::info("MCP: Asio io_context thread started.");
                    asio::error_code ioc_ec;
                    try {
                        m_io_context.run(ioc_ec);
                    } catch (const std::exception& e) {
                        Logger::error("MCP: Exception in io_context run: %s", e.what());
                    } catch (...) {
                        Logger::error("MCP: Unknown exception in io_context run.");
                    }
                    if (ioc_ec) {
                        Logger::error("MCP: io_context run finished with error: %s", ioc_ec.message().c_str());
                    }
                    Logger::info("MCP: Asio io_context thread finished.");
                });

                m_running = true;
                return true;
            }

            void MCPServer::stop() {
                if (!m_running.load() && m_io_context.stopped()) {
                     Logger::info("MCP: Server already stopped.");
                     return;
                }
                 if (!m_running.load() && !m_io_context.stopped()) {
                     Logger::info("MCP: Server was not fully started or is already stopping.");
                     // Attempt cleanup if thread exists
                     if (m_io_context_thread && m_io_context_thread->joinable()) {
                         m_work_guard.reset(); // Allow run to exit
                         if(!m_io_context.stopped()) m_io_context.stop();
                         m_io_context_thread->join();
                     }
                     m_io_context_thread.reset();
                     return;
                 }


                Logger::info("MCP: Server stopping...");

                // Prevent new operations from starting
                m_running = false;

                // Close the acceptor to stop accepting new connections
                // Post to io_context to ensure it's done within the Asio thread if possible,
                // or do it directly if necessary. Direct is usually fine for acceptor.
                if (m_acceptor.is_open()) {
                    asio::error_code ec;
                    m_acceptor.close(ec);
                    if (ec) {
                         Logger::error("MCP: Error closing acceptor: %s", ec.message().c_str());
                    }
                }

                // The work guard is what keeps io_context::run() from returning.
                // Resetting it allows run() to exit once all pending handlers are done.
                // This should be done before stopping the io_context.
                m_work_guard.reset();


                // If io_context is already stopped, run() would return immediately.
                // If not, this signals it to stop.
                if (!m_io_context.stopped()) {
                    m_io_context.stop(); // This will cause run() to return.
                }

                if (m_io_context_thread && m_io_context_thread->joinable()) {
                    try {
                        m_io_context_thread->join();
                         Logger::info("MCP: Asio io_context thread joined.");
                    } catch (const std::system_error& e) {
                        Logger::error("MCP: System error joining io_context thread: %s", e.what());
                    }
                } else {
                     Logger::info("MCP: io_context thread already joined or not started.");
                }
                m_io_context_thread.reset(); // Clean up the thread object
                m_port = 0; // Reset port when server stops

                Logger::info("MCP: Server stopped.");
            }

            void MCPServer::do_accept() {
                if (!m_acceptor.is_open()) { // Guard against accepting on a closed acceptor
                    Logger::warning("MCP: Acceptor not open, cannot call do_accept.");
                    return;
                }
                m_acceptor.async_accept(
                    [this](const asio::error_code& ec, asio::ip::tcp::socket socket) {
                        handle_accept(ec, std::move(socket));
                    });
            }

            void MCPServer::handle_accept(const asio::error_code& ec, asio::ip::tcp::socket socket) {
                if (!ec) {
                    // Check if server is still running before creating a session
                    if (!m_running.load()) {
                        Logger::info("MCP: Server is not running, ignoring accepted connection from %s", socket.remote_endpoint().address().to_string().c_str());
                        asio::error_code close_ec;
                        socket.close(close_ec); // Close the socket as we are not handling it
                        return;
                    }
                    Logger::info("MCP: Accepted connection from %s", socket.remote_endpoint().address().to_string().c_str());
                    std::make_shared<MCPSession>(std::move(socket), m_commandProcessor)->start();
                } else {
                    if (ec == asio::error::operation_aborted) {
                        Logger::info("MCP: Accept operation aborted (server likely stopping).");
                    } else {
                        Logger::error("MCP: Accept error: %s", ec.message().c_str());
                    }
                    // Do not call do_accept() again here if acceptor is closed or server is stopping.
                    // The main loop for do_accept() is now only if acceptor is open.
                }

                // Continue accepting new connections ONLY if the server is running and acceptor is open
                if (m_running.load() && m_acceptor.is_open()) {
                    do_accept();
                } else {
                    Logger::info("MCP: Server not running or acceptor closed, stopping further accepts.");
                }
            }
        }
    }
}
