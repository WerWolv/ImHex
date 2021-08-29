#pragma once

#include <imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include <hex.hpp>

#define IMHEX_PLUGIN_SETUP(name, author, description) IMHEX_PLUGIN_SETUP_IMPL(IMHEX_PLUGIN_NAME, name, author, description)

#define IMHEX_PLUGIN_SETUP_IMPL(namespaceName, name, author, description)                           \
    namespace hex::plugin::namespaceName::internal {                                                \
        [[gnu::visibility("default")]] void initializePlugin();                                     \
                                                                                                    \
        [[gnu::visibility("default")]] const char* getPluginName() { return name; }                 \
        [[gnu::visibility("default")]] const char* getPluginAuthor() { return author; }             \
        [[gnu::visibility("default")]] const char* getPluginDescription() { return description; }   \
        [[gnu::visibility("default")]] void setImGuiContext(ImGuiContext *ctx) { ImGui::SetCurrentContext(ctx); GImGui = ctx; }    \
    }                                                                                               \
    void hex::plugin::namespaceName::internal::initializePlugin()
