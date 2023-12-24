cmake_minimum_required(VERSION 3.16)

include(ImHexPlugin)

find_package(LibArchive 3.6 REQUIRED)

add_imhex_plugin(
    NAME
        decompress
    SOURCES
        source/plugin_decompress.cpp

        source/content/pl_functions.cpp
    INCLUDES
        include
    LIBRARIES
        ui
        LibArchive::LibArchive
)
