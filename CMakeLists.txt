cmake_minimum_required(VERSION 3.16)

# Updating the version here will update it throughout ImHex as well
set(IMHEX_VERSION "1.8.1")
project(imhex VERSION ${IMHEX_VERSION})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")
include("${CMAKE_SOURCE_DIR}/cmake/build_helpers.cmake")
setDefaultBuiltTypeIfUnset()

# List plugin names here. Project name must match folder name
set(PLUGINS
        builtin
        windows
        # example
        )

# List extra magic databases to compile here
set(MAGICDBS
        magic_dbs/nintendo_magic
        )

findLibraries()

detectOS()
detectArch()

if (NOT USE_SYSTEM_LLVM)
    add_subdirectory(external/llvm)
else()
    find_package(LLVM REQUIRED Demangle)
endif()
if (NOT USE_SYSTEM_YARA)
    add_subdirectory(external/yara)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(YARA REQUIRED IMPORTED_TARGET yara)
endif()

# Add bundled dependencies
add_subdirectory(plugins/libimhex)

# Add include directories
include_directories(include ${MBEDTLS_INCLUDE_DIRS} ${CAPSTONE_INCLUDE_DIRS} ${MAGIC_INCLUDE_DIRS} ${Python_INCLUDE_DIRS})
if (USE_SYSTEM_LLVM)
    include_directories(include ${LLVM_INCLUDE_DIRS})
endif()
if (USE_SYSTEM_YARA)
    include_directories(include ${YARA_INCLUDE_DIRS})
endif()

addVersionDefines()
configurePackageCreation()

add_executable(imhex ${application_type}
        source/main.cpp
        source/window.cpp

        source/init/splash_window.cpp
        source/init/tasks.cpp

        source/helpers/patches.cpp
        source/helpers/project_file_handler.cpp
        source/helpers/loader_script_handler.cpp
        source/helpers/plugin_manager.cpp
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
        source/views/view_constants.cpp

        ${imhex_icon}
        )

set_target_properties(imhex PROPERTIES CXX_VISIBILITY_PRESET hidden)
target_link_directories(imhex PRIVATE ${CAPSTONE_LIBRARY_DIRS} ${MAGIC_LIBRARY_DIRS})

if (WIN32)
    target_link_libraries(imhex magic ${CMAKE_DL_LIBS} capstone LLVMDemangle libimhex ${Python_LIBRARIES} wsock32 ws2_32 libyara)
else ()
    target_link_libraries(imhex magic ${CMAKE_DL_LIBS} capstone LLVMDemangle libimhex ${Python_LIBRARIES} dl pthread libyara)
endif ()

createPackage()
