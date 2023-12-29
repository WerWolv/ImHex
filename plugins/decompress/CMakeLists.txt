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

macro(addOptionalLibrary package library)
    find_package(${package})
    if (${package}_FOUND)
        set_property(TARGET ${package}::${library} PROPERTY POSITION_INDEPENDENT_CODE ON)
        string(TOUPPER ${package} PACKAGE)
        set(LIBRARIES ${LIBRARIES} ${package}::${library})
        message(STATUS "Enabling decompression support using ${package} (${${package}_VERSION})")
        enable_plugin_feature(${PACKAGE})
    endif()

endmacro()

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
    FEATURES
        LIBARCHIVE
        ZLIB
        BZIP2
        LIBLZMA
        ZSTD
)

addOptionalLibrary(LibArchive LibArchive)
addOptionalLibrary(ZLIB ZLIB)
addOptionalLibrary(BZip2 BZip2)
addOptionalLibrary(LibLZMA LibLZMA)
addOptionalLibrary(zstd libzstd_static)
target_link_libraries(decompress PRIVATE ${LIBRARIES})
