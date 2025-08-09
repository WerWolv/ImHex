cmake_minimum_required(VERSION 3.16)

include(ImHexPlugin)

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
    INCLUDES
        include
    LIBRARIES
        fonts
    LIBRARY_PLUGIN
)