cmake_minimum_required(VERSION 3.16)

include(ImHexPlugin)

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
        ZLIB
        BZIP2
        LIBLZMA
        ZSTD
)

find_package(zstd)
if (TARGET zstd::libzstd_static)
    addOptionalLibrary(zstd libzstd_static)
elseif(TARGET zstd::libzstd_shared)
    addOptionalLibrary(zstd libzstd_shared)
endif()

addOptionalLibrary(ZLIB ZLIB)
addOptionalLibrary(BZip2 BZip2)
addOptionalLibrary(LibLZMA LibLZMA)
target_link_libraries(decompress PRIVATE ${LIBRARIES})
