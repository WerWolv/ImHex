#pragma once

#include <string>

namespace hex {
    namespace plugins {
        namespace mcp {

            // Processes an MCP command (JSON string) and returns a JSON response string.
            // This function will be implemented in the plugin and can be called externally.
            std::string process_mcp_command(const std::string& command_json);

        }
    }
}
