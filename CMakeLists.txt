cmake_minimum_required(VERSION 3.20)

SET(NATIVE_CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
SET(NATIVE_CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")


# Options
option(IMHEX_PLUGINS_IN_SHARE "Put the plugins in share/imhex/plugins instead of lib[..]/imhex/plugins" OFF)
option(IMHEX_STRIP_RELEASE "Strip the release builds" ON)
option(IMHEX_OFFLINE_BUILD "Enable offline build" OFF)
option(IMHEX_IGNORE_BAD_CLONE "Disable the bad clone prevention checks" OFF)
option(IMHEX_PATTERNS_PULL_MASTER "Download latest files from master branch of the ImHex-Patterns repo" OFF)
option(IMHEX_IGNORE_BAD_COMPILER "Allow compiling with an unsupported compiler" OFF)
option(IMHEX_USE_GTK_FILE_PICKER "Use GTK file picker instead of xdg-desktop-portals" OFF)
option(IMHEX_DISABLE_STACKTRACE "Disables support for printing stack traces" OFF)
option(IMHEX_BUNDLE_DOTNET "Bundle .NET runtime" ON)
option(IMHEX_ENABLE_LTO "Enables Link Time Optimizations if possible" OFF)
option(IMHEX_USE_DEFAULT_BUILD_SETTINGS "Use default build settings" OFF)
option(IMHEX_STRICT_WARNINGS "Enable most available warnings and treat them as errors" ON)

# Basic compiler and cmake configurations
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_INCLUDE_DIRECTORIES_BEFORE ON)
set(IMHEX_BASE_FOLDER ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_MODULE_PATH "${IMHEX_BASE_FOLDER}/cmake/modules")
include("${IMHEX_BASE_FOLDER}/cmake/build_helpers.cmake")

# Setup project
loadVersion(IMHEX_VERSION)
setVariableInParent(IMHEX_VERSION ${IMHEX_VERSION})
configureCMake()
project(imhex
        LANGUAGES C CXX VERSION ${IMHEX_VERSION}
        DESCRIPTION "The ImHex Hex Editor"
        HOMEPAGE_URL "https://imhex.werwolv.net"
)

SET(ADDITIONAL_FLAGS "-Wno-c++11-narrowing -Wno-deprecated-literal-operator -pthread")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ADDITIONAL_FLAGS}")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ADDITIONAL_FLAGS}")

# Make sure project is configured correctly
setDefaultBuiltTypeIfUnset()
detectBadClone()
verifyCompiler()

# List plugin names here. Project name must match folder name
set(PLUGINS
    builtin
    windows
)

# Add various defines
detectOS()
detectArch()
addDefines()
configurePackingResources()
setUninstallTarget()
addBundledLibraries()

# Add ImHex sources
add_custom_target(imhex_all ALL)

add_subdirectory(lib/libimhex)
add_subdirectory(main)

# Add unit tests
enable_testing()
add_subdirectory(tests EXCLUDE_FROM_ALL)

# Configure packaging
createPackage()
generatePDBs()
