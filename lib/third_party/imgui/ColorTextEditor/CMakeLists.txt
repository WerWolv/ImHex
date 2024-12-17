cmake_minimum_required(VERSION 3.16)
# https://github.com/BalazsJako/ImGuiColorTextEdit
project(imgui_color_text_editor)

set(CMAKE_CXX_STANDARD 20)

if (NOT IMHEX_EXTERNAL_PLUGIN_BUILD)
    add_library(imgui_color_text_editor OBJECT
        source/TextEditor.cpp
    )

    target_include_directories(imgui_color_text_editor PUBLIC
        include
    )

    target_link_libraries(imgui_color_text_editor PRIVATE imgui_includes)
endif()

target_include_directories(imgui_all_includes INTERFACE include)

