cmake_minimum_required(VERSION 3.16)
# https://unifoundry.com/unifont/index.html
# https://github.com/microsoft/vscode-codicons
# https://github.com/FortAwesome/Font-Awesome
project(imgui_fonts)

set(CMAKE_CXX_STANDARD 17)

add_library(imgui_fonts OBJECT
    source/unifont_font.c
    source/codicons_font.c
    source/fontawesome_font.c
)

target_include_directories(imgui_fonts PUBLIC
    include
)

target_link_libraries(imgui_fonts PRIVATE imgui_includes)
set_property(TARGET imgui_fonts PROPERTY POSITION_INDEPENDENT_CODE ON)