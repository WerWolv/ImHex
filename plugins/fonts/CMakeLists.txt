cmake_minimum_required(VERSION 3.16)

include(ImHexPlugin)

add_imhex_plugin(
    NAME
        fonts
    SOURCES
        source/library_fonts.cpp
        source/fonts.cpp
        source/font_settings.cpp
        source/font_loader.cpp
    INCLUDES
        include
    	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/external/pattern_language/lib/include>
    	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/external/libwolv/libs/types/include>
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/external/libwolv/libs/io/include>
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/external/libwolv/libs/utils/lib/jthread/includes>
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/trace/include>
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/external/libwolv/libs/utils/include>
	$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/lib/third_party/nlohmann_json/include>
    LIBRARY_PLUGIN
)
