#pragma once

#include <hex/ui/view.hpp> // Main ImHex view header
#include <string>

namespace hex {
    namespace plugins {
        namespace mcp {

            // Forward declare MCPServer to avoid including its full header here if not needed
            class MCPServer;

            class MCPPluginView : public hex::View {
            public:
                MCPPluginView(); // Constructor
                ~MCPPluginView() override; // Destructor

                void drawContent() override; // Main drawing function for the view's content
                void drawMenu() override;    // For any menu items (optional)

            private:
                // Store the port as a string for ImGui input, convert to int when needed
                char m_port_buffer[6]; // Max port is 65535 (5 digits) + null terminator
                unsigned short m_configured_port;

                // Helper to update status string, could also directly query server
                std::string m_server_status_str;

                void update_server_status();

                // Reference to the global server instance (set in constructor or retrieved)
                // This is tricky if g_mcp_server is in the .cpp. We might need an accessor.
                // For now, assume we can get its state.
            };

        }
    }
}
