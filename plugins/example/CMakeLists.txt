cmake_minimum_required(VERSION 3.16)
project(plugins_example)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../external/ImGui ${CMAKE_CURRENT_BINARY_DIR}/external/ImGui)

add_library(plugins_example SHARED
        source/test.cpp
        )

target_include_directories(plugins_example PUBLIC include)
target_link_libraries(plugins_example imgui)
