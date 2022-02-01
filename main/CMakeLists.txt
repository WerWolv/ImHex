project(main)

add_executable(main ${application_type}
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

set_target_properties(main PROPERTIES
        OUTPUT_NAME "imhex"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/..
        CXX_VISIBILITY_PRESET hidden
        POSITION_INDEPENDENT_CODE ON)

if (WIN32)
    target_link_libraries(main PUBLIC libimhex wsock32 ws2_32 Dwmapi.lib)
else ()
    target_link_libraries(main PUBLIC libimhex pthread)
endif ()