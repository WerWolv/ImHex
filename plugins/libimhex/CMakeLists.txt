cmake_minimum_required(VERSION 3.16)
project(libimhex)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../external/ImGui ${CMAKE_CURRENT_BINARY_DIR}/external/ImGui)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../external/nlohmann_json ${CMAKE_CURRENT_BINARY_DIR}/external/nlohmann_json)


if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc -Wl,--allow-multiple-definition -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

set(CMAKE_SHARED_LIBRARY_PREFIX "")

set(SRC
    source/api/event.cpp
    source/api/imhex_api.cpp
    source/api/content_registry.cpp

    source/helpers/utils.cpp
    source/helpers/shared_data.cpp
    source/helpers/crypto.cpp
    source/helpers/lang.cpp

    source/lang/pattern_language.cpp
    source/lang/preprocessor.cpp
    source/lang/lexer.cpp
    source/lang/parser.cpp
    source/lang/validator.cpp
    source/lang/evaluator.cpp
    source/lang/builtin_functions.cpp

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

    set(SRC ${SRC} source/helpers/utils_mac.mm)
endif ()

add_library(libimhex SHARED ${SRC})

target_include_directories(libimhex PUBLIC include ${MBEDTLS_INCLUDE_DIR})
target_link_directories(libimhex PUBLIC ${MBEDTLS_LIBRARY_DIR})

if (WIN32)
    target_link_libraries(libimhex PUBLIC imgui nlohmann_json libmbedcrypto.a)
elseif (APPLE)
    find_library(FOUNDATION NAMES Foundation)
    target_link_libraries(libimhex PUBLIC imgui nlohmann_json mbedcrypto ${FOUNDATION})
else()
    target_link_libraries(libimhex PUBLIC imgui nlohmann_json mbedcrypto)
endif ()