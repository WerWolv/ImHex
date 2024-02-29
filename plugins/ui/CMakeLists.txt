cmake_minimum_required(VERSION 3.16)

include(ImHexPlugin)

add_imhex_plugin(
    NAME
        ui
    SOURCES
        source/library_ui.cpp

        source/ui/hex_editor.cpp
        source/ui/pattern_drawer.cpp
    INCLUDES
        include
    LIBRARIES
        libpl
        libpl-gen
        fonts
    LIBRARY_PLUGIN
)
