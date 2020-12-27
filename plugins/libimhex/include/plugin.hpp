#pragma once

#include <glad/glad.h>
#include <imgui.h>

#include <hex.hpp>
#include <views/view.hpp>
#include <providers/provider.hpp>

#define IMHEX_PLUGIN    namespace hex::plugin::internal {                                               \
                            void initializePlugin(ImGuiContext *ctx, hex::prv::Provider **provider) {   \
                                if (glGetString == NULL)                                                \
                                    gladLoadGL();                                                       \
                                ImGui::SetCurrentContext(ctx);                                          \
                                hex::prv::Provider::setProviderStorage(*provider);                      \
                            }                                                                           \
                        }                                                                               \
                        namespace hex::plugin
