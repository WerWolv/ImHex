cmake_minimum_required(VERSION 3.16)

include(ImHexPlugin)

# Homebrew only ships a libarchive keg, include directories have to be set manually
find_package(LibArchive REQUIRED)
if (NOT DEFINED LibArchive_INCLUDE_DIR OR LibArchive_INCLUDE_DIR STREQUAL "")
    if (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
        execute_process(
                COMMAND (${BREW_EXECUTABLE} --prefix libarchive) || exit 0
                OUTPUT_VARIABLE LIBARCHIVE_PREFIX
                OUTPUT_STRIP_TRAILING_WHITESPACE
                COMMAND_ERROR_IS_FATAL ANY
        )

        if (NOT LIBARCHIVE_PREFIX STREQUAL "")
            set(LibArchive_INCLUDE_DIR "${LIBARCHIVE_PREFIX}/include")
        endif()
    endif()
endif()

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
)
