#include <hex/plugins/mcp_integration/mcp_plugin_view.hpp>
#include <hex/plugins/mcp_integration/mcp_server.hpp> // Need full server header for control
#include <imgui.h>
#include <hex/helpers/logger.hpp>
#include <cstdio> // For snprintf
#include <cstring> // For strncpy

// Access to the global server instance defined in plugin_mcp_integration.cpp
// This is a common way to access shared plugin resources from views.
// Ensure g_mcp_server is accessible or provide an accessor function.
// For this subtask, we assume it's declared extern or accessible.
// Better approach: pass a reference or use a service locator if ImHex has one.
// For now, we'll try to declare it extern.

// In plugin_mcp_integration.cpp, g_mcp_server should be:
// std::unique_ptr<hex::plugins::mcp::MCPServer> g_mcp_server; // (remove static if not already)
// And here:
extern std::unique_ptr<hex::plugins::mcp::MCPServer> g_mcp_server;
extern const unsigned short DEFAULT_MCP_PORT; // If we want to use the default

namespace hex {
    namespace plugins {
        namespace mcp {

            MCPPluginView::MCPPluginView() : View("MCP Server Control") {
                // Initialize port buffer with default or last configured port
                m_configured_port = DEFAULT_MCP_PORT; // Start with the default
                snprintf(m_port_buffer, sizeof(m_port_buffer), "%u", m_configured_port);
                update_server_status();
            }

            MCPPluginView::~MCPPluginView() {
                // Cleanup if needed
            }

            void MCPPluginView::update_server_status() {
                if (g_mcp_server && g_mcp_server->is_running()) {
                    // Assuming MCPServer has a method to get its current port
                    // unsigned short current_port = g_mcp_server->get_current_port(); // Needs to be added to MCPServer
                    // For now, we use m_configured_port as the display port when running
                    // If the server failed to start on m_configured_port, this might be misleading.
                    // A get_current_port() on MCPServer would be more accurate.
                    m_server_status_str = "Running on port " + std::to_string(m_configured_port);
                } else if (g_mcp_server) {
                     // Could add more states like "Starting...", "Stopping...", "Error"
                    m_server_status_str = "Stopped";
                } else {
                    m_server_status_str = "Server not initialized";
                }
            }

            void MCPPluginView::drawContent() {
                ImGui::TextUnformatted("MCP Server Management");
                ImGui::Separator();

                if (!g_mcp_server) {
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: MCP Server instance is not available!");
                    ImGui::TextUnformatted("This usually means the plugin didn't load correctly.");
                    return;
                }

                // Call update_server_status at the beginning of drawContent to ensure UI reflects current state.
                update_server_status();

                // Port Configuration
                ImGui::TextUnformatted("Configure Port:");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100); // Set width for the input text field
                if (ImGui::InputText("##Port", m_port_buffer, sizeof(m_port_buffer), ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
                    int port_val = std::atoi(m_port_buffer);
                    if (port_val > 0 && port_val <= 65535) {
                        m_configured_port = static_cast<unsigned short>(port_val);
                        Logger::info("MCP: Port configured to %u via UI.", m_configured_port);
                        // TODO: Persist this change
                        // If server is running, user might expect it to restart on new port, or for new port to apply on next start.
                        // For now, it applies on next manual start/restart.
                    } else {
                        // Handle invalid input, maybe revert or show error
                        Logger::warning("MCP: Invalid port entered: %s. Reverting to %u.", m_port_buffer, m_configured_port);
                        snprintf(m_port_buffer, sizeof(m_port_buffer), "%u", m_configured_port); // Revert display
                    }
                }

                ImGui::Spacing();

                // Server Control Buttons
                bool server_is_running = g_mcp_server->is_running();

                if (ImGui::Button("Start Server", ImVec2(120, 0))) {
                    if (!server_is_running) {
                        Logger::info("MCP: UI: Start Server button clicked. Port: %u", m_configured_port);
                        if (g_mcp_server->start(m_configured_port)) {
                            Logger::info("MCP: Server started successfully via UI.");
                        } else {
                            Logger::error("MCP: Failed to start server via UI on port %u.", m_configured_port);
                            // Update status with a more specific error if possible
                            m_server_status_str = "Error starting on port " + std::to_string(m_configured_port);
                        }
                        // update_server_status(); // Called at start of next frame
                    } else {
                        Logger::info("MCP: UI: Start Server button clicked, but server already running.");
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Stop Server", ImVec2(120, 0))) {
                    if (server_is_running) {
                        Logger::info("MCP: UI: Stop Server button clicked.");
                        g_mcp_server->stop();
                        Logger::info("MCP: Server stopped via UI.");
                        // update_server_status();
                    } else {
                        Logger::info("MCP: UI: Stop Server button clicked, but server not running.");
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Restart Server", ImVec2(120, 0))) {
                    Logger::info("MCP: UI: Restart Server button clicked for port %u.", m_configured_port);
                    if (server_is_running) {
                        g_mcp_server->stop();
                        Logger::info("MCP: Server stopped for restart via UI.");
                    }
                    // Give a brief moment for resources to release, though Asio ops are async.
                    // std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Usually not needed with proper async stop
                    if (g_mcp_server->start(m_configured_port)) {
                         Logger::info("MCP: Server restarted successfully via UI on port %u.", m_configured_port);
                    } else {
                         Logger::error("MCP: Failed to restart server via UI on port %u.", m_configured_port);
                         m_server_status_str = "Error restarting on port " + std::to_string(m_configured_port);
                    }
                    // update_server_status();
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // Status Display
                ImGui::Text("Status: %s", m_server_status_str.c_str());
                if (g_mcp_server && g_mcp_server->is_running()){
                     ImGui::Text("Listening on: 0.0.0.0:%u", m_configured_port); // Assuming IPv4 and configured port
                }
            }

            void MCPPluginView::drawMenu() {
                // This is just an example placeholder.
                // Actual menu integration would depend on how ImHex manages plugin menus.
                // if (ImGui::BeginMenu("Tools")) {
                //     if (ImGui::MenuItem("MCP Server Control")) {
                //         // This requires a mechanism to show/hide views by title/ID.
                //         // hex::ContentRegistry::Views::toggleView("MCP Server Control");
                //     }
                //     ImGui::EndMenu();
                // }
            }
        }
    }
}
