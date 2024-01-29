cmake_minimum_required(VERSION 3.16)

include(ImHexPlugin)

if (USE_SYSTEM_EDLIB)
    find_package(edlib REQUIRED)
else()
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    add_subdirectory(${THIRD_PARTY_LIBS_FOLDER}/edlib ${CMAKE_CURRENT_BINARY_DIR}/edlib EXCLUDE_FROM_ALL)
    enableUnityBuild(edlib)
endif()

add_imhex_plugin(
    NAME
        diffing
    SOURCES
        source/plugin_diffing.cpp

        source/content/diffing_algorithms.cpp
        source/content/views/view_diff.cpp
    INCLUDES
        include
    LIBRARIES
        ui
        fonts
        edlib
)