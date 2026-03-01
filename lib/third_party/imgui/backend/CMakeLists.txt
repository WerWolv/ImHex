cmake_minimum_required(VERSION 3.16)
# https://github.com/ocornut/imgui backends with custom modifications made to the OpenGL 3 and GLFW backends
project(imgui_backend)

set(CMAKE_CXX_STANDARD 23)

if (NOT IMHEX_EXTERNAL_PLUGIN_BUILD)
    add_library(imgui_backend OBJECT
        source/imgui_impl_opengl3.cpp
        source/imgui_impl_glfw.cpp
    )

    target_include_directories(imgui_backend PUBLIC
        include
    )

    target_link_libraries(imgui_backend PRIVATE imgui_includes)

    find_package(OpenGL REQUIRED)
    find_package(Freetype REQUIRED)

    if (UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
        find_package(X11)
        target_include_directories(imgui_backend PUBLIC ${X11_INCLUDE_DIR})
        target_link_libraries(imgui_backend PUBLIC ${X11_LIBRARIES})
    endif()

    if (EMSCRIPTEN)
        target_compile_options(imgui_backend PRIVATE --use-port=contrib.glfw3)
    endif()

    find_package(GLFW QUIET)
    if (NOT GLFW_FOUND OR "${GLFW_LIBRARIES}" STREQUAL "")
        find_package(glfw3 QUIET)
        set(GLFW_INCLUDE_DIRS ${glfw3_INCLUDE_DIRS})
        set(GLFW_LIBRARIES ${glfw3_LIBRARIES})

        if (NOT glfw3_FOUND AND "${GLFW_LIBRARIES}" STREQUAL "")
            find_package(PkgConfig REQUIRED)
            pkg_search_module(GLFW REQUIRED glfw3)

            if ("${GLFW_LIBRARIES}" MATCHES ".+dll")
                set(GLFW_LIBRARIES "glfw3")
            endif ()
        else()
            set(GLFW_LIBRARIES GLFW::GLFW)
        endif ()
    endif()

    target_include_directories(imgui_backend PUBLIC ${FREETYPE_INCLUDE_DIRS} ${OpenGL_INCLUDE_DIRS})
    target_link_directories(imgui_backend PUBLIC ${FREETYPE_LIBRARY_DIRS} ${OpenGL_LIBRARY_DIRS})
    target_link_libraries(imgui_backend PUBLIC ${GLFW_LIBRARIES} ${OPENGL_LIBRARIES})
endif()

target_include_directories(imgui_all_includes INTERFACE include)