#pragma once

#include <glad/glad.h>
#include <imgui.h>

#include <hex.hpp>
#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/views/view.hpp>
#include <hex/providers/provider.hpp>
#include <hex/helpers/shared_data.hpp>
#include <hex/data_processor/node.hpp>

#define IMHEX_PLUGIN_SETUP(name, author, description) IMHEX_PLUGIN_SETUP_IMPL(IMHEX_PLUGIN_NAME, name, author, description)

#define IMHEX_PLUGIN_SETUP_IMPL(namespaceName, name, author, description)                           \
    namespace hex::plugin::namespaceName::internal {                                                \
        [[gnu::visibility("default")]] void initializePlugin();                                     \
                                                                                                    \
        [[gnu::visibility("default")]] const char* getPluginName() { return name; }                 \
        [[gnu::visibility("default")]] const char* getPluginAuthor() { return author; }             \
        [[gnu::visibility("default")]] const char* getPluginDescription() { return description; }   \
    }                                                                                               \
    void hex::plugin::namespaceName::internal::initializePlugin()
