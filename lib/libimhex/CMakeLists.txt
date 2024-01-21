cmake_minimum_required(VERSION 3.16)
project(libimhex)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_SHARED_LIBRARY_PREFIX "")

set(LIBIMHEX_SOURCES
        source/api/imhex_api.cpp
        source/api/content_registry.cpp
        source/api/event_manager.cpp
        source/api/task_manager.cpp
        source/api/shortcut_manager.cpp
        source/api/plugin_manager.cpp
        source/api/project_file_manager.cpp
        source/api/theme_manager.cpp
        source/api/layout_manager.cpp
        source/api/workspace_manager.cpp
        source/api/achievement_manager.cpp
        source/api/localization_manager.cpp
        source/api/tutorial_manager.cpp

        source/data_processor/attribute.cpp
        source/data_processor/link.cpp
        source/data_processor/node.cpp

        source/helpers/utils.cpp
        source/helpers/utils_linux.cpp
        source/helpers/fs.cpp
        source/helpers/magic.cpp
        source/helpers/crypto.cpp
        source/helpers/http_requests.cpp
        source/helpers/http_requests_native.cpp
        source/helpers/http_requests_emscripten.cpp
        source/helpers/opengl.cpp
        source/helpers/patches.cpp
        source/helpers/encoding_file.cpp
        source/helpers/logger.cpp
        source/helpers/tar.cpp
        source/helpers/debugging.cpp

        source/providers/provider.cpp
        source/providers/memory_provider.cpp
        source/providers/undo/stack.cpp

        source/ui/imgui_imhex_extensions.cpp
        source/ui/view.cpp
        source/ui/popup.cpp
        source/ui/toast.cpp

        source/subcommands/subcommands.cpp
)

if (APPLE)
    set(OSX_11_0_SDK_PATH  /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX11.0.sdk)
    if (NOT CMAKE_OSX_SYSROOT)
        if (IS_DIRECTORY ${OSX_11_0_SDK_PATH})
            set(CMAKE_OSX_SYSROOT ${OSX_11_0_SDK_PATH})
        else ()
            message(WARNING "CMAKE_OSX_SYSROOT not set and macOS 10.9 SDK not found! Using default one.")
        endif ()
    endif ()

    set(LIBIMHEX_SOURCES ${LIBIMHEX_SOURCES} source/helpers/utils_macos.m)
endif ()

if (IMHEX_EXTERNAL_PLUGIN_BUILD)
    add_library(libimhex IMPORTED SHARED GLOBAL)
    set(LIBIMHEX_LIBRARY_TYPE INTERFACE)
else()
    if (IMHEX_STATIC_LINK_PLUGINS)
        add_library(libimhex STATIC ${LIBIMHEX_SOURCES})
    else()
        add_library(libimhex SHARED ${LIBIMHEX_SOURCES})
    endif()

    set(LIBIMHEX_LIBRARY_TYPE PUBLIC)
    target_compile_definitions(libimhex PRIVATE IMHEX_PROJECT_NAME="${PROJECT_NAME}")
endif()

set_target_properties(libimhex PROPERTIES POSITION_INDEPENDENT_CODE ON)
enableUnityBuild(libimhex)
setupCompilerFlags(libimhex)

include(GenerateExportHeader)
generate_export_header(libimhex)

target_include_directories(libimhex ${LIBIMHEX_LIBRARY_TYPE} include ${XDGPP_INCLUDE_DIRS} ${MBEDTLS_INCLUDE_DIR} ${MAGIC_INCLUDE_DIRS} ${LLVM_INCLUDE_DIRS} ${FMT_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS} ${LIBBACKTRACE_INCLUDE_DIRS})
target_link_directories(libimhex ${LIBIMHEX_LIBRARY_TYPE} ${MBEDTLS_LIBRARY_DIR} ${MAGIC_LIBRARY_DIRS})

if (NOT EMSCRIPTEN)
    # curl is only used in non-emscripten builds
    target_include_directories(libimhex ${LIBIMHEX_LIBRARY_TYPE}  ${CURL_INCLUDE_DIRS})
    target_link_libraries(libimhex ${LIBIMHEX_LIBRARY_TYPE} ${LIBCURL_LIBRARIES})
endif()


if (NOT IMHEX_EXTERNAL_PLUGIN_BUILD)
    if (WIN32)
        set_target_properties(libimhex PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
        target_link_options(libimhex PRIVATE -Wl,--export-all-symbols)
    elseif (APPLE)
        find_library(FOUNDATION NAMES Foundation)
        target_link_libraries(libimhex PUBLIC ${FOUNDATION})
    endif ()

    target_link_libraries(libimhex PRIVATE microtar libwolv ${NFD_LIBRARIES} magic dl ${NLOHMANN_JSON_LIBRARIES} ${MBEDTLS_LIBRARIES} ${JTHREAD_LIBRARIES})
    target_link_libraries(libimhex PUBLIC libpl ${IMGUI_LIBRARIES})
endif()

target_link_libraries(libimhex ${LIBIMHEX_LIBRARY_TYPE} ${NLOHMANN_JSON_LIBRARIES} imgui_all_includes ${FMT_LIBRARIES})

set_property(TARGET libimhex PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)

if (DEFINED IMHEX_COMMIT_HASH_LONG AND DEFINED IMHEX_COMMIT_HASH_SHORT AND DEFINED IMHEX_COMMIT_BRANCH)
    set(GIT_COMMIT_HASH_LONG "${IMHEX_COMMIT_HASH_LONG}")
    set(GIT_COMMIT_HASH_SHORT "${IMHEX_COMMIT_HASH_SHORT}")
    set(GIT_BRANCH "${IMHEX_COMMIT_BRANCH}")
else()
    # Get the current working branch
    execute_process(
            COMMAND git rev-parse --abbrev-ref HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_BRANCH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE RESULT_BRANCH
            ERROR_QUIET
    )

    # Get the latest abbreviated commit hash of the working branch
    execute_process(
            COMMAND git log -1 --format=%h --abbrev=7
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_COMMIT_HASH_SHORT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE RESULT_HASH_SHORT
            ERROR_QUIET
    )

    execute_process(
            COMMAND git log -1 --format=%H
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_COMMIT_HASH_LONG
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE RESULT_HASH_LONG
            ERROR_QUIET
    )
endif ()

if ((NOT GIT_COMMIT_HASH_SHORT STREQUAL "") AND (NOT GIT_COMMIT_HASH_LONG STREQUAL "") AND (NOT GIT_BRANCH STREQUAL ""))
    addDefineToSource(source/api/imhex_api.cpp "GIT_COMMIT_HASH_SHORT=\"${GIT_COMMIT_HASH_SHORT}\"")
    addDefineToSource(source/api/imhex_api.cpp "GIT_COMMIT_HASH_LONG=\"${GIT_COMMIT_HASH_LONG}\"")
    addDefineToSource(source/api/imhex_api.cpp "GIT_BRANCH=\"${GIT_BRANCH}\"")
endif ()

addDefineToSource(source/api/imhex_api.cpp "IMHEX_VERSION=\"${IMHEX_VERSION_STRING}\"")

add_dependencies(imhex_all libimhex)