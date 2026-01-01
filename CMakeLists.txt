cmake_minimum_required(VERSION 3.25)

# Options
## General
option(IMHEX_STRIP_RELEASE              "Strip the release builds"                                                              ON )
option(IMHEX_OFFLINE_BUILD              "Enable offline build"                                                                  OFF)
option(IMHEX_IGNORE_BAD_CLONE           "Disable the bad clone prevention checks"                                               OFF)
option(IMHEX_PATTERNS_PULL_MASTER       "Download latest files from master branch of the ImHex-Patterns repo"                   OFF)
option(IMHEX_IGNORE_BAD_COMPILER        "Allow compiling with an unsupported compiler"                                          OFF)
option(IMHEX_USE_GTK_FILE_PICKER        "Use GTK file picker instead of xdg-desktop-portals (Linux only)"                       OFF)
option(IMHEX_BUNDLE_DOTNET              "Bundle .NET runtime"                                                                   ON )
option(IMHEX_ENABLE_LTO                 "Enables Link Time Optimizations if possible"                                           OFF)
option(IMHEX_USE_DEFAULT_BUILD_SETTINGS "Use default build settings"                                                            OFF)
option(IMHEX_BUILD_HARDENING            "Enable hardening flags for build"                                                      ON )
option(IMHEX_GENERATE_PACKAGE           "Specify if a cpack package should be built. (Windows only)"                            OFF)
option(IMHEX_MACOS_CREATE_BUNDLE        "Creates a macOS .app bundle when building (macOS only)"                                ON )
option(IMHEX_ENABLE_UNITY_BUILD         "Enables building ImHex as a unity build."                                              OFF)
option(IMHEX_ENABLE_PRECOMPILED_HEADERS "Enable precompiled headers"                                                            OFF)
option(IMHEX_ENABLE_CXX_MODULES         "Enable C++20 Module compilation. Testing only!"                                        OFF)
option(IMHEX_ENABLE_CPPCHECK            "Enable cppcheck static analysis"                                                       OFF)
option(IMHEX_BUNDLE_PLUGIN_SDK          "Enable bundling of Plugin SDK into install package"                                    ON )
option(IMHEX_ENABLE_UPDATER             "Enable automatic updater"                                                              ON )
## Testing
option(IMHEX_ENABLE_UNIT_TESTS          "Enable building unit tests"                                                            ON )
option(IMHEX_ENABLE_IMGUI_TEST_ENGINE   "Enable the ImGui Test Engine"                                                          OFF)
option(IMHEX_ENABLE_STD_ASSERTS         "Enable debug asserts in the C++ std library. (Breaks Plugin ABI!)"                     OFF)
## Debug info
option(IMHEX_COMPRESS_DEBUG_INFO        "Compress debug information"                                                            ON )
option(IMHEX_GENERATE_PDBS              "Enable generating PDB files in non-debug builds (Windows only)"                        OFF)
option(IMHEX_REPLACE_DWARF_WITH_PDB     "Remove DWARF information from binaries when generating PDBS (Windows only)"            OFF)
option(IMHEX_STRICT_WARNINGS            "Enable most available warnings and treat them as errors"                               ON )
option(IMHEX_DISABLE_STACKTRACE         "Disables support for printing stack traces"                                            OFF)
## Plugins
option(IMHEX_STATIC_LINK_PLUGINS        "Statically link all plugins into the main executable"                                  OFF)
option(IMHEX_ENABLE_PLUGIN_TESTS        "Enable building plugin tests"                                                          ON )
option(IMHEX_INCLUDE_PLUGINS            "Semicolon-separated list of plugins to include in the build (empty = build all)"       "" )
option(IMHEX_EXCLUDE_PLUGINS            "Semicolon-separated list of plugins to exclude from the build"                         "" )

set(IMHEX_BASE_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}")
set(CMAKE_MODULE_PATH "${IMHEX_BASE_FOLDER}/cmake/modules")

# Optional IDE support
include("${IMHEX_BASE_FOLDER}/cmake/ide_helpers.cmake")

# Basic compiler and cmake configurations
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_SCAN_FOR_MODULES ${IMHEX_ENABLE_CXX_MODULES})
set(CMAKE_INCLUDE_DIRECTORIES_BEFORE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include("${IMHEX_BASE_FOLDER}/cmake/build_helpers.cmake")

# Setup project
loadVersion(IMHEX_VERSION IMHEX_VERSION_PLAIN)
setVariableInParent(IMHEX_VERSION ${IMHEX_VERSION})

configureCMake()
project(ImHex
        LANGUAGES       C CXX
        VERSION         ${IMHEX_VERSION_PLAIN}
        DESCRIPTION     "The ImHex Hex Editor"
        HOMEPAGE_URL    "https://imhex.werwolv.net"
)
configureProject()

# Add ImHex sources
add_custom_target(imhex_all ALL)

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

add_subdirectory(lib/libimhex)
add_subdirectory(main)
addPluginDirectories()
add_subdirectory(lib/trace)

# Add unit tests
if (IMHEX_ENABLE_UNIT_TESTS)
    if (NOT TARGET unit_tests)
        enable_testing()
        add_custom_target(unit_tests)
        add_subdirectory(tests EXCLUDE_FROM_ALL)
    endif ()
endif ()

# Configure more resources that will be added to the install package
if (IMHEX_BUNDLE_PLUGIN_SDK)
    generateSDKDirectory()
endif()

# Handle package generation
createPackage()

# Accommodate IDEs with FOLDER support
tweakTargetsForIDESupport()
