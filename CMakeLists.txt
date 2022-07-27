cmake_minimum_required(VERSION 3.16)

# Options
option(IMHEX_OFFLINE_BUILD "Enable offline build" OFF)
option(IMHEX_PATTERNS_PULL_MASTER "Download latest files from master branch of the ImHex-Patterns repo" OFF)

# Setup project
loadVersion(IMHEX_VERSION)
set(IMHEX_VERSION ${IMHEX_VERSION} PARENT_SCOPE)
project(imhex VERSION ${IMHEX_VERSION})

# Basic compiler and cmake configurations
set(CMAKE_CXX_STANDARD 23)
set(IMHEX_BASE_FOLDER ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_MODULE_PATH "${IMHEX_BASE_FOLDER}/cmake/modules")
include("${IMHEX_BASE_FOLDER}/cmake/build_helpers.cmake")

# Make sure project is configured correctly
setDefaultBuiltTypeIfUnset()
detectBadClone()

# List plugin names here. Project name must match folder name
set(PLUGINS
    builtin
    windows
)

# Add various defines
detectOS()
detectArch()
addVersionDefines()
configurePackingResources()

# Add ImHex sources
add_subdirectory(lib/libimhex)
add_subdirectory(main)
add_custom_target(imhex_all ALL DEPENDS main)

# Add unit tests
enable_testing()
add_subdirectory(tests EXCLUDE_FROM_ALL)

# Configure packaging
createPackage()
