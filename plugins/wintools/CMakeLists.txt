cmake_minimum_required(VERSION 3.16)
project(wintools)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../libimhex ${CMAKE_CURRENT_BINARY_DIR}/plugins/libimhex)

set(CMAKE_SHARED_LIBRARY_PREFIX "plugin")

add_library(wintools SHARED
        source/plugin_wintools.cpp
        source/windows_api.cpp
)

target_include_directories(wintools PUBLIC include)
target_link_libraries(wintools PRIVATE libimhex imgui)
