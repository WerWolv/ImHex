#.rst:
# Findelfutils
# -----------------
#
# Try to locate the elfutils development installation
#
# On Debian-like systems, you can install libdw-dev.
#
# Otherwise, the ``ELFUTILS_INSTALL_DIR`` (CMake or Environment) variable
# can be used to pinpoint the elfutils binary installation
#
# If found, this will define the following variables:
#
# ``elfutils_FOUND``
#     True if the elfutils header and library files have been found
#
# ``elfutils::dw``
# ``elfutils::elf``
#     Imported targets containing the includes and libraries need to build
#     against
#
# ``elfutils::debuginfod``
# ``HAVE_DWFL_GET_DEBUGINFOD_CLIENT``
#     Optional, only defined when libdebuginfod and its header are installed
#     (libdebuginfod-dev on Debian-like systems) and elfutils is new enough to
#     export dwfl_get_debuginfod_client(). Note that elfutils downloads missing
#     debug information from a debuginfod server whether or not this is found;
#     what it buys is access to the client, and with it the ability to report
#     the progress of those downloads (see perfparser's PerfSymbolTable).
#

if (TARGET elfutils::dw AND TARGET elfutils::elf)
    set(elfutils_FOUND TRUE)
    return()
endif()

find_path(ELFUTILS_INCLUDE_DIR
        NAMES libdwfl.h elfutils/libdwfl.h
        PATH_SUFFIXES include
        HINTS
        "${ELFUTILS_INSTALL_DIR}" ENV ELFUTILS_INSTALL_DIR "${CMAKE_PREFIX_PATH}"
)

foreach(lib dw elf eu_compat debuginfod)
    find_library(ELFUTILS_LIB_${lib}
            NAMES ${lib}
            PATH_SUFFIXES lib
            HINTS
            "${ELFUTILS_INSTALL_DIR}" ENV ELFUTILS_INSTALL_DIR "${CMAKE_PREFIX_PATH}"
    )
endforeach()

find_path(ELFUTILS_DEBUGINFOD_INCLUDE_DIR
        NAMES debuginfod.h
        PATH_SUFFIXES include include/elfutils
        HINTS
        "${ELFUTILS_INSTALL_DIR}" ENV ELFUTILS_INSTALL_DIR "${CMAKE_PREFIX_PATH}"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(elfutils
        DEFAULT_MSG
        ELFUTILS_INCLUDE_DIR ELFUTILS_LIB_dw ELFUTILS_LIB_elf
)

if(elfutils_FOUND)
    foreach(lib dw elf)
        if (NOT TARGET elfutils::${lib})
            add_library(elfutils::${lib} UNKNOWN IMPORTED)
            set_target_properties(elfutils::${lib}
                    PROPERTIES
                    IMPORTED_LOCATION "${ELFUTILS_LIB_${lib}}"
                    INTERFACE_INCLUDE_DIRECTORIES
                    "${ELFUTILS_INCLUDE_DIR};${ELFUTILS_INCLUDE_DIR}/elfutils"
            )
            if (ELFUTILS_LIB_eu_compat)
                target_link_libraries(elfutils::${lib} INTERFACE ${ELFUTILS_LIB_eu_compat})
            endif()
        endif()
    endforeach()

    # elfutils exports dwfl_get_debuginfod_client() only since 0.179, and setting
    # the progress hook on what it returns needs libdebuginfod's own header, so
    # both have to be there. Cached so that the result survives the early return
    # above, which a second find_package(elfutils) takes once the targets exist.
    set(_elfutils_have_debuginfod FALSE)
    if (ELFUTILS_LIB_debuginfod AND ELFUTILS_DEBUGINFOD_INCLUDE_DIR)
        include(CheckLibraryExists)
        if (ELFUTILS_LIB_eu_compat)
            # libdw itself may need eu_compat, see the imported targets above
            set(CMAKE_REQUIRED_LIBRARIES ${ELFUTILS_LIB_eu_compat})
        endif()
        check_library_exists("${ELFUTILS_LIB_dw}" dwfl_get_debuginfod_client ""
                ELFUTILS_DW_HAS_GET_DEBUGINFOD_CLIENT)
        unset(CMAKE_REQUIRED_LIBRARIES)
        if (ELFUTILS_DW_HAS_GET_DEBUGINFOD_CLIENT)
            set(_elfutils_have_debuginfod TRUE)
        endif()
    endif()
    set(HAVE_DWFL_GET_DEBUGINFOD_CLIENT ${_elfutils_have_debuginfod} CACHE INTERNAL
            "elfutils can report the progress of its debuginfod downloads")

    if (_elfutils_have_debuginfod)
        message(STATUS "Found elfutils debuginfod: ${ELFUTILS_LIB_debuginfod}")
    else()
        message(STATUS "elfutils debuginfod progress reporting is disabled "
                "(install libdebuginfod-dev on a Debian-like system)")
    endif()
    unset(_elfutils_have_debuginfod)

    if (HAVE_DWFL_GET_DEBUGINFOD_CLIENT AND NOT TARGET elfutils::debuginfod)
        add_library(elfutils::debuginfod UNKNOWN IMPORTED)
        set_target_properties(elfutils::debuginfod
                PROPERTIES
                IMPORTED_LOCATION "${ELFUTILS_LIB_debuginfod}"
                INTERFACE_INCLUDE_DIRECTORIES "${ELFUTILS_DEBUGINFOD_INCLUDE_DIR}"
        )
    endif()
else()
    message(STATUS "                        (set ELFUTILS_INSTALL_DIR, or install libdw-dev on a Debian-like system)")
endif()

mark_as_advanced(ELFUTILS_INCLUDE_DIR ELFUTILS_LIB_elf ELFUTILS_LIB_dw
        ELFUTILS_LIB_debuginfod ELFUTILS_DEBUGINFOD_INCLUDE_DIR)

include(FeatureSummary)
set_package_properties(elfutils PROPERTIES
        URL "https://sourceware.org/elfutils/"
        DESCRIPTION "a collection of utilities and libraries to read, create and modify ELF binary files")