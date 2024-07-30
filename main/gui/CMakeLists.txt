project(main)

add_executable(main ${APPLICATION_TYPE}
    source/main.cpp
    source/crash_handlers.cpp
    source/stacktrace.cpp

    source/window/window.cpp
    source/window/win_window.cpp
    source/window/macos_window.cpp
    source/window/linux_window.cpp
    source/window/web_window.cpp

    source/messaging/common.cpp
    source/messaging/linux.cpp
    source/messaging/macos.cpp
    source/messaging/win.cpp
    source/messaging/web.cpp

    source/init/splash_window.cpp
    source/init/tasks.cpp

    source/init/run/common.cpp
    source/init/run/native.cpp
    source/init/run/web.cpp
    source/init/run/cli.cpp

    ${IMHEX_ICON}
)

target_include_directories(main PUBLIC include)
setupCompilerFlags(main)
setupCompilerFlags(libimhex)

set(LIBROMFS_RESOURCE_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/romfs)
set(LIBROMFS_PROJECT_NAME imhex)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../lib/external/libromfs ${CMAKE_CURRENT_BINARY_DIR}/main/gui/libromfs EXCLUDE_FROM_ALL)
add_dependencies(imhex_all main)

if (EMSCRIPTEN)
    target_link_options(main PRIVATE -sUSE_GLFW=3 -sUSE_PTHREADS=1 -sALLOW_MEMORY_GROWTH=1)
    target_link_options(main PRIVATE -sTOTAL_MEMORY=134217728)
    target_link_options(main PRIVATE -sMAX_WEBGL_VERSION=2)
    target_link_options(main PRIVATE -sEXPORTED_RUNTIME_METHODS=ccall)
    target_link_options(main PRIVATE -sFETCH)
    target_link_options(main PRIVATE -sWASM_BIGINT)
    target_link_options(main PRIVATE -O1)
    target_link_options(main PRIVATE -sLEGACY_GL_EMULATION)
    target_link_options(main PRIVATE -fsanitize=null)
    target_link_options(main PRIVATE -fsanitize-minimal-runtime)
    target_link_libraries(main PRIVATE idbfs.js)
endif ()

set_target_properties(main PROPERTIES
    OUTPUT_NAME "${IMHEX_APPLICATION_NAME}"
    RUNTIME_OUTPUT_DIRECTORY "${IMHEX_MAIN_OUTPUT_DIRECTORY}"
)

target_compile_definitions(main PRIVATE IMHEX_PROJECT_NAME="${PROJECT_NAME}")

target_link_libraries(main PRIVATE libromfs-imhex libimhex libwolv ${LIBBACKTRACE_LIBRARIES} LLVMDemangle)
if (WIN32)
    target_link_libraries(main PRIVATE usp10 wsock32 ws2_32 Dwmapi.lib Winmm.lib)
else ()
    target_link_libraries(main PRIVATE pthread)

    if (NOT APPLE)
        find_package(Fontconfig)
        if (TARGET Fontconfig::Fontconfig)
            message(STATUS "Using Fontconfig version: ${Fontconfig_VERSION}")
            target_link_libraries(main PRIVATE Fontconfig::Fontconfig)
            target_compile_definitions(main PRIVATE IMHEX_HAS_FONTCONFIG)
        endif ()
    endif ()
endif ()

precompileHeaders(main ${CMAKE_CURRENT_SOURCE_DIR}/include)

if (APPLE)
    add_compile_definitions(GL_SILENCE_DEPRECATION)
endif ()
