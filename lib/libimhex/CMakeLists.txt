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

    source/data_processor/attribute.cpp
    source/data_processor/link.cpp
    source/data_processor/node.cpp

    source/helpers/utils.cpp
    source/helpers/fs.cpp
    source/helpers/magic.cpp
    source/helpers/crypto.cpp
    source/helpers/net.cpp
    source/helpers/file.cpp
    source/helpers/socket.cpp
    source/helpers/patches.cpp
    source/helpers/encoding_file.cpp
    source/helpers/logger.cpp
    source/helpers/stacktrace.cpp
    source/helpers/tar.cpp

    source/providers/provider.cpp

    source/ui/imgui_imhex_extensions.cpp
    source/ui/view.cpp
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

    set(LIBIMHEX_SOURCES ${LIBIMHEX_SOURCES}
            source/helpers/fs_macos.m
            source/helpers/utils_macos.m)
endif ()

add_compile_definitions(IMHEX_PROJECT_NAME="${PROJECT_NAME}")

add_library(libimhex SHARED ${LIBIMHEX_SOURCES})
set_target_properties(libimhex PROPERTIES POSITION_INDEPENDENT_CODE ON)
setupCompilerWarnings(libimhex)


target_include_directories(libimhex PUBLIC include ${XDGPP_INCLUDE_DIRS} ${MBEDTLS_INCLUDE_DIRS} ${CAPSTONE_INCLUDE_DIRS} ${MAGIC_INCLUDE_DIRS} ${LLVM_INCLUDE_DIRS} ${FMT_INCLUDE_DIRS} ${CURL_INCLUDE_DIRS} ${YARA_INCLUDE_DIRS})
target_link_directories(libimhex PUBLIC ${MBEDTLS_LIBRARY_DIR} ${CAPSTONE_LIBRARY_DIRS} ${MAGIC_LIBRARY_DIRS})

if (WIN32)
    target_link_options(libimhex PRIVATE -Wl,--export-all-symbols)
elseif (APPLE)
    find_library(FOUNDATION NAMES Foundation)
    target_link_libraries(libimhex PUBLIC ${FOUNDATION})
endif ()

target_link_libraries(libimhex PRIVATE ${FMT_LIBRARIES})
target_link_libraries(libimhex PUBLIC dl imgui ${NFD_LIBRARIES} magic ${CAPSTONE_LIBRARIES} LLVMDemangle microtar ${NLOHMANN_JSON_LIBRARIES} ${YARA_LIBRARIES} ${LIBCURL_LIBRARIES} ${MBEDTLS_LIBRARIES} ${BACKTRACE_LIBRARIES} libpl intervaltree)