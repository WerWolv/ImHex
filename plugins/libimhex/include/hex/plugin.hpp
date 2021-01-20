#pragma once

#include <glad/glad.h>
#include <imgui.h>

#include <hex.hpp>
#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/views/view.hpp>
#include <hex/providers/provider.hpp>
#include <hex/helpers/shared_data.hpp>

#define IMHEX_PLUGIN_SETUP      namespace hex::plugin::internal {               \
                                    void initializePlugin();                    \
                                }                                               \
                                void hex::plugin::internal::initializePlugin()
