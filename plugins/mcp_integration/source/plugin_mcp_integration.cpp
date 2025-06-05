#include <hex/plugin.hpp>
#include <hex/plugin.hpp>
#include <hex/mcp_integration/command_processor.hpp> // From libimhex
#include <hex/api/content_registry.hpp> // Required for registering views
#include <hex/plugins/mcp_integration/mcp_interface.hpp> // The old interface header
#include <hex/plugins/mcp_integration/mcp_server.hpp>   // The new server header
#include <hex/helpers/logger.hpp>
#include <memory> // For std::unique_ptr

// Global instance of the command processor for this plugin
static hex::mcp::CommandProcessor g_commandProcessor;
hex::mcp::CommandProcessor& get_command_processor() { return g_commandProcessor; } // Accessor

// Global instance of the MCP Server
std::unique_ptr<hex::plugins::mcp::MCPServer> g_mcp_server; // Made non-static for extern access

#include <hex/plugins/mcp_integration/mcp_plugin_view.hpp> // Include the new view

// Default port for the MCP server
const unsigned short DEFAULT_MCP_PORT = 61100; // TODO: Make this configurable

// Implementation for the old interface function (may be deprecated later)
std::string hex::plugins::mcp::process_mcp_command(const std::string& command_json) {
    hex::Logger::info("MCPPlugin: Received command via direct call: %s", command_json.c_str());
    std::string response = g_commandProcessor.processCommand(command_json);
    hex::Logger::info("MCPPlugin: Sending response via direct call: %s", response.c_str());
    return response;
}

// ImHex plugin lifecycle functions are not explicitly defined like plugin_load/plugin_unload
// in the basic example. IMHEX_PLUGIN_SETUP is for initialization.
// Cleanup typically relies on destructors of static/global objects.
// We will instantiate and start the server in IMHEX_PLUGIN_SETUP.
// The MCPServer's destructor should handle stopping the server.

IMHEX_PLUGIN_SETUP("MCP Integration", "ImHex AI Agent", "Integrates MCP command processing with ImHex via TCP Server") {
    hex::Logger::info("MCP Integration Plugin Loading...");

    // Instantiate and start the server
    if (!g_mcp_server) { // Ensure it's only initialized once
        g_mcp_server = std::make_unique<hex::plugins::mcp::MCPServer>(g_commandProcessor);
        if (g_mcp_server->start(DEFAULT_MCP_PORT)) {
            hex::Logger::info("MCP Server started successfully on port %u.", DEFAULT_MCP_PORT);
        } else {
            hex::Logger::error("Failed to start MCP Server on port %u.", DEFAULT_MCP_PORT);
            // g_mcp_server will remain valid but not running, or could be reset.
            // For now, leave it, its is_running() will be false.
        }
    } else {
        // This case should ideally not happen if IMHEX_PLUGIN_SETUP is called only once.
        // If it can be called multiple times, we might need to check if server is already running.
        if (!g_mcp_server->is_running()) {
            hex::Logger::info("MCP Server was initialized but not running. Attempting to start...");
            if (g_mcp_server->start(DEFAULT_MCP_PORT)) {
                hex::Logger::info("MCP Server started successfully on port %u.", DEFAULT_MCP_PORT);
            } else {
                hex::Logger::error("Failed to start MCP Server on port %u.", DEFAULT_MCP_PORT);
            }
        } else {
             hex::Logger::info("MCP Server already running on port %u.", DEFAULT_MCP_PORT);
        }
    }

    hex::Logger::info("MCP Integration Plugin Loaded and Server initialized.");

    // To handle shutdown:
    // The g_mcp_server unique_ptr's destructor will call the MCPServer destructor
    // when the plugin is unloaded and static objects are destroyed.
    // The MCPServer destructor is responsible for calling stop().
}

// This function is called by ImHex to register plugin content.
IMHEX_PLUGIN_CONTENT() {
    hex::ContentRegistry::Views::add<hex::plugins::mcp::MCPPluginView>();
}

// If ImHex had explicit plugin_unload or similar:
// extern "C" void plugin_unload() {
//     hex::Logger::info("MCP Integration Plugin Unloading...");
//     if (g_mcp_server) {
//         g_mcp_server->stop();
//         g_mcp_server.reset(); // Release the server instance
//     }
//     hex::Logger::info("MCP Integration Plugin Unloaded.");
// }
// Since it doesn't, we rely on MCPServer's destructor.
