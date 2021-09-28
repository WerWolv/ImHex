cmake_minimum_required(VERSION 3.16)

# Updating the version here will update it throughout ImHex as well
set(IMHEX_VERSION "1.10.1")
project(imhex VERSION ${IMHEX_VERSION})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")
include("${CMAKE_SOURCE_DIR}/cmake/build_helpers.cmake")
setDefaultBuiltTypeIfUnset()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

# List plugin names here. Project name must match folder name
set(PLUGINS
        builtin
        windows
        example_rust
        # example
        )

findLibraries()

detectOS()
detectArch()

# Add bundled dependencies
add_subdirectory(plugins/libimhex)

# Add include directories
include_directories(include)

enable_testing()
add_subdirectory(tests)

addVersionDefines()
configurePackageCreation()

add_executable(imhex ${application_type}
        source/main.cpp

        source/window/window.cpp
        source/window/win_window.cpp
        source/window/macos_window.cpp
        source/window/linux_window.cpp

        source/init/splash_window.cpp
        source/init/tasks.cpp

        source/helpers/patches.cpp
        source/helpers/project_file_handler.cpp
        source/helpers/loader_script_handler.cpp
        source/helpers/plugin_manager.cpp
        source/helpers/encoding_file.cpp

        source/providers/file_provider.cpp

        source/views/view_hexeditor.cpp
        source/views/view_pattern_editor.cpp
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
        source/views/view_store.cpp
        source/views/view_diff.cpp

        ${imhex_icon}
        )

set_target_properties(imhex PROPERTIES CXX_VISIBILITY_PRESET hidden)

if (WIN32)
    target_link_libraries(imhex dl libimhex wsock32 ws2_32 Dwmapi.lib)
else ()
    target_link_libraries(imhex dl libimhex pthread)
endif ()

createPackage()
