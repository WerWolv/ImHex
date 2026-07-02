find_path(LIBMAGIC_INCLUDE_DIR magic.h)

find_library(LIBMAGIC_LIBRARY NAMES magic)

find_package_handle_standard_args(Magic DEFAULT_MSG
        LIBMAGIC_LIBRARY
        LIBMAGIC_INCLUDE_DIR
)

mark_as_advanced(
        LIBMAGIC_INCLUDE_DIR
        LIBMAGIC_LIBRARY
        Magic_FOUND
)
