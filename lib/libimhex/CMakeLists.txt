cmake_minimum_required(VERSION 3.16)
project(libimhex)

set(CMAKE_CXX_STANDARD 23)

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
        source/helpers/default_paths.cpp
        source/helpers/imgui_hooks.cpp
        source/helpers/semantic_version.cpp
        source/helpers/keys.cpp
        source/helpers/udp_server.cpp

        source/test/tests.cpp

        source/providers/provider.cpp
        source/providers/cached_provider.cpp
        source/providers/memory_provider.cpp
        source/providers/undo/stack.cpp

        source/ui/imgui_imhex_extensions.cpp
        source/ui/view.cpp
        source/ui/popup.cpp
        source/ui/toast.cpp
        source/ui/banner.cpp

        source/subcommands/subcommands.cpp
)

if (APPLE)
    set(OSX_SDK_PATH  /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX13.sdk)
    if (NOT CMAKE_OSX_SYSROOT)
        if (IS_DIRECTORY ${OSX_SDK_PATH})
            set(CMAKE_OSX_SYSROOT ${OSX_SDK_PATH})
        else ()
            message(WARNING "CMAKE_OSX_SYSROOT not set and macOS SDK not found! Using default one.")
        endif ()
    endif ()

    set(LIBIMHEX_SOURCES ${LIBIMHEX_SOURCES}
            source/helpers/utils_macos.m
            source/helpers/macos_menu.m
    )
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

    if (IMHEX_ENABLE_CXX_MODULES)
        target_sources(libimhex
                PUBLIC
                FILE_SET cxx_modules TYPE CXX_MODULES
                FILES
                include/hex.cppm
        )
    endif()

    set(LIBIMHEX_LIBRARY_TYPE PUBLIC)
    target_compile_definitions(libimhex PRIVATE IMHEX_PROJECT_NAME="${PROJECT_NAME}")
endif()

addCppCheck(libimhex)


if (DEFINED IMHEX_COMMIT_HASH_LONG AND DEFINED IMHEX_COMMIT_BRANCH)
    set(GIT_COMMIT_HASH_LONG "${IMHEX_COMMIT_HASH_LONG}")
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

    execute_process(
            COMMAND git log -1 --format=%H
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_COMMIT_HASH_LONG
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE RESULT_HASH_LONG
            ERROR_QUIET
    )
endif ()

if (GIT_COMMIT_HASH_LONG STREQUAL "" OR GIT_BRANCH STREQUAL "")
    message(WARNING "Failed to to determine commit hash/branch")
else()
    addDefineToSource(source/api/imhex_api.cpp "GIT_COMMIT_HASH_LONG=\"${GIT_COMMIT_HASH_LONG}\"")
    addDefineToSource(source/api/imhex_api.cpp "GIT_BRANCH=\"${GIT_BRANCH}\"")
endif ()

addDefineToSource(source/api/imhex_api.cpp "IMHEX_VERSION=\"${IMHEX_VERSION_STRING}\"")

string(TIMESTAMP IMHEX_BUILD_DATE UTC)
addDefineToSource(source/api/imhex_api.cpp "IMHEX_BUILD_DATE=\"${IMHEX_BUILD_DATE}\"")

enableUnityBuild(libimhex)
setupCompilerFlags(libimhex)

include(GenerateExportHeader)
generate_export_header(libimhex)

target_include_directories(libimhex ${LIBIMHEX_LIBRARY_TYPE} include ${XDGPP_INCLUDE_DIRS} ${LLVM_INCLUDE_DIRS} ${FMT_INCLUDE_DIRS})

if (NOT IMHEX_EXTERNAL_PLUGIN_BUILD)
    if (WIN32)
        set_target_properties(libimhex PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
        if (NOT MSVC)
            target_link_options(libimhex PRIVATE -Wl,--export-all-symbols)
        endif()
        target_link_libraries(libimhex PRIVATE Netapi32.lib)
        target_compile_definitions(libimhex PRIVATE EXPORT_SYMBOLS=1)
    elseif (APPLE)
        find_library(FOUNDATION NAMES Foundation)
        target_link_libraries(libimhex PUBLIC ${FOUNDATION})
    endif ()

    target_link_libraries(libimhex PRIVATE libpl microtar ${NFD_LIBRARIES} magic)
    target_link_libraries(libimhex PUBLIC libwolv libpl_includes libpl-gen ${IMGUI_LIBRARIES} ${JTHREAD_LIBRARIES})

    if (IMHEX_ENABLE_IMGUI_TEST_ENGINE)
        target_link_libraries(libimhex PUBLIC imgui_test_engine)
    endif()

    if (NOT WIN32)
        target_link_libraries(libimhex PRIVATE dl)
    endif()

    if (NOT EMSCRIPTEN)
        # curl is only used in non-emscripten builds
        target_link_libraries(libimhex ${LIBIMHEX_LIBRARY_TYPE} CURL::libcurl)
    endif()

    target_include_directories(libimhex ${LIBIMHEX_LIBRARY_TYPE} ${MBEDTLS_INCLUDE_DIR} ${LIBBACKTRACE_INCLUDE_DIRS} ${MAGIC_INCLUDE_DIRS})
    target_link_libraries(libimhex ${LIBIMHEX_LIBRARY_TYPE} ${MBEDTLS_LIBRARIES})
    target_link_directories(libimhex ${LIBIMHEX_LIBRARY_TYPE} ${MBEDTLS_LIBRARY_DIR} ${MAGIC_LIBRARY_DIRS})

    precompileHeaders(libimhex "${CMAKE_CURRENT_SOURCE_DIR}/include")
endif()

target_link_libraries(libimhex ${LIBIMHEX_LIBRARY_TYPE} ${NLOHMANN_JSON_LIBRARIES} imgui_all_includes ${FMT_LIBRARIES} ${LUNASVG_LIBRARIES} ${BOOST_LIBRARIES} tracing)

set_property(TARGET libimhex PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)

add_dependencies(imhex_all libimhex)

install(FILES "$<TARGET_FILE:libimhex>" DESTINATION "${CMAKE_INSTALL_LIBDIR}" PERMISSIONS ${LIBRARY_PERMISSIONS})
set_target_properties(libimhex PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set_target_properties(libimhex PROPERTIES PREFIX "")