cmake_minimum_required(VERSION 3.16)
# https://github.com/ocornut/imgui backends with custom modifications made to the OpenGL 3 and GLFW backends
project(imgui_backend)

set(CMAKE_CXX_STANDARD 23)

if (NOT IMHEX_EXTERNAL_PLUGIN_BUILD)
    add_library(imgui_backend OBJECT
        source/imgui_impl_opengl3.cpp
    )

    add_library(imgui_platform_backend OBJECT)

    target_include_directories(imgui_backend PUBLIC
        include
    )

    target_include_directories(imgui_platform_backend PUBLIC
        include
    )

    target_link_libraries(imgui_backend PRIVATE imgui_includes)
    target_link_libraries(imgui_platform_backend PRIVATE imgui_includes)
    target_compile_definitions(imgui_platform_backend PRIVATE IMGUI_IMPL_API=)

    find_package(OpenGL REQUIRED)
    find_package(Freetype REQUIRED)

    if (IMHEX_RENDERING_BACKEND STREQUAL "GLFW")
        target_sources(imgui_platform_backend PRIVATE source/imgui_impl_glfw.cpp)
        if (EMSCRIPTEN)
            target_compile_options(imgui_platform_backend PRIVATE --use-port=contrib.glfw3)
        endif()

        if (UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
            find_package(X11)
            target_include_directories(imgui_platform_backend PUBLIC ${X11_INCLUDE_DIR})
            target_link_libraries(imgui_platform_backend PUBLIC ${X11_LIBRARIES})
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

        target_link_libraries(imgui_platform_backend PUBLIC ${GLFW_LIBRARIES})
    elseif (IMHEX_RENDERING_BACKEND STREQUAL "SDL3")
        target_sources(imgui_platform_backend PRIVATE source/imgui_impl_sdl3.cpp)

        find_package(SDL3 3.4.0 CONFIG QUIET)
        if (TARGET SDL3::SDL3)
            target_link_libraries(imgui_platform_backend PUBLIC SDL3::SDL3)
        else()
            find_package(PkgConfig REQUIRED)
            pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET "sdl3>=3.4.0")
            target_link_libraries(imgui_platform_backend PUBLIC PkgConfig::SDL3)
        endif()

    else()
        message(FATAL_ERROR "Invalid ImGui rendering backend was selected: IMHEX_RENDERING_BACKEND = '${IMHEX_RENDERING_BACKEND}'")
    endif()

    target_include_directories(imgui_backend PUBLIC ${FREETYPE_INCLUDE_DIRS} ${OpenGL_INCLUDE_DIRS})
    target_link_directories(imgui_backend PUBLIC ${FREETYPE_LIBRARY_DIRS} ${OpenGL_LIBRARY_DIRS})
    target_link_libraries(imgui_backend PUBLIC ${OPENGL_LIBRARIES})
endif()

target_include_directories(imgui_all_includes INTERFACE include)
