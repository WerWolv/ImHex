cmake_minimum_required(VERSION 3.16)

include(ImHexPlugin)

# Homebrew only ships a libarchive keg, include directories have to be set manually
if(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
    execute_process(
            COMMAND brew --prefix libarchive
            OUTPUT_VARIABLE LIBARCHIVE_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
            COMMAND_ERROR_IS_FATAL ANY
    )
    set(LibArchive_INCLUDE_DIR "${LIBARCHIVE_PREFIX}/include")
endif()
find_package(LibArchive REQUIRED)
find_package(ZLIB REQUIRED)
find_package(BZip2 REQUIRED)
find_package(LibLZMA REQUIRED)
find_package(zstd REQUIRED)

set_property(TARGET LibArchive::LibArchive  PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET ZLIB::ZLIB              PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET BZip2::BZip2            PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET LibLZMA::LibLZMA        PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET zstd::libzstd_static    PROPERTY POSITION_INDEPENDENT_CODE ON)


add_imhex_plugin(
    NAME
        decompress
    SOURCES
        source/plugin_decompress.cpp

        source/content/pl_functions.cpp
    INCLUDES
        include
    LIBRARIES
        ui
        LibArchive::LibArchive
        ZLIB::ZLIB
        BZip2::BZip2
        LibLZMA::LibLZMA
        zstd::libzstd_static
)
