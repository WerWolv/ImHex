#pragma once

#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <hex.hpp>
#include <hex/api/plugin_manager.hpp>

#include <wolv/utils/string.hpp>

/**
 * This macro is used to define all the required entry points for a plugin.
 * Name, Author and Description will be displayed in the in the plugin list on the Welcome screen.
 */
#define IMHEX_PLUGIN_SETUP(name, author, description) IMHEX_PLUGIN_SETUP_IMPL(name, author, description)

#define IMHEX_PLUGIN_SETUP_IMPL(name, author, description)                                                 \
    extern "C" [[gnu::visibility("default")]] const char *getPluginName() { return name; }                 \
    extern "C" [[gnu::visibility("default")]] const char *getPluginAuthor() { return author; }             \
    extern "C" [[gnu::visibility("default")]] const char *getPluginDescription() { return description; }   \
    extern "C" [[gnu::visibility("default")]] const char *getCompatibleVersion() { return IMHEX_VERSION; } \
    extern "C" [[gnu::visibility("default")]] void setImGuiContext(ImGuiContext *ctx) {                    \
        ImGui::SetCurrentContext(ctx);                                                                     \
        GImGui = ctx;                                                                                      \
    }                                                                                                      \
    extern "C" [[gnu::visibility("default")]] void initializePlugin()

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
    extern "C" [[gnu::visibility("default")]] hex::SubCommandList getSubCommands() {    \
        return hex::SubCommandList {                                                    \
            g_subCommands.data(),                                                       \
            g_subCommands.size()                                                        \
        };                                                                              \
    }                                                                                   \
    std::vector<hex::SubCommand> g_subCommands
