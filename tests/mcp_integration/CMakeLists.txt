cmake_minimum_required(VERSION 3.16)
project(MCPIntegrationTestsCXX LANGUAGES CXX) # Renamed to avoid conflict if old name was too generic

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ImHex REQUIRED)
find_package(Catch2 REQUIRED)
find_package(Threads REQUIRED) # For Asio

# --- Asio Configuration (mirroring plugin's CMake) ---
set(ASIO_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/lib/external/asio/include)
if(NOT EXISTS ${ASIO_INCLUDE_DIR}/asio.hpp)
    message(WARNING "Asio headers not found at ${ASIO_INCLUDE_DIR} for tests. MCP Server tests will likely fail to build.")
endif()

# Test sources for Command Processor
set(MCP_CMD_PROCESSOR_TEST_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/command_processor_tests.cpp" # Existing tests
)

add_executable(mcp_command_processor_tests ${MCP_CMD_PROCESSOR_TEST_SRCS})
target_link_libraries(mcp_command_processor_tests PRIVATE
    ImHex::libimhex
    Catch2::Catch2WithMain
    Threads::Threads # Command processor doesn't directly use threads/asio, but libimhex might link it
)
target_include_directories(mcp_command_processor_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/lib/libimhex/include"
    "${CMAKE_SOURCE_DIR}/plugins/mcp_integration/include" # For mcp_interface.hpp etc.
    "${ASIO_INCLUDE_DIR}"
)
add_test(NAME mcp_command_processor_tests COMMAND mcp_command_processor_tests)


# Test sources for MCP Server
set(MCP_SERVER_TEST_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/mcp_server_tests.cpp" # New server tests
)
add_executable(mcp_server_integration_tests ${MCP_SERVER_TEST_SRCS})
target_link_libraries(mcp_server_integration_tests PRIVATE
    ImHex::libimhex # For CommandProcessor and MCPServer/Session classes
    Catch2::Catch2WithMain
    Threads::Threads # Asio needs this
)
target_include_directories(mcp_server_integration_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/lib/libimhex/include"
    "${CMAKE_SOURCE_DIR}/plugins/mcp_integration/include" # For mcp_server.hpp etc.
    "${ASIO_INCLUDE_DIR}"
)
add_test(NAME mcp_server_integration_tests COMMAND mcp_server_integration_tests)
