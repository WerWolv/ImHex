cmake_minimum_required(VERSION 3.16)
project(libimhex)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_SHARED_LIBRARY_PREFIX "")

set(LIBIMHEX_SOURCES
        source/api/event.cpp
        source/api/imhex_api.cpp
        source/api/content_registry.cpp
        source/api/task.cpp
        source/api/keybinding.cpp
        source/api/plugin_manager.cpp
        source/api/localization.cpp
        source/api/project_file_manager.cpp
        source/api/theme_manager.cpp
        source/api/layout_manager.cpp
        source/api/achievement_manager.cpp

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
        source/helpers/stacktrace.cpp
        source/helpers/tar.cpp

        source/providers/provider.cpp

        source/ui/imgui_imhex_extensions.cpp
        source/ui/view.cpp
        source/ui/popup.cpp

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

add_compile_definitions(IMHEX_PROJECT_NAME="${PROJECT_NAME}")

add_library(libimhex SHARED ${LIBIMHEX_SOURCES})
set_target_properties(libimhex PROPERTIES POSITION_INDEPENDENT_CODE ON)
setupCompilerFlags(libimhex)

include(GenerateExportHeader)
generate_export_header(libimhex)

target_include_directories(libimhex PUBLIC include ${XDGPP_INCLUDE_DIRS} ${MBEDTLS_INCLUDE_DIR} ${CAPSTONE_INCLUDE_DIRS} ${MAGIC_INCLUDE_DIRS} ${LLVM_INCLUDE_DIRS} ${FMT_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS} ${YARA_INCLUDE_DIRS} ${LIBBACKTRACE_INCLUDE_DIRS})
target_link_directories(libimhex PUBLIC ${MBEDTLS_LIBRARY_DIR} ${CAPSTONE_LIBRARY_DIRS} ${MAGIC_LIBRARY_DIRS})

if (EMSCRIPTEN)
    find_path(JOSUTTIS_JTHREAD_INCLUDE_DIRS "condition_variable_any2.hpp")
    target_include_directories(libimhex PRIVATE ${JOSUTTIS_JTHREAD_INCLUDE_DIRS})
else()
    # curl is only used in non-emscripten builds
    target_include_directories(libimhex PUBLIC  ${CURL_INCLUDE_DIRS})
    target_link_libraries(libimhex PUBLIC ${LIBCURL_LIBRARIES})
endif()


if (WIN32)
    set_target_properties(libimhex PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
    target_link_options(libimhex PRIVATE -Wl,--export-all-symbols)
elseif (APPLE)
    find_library(FOUNDATION NAMES Foundation)
    target_link_libraries(libimhex PUBLIC ${FOUNDATION})
endif ()

target_link_libraries(libimhex PRIVATE ${FMT_LIBRARIES})
target_link_libraries(libimhex PUBLIC dl imgui ${NFD_LIBRARIES} magic ${CAPSTONE_LIBRARIES} LLVMDemangle microtar ${NLOHMANN_JSON_LIBRARIES} ${YARA_LIBRARIES} ${MBEDTLS_LIBRARIES} ${LIBBACKTRACE_LIBRARIES} plcli libpl libpl-gen ${MINIAUDIO_LIBRARIES} libwolv-utils libwolv-io libwolv-hash libwolv-net libwolv-containers)

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
    )

    # Get the latest abbreviated commit hash of the working branch
    execute_process(
            COMMAND git log -1 --format=%h --abbrev=7
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_COMMIT_HASH_SHORT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE RESULT_HASH_SHORT
    )

    execute_process(
            COMMAND git log -1 --format=%H
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_COMMIT_HASH_LONG
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE RESULT_HASH_LONG
    )
endif ()

if ((NOT GIT_COMMIT_HASH_SHORT STREQUAL "") AND (NOT GIT_COMMIT_HASH_LONG STREQUAL "") AND (NOT GIT_BRANCH STREQUAL ""))
    addDefineToSource(source/api/imhex_api.cpp "GIT_COMMIT_HASH_SHORT=\"${GIT_COMMIT_HASH_SHORT}\"")
    addDefineToSource(source/api/imhex_api.cpp "GIT_COMMIT_HASH_LONG=\"${GIT_COMMIT_HASH_LONG}\"")
    addDefineToSource(source/api/imhex_api.cpp "GIT_BRANCH=\"${GIT_BRANCH}\"")
endif ()

addDefineToSource(source/api/imhex_api.cpp "IMHEX_VERSION=\"${IMHEX_VERSION_STRING}\"")

add_dependencies(imhex_all libimhex)