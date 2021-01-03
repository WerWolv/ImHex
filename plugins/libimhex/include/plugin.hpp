#pragma once

#include <glad/glad.h>
#include <imgui.h>

#include <hex.hpp>
#include <views/view.hpp>
#include <providers/provider.hpp>
#include <helpers/shared_data.hpp>

#define IMHEX_PLUGIN    namespace hex::plugin::internal {                     \
                            void initializePlugin(SharedData &sharedData) {   \
                                if (glGetString == NULL)                      \
                                    gladLoadGL();                             \
                                SharedData::get().initializeData(sharedData); \
                            }                                                 \
                        }                                                     \
                        namespace hex::plugin
