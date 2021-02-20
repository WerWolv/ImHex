cmake_minimum_required(VERSION 3.16)

# Updating the version here will update it throughout ImHex as well
project(imhex VERSION 1.7.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")
include("${CMAKE_SOURCE_DIR}/cmake/build_helpers.cmake")
setDefaultBuiltTypeIfUnset()

# List plugin names here. Project name must match folder name
set(PLUGINS
        builtin
        # example
        )

# List extra magic databases to compile here
set(MAGICDBS
        magic_dbs/nintendo_magic
        )

findLibraries()

detectOS()
detectArch()

# Add bundled dependencies
add_subdirectory(external/llvm)
add_subdirectory(external/yara)
add_subdirectory(plugins/libimhex)

# Add include directories
include_directories(include ${MBEDTLS_INCLUDE_DIRS} ${CAPSTONE_INCLUDE_DIRS} ${MAGIC_INCLUDE_DIRS} ${Python_INCLUDE_DIRS})

addVersionDefines()
configurePackageCreation()

add_executable(imhex ${application_type}
        source/main.cpp
        source/window.cpp

        source/helpers/patches.cpp
        source/helpers/project_file_handler.cpp
        source/helpers/loader_script_handler.cpp
        source/helpers/plugin_handler.cpp
        source/helpers/encoding_file.cpp

        source/providers/file_provider.cpp

        source/views/view_hexeditor.cpp
        source/views/view_pattern.cpp
        source/views/view_pattern_data.cpp
        source/views/view_hashes.cpp
        source/views/view_information.cpp
        source/views/view_help.cpp
        source/views/view_tools.cpp
        source/views/view_strings.cpp
        source/views/view_data_inspector.cpp
        source/views/view_disassembler.cpp
        source/views/view_bookmarks.cpp
        source/views/view_patches.cpp
        source/views/view_command_palette.cpp
        source/views/view_settings.cpp
        source/views/view_data_processor.cpp
        source/views/view_yara.cpp

        ${imhex_icon}
        )

set_target_properties(imhex PROPERTIES CXX_VISIBILITY_PRESET hidden)
target_link_directories(imhex PRIVATE ${CAPSTONE_LIBRARY_DIRS} ${MAGIC_LIBRARY_DIRS})

if (WIN32)
    target_link_libraries(imhex libdl.a libmagic.a libgnurx.a libtre.a libintl.a libiconv.a libshlwapi.a libcapstone.a LLVMDemangle libimhex ${Python_LIBRARIES} wsock32 ws2_32 libyara)
elseif (UNIX)
    target_link_libraries(imhex magic ${CMAKE_DL_LIBS} capstone LLVMDemangle libimhex ${Python_LIBRARIES} dl pthread libyara)
endif()

createPackage()
