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

#define IMHEX_PLUGIN_SETUP IMHEX_PLUGIN_SETUP_IMPL(IMHEX_PLUGIN_NAME)

#define IMHEX_PLUGIN_SETUP_IMPL(name)   namespace hex::plugin::name::internal {                     \
                                            [[gnu::visibility("default")]] void initializePlugin(); \
                                        }                                                           \
                                        void hex::plugin::name::internal::initializePlugin()
