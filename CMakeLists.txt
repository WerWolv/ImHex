cmake_minimum_required(VERSION 3.16)
project(HexEditor)

set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)
find_package(OpenGL REQUIRED)

include_directories(include ${GLFW_INCLUDE_DIRS} libs/ImGui/include libs/glad/include)
SET(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -DIMGUI_IMPL_OPENGL_LOADER_GLAD -static-libstdc++ -static-libgcc -static")

add_executable(ImHex
        source/main.cpp
        source/window.cpp
        source/utils.cpp
        source/crypto.cpp

        source/parser/lexer.cpp
        source/parser/parser.cpp

        source/provider/file_provider.cpp

        source/views/view_hexeditor.cpp
        source/views/view_pattern.cpp
        source/views/view_pattern_data.cpp
        source/views/view_hashes.cpp
        source/views/view_information.cpp
        source/views/view_help.cpp
        source/views/view_tools.cpp

        libs/glad/source/glad.c

        libs/ImGui/source/imgui.cpp
        libs/ImGui/source/imgui_draw.cpp
        libs/ImGui/source/imgui_widgets.cpp
        libs/ImGui/source/imgui_impl_glfw.cpp
        libs/ImGui/source/imgui_impl_opengl3.cpp

        res.rc
        )

if (WIN32)
    target_link_libraries(ImHex libglfw3.a libgcc.a libstdc++.a libmagic.a libgnurx.a libtre.a libintl.a libiconv.a shlwapi.lib libcrypto.a libwinpthread.a)
endif (WIN32)

if (UNIX)
    target_link_libraries(ImHex libglfw.so libmagic.so libcrypto.so libdl.so)
endif (UNIX)