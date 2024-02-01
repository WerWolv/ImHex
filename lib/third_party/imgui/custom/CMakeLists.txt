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
target_include_directories(imgui_all_includes INTERFACE include)

find_package(OpenGL REQUIRED)
find_package(Freetype REQUIRED)

find_package(GLFW QUIET)
if (NOT GLFW_FOUND OR "${GLFW_LIBRARIES}" STREQUAL "")
    find_package(glfw3 QUIET)
    set(GLFW_INCLUDE_DIRS ${glfw3_INCLUDE_DIRS})
    set(GLFW_LIBRARIES ${glfw3_LIBRARIES})

    if (NOT glfw3_FOUND OR "${GLFW_LIBRARIES}" STREQUAL "")
        find_package(PkgConfig REQUIRED)
        pkg_search_module(GLFW REQUIRED glfw3)
    endif ()
endif()

if ("${GLFW_LIBRARIES}" MATCHES ".+dll")
    set(GLFW_LIBRARIES "glfw3")
endif ()

target_include_directories(imgui_custom PUBLIC ${FREETYPE_INCLUDE_DIRS} ${OpenGL_INCLUDE_DIRS})
target_link_directories(imgui_custom PUBLIC ${FREETYPE_LIBRARY_DIRS} ${OpenGL_LIBRARY_DIRS})
target_link_libraries(imgui_custom PUBLIC ${GLFW_LIBRARIES} ${OPENGL_LIBRARIES})