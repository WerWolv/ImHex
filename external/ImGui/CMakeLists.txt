cmake_minimum_required(VERSION 3.16)
project(imgui)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
find_package(Freetype REQUIRED)
pkg_search_module(GLM REQUIRED glm)
pkg_search_module(GLFW REQUIRED glfw3)

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

add_compile_definitions(IMGUI_IMPL_OPENGL_LOADER_GLAD)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../glad ${CMAKE_CURRENT_BINARY_DIR}/external/glad)

target_include_directories(imgui PUBLIC include ${FREETYPE_INCLUDE_DIRS} ${GLFW_INCLUDE_DIRS})

target_link_directories(imgui PUBLIC ${GLM_INCLUDE_DIRS} ${GLFW_LIBRARY_DIRS})

if (WIN32)
    target_link_libraries(imgui Freetype::Freetype glad libglfw3.a)
elseif (UNIX)
    target_link_libraries(imgui Freetype::Freetype glad glfw3)
endif()
