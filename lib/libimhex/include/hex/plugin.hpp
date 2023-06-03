#pragma once

#include <imgui.h>
#include <imgui_internal.h>

#include <hex.hpp>

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
