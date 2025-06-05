cmake_minimum_required(VERSION 3.16)
project(MCPIntegration LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ImHex REQUIRED)

# --- BEGIN Asio Configuration ---
# Option 1: Assuming Asio headers are manually placed in lib/external/asio/include
# Create an INTERFACE library for Asio for easier usage
# add_library(asio_external INTERFACE)
# target_include_directories(asio_external INTERFACE ${CMAKE_SOURCE_DIR}/lib/external/asio/include)
# Or, if you add Asio via FetchContent or find_package(asio), that would be different.
# For this subtask, we'll assume it's manually placed and the include path is added directly.
# Ensure this path is correct based on where Asio headers are actually placed.
set(ASIO_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/lib/external/asio/include) # Example path
if(NOT EXISTS ${ASIO_INCLUDE_DIR}/asio.hpp)
    message(WARNING "Asio headers not found at ${ASIO_INCLUDE_DIR}. MCP Server will likely fail to build.")
endif()
# --- END Asio Configuration ---


# Plugin sources and headers
set(MCP_INTEGRATION_HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/hex/plugins/mcp_integration/mcp_interface.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/hex/plugins/mcp_integration/mcp_session.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/hex/plugins/mcp_integration/mcp_server.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/hex/plugins/mcp_integration/mcp_plugin_view.hpp"
)

set(MCP_INTEGRATION_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/plugin_mcp_integration.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/mcp_session.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/mcp_server.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/mcp_plugin_view.cpp"
)

# Create the plugin library
add_library(MCPIntegration MODULE ${MCP_INTEGRATION_SRCS} ${MCP_INTEGRATION_HDRS})

# Link against libimhex
target_link_libraries(MCPIntegration PRIVATE ImHex::libimhex) # Add asio_external if using INTERFACE library approach

# Include directories
target_include_directories(MCPIntegration PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
    "${ASIO_INCLUDE_DIR}" # Add Asio include directory
)

# Asio might require thread support
find_package(Threads REQUIRED)
target_link_libraries(MCPIntegration PRIVATE Threads::Threads)


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
