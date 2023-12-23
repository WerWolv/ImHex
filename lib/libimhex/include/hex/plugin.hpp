#pragma once

#include <hex.hpp>
#include <hex/api/plugin_manager.hpp>

#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <wolv/utils/string.hpp>
#include <wolv/utils/preproc.hpp>

#if defined (IMHEX_STATIC_LINK_PLUGINS)
    #define IMHEX_PLUGIN_VISIBILITY_PREFIX static
#else
    #define IMHEX_PLUGIN_VISIBILITY_PREFIX extern "C" [[gnu::visibility("default")]]
#endif

/**
 * This macro is used to define all the required entry points for a plugin.
 * Name, Author and Description will be displayed in the in the plugin list on the Welcome screen.
 */
#define IMHEX_PLUGIN_SETUP(name, author, description) IMHEX_PLUGIN_SETUP_IMPL(name, author, description)
#define IMHEX_LIBRARY_SETUP() IMHEX_LIBRARY_SETUP_IMPL()

#define IMHEX_LIBRARY_SETUP_IMPL()                                                                              \
    IMHEX_PLUGIN_VISIBILITY_PREFIX void initializeLibrary();                                                    \
    extern "C" [[gnu::visibility("default")]] void WOLV_TOKEN_CONCAT(forceLinkPlugin_, IMHEX_PLUGIN_NAME)() {   \
        hex::PluginManager::addPlugin(hex::PluginFunctions {                                                    \
            nullptr,                                                                                            \
            initializeLibrary,                                                                                  \
            nullptr,                                                                                            \
            nullptr,                                                                                            \
            nullptr,                                                                                            \
            nullptr,                                                                                            \
            nullptr,                                                                                            \
            nullptr,                                                                                            \
            nullptr                                                                                             \
        });                                                                                                     \
    }                                                                                                           \
    IMHEX_PLUGIN_VISIBILITY_PREFIX void initializeLibrary()

#define IMHEX_PLUGIN_SETUP_IMPL(name, author, description)                                                      \
    IMHEX_PLUGIN_VISIBILITY_PREFIX const char *getPluginName() { return name; }                                 \
    IMHEX_PLUGIN_VISIBILITY_PREFIX const char *getPluginAuthor() { return author; }                             \
    IMHEX_PLUGIN_VISIBILITY_PREFIX const char *getPluginDescription() { return description; }                   \
    IMHEX_PLUGIN_VISIBILITY_PREFIX const char *getCompatibleVersion() { return IMHEX_VERSION; }                 \
    IMHEX_PLUGIN_VISIBILITY_PREFIX void setImGuiContext(ImGuiContext *ctx) {                                    \
        ImGui::SetCurrentContext(ctx);                                                                          \
        GImGui = ctx;                                                                                           \
    }                                                                                                           \
    IMHEX_PLUGIN_VISIBILITY_PREFIX void initializePlugin();                                                     \
    extern "C" [[gnu::visibility("default")]] void WOLV_TOKEN_CONCAT(forceLinkPlugin_, IMHEX_PLUGIN_NAME)() {   \
        hex::PluginManager::addPlugin(hex::PluginFunctions {                                                    \
            initializePlugin,                                                                                   \
            nullptr,                                                                                            \
            getPluginName,                                                                                      \
            getPluginAuthor,                                                                                    \
            getPluginDescription,                                                                               \
            getCompatibleVersion,                                                                               \
            setImGuiContext,                                                                                    \
            nullptr,                                                                                            \
            nullptr                                                                                             \
        });                                                                                                     \
    }                                                                                                           \
    IMHEX_PLUGIN_VISIBILITY_PREFIX void initializePlugin()

/**
 * This macro is used to define subcommands defined by the plugin
 * A subcommand consists of a key, a description, and a callback
 * The key is what the first argument to ImHex should be, prefixed by `--`
 * For example, if the key if `help`, ImHex should be started with `--help` as its first argument to trigger the subcommand
 * when the subcommand is triggerred, it's callback will be executed. The callback is executed BEFORE most of ImHex initialization
 * so to do anything meaningful, you should subscribe to an event (like EventImHexStartupFinished) and run your code there.
 */
#define IMHEX_PLUGIN_SUBCOMMANDS() IMHEX_PLUGIN_SUBCOMMANDS_IMPL()

#define IMHEX_PLUGIN_SUBCOMMANDS_IMPL()                                                 \
    extern std::vector<hex::SubCommand> g_subCommands;                                  \
    extern "C" [[gnu::visibility("default")]] void* getSubCommands() {                  \
        return &g_subCommands;                                                          \
    }                                                                                   \
    std::vector<hex::SubCommand> g_subCommands
