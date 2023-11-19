cmake_minimum_required(VERSION 3.16)
# https://github.com/ocornut/imgui with custom modifications made to the OpenGL 3 and GLFW backends
project(imgui_custom)

set(CMAKE_CXX_STANDARD 17)

add_library(imgui_custom OBJECT
    source/imgui_impl_opengl3.cpp
    source/imgui_impl_glfw.cpp
)

target_include_directories(imgui_custom PUBLIC
    include
)

target_link_libraries(imgui_custom PRIVATE imgui_includes)
set_property(TARGET imgui_custom PROPERTY POSITION_INDEPENDENT_CODE ON)