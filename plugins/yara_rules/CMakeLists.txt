cmake_minimum_required(VERSION 3.16)

include(ImHexPlugin)

if (NOT USE_SYSTEM_YARA)
    add_subdirectory(${THIRD_PARTY_LIBS_FOLDER}/yara ${CMAKE_CURRENT_BINARY_DIR}/yara EXCLUDE_FROM_ALL)
    set_target_properties(libyara PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set(YARA_LIBRARIES libyara)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(YARA REQUIRED IMPORTED_TARGET yara)
endif()

add_imhex_plugin(
    NAME
        yara_rules
    SOURCES
        source/plugin_yara.cpp

        source/content/views/view_yara.cpp
    INCLUDES
        include
        ${YARA_INCLUDE_DIRS}
    LIBRARIES
        ${JTHREAD_LIBRARIES}
        ${YARA_LIBRARIES}
)
