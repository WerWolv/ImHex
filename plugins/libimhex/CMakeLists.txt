cmake_minimum_required(VERSION 3.16)
project(libimhex)

set(CMAKE_CXX_STANDARD 20)

if (TARGET libimhex)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../external/ImGui ${CMAKE_CURRENT_BINARY_DIR}/external/ImGui)
endif()

set(CMAKE_STATIC_LIBRARY_PREFIX "")

add_library(libimhex STATIC
        source/helpers/event.cpp
        source/providers/provider.cpp
        source/views/view.cpp
    )

target_include_directories(libimhex PUBLIC include)
target_link_libraries(libimhex PRIVATE imgui)
