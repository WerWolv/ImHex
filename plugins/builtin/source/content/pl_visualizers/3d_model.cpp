#include <hex/helpers/utils.hpp>

#include <content/pl_visualizers/visualizer_helpers.hpp>

#include <fonts/codicons_font.h>
#include <fonts/fontawesome_font.h>
#include <fonts/blendericons_font.h>

#include <imgui.h>
#include <hex/helpers/opengl.hpp>
#include <opengl_support.h>
#include <GLFW/glfw3.h>

#include <numbers>

#include <hex/ui/imgui_imhex_extensions.h>
#include <hex/api/imhex_api.hpp>
#include <hex/api/localization_manager.hpp>

#include <romfs/romfs.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace hex::plugin::builtin {

    namespace {

        enum class IndexType {
            U8,
            U16,
            U32,
            Invalid,
        };

        struct Vectors {
            std::vector<float> vertices;
            std::vector<float> normals;
            std::vector<float> colors;
            std::vector<float> uv1;
            std::vector<u8> indices8;
            std::vector<u16> indices16;
            std::vector<u32> indices32;
        };

        struct LineVectors {
            std::vector<float> vertices;
            std::vector<float> colors;
            std::vector<u8> indices8;
            std::vector<u16> indices16;
            std::vector<u32> indices32;
        };

        struct Buffers {
            gl::Buffer<float> vertices;
            gl::Buffer<float> normals;
            gl::Buffer<float> colors;
            gl::Buffer<float> uv1;
            gl::Buffer<u8> indices8;
            gl::Buffer<u16> indices16;
            gl::Buffer<u32> indices32;
        };

        struct LineBuffers {
            gl::Buffer<float> vertices;
            gl::Buffer<float> colors;
            gl::Buffer<u8> indices8;
            gl::Buffer<u16> indices16;
            gl::Buffer<u32> indices32;
        };

        ImVec2 s_renderingWindowSize;

        int s_drawMode = GL_TRIANGLES;
        float s_nearLimit = 0.9f;
        float s_farLimit = 100.0f;
        float s_scaling = 1.0F;
        float s_max;
        float s_rpms = 10;
        double s_previousTime;
        double s_initialTime;

        bool s_isPerspective = true;
        bool s_drawAxes = true;
        bool s_drawGrid = true;
        bool s_animationOn = false;
        bool s_drawSource = true;
        bool s_drawTexture = false;
        bool s_shouldReset = false;
        bool s_shouldUpdateSource = true;

        IndexType s_indexType;

        ImGuiExt::Texture s_modelTexture;

        gl::Vector<float, 3> s_translation = {{0.0f, 0.0F, -3.0F}};
        gl::Vector<float, 3> s_rotation = {{0.0F, 0.0F, 0.0F}};
        gl::Vector<float, 3> s_lightPosition = {{-0.7F, 0.0F, 0.0F}};
        gl::Vector<float, 4> s_strength = {{0.5F, 0.5F, 0.5F, 32}};
        gl::Matrix<float, 4, 4> s_rotate = gl::Matrix<float, 4, 4>::identity();

        ImGuiExt::Texture s_texture;
        std::fs::path s_texturePath, s_texturePathOld;


        template<typename T>
        void indicesForLines(std::vector<T> &vertexIndices) {
            std::vector<u32> indices;

            u32 vertexCount = vertexIndices.size() / 3;
            indices.resize(vertexCount * 6);

            for (u32 i = 0; i < vertexCount; ++i) {
                indices[i * 6] = vertexIndices[3 * i];
                indices[i * 6 + 1] = vertexIndices[3 * i + 1];

                indices[i * 6 + 2] = vertexIndices[3 * i + 1];
                indices[i * 6 + 3] = vertexIndices[3 * i + 2];

                indices[i * 6 + 4] = vertexIndices[3 * i + 2];
                indices[i * 6 + 5] = vertexIndices[3 * i];
            }

            vertexIndices.resize(indices.size());
            for (u32 i = 0; i < indices.size(); ++i)
                vertexIndices[i] = indices[i];
        }


        void boundingBox(auto vertices, auto &max) {
            gl::Vector<float, 4> minWorld(std::numeric_limits<float>::infinity()), maxWorld(-std::numeric_limits<float>::infinity());
            for (u32 i = 0; i < vertices.size(); i += 3) {
                if (vertices[i] < minWorld[0]) minWorld[0] = vertices[i];
                if (vertices[i + 1] < minWorld[1]) minWorld[1] = vertices[i + 1];
                if (vertices[i + 2] < minWorld[2]) minWorld[2] = vertices[i + 2];

                if (vertices[i] > maxWorld[0]) maxWorld[0] = vertices[i];
                if (vertices[i + 1] > maxWorld[1]) maxWorld[1] = vertices[i + 1];
                if (vertices[i + 2] > maxWorld[2]) maxWorld[2] = vertices[i + 2];
            }

            minWorld[3] = 1;
            maxWorld[3] = 1;

            gl::Vector<float, 4> minCamera = minWorld, maxCamera = maxWorld;

            if (maxCamera[3] != 0)
                maxCamera = maxCamera * (1.0f / maxCamera[3]);

            if (minCamera[3] != 0)
                minCamera = minCamera * (1.0f / minCamera[3]);

            float maxx = std::max(std::fabs(minCamera[0]), std::fabs(maxCamera[0]));
            float maxy = std::max(std::fabs(minCamera[1]), std::fabs(maxCamera[1]));
            max = std::max(maxx, maxy);
        }

        void setDefaultColors(auto &colors, auto size, int color) {
            colors.resize(size / 3 * 4);

            float red   = float(color & 0xFF) / 255.0F;
            float green = float((color >> 8) & 0xFF) / 255.0F;
            float blue  = float((color >> 16) & 0xFF) / 255.0F;
            float alpha = float((color >> 24) & 0xFF) / 255.0F;

            for (u32 i = 0; i < colors.size(); i += 4) {
                colors[i] = red;
                colors[i + 1] = green;
                colors[i + 2] = blue;
                colors[i + 3] = alpha;
            }
        }

        void setNormals(auto vertices, auto &normals) {
            for (u32 i = 0; i < normals.size(); i += 9) {

                auto v1 = gl::Vector<float, 3>({vertices[i], vertices[i + 1], vertices[i + 2]});
                auto v2 = gl::Vector<float, 3>({vertices[i + 3], vertices[i + 4], vertices[i + 5]});
                auto v3 = gl::Vector<float, 3>({vertices[i + 6], vertices[i + 7], vertices[i + 8]});

                auto normal = ((v2 - v1).cross(v3 - v1));
                normals[i] += normal[0];
                normals[i + 1] += normal[1];
                normals[i + 2] += normal[2];
                normals[i + 3] += normal[0];
                normals[i + 4] += normal[1];
                normals[i + 5] += normal[2];
                normals[i + 6] += normal[0];
                normals[i + 7] += normal[1];
                normals[i + 8] += normal[2];
            }
            for (u32 i = 0; i < normals.size(); i += 3) {
                auto normal = gl::Vector<float, 3>({normals[i], normals[i + 1], normals[i + 2]});
                normal.normalize();
                normals[i] = normal[0];
                normals[i + 1] = normal[1];
                normals[i + 2] = normal[2];
            }
        }

        void setNormalsWithIndices(auto vertices, auto &normals, auto indices) {
            for (u32 i = 0; i < indices.size(); i += 3) {
                auto idx = indices[i];
                auto idx1 = indices[i + 1];
                auto idx2 = indices[i + 2];

                auto v1 = gl::Vector<float, 3>({vertices[3 * idx], vertices[3 * idx + 1], vertices[3 * idx + 2]});
                auto v2 = gl::Vector<float, 3>(
                        {vertices[3 * idx1], vertices[3 * idx1 + 1], vertices[3 * idx1 + 2]});
                auto v3 = gl::Vector<float, 3>(
                        {vertices[3 * idx2], vertices[3 * idx2 + 1], vertices[3 * idx2 + 2]});

                auto weighted = ((v2 - v1).cross(v3 - v1));

                normals[3 * idx] += weighted[0];
                normals[3 * idx + 1] += weighted[1];
                normals[3 * idx + 2] += weighted[2];
                normals[3 * idx1] += weighted[0];
                normals[3 * idx1 + 1] += weighted[1];
                normals[3 * idx1 + 2] += weighted[2];
                normals[3 * idx2] += weighted[0];
                normals[3 * idx2 + 1] += weighted[1];
                normals[3 * idx2 + 2] += weighted[2];
            }
            for (u32 i = 0; i < normals.size(); i += 3) {

                auto normal = gl::Vector<float, 3>({normals[i], normals[i + 1], normals[i + 2]});
                auto mag = normal.magnitude();
                if (mag > 0.001F) {
                    normals[i] = normal[0] / mag;
                    normals[i + 1] = normal[1] / mag;
                    normals[i + 2] = normal[2] / mag;
                }
            }
        }

        void loadVectors(auto &vectors, auto indexType) {
            boundingBox(vectors.vertices, s_max);

            if (s_drawTexture)
                setDefaultColors(vectors.colors, vectors.vertices.size(), 0x00000000);
            else if (vectors.colors.empty())
                setDefaultColors(vectors.colors, vectors.vertices.size(), 0xFF337FFF);

            if (vectors.normals.empty()) {
                vectors.normals.resize(vectors.vertices.size());

                if ((indexType == IndexType::U8 && vectors.indices8.empty()) || (indexType == IndexType::Invalid) ||
                    (indexType == IndexType::U16 && vectors.indices16.empty()) ||
                    (indexType == IndexType::U32 && vectors.indices32.empty())) {

                    setNormals(vectors.vertices, vectors.normals);

                } else {
                    std::vector<u32> indices;

                    if (indexType == IndexType::U16) {
                        indices.resize(vectors.indices16.size());
                        for (u32 i = 0; i < vectors.indices16.size(); ++i)
                            indices[i] = vectors.indices16[i];

                    } else if (indexType == IndexType::U8) {
                        indices.resize(vectors.indices8.size());
                        for (u32 i = 0; i < vectors.indices8.size(); ++i)
                            indices[i] = vectors.indices8[i];

                    } else {
                        indices.resize(vectors.indices32.size());
                        for (u32 i = 0; i < vectors.indices32.size(); ++i)
                            indices[i] = vectors.indices32[i];
                    }
                    setNormalsWithIndices(vectors.vertices, vectors.normals, indices);
                }
            }
        }

        void loadLineVectors(auto &lineVectors, auto indexType) {
            boundingBox(lineVectors.vertices, s_max);

            if (lineVectors.colors.empty())
                setDefaultColors(lineVectors.colors, lineVectors.vertices.size(), 0xFF337FFF);

            std::vector<u32> indices;
            if (indexType == IndexType::U16)
                indicesForLines(lineVectors.indices16);
            else if (indexType == IndexType::U8)
                indicesForLines(lineVectors.indices8);
            else
                indicesForLines(lineVectors.indices32);
        }

        void processKeyEvent(ImGuiKey key, auto &variable, auto incr, auto accel) {
            if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(key))) {
                auto temp = variable + incr * accel;
                if (variable * temp < 0.0F)
                    variable = 0.0F;
                else
                    variable = temp;
            }
        }

        void processInputEvents(auto &rotation, auto &translation, auto &scaling, auto &nearLimit, auto &farLimit) {
            auto accel = 1.0F;
            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_LeftShift)) ||
                ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_RightShift)))
                accel = 10.0F;


            auto dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
            if (dragDelta.x != 0) {
                rotation[1] += dragDelta.x * 0.0075F * accel;
            }
            if (dragDelta.y != 0) {
                rotation[0] += dragDelta.y * 0.0075F * accel;
            }
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);

            dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
            translation[0] += dragDelta.x * 0.0075F * accel;
            translation[1] -= dragDelta.y * 0.0075F * accel;
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);

            auto scrollDelta = ImGui::GetIO().MouseWheel;
            scaling += scrollDelta * 0.1F * accel;

            if (scaling < 0.01F)
                scaling = 0.01F;

            processKeyEvent(ImGuiKey_Keypad4, translation[0], -0.1F, accel);
            processKeyEvent(ImGuiKey_Keypad6, translation[0], 0.1F, accel);
            processKeyEvent(ImGuiKey_Keypad8, translation[1], 0.1F, accel);
            processKeyEvent(ImGuiKey_Keypad2, translation[1], -0.1F, accel);
            processKeyEvent(ImGuiKey_Keypad1, translation[2], 0.1F, accel);
            processKeyEvent(ImGuiKey_Keypad7, translation[2], -0.1F, accel);
            processKeyEvent(ImGuiKey_Keypad9, nearLimit, -0.01F, accel);
            processKeyEvent(ImGuiKey_Keypad3, nearLimit, 0.01F, accel);
            if (ImHexApi::System::isDebugBuild()) {
                processKeyEvent(ImGuiKey_KeypadDivide, farLimit, -1.0F, accel);
                processKeyEvent(ImGuiKey_KeypadMultiply, farLimit, 1.0F, accel);
            }
            processKeyEvent(ImGuiKey_KeypadAdd, rotation[2], -0.075F, accel);
            processKeyEvent(ImGuiKey_KeypadSubtract, rotation[2], 0.075F, accel);
            rotation[2] = std::fmod(rotation[2], 2 * std::numbers::pi);
        }


        void bindBuffers(auto &buffers, auto &vertexArray, auto vectors, auto indexType) {
            buffers.vertices = {};
            buffers.normals = {};
            buffers.colors = {};
            buffers.indices8 = {};
            buffers.uv1 = {};
            buffers.indices16 = {};
            buffers.indices32 = {};

            vertexArray.bind();
            buffers.vertices = gl::Buffer<float>(gl::BufferType::Vertex, vectors.vertices);
            buffers.colors = gl::Buffer<float>(gl::BufferType::Vertex, vectors.colors);
            buffers.normals = gl::Buffer<float>(gl::BufferType::Vertex, vectors.normals);

            if (indexType == IndexType::U16)
                buffers.indices16 = gl::Buffer<u16>(gl::BufferType::Index, vectors.indices16);
            else if (indexType == IndexType::U8)
                buffers.indices8 = gl::Buffer<u8>(gl::BufferType::Index, vectors.indices8);
            else
                buffers.indices32 = gl::Buffer<u32>(gl::BufferType::Index, vectors.indices32);

            if (!vectors.uv1.empty())
                buffers.uv1 = gl::Buffer<float>(gl::BufferType::Vertex, vectors.uv1);

            vertexArray.addBuffer(0, buffers.vertices);
            vertexArray.addBuffer(1, buffers.colors, 4);
            vertexArray.addBuffer(2, buffers.normals);

            if (!vectors.uv1.empty())
                vertexArray.addBuffer(3, buffers.uv1, 2);

            buffers.vertices.unbind();
            buffers.colors.unbind();
            buffers.normals.unbind();

            if (!vectors.uv1.empty())
                buffers.uv1.unbind();

            if (indexType == IndexType::U8)
                buffers.indices8.unbind();

            else if (indexType == IndexType::U16)
                buffers.indices16.unbind();

            else if (indexType == IndexType::U32)
                buffers.indices32.unbind();

            vertexArray.unbind();

        }

        void bindLineBuffers(auto &lineBuffers, auto &vertexArray, auto lineVectors, auto indexType) {
            lineBuffers.vertices = {};
            lineBuffers.colors = {};
            lineBuffers.indices8 = {};
            lineBuffers.indices16 = {};
            lineBuffers.indices32 = {};

            vertexArray.bind();
            lineBuffers.vertices = gl::Buffer<float>(gl::BufferType::Vertex, lineVectors.vertices);
            lineBuffers.colors = gl::Buffer<float>(gl::BufferType::Vertex, lineVectors.colors);

            if (indexType == IndexType::U16)
                lineBuffers.indices16 = gl::Buffer<u16>(gl::BufferType::Index, lineVectors.indices16);
            else if (indexType == IndexType::U8)
                lineBuffers.indices8 = gl::Buffer<u8>(gl::BufferType::Index, lineVectors.indices8);
            else
                lineBuffers.indices32 = gl::Buffer<u32>(gl::BufferType::Index, lineVectors.indices32);

            vertexArray.addBuffer(0, lineBuffers.vertices);
            vertexArray.addBuffer(1, lineBuffers.colors, 4);

            lineBuffers.vertices.unbind();
            lineBuffers.colors.unbind();

            if (indexType == IndexType::U8)
                lineBuffers.indices8.unbind();

            else if (indexType == IndexType::U16)
                lineBuffers.indices16.unbind();

            else if (indexType == IndexType::U32)
                lineBuffers.indices32.unbind();

            vertexArray.unbind();

        }

        /*void styledToolTip(const std::string &tip, bool isSeparator = false) {
            ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 3.0F);

            if (isSeparator) {
                ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextBorderSize,3.0);
                ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextAlign, ImVec2(0.5F, 0.5F));
                ImGuiExt::InfoTooltip(tip.c_str(), true);
                ImGui::PopStyleVar();
                ImGui::PopStyleVar();
            } else
                ImGuiExt::InfoTooltip(tip.c_str(),false);

            ImGui::PopStyleVar();
        }*/

        /*void drawNearPlaneUI(float availableWidth, auto &nearLimit, auto &farLimit) {
            if (std::fabs(nearLimit) < 1e-6)
                nearLimit = 0.0;

            auto label = "hex.builtin.pl_visualizer.3d.nearClipPlane"_lang.get() + ":";
            label += fmt::format("{:.3}", nearLimit);

            if (ImHexApi::System::isDebugBuild())
                label +=  fmt::format(", {:.3}", farLimit);

            ImGui::PushItemWidth(availableWidth);
            ImGui::TextUnformatted(label.c_str());
            if (ImGui::IsItemHovered()) {
                std::string tip = "hex.builtin.pl_visualizer.3d.nearlimit"_lang.get();
                styledToolTip(tip);
            }
            ImGui::PopItemWidth();
        }

        void drawTranslateUI(auto availableWidth, auto &translation) {

            if (std::fabs(translation[0]) < 1e-6)
                translation[0] = 0.0;
            if (std::fabs(translation[1]) < 1e-6)
                translation[1] = 0.0;
            if (std::fabs(translation[2]) < 1e-6)
                translation[2] = 0.0;

            auto label = fmt::format("{:.3},{:.3},{:.3}", translation[0], translation[1], translation[2]);
            ImGui::PushItemWidth(availableWidth);
            ImGui::TextUnformatted(label.c_str());
            if (ImGui::IsItemHovered()) {
                std::string tip = "hex.builtin.pl_visualizer.3d.translation"_lang.get();
                styledToolTip(tip,true);
                tip = "hex.builtin.pl_visualizer.3d.translationMouse"_lang.get();
                tip += "\n";
                tip += "hex.builtin.pl_visualizer.3d.translationKeyboard"_lang.get();
                styledToolTip(tip);
            }
            ImGui::PopItemWidth();
        }*/

        /*void drawRotationsScaleUI(auto availableWidth_1, auto availableWidth_2, auto &rotation, auto &rotate,
                                                    auto &scaling, auto &strength, auto &lightPosition,
                                                    auto &renderingWindowSize, auto minSize, auto &resetEverything,
                                                    auto &translation, auto &nearLimit, auto &farLimit) {

            float fontSize = ImHexApi::System::getFontSize();
            float sliderWidth = std::floor(fontSize / 2.0f) * 3.0f;
            ImVec2 size(sliderWidth, renderingWindowSize.y);
            std::string units = "hex.builtin.pl_visualizer.3d.degrees"_lang.get();

            ImGuiExt::VSliderAngle("##X", size, &rotation.data()[1], 0, 360, "X", ImGuiSliderFlags_AlwaysClamp);
            std::string temp = fmt::format("{:.3} ", rotation[1] * 180 * std::numbers::inv_pi);
            temp += units;

            if (ImGui::IsItemHovered())
                styledToolTip(temp);

            ImGui::SameLine();

            ImGuiExt::VSliderAngle("##Y", size, &rotation.data()[0], 0, 360, "Y", ImGuiSliderFlags_AlwaysClamp);
            temp = fmt::format("{:.3} ", rotation[0] * 180 * std::numbers::inv_pi);
            temp += units;

            if (ImGui::IsItemHovered())
                styledToolTip(temp);

            ImGui::SameLine();

            ImGuiExt::VSliderAngle("##Z", size, &rotation.data()[2], 0, 360, "Z", ImGuiSliderFlags_AlwaysClamp);
            temp = fmt::format("{:.3} ", rotation[2] * 180 * std::numbers::inv_pi);
            temp += units;

            if (ImGui::IsItemHovered())
                styledToolTip(temp);

            for (u8 i = 0; i < 3; i++) {
                i32 winding = 0;
                if (rotation.data()[i] < 0 || rotation.data()[i] > std::numbers::pi * 2)
                    winding = std::floor(rotation.data()[i] / std::numbers::pi / 2.0);

                rotation.data()[i] -= std::numbers::pi * winding * 2.0;
                if (rotation.data()[i] < 0.001)
                    rotation.data()[i] = 0.0F;
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            std::string label = "hex.builtin.pl_visualizer.3d.scale"_lang.get();
            label += ": %.3f";

            ImGui::PushItemWidth(availableWidth_1);

            if (scaling < s_minScaling)
                s_minScaling = scaling;
            if (scaling > s_maxScaling)
                s_maxScaling = scaling;

            ImGui::SliderFloat("##Scale", &scaling, s_minScaling, s_maxScaling, label.c_str());
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();

            label = "hex.builtin.common.reset"_lang.get();
            auto labelWidth = ImGui::CalcTextSize(label.c_str()).x;
            auto spacing = (availableWidth_2 - labelWidth) / 2.0f;

            ImGui::Indent(spacing);

            ImGui::TextUnformatted(label.c_str());
            if (ImGui::IsItemHovered()) {
                std::string tip = "hex.builtin.pl_visualizer.3d.reset_description"_lang.get();
                styledToolTip(tip);
            }
            ImGui::Unindent(spacing);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            drawTranslateUI(availableWidth_1, translation);

            ImGui::TableNextColumn();

            static auto resetIcon = ICON_FA_SYNC_ALT;
            auto buttonSize = ImGui::CalcTextSize(resetIcon) + ImGui::GetStyle().FramePadding * 2;

            spacing = (availableWidth_2 - buttonSize.x) / 2.0f;
            ImGui::Indent(spacing);

            bool dummy = true;
            if (ImGuiExt::DimmedIconToggle(ICON_FA_SYNC_ALT, &dummy)) {
                if (ImGui::GetIO().KeyCtrl) {
                    strength = {{0.5F, 0.5F, 0.5F, 32}};
                    lightPosition = {{-0.7F, 0.0F, 0.0F}};
                    renderingWindowSize.x = minSize;
                    renderingWindowSize.y = minSize;
                    resetEverything = true;

                } else if (resetEverything)
                    resetEverything = false;

                translation = {{0.0F, 0.0F, -3.0F}};
                rotation = {{0.0F, 0.0F, 0.0F}};
                rotate = gl::Matrix<float, 4, 4>::identity();
                scaling = 1.0F;
                nearLimit = 0.9f;
                farLimit = 100.0f;

            } else if (resetEverything)
                resetEverything = false;

            if (ImGui::IsItemHovered()) {
                std::string tip = "hex.builtin.pl_visualizer.3d.reset_usage"_lang.get();
                styledToolTip(tip);
            }
            ImGui::Unindent(spacing);
        }

        void drawLightPositionUI(float availableWidth, auto &lightPosition, auto &shouldUpdateSource, auto &resetEverything) {
            if (ImGui::BeginTable("##LightPosition", 1, ImGuiTableFlags_SizingFixedFit)) {

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                std::string label = "hex.builtin.pl_visualizer.3d.lightPosition"_lang.get();
                auto labelWidth = ImGui::CalcTextSize(label.c_str()).x;

                auto indent = (availableWidth - labelWidth) / 2.0f;
                ImGui::Indent(indent);

                ImGuiExt::TextFormatted(label);
                if (ImGui::IsItemHovered()) {
                    std::string tip = "hex.builtin.pl_visualizer.3d.lightPosition_description"_lang.get();
                    styledToolTip(tip);
                }
                ImGui::Unindent(indent);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::PushItemWidth(availableWidth);
                if (ImGui::InputFloat3("##lightDir", lightPosition.data(), "%.3f",
                                       ImGuiInputTextFlags_EnterReturnsTrue) || resetEverything) {
                    shouldUpdateSource = true;
                    s_previousTime = 0;
                }
                if (ImGui::IsItemHovered()) {
                    std::string tip = "hex.builtin.pl_visualizer.3d.lightPosition_usage"_lang.get();
                    styledToolTip(tip);
                }
                ImGui::PopItemWidth();
            }
            ImGui::EndTable();
        }

        void drawProjectionUI(auto availableWidth, auto &isPerspective, auto &shouldResetLocally, auto &resetEverything) {
            if (ImGui::BeginTable("##Proj", 1, ImGuiTableFlags_SizingFixedFit)) {

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                static auto projectionIcon = ICON_BI_VIEW_PERSPECTIVE;
                auto label = "hex.builtin.pl_visualizer.3d.projection"_lang.get();
                auto labelWidth = ImGui::CalcTextSize(label.c_str()).x;

                float spacing = (availableWidth - labelWidth) / 2.0f;
                ImGui::Indent(spacing);

                ImGui::TextUnformatted(label.c_str());
                if (ImGui::IsItemHovered()) {
                    std::string tip;
                    if (isPerspective)
                        tip = "hex.builtin.pl_visualizer.3d.perspectiveSelected"_lang.get();
                    else
                        tip = "hex.builtin.pl_visualizer.3d.orthographicSelected"_lang.get();
                    styledToolTip(tip);
                }
                ImGui::Unindent(spacing);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                auto buttonSize = ImGui::CalcTextSize(projectionIcon) + ImGui::GetStyle().FramePadding * 2;
                spacing = (availableWidth - buttonSize.x) / 2.0f;
                ImGui::Indent(spacing);

                bool dummy = true;
                if (ImGuiExt::DimmedIconToggle(projectionIcon, &dummy) || resetEverything) {
                    if (!isPerspective || resetEverything) {
                        projectionIcon = ICON_BI_VIEW_PERSPECTIVE;
                        isPerspective = true;
                    } else {
                        projectionIcon = ICON_BI_VIEW_ORTHO;
                        isPerspective = false;
                    }
                    shouldResetLocally = true;
                }

                if (ImGui::IsItemHovered()) {
                    std::string tip;
                    if (isPerspective)
                        tip = "hex.builtin.pl_visualizer.3d.selectOrthographic"_lang.get();
                    else
                        tip = "hex.builtin.pl_visualizer.3d.selectPerspective"_lang.get();
                    styledToolTip(tip);
                }
                ImGui::Unindent(spacing);
            }
            ImGui::EndTable();
        }

        void drawPrimitiveUI(auto availableWidth) {
            if (ImGui::BeginTable("##Prim", 1, ImGuiTableFlags_SizingFixedFit)) {

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                auto label = "hex.builtin.pl_visualizer.3d.primitive"_lang.get();
                auto labelWidth = ImGui::CalcTextSize(label.c_str()).x;

                float spacing = (availableWidth - labelWidth) / 2.0f;
                ImGui::Indent(spacing);

                ImGui::TextUnformatted(label.c_str());
                if (ImGui::IsItemHovered()) {
                    std::string tip;
                    if (s_drawMode == GL_TRIANGLES)
                        tip = "hex.builtin.pl_visualizer.3d.trianglesSelected"_lang.get();
                    else
                        tip = "hex.builtin.pl_visualizer.3d.linesSelected"_lang.get();
                    styledToolTip(tip);
                }
                ImGui::Unindent(spacing);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                static auto primitiveIcon = ICON_BI_MOD_SOLIDIFY;
                auto buttonSize = ImGui::CalcTextSize(primitiveIcon) + ImGui::GetStyle().FramePadding * 2;
                spacing = (availableWidth - buttonSize.x) / 2.0f;

                ImGui::Indent(spacing);
                bool dummy = true;
                if (ImGuiExt::DimmedIconToggle(primitiveIcon, &dummy) || s_resetEverything) {
                    if (s_drawMode == GL_LINES || s_resetEverything) {
                        primitiveIcon = ICON_BI_MOD_SOLIDIFY;
                        s_drawMode = GL_TRIANGLES;
                    } else {
                        primitiveIcon = ICON_BI_CUBE;
                        s_drawMode = GL_LINES;
                    }
                    s_shouldReset = true;
                }
                if (ImGui::IsItemHovered()) {
                    std::string tip;
                    if (s_drawMode == GL_TRIANGLES)
                        tip = "hex.builtin.pl_visualizer.3d.line"_lang.get();
                    else
                        tip = "hex.builtin.pl_visualizer.3d.triangle"_lang.get();
                    styledToolTip(tip);
                }
                ImGui::Unindent(spacing);
            }
            ImGui::EndTable();
        }

        void drawLightPropertiesUI(float availableWidth, auto &strength) {
            if (ImGui::BeginTable("##LightProperties", 1, ImGuiTableFlags_SizingFixedFit)) {

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                auto label = "hex.builtin.pl_visualizer.3d.lightProperties"_lang.get();
                auto labelWidth = ImGui::CalcTextSize(label.c_str()).x;

                auto indent = (availableWidth - labelWidth) / 2.0f;
                ImGui::Indent(indent);

                ImGuiExt::TextFormatted("{}", label);
                ImGui::Unindent(indent);
                ImGui::SameLine();

                float labelHeight = ImGui::CalcTextSize("Any text").y;
                auto framePadding = ImGui::GetStyle().FramePadding.x;
                auto bSize = ImVec2({((availableWidth) / 4.0f) - framePadding* 2, labelHeight});

                ImGui::Indent(framePadding);

                ImGui::InvisibleButton("##ambient", bSize);
                if (ImGui::IsItemHovered()) {
                    std::string tip = "hex.builtin.pl_visualizer.3d.ambientIntensity"_lang.get();
                    styledToolTip(tip);
                }
                ImGui::Unindent(framePadding);

                ImGui::SameLine();

                indent = bSize.x + framePadding;
                ImGui::Indent(indent);

                ImGui::InvisibleButton("##diffuse", bSize);
                if (ImGui::IsItemHovered()) {
                    std::string tip = "hex.builtin.pl_visualizer.3d.diffuseIntensity"_lang.get();
                    styledToolTip(tip);
                }
                ImGui::Unindent(indent);

                ImGui::SameLine();

                indent = 2 * bSize.x + framePadding;
                ImGui::Indent(indent);

                ImGui::InvisibleButton("##specular", bSize);
                if (ImGui::IsItemHovered()) {
                    std::string tip = "hex.builtin.pl_visualizer.3d.specularIntensity"_lang.get();
                    styledToolTip(tip);
                }
                ImGui::Unindent(indent);

                ImGui::SameLine();

                indent = 3 * bSize.x + framePadding;
                ImGui::Indent(indent);

                ImGui::InvisibleButton("##shininess", bSize);
                if (ImGui::IsItemHovered()) {
                    std::string tip = "hex.builtin.pl_visualizer.3d.shininessIntensity"_lang.get();
                    styledToolTip(tip);
                }
                ImGui::Unindent(indent);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::PushItemWidth(availableWidth);
                ImGui::InputFloat4("##strength", strength.data(), "%.3f", ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::PopItemWidth();
                ImGui::SameLine();

                bSize = ImVec2({availableWidth / 4.0f, labelHeight});

                ImGui::Indent(0);
                ImGui::InvisibleButton("##ambient2", bSize);
                if (ImGui::IsItemHovered()) {
                    std::string tip = "hex.builtin.pl_visualizer.3d.ambient"_lang.get();
                    styledToolTip(tip);
                }
                ImGui::Unindent(0);
                ImGui::SameLine();

                ImGui::Indent(bSize.x);
                ImGui::InvisibleButton("##diffuse2", bSize);
                if (ImGui::IsItemHovered()) {
                    std::string tip = "hex.builtin.pl_visualizer.3d.diffuse"_lang.get();
                    styledToolTip(tip);
                }
                ImGui::Unindent(bSize.x);
                ImGui::SameLine();

                ImGui::Indent(2 * bSize.x);
                ImGui::InvisibleButton("##specular2", bSize);
                if (ImGui::IsItemHovered()) {
                    std::string tip = "hex.builtin.pl_visualizer.3d.specular"_lang.get();
                    styledToolTip(tip);
                }

                ImGui::Unindent(2 * bSize.x);
                ImGui::SameLine();

                ImGui::Indent(3 * bSize.x);
                ImGui::InvisibleButton("##shininess2", bSize);
                if (ImGui::IsItemHovered()) {
                    std::string tip = "hex.builtin.pl_visualizer.3d.shininess"_lang.get();
                    styledToolTip(tip);
                }
                ImGui::Unindent(3 * bSize.x);
            }
            ImGui::EndTable();
        }

        void drawTextureUI(float availableWidth, auto texturePath) {
            if (ImGui::BeginTable("##Texture", 1, ImGuiTableFlags_SizingFixedFit)) {

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                auto label = "hex.builtin.pl_visualizer.3d.textureFile"_lang.get();
                auto labelWidth = ImGui::CalcTextSize(label.c_str()).x;

                auto indent = (availableWidth - labelWidth) / 2.0f;
                ImGui::Indent(indent);

                ImGuiExt::TextFormatted("{}", label);
                if (ImGui::IsItemHovered()) {
                    std::string tip = "hex.builtin.pl_visualizer.3d.textureFile"_lang.get();
                    styledToolTip(tip);
                }
                ImGui::Unindent(indent);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::PushItemWidth(availableWidth);
                ImGui::InputText("##Texture", texturePath, ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::PopItemWidth();
            }
            ImGui::EndTable();
        }

        void drawAnimationSpeedUI(float availableWidth, auto &rpms) {
            if (ImGui::BeginTable("##AnimationSpeed", 1, ImGuiTableFlags_SizingFixedFit)) {

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                auto label = "hex.builtin.pl_visualizer.3d.animationSpeed"_lang.get();
                auto labelWidth = ImGui::CalcTextSize(label.c_str()).x;

                auto indent = (availableWidth - labelWidth) / 2.0f;
                ImGui::Indent(indent);

                ImGui::TextUnformatted(label.c_str());
                ImGui::Unindent(indent);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::PushItemWidth(availableWidth);
                std::string format = "%.3f rpm";
                format += std::fabs(rpms - 1.0) > 1e-6 ? "s" : "";
                ImGui::SliderFloat("##animationSpeed", &rpms, 0.1F, 200.0f, format.c_str());
                ImGui::PopItemWidth();
            }
            ImGui::EndTable();
        }

        void drawLightUI(float availableWidth) {
            if (ImGui::BeginTable("##ligth", 1, ImGuiTableFlags_SizingFixedFit)) {

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                auto label = "hex.builtin.pl_visualizer.3d.light"_lang.get();
                auto labelWidth = ImGui::CalcTextSize(label.c_str()).x;

                auto spacing = (availableWidth - labelWidth) / 2.0f;
                ImGui::Indent(spacing);

                ImGui::TextUnformatted(label.c_str());
                if (ImGui::IsItemHovered()) {
                    std::string tip;
                    if (s_drawSource)
                        tip = "hex.builtin.pl_visualizer.3d.renderingLightSource"_lang.get();
                    else
                        tip = "hex.builtin.pl_visualizer.3d.notRenderingLightSource"_lang.get();
                    styledToolTip(tip);
                }
                ImGui::Unindent(spacing);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                static auto sourceIcon = ICON_VS_LIGHTBULB;
                auto buttonSize = ImGui::CalcTextSize(sourceIcon) + ImGui::GetStyle().FramePadding * 2;

                spacing = (availableWidth - buttonSize.x) / 2.0f;
                ImGui::Indent(spacing);

                if (ImGuiExt::DimmedIconToggle(sourceIcon, &s_drawSource) || s_resetEverything) {
                    if (s_resetEverything)
                        s_drawSource = true;
                }
                if (ImGui::IsItemHovered()) {
                    std::string tip;
                    if (s_drawSource)
                        tip = "hex.builtin.pl_visualizer.3d.dontDrawSource"_lang.get();
                    else
                        tip = "hex.builtin.pl_visualizer.3d.drawSource"_lang.get();
                    styledToolTip(tip);
                }
                ImGui::Unindent(spacing);
            }
            ImGui::EndTable();
        }

        void drawAnimateUI(float availableWidth, auto &animationOn, auto &previousTime, auto &initialTime, auto &resetEverything) {
            if (ImGui::BeginTable("##Animate", 1, ImGuiTableFlags_SizingFixedFit)) {

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                auto label = "hex.builtin.pl_visualizer.3d.animation"_lang.get();
                auto labelWidth = ImGui::CalcTextSize(label.c_str()).x;

                auto spacing = (availableWidth - labelWidth) / 2.0f;
                ImGui::Indent(spacing);

                ImGui::TextUnformatted(label.c_str());
                if (ImGui::IsItemHovered()) {
                    std::string tip;
                    if (animationOn)
                        tip = "hex.builtin.pl_visualizer.3d.animationIsRunning"_lang.get();
                    else
                        tip = "hex.builtin.pl_visualizer.3d.animationIsNotRunning"_lang.get();
                    styledToolTip(tip);
                }
                ImGui::Unindent(spacing);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                static auto animateIcon = ICON_FA_PLAY;
                auto buttonSize = ImGui::CalcTextSize(animateIcon) + ImGui::GetStyle().FramePadding * 2;

                spacing = (availableWidth - buttonSize.x) / 2.0f;
                ImGui::Indent(spacing);

                bool dummy = true;
                if (ImGuiExt::DimmedIconToggle(animateIcon, &dummy) || resetEverything) {
                    if (animationOn || resetEverything) {
                        animateIcon = ICON_FA_PLAY;
                        animationOn = false;
                    } else {
                        animateIcon = ICON_FA_STOP;
                        animationOn = true;
                        previousTime = 0;
                        initialTime = glfwGetTime();
                    }
                }
                if (ImGui::IsItemHovered()) {
                    std::string tip;
                    if (animationOn)
                        tip = "hex.builtin.pl_visualizer.3d.dontAnimate"_lang.get();
                    else
                        tip = "hex.builtin.pl_visualizer.3d.animate"_lang.get();
                    styledToolTip(tip);
                }
                ImGui::Unindent(spacing);
            }
            ImGui::EndTable();
        }*/

        void drawWindow(auto &texture, auto &renderingWindowSize, auto mvp) {
            auto textureSize = texture.getSize();
            auto textureWidth = textureSize.x;
            auto textureHeight = textureSize.y;

            ImVec2 screenPos = ImGui::GetCursorScreenPos();
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

            ImGui::SetNextWindowSizeConstraints(scaled({ 350, 350 }), ImVec2(FLT_MAX, FLT_MAX));
            if (ImGui::BeginChild("##image", textureSize, ImGuiChildFlags_ResizeX | ImGuiChildFlags_ResizeY | ImGuiChildFlags_Border, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                renderingWindowSize = ImGui::GetContentRegionAvail();

                ImGui::Image(texture, textureSize, ImVec2(0, 1), ImVec2(1, 0));

                if (s_drawAxes) {
                    gl::Matrix<float, 4, 4> axes = gl::Matrix<float, 4, 4>::identity();
                    axes(0, 3) = 1.0f;
                    axes(1, 3) = 1.0f;
                    axes(2, 3) = 1.0f;

                    axes = axes * mvp;
                    bool showX = axes(0, 3) > 0.0f;
                    bool showY = axes(1, 3) > 0.0f;
                    bool showZ = axes(2, 3) > 0.0f;

                    axes.updateRow(0, axes.getRow(0) * (1.0f / axes(0, 3)));
                    axes.updateRow(1, axes.getRow(1) * (1.0f / axes(1, 3)));
                    axes.updateRow(2, axes.getRow(2) * (1.0f / axes(2, 3)));

                    auto axesPosx = (axes.getColumn(0) + 1.0f) * (textureWidth / 2.0f);
                    auto axesPosy = (axes.getColumn(1) + 1.0f) * (-textureHeight / 2.0f) + textureHeight;

                    ImDrawList *drawList = ImGui::GetWindowDrawList();

                    if (showX)
                        drawList->AddText(ImVec2(axesPosx[0], axesPosy[0]) + screenPos, IM_COL32(255, 0, 0, 255), "X");
                    if (showY)
                        drawList->AddText(ImVec2(axesPosx[1], axesPosy[1]) + screenPos, IM_COL32(0, 255, 0, 255), "Y");
                    if (showZ)
                        drawList->AddText(ImVec2(axesPosx[2], axesPosy[2]) + screenPos, IM_COL32(0, 0, 255, 255), "Z");
                }

                if (ImHexApi::System::isDebugBuild()) {
                    auto mousePos = ImClamp(ImGui::GetMousePos() - screenPos, { 0, 0 }, textureSize);
                    ImDrawList *drawList = ImGui::GetWindowDrawList();
                    drawList->AddText(
                        screenPos + scaled({ 5, 5 }),
                        ImGui::GetColorU32(ImGuiCol_Text),
                        hex::format("X: {:.5}\nY: {:.5}", mousePos.x, mousePos.y).c_str());
                }

            }
            ImGui::EndChild();
            ImGui::PopStyleVar();

            // Draw axis arrows toggle
            {
                ImGui::PushID(1);
                if (ImGuiExt::DimmedIconToggle(ICON_BI_EMPTY_ARROWS, &s_drawAxes))
                    s_shouldReset = true;
                ImGui::PopID();
            }

            ImGui::SameLine();

            // Draw grid toggle
            {
                ImGui::PushID(2);
                if (ImGuiExt::DimmedIconToggle(ICON_BI_GRID, &s_drawGrid))
                    s_shouldReset = true;
                ImGui::PopID();
            }

            ImGui::SameLine();

            // Draw light source toggle
            {
                ImGui::PushID(3);
                if (ImGuiExt::DimmedIconToggle(ICON_VS_LIGHTBULB, &s_drawSource))
                    s_shouldReset = true;

                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    ImGui::OpenPopup("LightSettings");
                }

                if (ImGui::BeginPopup("LightSettings")) {
                    if (ImGui::DragFloat3("Position", s_lightPosition.data(), 0.05F)) {
                        s_shouldUpdateSource = true;
                    }

                    ImGui::SliderFloat("Ambient Brightness",   &s_strength.data()[0], 0, 2);
                    ImGui::SliderFloat("Diffuse Brightness",   &s_strength.data()[1], 0, 2);
                    ImGui::SliderFloat("Specular Brightness ",  &s_strength.data()[2], 0, 2);
                    ImGui::SliderFloat("Light source strength", &s_strength.data()[3], 0, 64);
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }

            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();

            // Draw projection toggle
            {
                ImGui::PushID(4);
                if (ImGuiExt::DimmedIconToggle(ICON_BI_VIEW_PERSPECTIVE, ICON_BI_VIEW_ORTHO, &s_isPerspective)) {
                    s_shouldReset = true;
                }
                ImGui::PopID();
            }

            ImGui::SameLine();

            // Draw solid / line mode toggle
            {
                ImGui::PushID(4);
                bool isSolid = s_drawMode == GL_TRIANGLES;
                if (ImGuiExt::DimmedIconToggle(ICON_BI_MOD_SOLIDIFY, ICON_BI_CUBE , &isSolid)) {
                    s_shouldReset = true;

                    s_drawMode = isSolid ? GL_TRIANGLES : GL_LINES;
                }
                ImGui::PopID();
            }

            // Draw more settings
            if (ImGui::CollapsingHeader("More settings")) {
                ImGuiExt::InputFilePicker("Texture File", s_texturePath, {});
            }

            /*if (ImGui::BeginTable("##3DVisualizer", columnCount, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX)) {
                constexpr static auto ColumnFlags =  ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoResize;
                ImGui::TableSetupColumn("##FirstColumn", ColumnFlags, textureWidth + 4_scaled);

                if (showUI) {
                    float column2Width = secondColumnWidth;
                    ImGui::TableSetupColumn("##SecondColumn", ColumnFlags, column2Width);
                }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                // Button to enable/disable  user interface
                if (ImGui::Button("UI"))
                    showUI = !showUI;

                if (ImGui::IsItemHovered()) {
                    std::string tip;
                    if (showUI)
                        tip = "hex.builtin.pl_visualizer.3d.hideUI"_lang.get();
                    else
                        tip = "hex.builtin.pl_visualizer.3d.showUI"_lang.get();
                    styledToolTip(tip);
                }

                ImGui::SameLine();

                static auto axesIcon = ICON_BI_EMPTY_ARROWS;

                if (ImGuiExt::DimmedIconToggle(axesIcon, &s_drawAxes))
                    s_shouldReset = true;
                if (s_resetEverything) {
                    s_drawAxes = true;
                    s_shouldReset = true;
                }

                if (ImGui::IsItemHovered()) {
                    std::string tip;
                    if (s_drawAxes)
                        tip = "hex.builtin.pl_visualizer.3d.dontDrawAxes"_lang.get();
                    else
                        tip = "hex.builtin.pl_visualizer.3d.drawAxes"_lang.get();
                    styledToolTip(tip);
                }

                ImGui::SameLine();

                static auto gridIcon = ICON_BI_GRID;
                if (s_isPerspective)
                    gridIcon = ICON_BI_GRID;
                else
                    gridIcon = ICON_VS_SYMBOL_NUMBER;
                if (ImGuiExt::DimmedIconToggle(gridIcon, &s_drawGrid))
                    s_shouldReset = true;

                if (s_resetEverything) {
                    s_drawGrid = true;
                    s_shouldReset = true;
                }

                if (ImGui::IsItemHovered()) {
                    std::string tip;
                    if (s_drawGrid)
                        tip = "hex.builtin.pl_visualizer.3d.hideGrid"_lang.get();
                    else
                        tip = "hex.builtin.pl_visualizer.3d.renderGrid"_lang.get();
                    styledToolTip(tip);
                }

                if (showUI) {
                    ImGui::TableNextColumn();
                    auto textSize = ImGui::CalcTextSize("hex.builtin.pl_visualizer.3d.rotation"_lang);

                    float indent = (secondColumnWidth - textSize.x) / 2.0f;
                    ImGui::Indent(indent);

                    ImGui::TextUnformatted("hex.builtin.pl_visualizer.3d.rotation"_lang);
                    ImGui::Unindent(indent);

                    if (ImGui::IsItemHovered()) {
                        std::string tip = "hex.builtin.pl_visualizer.3d.rotation_usage"_lang.get();
                        styledToolTip(tip);
                    }
                }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                if (showUI) {

                    ImGui::TableNextColumn();

                    drawRotationsScaleUI(firstColumnWidth, secondColumnWidth, s_rotation, s_rotate, s_scaling,
                                         s_strength, s_lightPosition, renderingWindowSize, minSize, s_resetEverything,
                                         s_translation, s_nearLimit, s_farLimit);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    drawLightPositionUI(firstColumnWidth, s_lightPosition, s_shouldUpdateSource, s_resetEverything);

                    ImGui::TableNextColumn();

                    drawProjectionUI(secondColumnWidth, s_isPerspective, s_shouldReset, s_resetEverything);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    drawLightPropertiesUI(firstColumnWidth, s_strength);

                    ImGui::TableNextColumn();

                    drawLightUI(secondColumnWidth);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    drawTextureUI(firstColumnWidth, s_texturePath);

                    ImGui::TableNextColumn();

                    drawPrimitiveUI(secondColumnWidth);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    drawAnimationSpeedUI(firstColumnWidth, s_rpms);

                    ImGui::TableNextColumn();

                    drawAnimateUI(secondColumnWidth, s_animationOn, s_previousTime, s_initialTime, s_resetEverything);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    drawNearPlaneUI(firstColumnWidth, s_nearLimit, s_farLimit);
                }
            }
            ImGui::EndTable();*/
        }

    }

    void draw3DVisualizer(pl::ptrn::Pattern &, pl::ptrn::IIterable &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
        static gl::LightSourceVectors sourceVectors(10);
        static gl::VertexArray sourceVertexArray = gl::VertexArray();
        static gl::LightSourceBuffers sourceBuffers(sourceVertexArray, sourceVectors);

        static gl::VertexArray gridVertexArray = gl::VertexArray();
        static gl::GridVectors gridVectors(9);
        static gl::GridBuffers gridBuffers(gridVertexArray, gridVectors);

        static gl::VertexArray axesVertexArray = gl::VertexArray();
        static gl::AxesVectors axesVectors;
        static gl::AxesBuffers axesBuffers(axesVertexArray, axesVectors);

        static gl::VertexArray vertexArray = gl::VertexArray();
        static Buffers buffers;
        static LineBuffers lineBuffers;

        std::shared_ptr<pl::ptrn::Pattern> verticesPattern = arguments[0].toPattern();
        std::shared_ptr<pl::ptrn::Pattern> indicesPattern = arguments[1].toPattern();
        std::shared_ptr<pl::ptrn::Pattern> normalsPattern = nullptr;
        std::shared_ptr<pl::ptrn::Pattern> colorsPattern = nullptr;
        std::shared_ptr<pl::ptrn::Pattern> uvPattern1 = nullptr;

        std::string textureFile;
        if (arguments.size() > 2) {
            normalsPattern = arguments[2].toPattern();
            if (arguments.size() > 3) {
                colorsPattern = arguments[3].toPattern();
                if (arguments.size() > 4) {
                    uvPattern1 = arguments[4].toPattern();
                    if (arguments.size() > 5)
                        textureFile = arguments[5].toString();
                }
            }
        }

        if (shouldReset)
            s_shouldReset = true;

        const auto fontSize = ImGui::GetFontSize();
        const auto framePad = ImGui::GetStyle().FramePadding;
        float minSize = fontSize * 8_scaled + framePad.x * 20_scaled;
        minSize = minSize > 200_scaled ? minSize : 200_scaled;

        if (s_renderingWindowSize.x <= 0 || s_renderingWindowSize.y <= 0)
            s_renderingWindowSize = { minSize, minSize };

        if (!textureFile.empty())
            s_texturePath = textureFile;
        else
            s_drawTexture = false;

        if (s_renderingWindowSize.x < minSize)
            s_renderingWindowSize.x = minSize;
        if (s_renderingWindowSize.y < minSize)
            s_renderingWindowSize.y = minSize;

        gl::Matrix<float, 4, 4> mvp(0);

        processInputEvents(s_rotation, s_translation, s_scaling, s_nearLimit, s_farLimit);

        if (s_shouldReset) {
            s_shouldReset = false;

            auto *iterable = dynamic_cast<pl::ptrn::IIterable*>(indicesPattern.get());
            if (iterable != nullptr && iterable->getEntryCount() > 0) {
                const auto &content = iterable->getEntry(0);
                if (content->getSize() == 1) {
                    s_indexType = IndexType::U8;
                } else if (content->getSize() == 2) {
                    s_indexType = IndexType::U16;
                } else if (content->getSize() == 4) {
                    s_indexType = IndexType::U32;
                } else {
                    s_indexType = IndexType::Invalid;
                }
            }

            if (s_drawMode == GL_TRIANGLES) {
                Vectors vectors;

                vectors.vertices = patternToArray<float>(verticesPattern.get());
                if (s_indexType == IndexType::U16)
                    vectors.indices16 = patternToArray<u16>(indicesPattern.get());
                else if (s_indexType == IndexType::U32)
                    vectors.indices32 = patternToArray<u32>(indicesPattern.get());
                else if (s_indexType == IndexType::U8)
                    vectors.indices8 = patternToArray<u8>(indicesPattern.get());

                if (colorsPattern != nullptr)
                    vectors.colors = patternToArray<float>(colorsPattern.get());
                if (normalsPattern != nullptr)
                    vectors.normals = patternToArray<float>(normalsPattern.get());
                if (uvPattern1 != nullptr)
                    vectors.uv1 = patternToArray<float>(uvPattern1.get());

                loadVectors(vectors, s_indexType);

                bindBuffers(buffers, vertexArray, vectors, s_indexType);
            } else {
                LineVectors lineVectors;

                lineVectors.vertices = patternToArray<float>(verticesPattern.get());
                if (s_indexType == IndexType::U16)
                    lineVectors.indices16 = patternToArray<u16>(indicesPattern.get());

                else if (s_indexType == IndexType::U32)
                    lineVectors.indices32 = patternToArray<u32>(indicesPattern.get());

                else if (s_indexType == IndexType::U8)
                    lineVectors.indices8 = patternToArray<u8>(indicesPattern.get());

                if (colorsPattern != nullptr)
                    lineVectors.colors = patternToArray<float>(colorsPattern.get());

                loadLineVectors(lineVectors, s_indexType);

                bindLineBuffers(lineBuffers, vertexArray, lineVectors, s_indexType);
            }
        }

        if (s_shouldUpdateSource) {
            s_shouldUpdateSource = false;
            sourceVectors.moveTo(s_lightPosition);
            sourceBuffers.moveVertices(sourceVertexArray, sourceVectors);
        }
        {
            gl::Matrix<float, 4, 4> model(0);
            gl::Matrix<float, 4, 4> scaledModel(0);
            gl::Matrix<float, 4, 4> view(0);
            gl::Matrix<float, 4, 4> projection(0);

            unsigned width = std::floor(s_renderingWindowSize.x);
            unsigned height = std::floor(s_renderingWindowSize.y);

            gl::FrameBuffer frameBuffer(width, height);
            gl::Texture renderTexture(width, height);
            frameBuffer.attachTexture(renderTexture);
            frameBuffer.bind();

            s_rotate = gl::getRotationMatrix<float>(s_rotation, true, gl::RotationSequence::ZYX);

            gl::Matrix<float, 4, 4> scale = gl::Matrix<float, 4, 4>::identity();
            gl::Matrix<float, 4, 4> scaleForVertices = gl::Matrix<float, 4, 4>::identity();
            gl::Matrix<float, 4, 4> translate = gl::Matrix<float, 4, 4>::identity();

            float totalScale;
            float viewWidth =  s_renderingWindowSize.x / 500.0f;
            float viewHeight = s_renderingWindowSize.y / 500.0f;
            glViewport(0,0 , GLsizei(renderTexture.getWidth()), GLsizei(renderTexture.getHeight()));
            glDepthRangef(s_nearLimit, s_farLimit);
            glClearColor(0.00F, 0.00F, 0.00F, 0.00f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);

            if (s_isPerspective == 0) {
                projection = gl::GetOrthographicMatrix(viewWidth, viewHeight, s_nearLimit, s_farLimit, false);
                totalScale = s_scaling / (std::fabs(s_translation[2]));
                scale(0, 0) = totalScale;
                scale(1, 1) = totalScale;
                scale(2, 2) = totalScale;

                translate(3, 0) = s_translation[0] / std::fabs(s_translation[2]);
                translate(3, 1) = s_translation[1] / std::fabs(s_translation[2]);
                translate(3, 2) = s_translation[2];
            } else {
                projection = gl::GetPerspectiveMatrix(viewWidth, viewHeight, s_nearLimit, s_farLimit, false);
                totalScale = s_scaling;
                scale(0, 0) = totalScale;
                scale(1, 1) = totalScale;
                scale(2, 2) = totalScale;

                translate(3, 0) = s_translation[0];
                translate(3, 1) = s_translation[1];
                translate(3, 2) = s_translation[2];
            }
            totalScale /= (3.0f * s_max);
            scaleForVertices(0, 0) = totalScale;
            scaleForVertices(1, 1) = totalScale;
            scaleForVertices(2, 2) = totalScale;

            model = s_rotate * scale;
            scaledModel = s_rotate * scaleForVertices;
            view = translate;
            mvp = model * view * projection;

            if (s_drawMode == GL_TRIANGLES) {
                static gl::Shader shader = gl::Shader(romfs::get("shaders/default/vertex.glsl").string(),
                                                      romfs::get("shaders/default/fragment.glsl").string());
                shader.bind();

                shader.setUniform<4>("ScaledModel", scaledModel);
                shader.setUniform<4>("Model", model);
                shader.setUniform<4>("View", view);
                shader.setUniform<4>("Projection", projection);
                shader.setUniform<3>("LightPosition", s_lightPosition);
                shader.setUniform<4>("Strength", s_strength);

                vertexArray.bind();
                if (s_texturePath != s_texturePathOld)
                    s_modelTexture = ImGuiExt::Texture(s_texturePath);
                if (s_drawTexture)
                    glBindTexture(GL_TEXTURE_2D, s_modelTexture);

                if (s_indexType == IndexType::U8) {

                    buffers.indices8.bind();
                    if (buffers.indices8.getSize() == 0)
                        buffers.vertices.draw(s_drawMode);
                    else
                        buffers.indices8.draw(s_drawMode);
                    buffers.indices8.unbind();

                } else if (s_indexType == IndexType::U16) {

                    buffers.indices16.bind();
                    if (buffers.indices16.getSize() == 0)
                        buffers.vertices.draw(s_drawMode);
                    else
                        buffers.indices16.draw(s_drawMode);
                    buffers.indices16.unbind();
                } else {

                    buffers.indices32.bind();
                    if (buffers.indices32.getSize() == 0)
                        buffers.vertices.draw(s_drawMode);
                    else
                        buffers.indices32.draw(s_drawMode);
                    buffers.indices32.unbind();

                }
            } else {
                static gl::Shader lineShader = gl::Shader(
                        romfs::get("shaders/default/lineVertex.glsl").string(),
                        romfs::get("shaders/default/lineFragment.glsl").string());
                lineShader.bind();
                lineShader.setUniform<4>("Model", scaledModel);
                lineShader.setUniform<4>("View", view);
                lineShader.setUniform<4>("Projection", projection);
                vertexArray.bind();
                if (s_indexType == IndexType::U8) {
                    lineBuffers.indices8.bind();
                    if (lineBuffers.indices8.getSize() == 0)
                        lineBuffers.vertices.draw(s_drawMode);
                    else
                        lineBuffers.indices8.draw(s_drawMode);
                    lineBuffers.indices8.unbind();

                } else if (s_indexType == IndexType::U16) {
                    lineBuffers.indices16.bind();
                    if (lineBuffers.indices16.getSize() == 0)
                        lineBuffers.vertices.draw(s_drawMode);
                    else
                        lineBuffers.indices16.draw(s_drawMode);
                    lineBuffers.indices16.unbind();
                } else {
                    lineBuffers.indices32.bind();
                    if (lineBuffers.indices32.getSize() == 0)
                        lineBuffers.vertices.draw(s_drawMode);
                    else
                        lineBuffers.indices32.draw(s_drawMode);
                    lineBuffers.indices32.unbind();
                }
            }

            if (s_drawGrid || s_drawAxes) {
                static gl::Shader gridAxesShader = gl::Shader(
                        romfs::get("shaders/default/lineVertex.glsl").string(),
                        romfs::get("shaders/default/lineFragment.glsl").string());
                gridAxesShader.bind();

                gridAxesShader.setUniform<4>("Model", model);
                gridAxesShader.setUniform<4>("View", view);
                gridAxesShader.setUniform<4>("Projection", projection);

                if (s_drawGrid) {
                    gridVertexArray.bind();
                    gridBuffers.getIndices().bind();
                    gridBuffers.getIndices().draw(GL_LINES);
                    gridBuffers.getIndices().unbind();
                    gridVertexArray.unbind();
                }

                if (s_drawAxes) {
                    axesVertexArray.bind();
                    axesBuffers.getIndices().bind();
                    axesBuffers.getIndices().draw(GL_LINES);
                    axesBuffers.getIndices().unbind();
                    axesVertexArray.unbind();
                }
                gridAxesShader.unbind();
            }
            if (s_drawSource) {
                static gl::Shader sourceShader = gl::Shader(
                        romfs::get("shaders/default/lineVertex.glsl").string(),
                        romfs::get("shaders/default/lineFragment.glsl").string());
                sourceShader.bind();

                sourceShader.setUniform<4>("Model", model);
                sourceShader.setUniform<4>("View", view);
                sourceShader.setUniform<4>("Projection", projection);

                sourceVertexArray.bind();
                sourceBuffers.getIndices().bind();
                sourceBuffers.getIndices().draw(GL_TRIANGLES);
                sourceBuffers.getIndices().unbind();
                sourceVertexArray.unbind();
                sourceShader.unbind();
            }

            if (s_animationOn) {
                static float radius;
                static float initialAngle;
                double currentTime = glfwGetTime() - s_initialTime;

                if (s_previousTime == 0) {
                    radius = std::sqrt(
                            s_lightPosition[0] * s_lightPosition[0] + s_lightPosition[2] * s_lightPosition[2]);
                    initialAngle = std::atan2(s_lightPosition[2], s_lightPosition[0]);
                }

                float unitConvert = s_rpms * std::numbers::pi_v<float> / 30.0F;
                float angle = std::fmod(initialAngle + unitConvert * currentTime, 2 * std::numbers::pi_v<float>);

                s_lightPosition[0] = radius * std::cos(angle);
                s_lightPosition[2] = radius * std::sin(angle);

                s_shouldUpdateSource = true;
                s_previousTime = currentTime;
            }

            vertexArray.unbind();
            frameBuffer.unbind();

            s_texture = ImGuiExt::Texture(renderTexture.release(), GLsizei(renderTexture.getWidth()), GLsizei(renderTexture.getHeight()));

            drawWindow(s_texture, s_renderingWindowSize, mvp);
        }
    }

}