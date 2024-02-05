cmake_minimum_required(VERSION 3.20)

# Options
option(IMHEX_PLUGINS_IN_SHARE "Put the plugins in share/imhex/plugins instead of lib[..]/imhex/plugins (Linux only)" OFF)
option(IMHEX_STRIP_RELEASE "Strip the release builds" ON)
option(IMHEX_OFFLINE_BUILD "Enable offline build" OFF)
option(IMHEX_IGNORE_BAD_CLONE "Disable the bad clone prevention checks" OFF)
option(IMHEX_PATTERNS_PULL_MASTER "Download latest files from master branch of the ImHex-Patterns repo" OFF)
option(IMHEX_IGNORE_BAD_COMPILER "Allow compiling with an unsupported compiler" OFF)
option(IMHEX_USE_GTK_FILE_PICKER "Use GTK file picker instead of xdg-desktop-portals (Linux only)" OFF)
option(IMHEX_DISABLE_STACKTRACE "Disables support for printing stack traces" OFF)
option(IMHEX_BUNDLE_DOTNET "Bundle .NET runtime" ON)
option(IMHEX_ENABLE_LTO "Enables Link Time Optimizations if possible" OFF)
option(IMHEX_USE_DEFAULT_BUILD_SETTINGS "Use default build settings" OFF)
option(IMHEX_STRICT_WARNINGS "Enable most available warnings and treat them as errors" ON)
option(IMHEX_STATIC_LINK_PLUGINS "Statically link all plugins into the main executable" OFF)
option(IMHEX_GENERATE_PACKAGE "Specify if a native package should be built. (Windows and MacOS only)" OFF)
option(IMHEX_ENABLE_UNITY_BUILD "Enables building ImHex as a unity build." OFF)
option(IMHEX_GENERATE_PDBS "Enable generating PDB files in non-debug builds (Windows only)" OFF)
option(IMHEX_REPLACE_DWARF_WITH_PDB "Remove DWARF information from binaries when generating PDBS (Windows only)" OFF)
option(IMHEX_ENABLE_STD_ASSERTS "Enable debug asserts in the C++ std library. (Breaks Plugin ABI!)" OFF)
option(IMHEX_ENABLE_UNIT_TESTS "Enable building unit tests" OFF)

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

# Make sure project is configured correctly
setDefaultBuiltTypeIfUnset()
detectBadClone()
verifyCompiler()

detectBundledPlugins()

# Add various defines
detectOS()
addDefines()

# Configure packaging and install targets
configurePackingResources()
setUninstallTarget()
addBundledLibraries()

# Add ImHex sources
add_custom_target(imhex_all ALL)

add_subdirectory(lib/libimhex)
add_subdirectory(main)

# Add unit tests
if (IMHEX_ENABLE_UNIT_TESTS)
    enable_testing()
    add_subdirectory(tests EXCLUDE_FROM_ALL)
endif ()

# Configure more resources that will be added to the install package
createPackage()
generatePDBs()
generateSDKDirectory()