cmake_minimum_required(VERSION 3.16)
project(libimhex)

set(CMAKE_CXX_STANDARD 20)

set(BUILD_SHARED_LIBS OFF)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../external/ImGui ${CMAKE_CURRENT_BINARY_DIR}/external/ImGui)

if(USE_SYSTEM_NLOHMANN_JSON)
  find_package(nlohmann_json REQUIRED)
else()
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../external/nlohmann_json ${CMAKE_CURRENT_BINARY_DIR}/external/nlohmann_json)
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../external/nativefiledialog ${CMAKE_CURRENT_BINARY_DIR}/external/nativefiledialog EXCLUDE_FROM_ALL)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../external/fmt ${CMAKE_CURRENT_BINARY_DIR}/external/fmt)

set(XDGPP_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../../external/xdgpp")
set(CMAKE_USE_MBEDTLS ON)
set(BUILD_CURL_EXE OFF)
set(FPHSA_NAME_MISMATCHED ON CACHE BOOL "")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../external/curl ${CMAKE_CURRENT_BINARY_DIR}/external/curl EXCLUDE_FROM_ALL)
set_target_properties(libcurl PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

set(CMAKE_SHARED_LIBRARY_PREFIX "")

set(LIBIMHEX_SOURCES
    source/api/event.cpp
    source/api/imhex_api.cpp
    source/api/content_registry.cpp

    source/helpers/utils.cpp
    source/helpers/shared_data.cpp
    source/helpers/crypto.cpp
    source/helpers/lang.cpp
    source/helpers/net.cpp

    source/lang/pattern_language.cpp
    source/lang/preprocessor.cpp
    source/lang/lexer.cpp
    source/lang/parser.cpp
    source/lang/validator.cpp
    source/lang/evaluator.cpp
    source/providers/provider.cpp

    source/views/view.cpp
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
    set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0)

    set(LIBIMHEX_SOURCES ${LIBIMHEX_SOURCES} source/helpers/utils_mac.mm)
endif ()

add_library(libimhex SHARED ${LIBIMHEX_SOURCES})

target_include_directories(libimhex PUBLIC include ${MBEDTLS_INCLUDE_DIR} ${XDGPP_INCLUDE_DIRS})
target_link_directories(libimhex PUBLIC ${MBEDTLS_LIBRARY_DIR})

if (APPLE)
    find_library(FOUNDATION NAMES Foundation)
    target_link_libraries(libimhex PUBLIC imgui nlohmann_json mbedcrypto ${FOUNDATION} nfd fmt-header-only libcurl)
else ()
    target_link_libraries(libimhex PUBLIC imgui nlohmann_json mbedcrypto nfd fmt-header-only libcurl)
endif ()