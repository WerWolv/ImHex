# Find Nettle
#
# This module defines:
#  Nettle_FOUND
#  Nettle_VERSION
#  Nettle_INCLUDE_DIR
#  Nettle_LIBRARY
#  Nettle::Nettle

find_path(Nettle_INCLUDE_DIR NAMES nettle/version.h)
find_library(Nettle_LIBRARY NAMES nettle libnettle)

if (Nettle_INCLUDE_DIR)
    file(STRINGS "${Nettle_INCLUDE_DIR}/nettle/version.h" Nettle_VERSION_MAJOR_LINE
        REGEX "^#define[ \t]+NETTLE_VERSION_MAJOR[ \t]+[0-9]+")
    file(STRINGS "${Nettle_INCLUDE_DIR}/nettle/version.h" Nettle_VERSION_MINOR_LINE
        REGEX "^#define[ \t]+NETTLE_VERSION_MINOR[ \t]+[0-9]+")
    string(REGEX REPLACE ".*NETTLE_VERSION_MAJOR[ \t]+([0-9]+).*" "\\1" Nettle_VERSION_MAJOR "${Nettle_VERSION_MAJOR_LINE}")
    string(REGEX REPLACE ".*NETTLE_VERSION_MINOR[ \t]+([0-9]+).*" "\\1" Nettle_VERSION_MINOR "${Nettle_VERSION_MINOR_LINE}")
    if (Nettle_VERSION_MAJOR MATCHES "^[0-9]+$" AND Nettle_VERSION_MINOR MATCHES "^[0-9]+$")
        set(Nettle_VERSION "${Nettle_VERSION_MAJOR}.${Nettle_VERSION_MINOR}")
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Nettle
    REQUIRED_VARS Nettle_LIBRARY Nettle_INCLUDE_DIR
    VERSION_VAR Nettle_VERSION
)

if (Nettle_FOUND AND NOT TARGET Nettle::Nettle)
    add_library(Nettle::Nettle UNKNOWN IMPORTED)
    set_target_properties(Nettle::Nettle PROPERTIES
        IMPORTED_LOCATION "${Nettle_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Nettle_INCLUDE_DIR}"
    )
endif ()

mark_as_advanced(Nettle_INCLUDE_DIR Nettle_LIBRARY)
