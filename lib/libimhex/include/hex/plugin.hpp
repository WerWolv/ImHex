#pragma once

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include <hex.hpp>

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
