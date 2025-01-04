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
    INCLUDES
        include
    LIBRARIES
        libpl-gen
        fonts
    LIBRARY_PLUGIN
)

if (APPLE)
    target_sources(ui PRIVATE source/ui/macos_menu.m)
    find_library(FOUNDATION NAMES Foundation REQUIRED)
    find_library(COCOA NAMES Cocoa REQUIRED)
    target_link_libraries(ui PUBLIC ${FOUNDATION} ${COCOA})
endif()