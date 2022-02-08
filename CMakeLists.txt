cmake_minimum_required(VERSION 3.16)

# Updating the version here will update it throughout ImHex as well
set(IMHEX_VERSION "1.14.0")
project(imhex VERSION ${IMHEX_VERSION})

set(CMAKE_CXX_STANDARD 20)
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
add_custom_target(imhex ALL DEPENDS main)

# Add unit tests
add_subdirectory(tests EXCLUDE_FROM_ALL)

# Configure packaging
createPackage()
