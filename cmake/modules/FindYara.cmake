find_library(YARA_LIBRARIES NAMES yara)
find_file(yara.h YARA_INCLUDE_DIRS)

mark_as_advanced(YARA_LIBRARIES YARA_INCLUDE_DIRS)