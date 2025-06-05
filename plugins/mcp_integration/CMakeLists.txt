cmake_minimum_required(VERSION 3.16)
project(MCPIntegration LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ImHex REQUIRED)

# Plugin sources and headers
set(MCP_INTEGRATION_HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/hex/plugins/mcp_integration/mcp_interface.hpp"
)

set(MCP_INTEGRATION_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/plugin_mcp_integration.cpp"
)

# Create the plugin library
add_library(MCPIntegration MODULE ${MCP_INTEGRATION_SRCS} ${MCP_INTEGRATION_HDRS})

# Link against libimhex
target_link_libraries(MCPIntegration PRIVATE ImHex::libimhex)

# Include directories
target_include_directories(MCPIntegration PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
    # To find libimhex's mcp_integration headers (command_processor.hpp)
    # This assumes libimhex exports its include directories correctly via ImHex::libimhex
)

# Set plugin properties
set_target_properties(MCPIntegration PROPERTIES
    OUTPUT_NAME "mcp_integration"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

# Copy headers to a common include directory (optional, good practice)
install(FILES ${MCP_INTEGRATION_HDRS}
    DESTINATION include/hex/plugins/mcp_integration
)

install(TARGETS MCPIntegration
    LIBRARY DESTINATION lib/imhex/plugins
    RUNTIME DESTINATION bin/imhex/plugins
)
