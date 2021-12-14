cmake_minimum_required(VERSION 3.16)

# Updating the version here will update it throughout ImHex as well
set(IMHEX_VERSION "1.12.0")
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
        # example_cpp
        # example_rust
        )

detectOS()
detectArch()

# Add bundled dependencies
add_subdirectory(plugins/libimhex)

# Add include directories
include_directories(include)

enable_testing()
add_subdirectory(tests EXCLUDE_FROM_ALL)

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

        source/helpers/plugin_manager.cpp

        ${imhex_icon}
        )

set_target_properties(imhex PROPERTIES CXX_VISIBILITY_PRESET hidden)

if (WIN32)
    target_link_libraries(imhex PUBLIC dl libimhex wsock32 ws2_32 Dwmapi.lib)
else ()
    target_link_libraries(imhex PUBLIC dl libimhex pthread)
endif ()

createPackage()
