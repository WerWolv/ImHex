cmake_minimum_required(VERSION 3.20)
project(ImHexSDK)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")
set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "" FORCE)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/build_helpers.cmake")

set(IMHEX_BASE_FOLDER ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)
include(ImHexPlugin)

function(add_subdirectory_if_exists folder)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${folder}/CMakeLists.txt")
        add_subdirectory("${folder}" EXCLUDE_FROM_ALL)
    endif()
endfunction()

set(IMHEX_EXTERNAL_PLUGIN_BUILD ON PARENT_SCOPE)
set(IMHEX_EXTERNAL_PLUGIN_BUILD ON)
add_custom_target(imhex_all)

add_subdirectory(lib/third_party/imgui EXCLUDE_FROM_ALL)

set(FMT_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory_if_exists(lib/third_party/fmt)
set(FMT_LIBRARIES fmt::fmt-header-only PARENT_SCOPE)
set(FMT_LIBRARIES fmt::fmt-header-only)

add_subdirectory_if_exists(lib/third_party/nlohmann_json)
set(NLOHMANN_JSON_LIBRARIES nlohmann_json PARENT_SCOPE)
set(NLOHMANN_JSON_LIBRARIES nlohmann_json)

add_subdirectory_if_exists(lib/third_party/boost)
set(BOOST_LIBRARIES boost::regex PARENT_SCOPE)
set(BOOST_LIBRARIES boost::regex)

add_subdirectory(lib/external/libwolv EXCLUDE_FROM_ALL)

set(LIBPL_ENABLE_CLI OFF CACHE BOOL "" FORCE)
add_subdirectory(lib/external/pattern_language EXCLUDE_FROM_ALL)

find_package(CURL REQUIRED)
find_package(mbedTLS 3.4.0 REQUIRED)
set(CURL_LIBRARIES ${CURL_LIBRARIES} PARENT_SCOPE)
set(MBEDTLS_LIBRARIES ${MBEDTLS_LIBRARIES} PARENT_SCOPE)

add_subdirectory(lib/libimhex)

if (WIN32)
    set_target_properties(libimhex PROPERTIES
            IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../libimhex.dll"
            IMPORTED_IMPLIB "${CMAKE_CURRENT_SOURCE_DIR}/lib/liblibimhex.dll.a"
            INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/lib/libimhex/include")
elseif (APPLE)
    file(GLOB LIBIMHEX_DYLIB "${CMAKE_CURRENT_SOURCE_DIR}/../../Frameworks/libimhex.*.dylib")
    set_target_properties(libimhex PROPERTIES
            IMPORTED_LOCATION "${LIBIMHEX_DYLIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/lib/libimhex/include")
else()
    set_target_properties(libimhex PROPERTIES
            IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../libimhex.so"
            INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/lib/libimhex/include")
endif()
