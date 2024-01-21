cmake_minimum_required(VERSION 3.16)

include(ImHexPlugin)

add_subdirectory(${THIRD_PARTY_LIBS_FOLDER}/edlib ${CMAKE_CURRENT_BINARY_DIR}/edlib)

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
        edlib
)