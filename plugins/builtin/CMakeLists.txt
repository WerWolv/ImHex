cmake_minimum_required(VERSION 3.16)
project(builtin)

set(CMAKE_CXX_STANDARD 20)

if (NOT TARGET libimhex)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../libimhex ${CMAKE_CURRENT_BINARY_DIR}/plugins/libimhex)
endif()

set(CMAKE_SHARED_LIBRARY_PREFIX "plugin")

add_library(builtin SHARED
        source/plugin_builtin.cpp

        source/content/command_palette_commands.cpp
        source/content/data_inspector.cpp
        source/content/lang_builtin_functions.cpp
        source/content/settings_entries.cpp
        source/content/tools_entries.cpp

        source/math_evaluator.cpp
)

target_include_directories(builtin PRIVATE include)
target_link_libraries(builtin PRIVATE libimhex LLVMDemangle)
