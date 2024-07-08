cmake_minimum_required(VERSION 3.16)

include(ImHexPlugin)

macro(addOptionalLibrary package library)
    find_package(${package})
    if (${package}_FOUND)
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
        fonts
    FEATURES
        ZLIB "ZLib support"
        BZIP2 "Bzip2 support"
        LIBLZMA "LZMA support"
        ZSTD "zstd Support"
        LZ4 "lz4 Support"
)

set(LIBLZMA_HAS_AUTO_DECODER 1)
set(LIBLZMA_HAS_EASY_ENCODER 1)
set(LIBLZMA_HAS_LZMA_PRESET  1)

addOptionalLibrary(ZLIB ZLIB)
addOptionalLibrary(BZip2 BZip2)
addOptionalLibrary(LibLZMA LibLZMA)
addOptionalLibrary(ZSTD zstd)
addOptionalLibrary(LZ4 lz4)
target_link_libraries(decompress PRIVATE ${LIBRARIES})
