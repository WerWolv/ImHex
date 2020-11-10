cmake_minimum_required(VERSION 3.16)
project(HexEditor)

set(CMAKE_CXX_STANDARD 20)

find_library(GLFW REQUIRED glfw3s)
find_library(OPENGL REQUIRED OpenGL)

include_directories(include ${GLFW_INCLUDE_DIRS} libs/ImGui/include libs/glad/include)

add_executable(HexEditor
        source/main.cpp
        source/window.cpp

        source/parser/lexer.cpp
        source/parser/parser.cpp

        libs/glad/source/glad.c

        libs/ImGui/source/imgui.cpp
        libs/ImGui/source/imgui_demo.cpp
        libs/ImGui/source/imgui_draw.cpp
        libs/ImGui/source/imgui_widgets.cpp
        libs/ImGui/source/imgui_impl_glfw.cpp
        libs/ImGui/source/imgui_impl_opengl3.cpp

        res.rc
        )

target_link_libraries(HexEditor glfw3)


