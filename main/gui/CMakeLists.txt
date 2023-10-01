project(main)

add_executable(main ${APPLICATION_TYPE}
    source/main.cpp
    source/crash_handlers.cpp

    source/window/window.cpp
    source/window/win_window.cpp
    source/window/macos_window.cpp
    source/window/linux_window.cpp

    source/messaging/common.cpp
    source/messaging/linux.cpp
    source/messaging/macos.cpp
    source/messaging/win.cpp

    source/init/splash_window.cpp
    source/init/tasks.cpp

    ${IMHEX_ICON}
)

target_include_directories(main PUBLIC include)
setupCompilerFlags(main)

set(LIBROMFS_RESOURCE_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/romfs)
set(LIBROMFS_PROJECT_NAME imhex)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../lib/external/libromfs ${CMAKE_CURRENT_BINARY_DIR}/main/gui/libromfs EXCLUDE_FROM_ALL)
set_target_properties(${LIBROMFS_LIBRARY} PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_dependencies(imhex_all main)

if(EMSCRIPTEN)
    target_link_options(main PRIVATE -sUSE_GLFW=3 -sUSE_PTHREADS=1 -sALLOW_MEMORY_GROWTH=1)
    target_link_options(main PRIVATE -O0)
    target_link_options(main PRIVATE -sTOTAL_MEMORY=134217728)
    target_link_options(main PRIVATE -sMAX_WEBGL_VERSION=2)
    target_link_options(main PRIVATE -sEXPORTED_RUNTIME_METHODS=ccall)
    target_link_options(main PRIVATE -sFETCH)
    target_link_options(main PRIVATE -sLEGACY_GL_EMULATION)
    target_link_libraries(main PRIVATE idbfs.js)
endif()

set_target_properties(main PROPERTIES
    OUTPUT_NAME ${IMHEX_APPLICATION_NAME}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/../..
    CXX_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE ON)

add_compile_definitions(IMHEX_PROJECT_NAME="${PROJECT_NAME}")

target_link_libraries(main PRIVATE libromfs-imhex libimhex plugin-bundle ${FMT_LIBRARIES})
if (WIN32)
    target_link_libraries(main PRIVATE usp10 wsock32 ws2_32 Dwmapi.lib)
else ()
    target_link_libraries(main PRIVATE pthread)
endif ()

if (APPLE)
    add_compile_definitions(GL_SILENCE_DEPRECATION)
endif ()