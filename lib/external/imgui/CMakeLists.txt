cmake_minimum_required(VERSION 3.16)
project(imgui)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
find_package(Freetype REQUIRED)
find_package(OpenGL REQUIRED)
pkg_search_module(GLFW REQUIRED glfw3)

add_library(imgui OBJECT
        source/imgui.cpp
        source/imgui_demo.cpp
        source/imgui_draw.cpp
        include/misc/freetype/imgui_freetype.cpp # TODO move source and includes in the same directory
        source/imgui_impl_glfw.cpp
        source/imgui_impl_opengl3.cpp
        source/imgui_tables.cpp
        source/imgui_widgets.cpp

        source/TextEditor.cpp

        source/imnodes.cpp

        source/implot.cpp
        source/implot_items.cpp
        source/implot_demo.cpp

        source/fonts/fontawesome_font.c
        source/fonts/codicons_font.c
        source/fonts/unifont_font.c
)

target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD)
target_compile_options(imgui PRIVATE -Wno-stringop-overflow)

# ImGui config
target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS=ON) # Use ImGui math operators for ImVec2
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS=ON) # disable obsolete functions
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_KEYIO=ON) # disable obsolete functions
target_compile_definitions(imgui PUBLIC IMGUI_ENABLE_FREETYPE=ON)

target_include_directories(imgui PUBLIC include ${FREETYPE_INCLUDE_DIRS} ${GLFW_INCLUDE_DIRS} ${OpenGL_INCLUDE_DIRS})
target_link_directories(imgui PUBLIC ${GLFW_LIBRARY_DIRS} ${OpenGL_LIBRARY_DIRS})
target_link_libraries(imgui PUBLIC Freetype::Freetype ${GLFW_LIBRARIES} ${OPENGL_LIBRARIES})
