cmake_minimum_required(VERSION 3.16)

include(ImHexPlugin)

add_imhex_plugin(
    NAME
        fonts
    SOURCES
        source/library_fonts.cpp
        source/font_loader.cpp
        source/fonts.cpp
    INCLUDES
        include
    LIBRARY_PLUGIN
)
