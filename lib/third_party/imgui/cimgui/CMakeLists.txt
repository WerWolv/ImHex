cmake_minimum_required(VERSION 3.16)

# https://github.com/cimgui/cimgui
project(imgui_cimgui)

set(CMAKE_CXX_STANDARD 17)

if (NOT IMHEX_EXTERNAL_PLUGIN_BUILD)
    add_library(imgui_cimgui OBJECT
        source/cimgui.cpp
    )

    target_include_directories(imgui_cimgui PUBLIC
        include
    )

    target_link_libraries(imgui_cimgui PRIVATE imgui_includes)
    add_dependencies(imhex_all imgui_cimgui)
endif()

