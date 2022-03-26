project(main)

add_executable(main ${APPLICATION_TYPE}
    source/main.cpp

    source/window/window.cpp
    source/window/win_window.cpp
    source/window/macos_window.cpp
    source/window/linux_window.cpp

    source/init/splash_window.cpp
    source/init/tasks.cpp

    ${IMHEX_ICON}
)

target_include_directories(main PUBLIC include)
target_compile_options(main PRIVATE -Wall -Wextra -Werror)

set_target_properties(main PROPERTIES
        OUTPUT_NAME "imhex"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/..
        CXX_VISIBILITY_PRESET hidden
        POSITION_INDEPENDENT_CODE ON)

add_compile_definitions(IMHEX_PROJECT_NAME="${PROJECT_NAME}")

if (WIN32)
    target_link_libraries(main PUBLIC libimhex wsock32 ws2_32 Dwmapi.lib)
else ()
    target_link_libraries(main PUBLIC libimhex pthread)
endif ()

if (APPLE)
    add_compile_definitions(GL_SILENCE_DEPRECATION)
endif ()