# ImHex Plugin Template
# =====================

# This is the official CMake template for making your own ImHex plugins
# To use this template, copy the file into its own directory and modify it to your needs
# For the most part, this is a regular CMake project with some extra functions provided by the ImHex SDK
#
# [NOTE FOR NON-C++ PLUGINS]
# The template is laid out for a C++ plugin, however you can write your plugin in any language you want
# and just make the plugin statically link against your code. The only thing that's required is a .cpp file with
# the IMHEX_PLUGIN_SETUP() macro used in it. This macro is used to setup the plugin and register it with ImHex
#
# [CMAKE FUNCTIONS]
# add_imhex_plugin(): Registers a new plugin
#   NAME:           The name of the plugin
#   IMHEX_VERSION:  The ImHex version this plugin is compatible with. If unset, the plugin will be loaded on all versions (this may not work though)
#   SOURCES:        Source files of the plugin
#   INCLUDES:       Include directories of the plugin
#   LIBRARIES:      Libraries to link against
#   FEATURES:       Optional features that can be enabled or disabled
#   LIBRARY_PLUGIN: If set, turns this plugin into a library plugin. Library plugins can be linked against by other plugins
#
# add_romfs_resource(filePath romfsPath): Adds a file to the romfs of the plugin
# The RomFS is a virtual filesystem whose files can be accessed by the plugin using the functions in the `romfs::` namespace
# This function is used to add a single file to the romfs. You can however also simply create a `romfs` directory in your plugin directory and place your files and folders in there
#   filePath:   The path to the file on the disk
#   romfsPath:  The path to the file in the romfs
#
# enable_plugin_feature(feature): Enables a plugin feature
# Features are optional parts of the plugin that may or may not be available depending on build settings
# When a feature is enabled, `IMHEX_FEATURE_ENABLED(feature)` will be defined to true. Otherwise, it will be defined to false
# Use the `IMHEX_PLUGIN_FEATURES` macro in the main plugin file to define names to each feature and have them be listed in the plugin list
#   feature:    The name of the feature to enable

cmake_minimum_required(VERSION 3.20)
project(ImHexPlugin)

# Include the ImHex SDK
# For this to work, you need to set the IMHEX_SDK_PATH environment variable to the path of the ImHex SDK
#
# On Windows, the SDK is next to the ImHex executable
# On Linux, the SDK is usually in /usr/share/imhex/sdk but this may vary depending on your distribution
# On MacOS, the SDK is located inside of the ImHex.app bundle under ImHex.app/Contents/Resources/sdk
if (NOT EXISTS $ENV{IMHEX_SDK_PATH})
    message(FATAL_ERROR "The IMHEX_SDK_PATH environment variable is not set")
endif()
add_subdirectory($ENV{IMHEX_SDK_PATH} ImHexSDK)

# Register the plugin
# This will configure everything you need to make your plugin work
# Modify the arguments to your needs. Right now it defines a plugin called `example_plugin`
# with a single source file called `example_plugin.cpp` in the `source` directory
# By default you have access to the libimhex library to interact with ImHex
# as well as libwolv, libromfs, libfmt and ImGui, but you can link against any libraries you want
add_imhex_plugin(
    NAME
        example_plugin
    SOURCES
        source/example_plugin.cpp
)
