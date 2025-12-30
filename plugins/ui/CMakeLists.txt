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
        source/ui/widgets.cpp
        source/ui/markdown.cpp
        source/ui/text_editor/editor.cpp
        source/ui/text_editor/highlighter.cpp
        source/ui/text_editor/navigate.cpp
        source/ui/text_editor/render.cpp
        source/ui/text_editor/support.cpp
        source/ui/text_editor/utf8.cpp
        source/ui/text_editor/codeFolder.cpp
    INCLUDES
        include
    LIBRARIES
        fonts
        md4c::md4c
    LIBRARY_PLUGIN
)