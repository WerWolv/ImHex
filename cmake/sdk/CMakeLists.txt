cmake_minimum_required(VERSION 3.20)
project(ImHexSDK)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/build_helpers.cmake")

set(IMHEX_BASE_FOLDER ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)
include(ImHexPlugin)

function(add_subdirectory_if_exists folder)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${folder}/CMakeLists.txt")
        add_subdirectory("${folder}")
    endif()
endfunction()

set(IMHEX_EXTERNAL_PLUGIN_BUILD ON)
add_custom_target(imhex_all)

add_subdirectory(lib/third_party/imgui)
add_subdirectory_if_exists(lib/third_party/fmt)
add_subdirectory_if_exists(lib/third_party/nlohmann_json)

set(FMT_LIBRARIES fmt::fmt-header-only)

add_subdirectory(lib/external/libwolv)

set(LIBPL_BUILD_CLI_AS_EXECUTABLE OFF)
add_subdirectory(lib/external/pattern_language)

add_subdirectory(lib/libimhex)

if (WIN32)
    set_target_properties(libimhex PROPERTIES
            IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lib/libimhex.dll"
            IMPORTED_IMPLIB "${CMAKE_CURRENT_SOURCE_DIR}/lib/liblibimhex.dll.a"
            INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/lib/libimhex/include")
elseif (APPLE)
    set_target_properties(libimhex PROPERTIES
            IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lib/libimhex.dylib"
            INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/lib/libimhex/include")
else()
    set_target_properties(libimhex PROPERTIES
            IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lib/libimhex.so"
            INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/lib/libimhex/include")
endif()
