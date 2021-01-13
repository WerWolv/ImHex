#pragma once

#include <glad/glad.h>
#include <imgui.h>

#include <hex.hpp>
#include <views/view.hpp>
#include <providers/provider.hpp>
#include <helpers/shared_data.hpp>
#include <helpers/content_registry.hpp>

#define IMHEX_PLUGIN_SETUP      namespace hex::plugin::internal {               \
                                    void initializePlugin();                    \
                                }                                               \
                                void hex::plugin::internal::initializePlugin()
