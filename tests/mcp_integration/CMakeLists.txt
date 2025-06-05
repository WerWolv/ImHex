cmake_minimum_required(VERSION 3.16)
project(MCPIntegrationTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ImHex REQUIRED)
find_package(Catch2 REQUIRED) # ImHex uses Catch2

# Test sources
set(MCP_INTEGRATION_TEST_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/command_processor_tests.cpp"
)

add_executable(mcp_integration_tests ${MCP_INTEGRATION_TEST_SRCS})

# Link against libimhex and Catch2
target_link_libraries(mcp_integration_tests PRIVATE
    ImHex::libimhex
    Catch2::Catch2WithMain # Or Catch2::Catch2 if main is provided elsewhere
)

# Include directories
target_include_directories(mcp_integration_tests PRIVATE
    # To find libimhex's mcp_integration headers (command_processor.hpp)
    # And plugin interface headers if needed for future tests
    "${CMAKE_SOURCE_DIR}/lib/libimhex/include"
    "${CMAKE_SOURCE_DIR}/plugins/mcp_integration/include" # If testing plugin interface
)

# Add test to CTest
include(CTest)
add_test(NAME mcp_integration_tests COMMAND mcp_integration_tests)
