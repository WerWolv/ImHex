cmake_minimum_required(VERSION 3.16)
project(imgui)

set(CMAKE_CXX_STANDARD 17)

find_package(Freetype REQUIRED)

add_library(imgui
    source/imgui.cpp
    source/imgui_demo.cpp
    source/imgui_draw.cpp
    source/imgui_freetype.cpp
    source/imgui_impl_glfw.cpp
    source/imgui_impl_opengl3.cpp
    source/imgui_widgets.cpp
    source/ImGuiFileBrowser.cpp
    source/TextEditor.cpp
)

add_subdirectory(glad)

add_compile_definitions(IMGUI_IMPL_OPENGL_LOADER_GLAD)

target_include_directories(imgui PUBLIC include ${FREETYPE_INCLUDE_DIRS})
target_link_libraries(imgui Freetype::Freetype glad)
