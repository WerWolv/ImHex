cmake_minimum_required(VERSION 3.16)

include(ImHexPlugin)

if (NOT USE_SYSTEM_CAPSTONE)
    set(CAPSTONE_BUILD_STATIC_RUNTIME OFF CACHE BOOL "Disable shared library building")
    set(CAPSTONE_BUILD_SHARED OFF CACHE BOOL "Disable shared library building")
    set(CAPSTONE_BUILD_TESTS OFF CACHE BOOL "Disable tests")
    set(CAPSTONE_BUILD_MACOS_THIN ON CACHE BOOL "Enable thin builds of capstone for macOS" FORCE)
    add_subdirectory(${THIRD_PARTY_LIBS_FOLDER}/capstone ${CMAKE_CURRENT_BINARY_DIR}/capstone EXCLUDE_FROM_ALL)
    target_compile_options(capstone PRIVATE -Wno-unused-function)
    set(CAPSTONE_LIBRARY "capstone")
    set(CAPSTONE_INCLUDE_DIR ${THIRD_PARTY_LIBS_FOLDER}/capstone/include)
else()
    find_package(Capstone REQUIRED)
endif()

add_imhex_plugin(
    NAME
        disassembler
    SOURCES
        source/plugin_disassembler.cpp

        source/content/views/view_disassembler.cpp

        source/content/pl_visualizers/disassembler.cpp
        source/content/pl_builtin_types.cpp

        source/content/disassemblers/capstone_architectures.cpp
        source/content/disassemblers/custom_architectures.cpp
    INCLUDES
        include
        ${CAPSTONE_INCLUDE_DIR}
    LIBRARIES
        ui
        fonts
        ${CAPSTONE_LIBRARY}
        libdisassembler
)
