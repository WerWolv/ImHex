cmake_minimum_required(VERSION 3.16)

include(ImHexPlugin)

find_package(md4c REQUIRED)

add_imhex_plugin(
    NAME
        ui
    SOURCES
        source/library_ui.cpp

        source/ui/hex_editor.cpp
        source/ui/pattern_drawer.cpp
        source/ui/visualizer_drawer.cpp
        source/ui/menu_items.cpp
        source/ui/pattern_value_editor.cpp
        source/ui/markdown.cpp
    INCLUDES
        include
    LIBRARIES
        fonts
        md4c::md4c
    LIBRARY_PLUGIN
)