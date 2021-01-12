cmake_minimum_required(VERSION 3.16)
project(example)

set(CMAKE_CXX_STANDARD 20)

if (NOT TARGET libimhex)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../libimhex ${CMAKE_CURRENT_BINARY_DIR}/plugins/libimhex)
endif()

set(CMAKE_SHARED_LIBRARY_PREFIX "plugin")

add_library(example SHARED
        source/plugin_example.cpp
)

target_include_directories(example PRIVATE include)
target_link_libraries(example PRIVATE libimhex)
