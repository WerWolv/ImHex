cmake_minimum_required(VERSION 3.16)
project(imgui)

set(CMAKE_CXX_STANDARD 17)

add_library(imgui_all_includes INTERFACE)

add_subdirectory(imgui)
add_subdirectory(cimgui)
add_subdirectory(implot)
add_subdirectory(implot3d)
add_subdirectory(imnodes)
add_subdirectory(backend)
add_subdirectory(ColorTextEditor)

set(IMGUI_LIBRARIES imgui_imgui imgui_cimgui imgui_implot imgui_implot3d imgui_imnodes imgui_backend imgui_color_text_editor PARENT_SCOPE)