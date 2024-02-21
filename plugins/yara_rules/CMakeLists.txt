cmake_minimum_required(VERSION 3.16)

include(ImHexPlugin)

if (NOT USE_SYSTEM_YARA)
    add_subdirectory(${THIRD_PARTY_LIBS_FOLDER}/yara ${CMAKE_CURRENT_BINARY_DIR}/yara EXCLUDE_FROM_ALL)
    set(YARA_LIBRARIES libyara)
else()
    find_package(Yara REQUIRED)
endif()

add_imhex_plugin(
    NAME
        yara_rules
    SOURCES
        source/plugin_yara.cpp

        source/content/yara_rule.cpp
        source/content/data_information_sections.cpp
        source/content/views/view_yara.cpp
    INCLUDES
        include
        ${YARA_INCLUDE_DIRS}
    LIBRARIES
        ui
        fonts
        ${JTHREAD_LIBRARIES}
        ${YARA_LIBRARIES}
)
