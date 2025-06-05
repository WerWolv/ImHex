#include <hex/plugin.hpp>
#include <hex/mcp_integration/command_processor.hpp> // From libimhex
#include <hex/plugins/mcp_integration/mcp_interface.hpp> // The interface header
#include <hex/helpers/logger.hpp>

// Global instance of the command processor for this plugin
static hex::mcp::CommandProcessor g_commandProcessor;

// Implementation for the interface function
std::string hex::plugins::mcp::process_mcp_command(const std::string& command_json) {
    hex::Logger::info("MCPPlugin: Received command: %s", command_json.c_str());
    std::string response = g_commandProcessor.processCommand(command_json);
    hex::Logger::info("MCPPlugin: Sending response: %s", response.c_str());
    return response;
}

IMHEX_PLUGIN_SETUP("MCP Integration", "ImHex AI Agent", "Integrates MCP command processing with ImHex") {
    hex::Logger::info("MCP Integration Plugin Loaded");
    // No views or other UI elements to register for now.
    // The plugin's main role is to initialize the command processor and expose the interface.
}

// Optional: Entry/exit functions if needed for more complex setup/teardown
// extern "C" void __declspec(dllexport) plugin_load() { }
// extern "C" void __declspec(dllexport) plugin_unload() { }
