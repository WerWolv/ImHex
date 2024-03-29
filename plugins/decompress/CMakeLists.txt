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
        ZLIB
        BZIP2
        LIBLZMA
        ZSTD
)

find_package(ZSTD)
if(ZSTD_FOUND)
    set(LIBRARIES ${LIBRARIES} "${ZSTD_LIBRARY}")
    message(STATUS "Enabling decompression support using ZSTD (${ZSTD_VERSION})")
    enable_plugin_feature(ZSTD)
endif()

addOptionalLibrary(ZLIB ZLIB)
addOptionalLibrary(BZip2 BZip2)
addOptionalLibrary(LibLZMA LibLZMA)
target_link_libraries(decompress PRIVATE ${LIBRARIES})
