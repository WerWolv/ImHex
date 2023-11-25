#include <hex/helpers/utils.hpp>

#include <content/pl_visualizers/visualizer_helpers.hpp>

#include <fonts/codicons_font.h>
#include <fonts/fontawesome_font.h>
#include <fonts/blendericons_font.h>

#include <imgui.h>
#include <hex/helpers/opengl.hpp>
#include <opengl_support.h>
#include <GLFW/glfw3.h>

#include <hex/ui/imgui_imhex_extensions.h>
#include <hex/api/imhex_api.hpp>
#include <hex/api/localization_manager.hpp>

#include <romfs/romfs.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace hex::plugin::builtin {

    void draw3DVisualizer(pl::ptrn::Pattern &, pl::ptrn::IIterable &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
        std::shared_ptr<pl::ptrn::Pattern> verticesPattern = arguments[0].toPattern();
        std::shared_ptr<pl::ptrn::Pattern> indicesPattern = arguments[1].toPattern();
        std::shared_ptr<pl::ptrn::Pattern> normalsPattern = nullptr;
        std::shared_ptr<pl::ptrn::Pattern> colorsPattern = nullptr;
        std::shared_ptr<pl::ptrn::Pattern> uvPattern1 = nullptr;

        std::string textureFile = "";
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

        static bool shouldResetLocally = shouldReset;
        if (shouldReset)
            shouldResetLocally = true;

        const auto fontSize = ImGui::GetFontSize();
        const auto framePad = ImGui::GetStyle().FramePadding;
        float minSize = fontSize * 8_scaled + framePad.x * 20_scaled;
        minSize = minSize > 200_scaled ? minSize : 200_scaled;
        const ImVec2 UISize = {(fontSize / 2.0F) * 9_scaled + framePad.x * 6_scaled,
                               ImGui::GetFrameHeightWithSpacing() * 12_scaled + framePad.y * 4_scaled - 6_scaled};
        static ImVec2 renderingWindowSize = {minSize, minSize};

        static int drawMode = GL_TRIANGLES;
        static float nearLimit = 0.9f;
        static float farLimit = 100.0f;
        static float scaling = 1.0F;
        static float max;
        static float rpms = 10;
        static float previousTime;
        static float initialTime;

        static bool showUI = false;
        static bool isPerspective = true;
        static bool drawAxes = true;
        static bool drawGrid = true;
        static bool animationOn = false;
        static bool drawSource = true;
        static bool drawTexture = false;
        static bool resetEverything = false;
        static bool shouldUpdateSource = true;

        static gl::Vector<float, 3> translation = {{0.0f, 0.0F, -3.0F}};
        static gl::Vector<float, 3> rotation = {{0.0F, 0.0F, 0.0F}};
        static gl::Vector<float, 3> lightPosition = {{-0.7F, 0.0F, 0.0F}};
        static gl::Vector<float, 4> strength = {{0.5F, 0.5F, 0.5F, 32}};
        static gl::Matrix<float, 4, 4> rotate = gl::Matrix<float, 4, 4>::identity();

        static ImGuiExt::Texture texture;
        static char texturePath[256] = "";
        if (textureFile != "")
            std::strcpy(texturePath, textureFile.c_str());
        else
            drawTexture = false;
        static unsigned int ogltexture;
        static std::string texturePathStrOld;



        if (renderingWindowSize.x < minSize)
            renderingWindowSize.x = minSize;
        if (renderingWindowSize.y < minSize)
            renderingWindowSize.y = minSize;

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

        const static auto IndicesForLines = [](auto &vertexIndices) {
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
        };


        const static auto BoundingBox = [](auto vertices, auto &max) {
            const float kInfinity = std::numeric_limits<float>::max();

            gl::Vector<float, 4> minWorld(kInfinity), maxWorld(-kInfinity);
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

            gl::Vector<float, 4> minCamera, maxCamera;

            maxCamera = maxWorld;
            if (maxCamera[3] != 0)
                maxCamera = maxCamera * (1.0f / maxCamera[3]);

            minCamera = minWorld;
            if (minCamera[3] != 0)
                minCamera = minCamera * (1.0f / minCamera[3]);

            float maxx = std::max(std::fabs(minCamera[0]), std::fabs(maxCamera[0]));
            float maxy = std::max(std::fabs(minCamera[1]), std::fabs(maxCamera[1]));
            max = std::max(maxx, maxy);
        };

        const static auto SetDefaultColors = [](auto &colors, auto size, int color) {
            colors.resize(size / 3 * 4);

            float red = (color & 0xFF) / 255.0F;
            float green = ((color >> 8) & 0xFF) / 255.0F;
            float blue = ((color >> 16) & 0xFF) / 255.0F;
            float alpha = ((color >> 24) & 0xFF) / 255.0F;

            for (u32 i = 0; i < colors.size(); i += 4) {
                colors[i] = red;
                colors[i + 1] = green;
                colors[i + 2] = blue;
                colors[i + 3] = alpha;
            }
        };

        const static auto SetNormals = [](auto vertices, auto &normals) {
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
        };

        const static auto SetNormalsWithIndices = [](auto vertices, auto &normals, auto indices) {

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
        };

        const static auto LoadTexture = [](char *texturePath, auto &ogltexture, auto &drawTexture) {

            int width, height, nrChannels;
            unsigned char *data = nullptr;

            std::string texturePathStr = texturePath;
            if (texturePathStr != texturePathStrOld) {
                shouldResetLocally = true;
                glGenTextures(1, &ogltexture);
                glBindTexture(GL_TEXTURE_2D, ogltexture);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                data = stbi_load(texturePath, &width, &height, &nrChannels, 0);

                if (data) {
                    if (nrChannels == 3)
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
                    else if (nrChannels == 4)
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
                    else
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

                    glGenerateMipmap(GL_TEXTURE_2D);

                    stbi_image_free(data);
                    drawTexture = true;
                } else
                    drawTexture = false;
                texturePathStrOld = texturePathStr;
            }
        };

        const static auto LoadVectors = [](auto &vectors, auto indexType) {
            BoundingBox(vectors.vertices, max);

            if (drawTexture)
                SetDefaultColors(vectors.colors, vectors.vertices.size(), 0x00000000);
            else if (vectors.colors.empty())
                SetDefaultColors(vectors.colors, vectors.vertices.size(), 0xFF337FFF);

            if (vectors.normals.empty()) {
                vectors.normals.resize(vectors.vertices.size());

                if ((indexType == IndexType::U8 && vectors.indices8.empty()) || (indexType == IndexType::Invalid) ||
                    (indexType == IndexType::U16 && vectors.indices16.empty()) ||
                    (indexType == IndexType::U32 && vectors.indices32.empty())) {

                    SetNormals(vectors.vertices, vectors.normals);

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
                    SetNormalsWithIndices(vectors.vertices, vectors.normals, indices);
                }
            }
        };

        const static auto LoadLineVectors = [](auto &lineVectors, auto indexType) {

            BoundingBox(lineVectors.vertices, max);

            if (lineVectors.colors.empty())
                SetDefaultColors(lineVectors.colors, lineVectors.vertices.size(), 0xFF337FFF);

            std::vector<u32> indices;
            if (indexType == IndexType::U16)
                IndicesForLines(lineVectors.indices16);
            else if (indexType == IndexType::U8)
                IndicesForLines(lineVectors.indices8);
            else
                IndicesForLines(lineVectors.indices32);
        };

        const static auto ProcessKeyEvent = [](ImGuiKey key, auto &variable, auto incr, auto accel ){
            if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(key))) {
                auto temp = variable + incr * accel;
                if (variable * temp < 0.0F)
                    variable = 0.0F;
                else
                    variable = temp;
            }
        };

        const static auto ProcessInputEvents = [](auto &rotation, auto &translation, auto &scaling,
                                                  auto &nearLimit, auto &farLimit) {

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

            ProcessKeyEvent(ImGuiKey_Keypad4, translation[0], -0.1F, accel);
            ProcessKeyEvent(ImGuiKey_Keypad6, translation[0], 0.1F, accel);
            ProcessKeyEvent(ImGuiKey_Keypad8, translation[1], 0.1F, accel);
            ProcessKeyEvent(ImGuiKey_Keypad2, translation[1], -0.1F, accel);
            ProcessKeyEvent(ImGuiKey_Keypad1, translation[2], 0.1F, accel);
            ProcessKeyEvent(ImGuiKey_Keypad7, translation[2], -0.1F, accel);
            ProcessKeyEvent(ImGuiKey_Keypad9, nearLimit, -0.01F, accel);
            ProcessKeyEvent(ImGuiKey_Keypad3, nearLimit, 0.01F, accel);
            if (ImHexApi::System::isDebugBuild()) {
                ProcessKeyEvent(ImGuiKey_KeypadDivide, farLimit, -1.0F, accel);
                ProcessKeyEvent(ImGuiKey_KeypadMultiply, farLimit, 1.0F, accel);
            }
            ProcessKeyEvent(ImGuiKey_KeypadAdd, rotation[2], -0.075F, accel);
            ProcessKeyEvent(ImGuiKey_KeypadSubtract, rotation[2], 0.075F, accel);
            rotation[2] = std::fmod(rotation[2], 2 * std::numbers::pi);
        };


        const static auto BindBuffers = [](auto &buffers, auto &vertexArray, auto vectors, auto indexType) {
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

        };

        const static auto BindLineBuffers = [](auto &lineBuffers, auto &vertexArray, auto lineVectors, auto indexType) {
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

        };

        const static auto StyledToolTip = [](std::string tip, bool isSeparator = false) {
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
        };

        const static auto DrawNearPlaneUI = [](float availableWidth, auto &nearLimit, auto &farLimit) {
            if (std::fabs(nearLimit) < 1e-6)
                nearLimit = 0.0;

            auto label = "hex.builtin.pl_visualizer.3d.nearClipPlane"_lang.operator std::string() + ":";
            label += fmt::format("{:.3}", nearLimit);

            if (ImHexApi::System::isDebugBuild())
                label +=  fmt::format(", {:.3}", farLimit);

            ImGui::PushItemWidth(availableWidth);
            ImGui::TextUnformatted(label.c_str());
            if (ImGui::IsItemHovered()) {
                std::string tip = "hex.builtin.pl_visualizer.3d.nearlimit"_lang.operator std::string();
                StyledToolTip(tip);
            }
            ImGui::PopItemWidth();
        };

        const static auto DrawTranslateUI = [](auto availableWidth, auto &translation) {

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
                std::string tip = "hex.builtin.pl_visualizer.3d.translation"_lang.operator std::string();
                StyledToolTip(tip,true);
                tip = "hex.builtin.pl_visualizer.3d.translationMouse"_lang.operator std::string();
                tip += "\n";
                tip += "hex.builtin.pl_visualizer.3d.translationKeyboard"_lang.operator std::string();
                StyledToolTip(tip);
            }
            ImGui::PopItemWidth();
        };

        static float minScaling = 0.01F;
        static float maxScaling = 10.0F;

        const static auto DrawRotationsScaleUI = [](auto availableWidth_1, auto availableWidth_2, auto &rotation, auto &rotate,
                                                    auto &scaling, auto &strength, auto &lightPosition,
                                                    auto &renderingWindowSize, auto minSize, auto &resetEverything,
                                                    auto &translation, auto &nearLimit, auto &farLimit) {

            float fontSize = ImHexApi::System::getFontSize();
            float sliderWidth = std::floor(fontSize / 2.0f) * 3.0f;
            ImVec2 size(sliderWidth, renderingWindowSize.y);
            std::string units = "hex.builtin.pl_visualizer.3d.degrees"_lang.operator std::string();

            ImGuiExt::VSliderAngle("##X", size, &rotation.data()[1], 0, 360, "X", ImGuiSliderFlags_AlwaysClamp);
            std::string temp = fmt::format("{:.3} ", rotation[1] * 180 * std::numbers::inv_pi);
            temp += units;

            if (ImGui::IsItemHovered())
                StyledToolTip(temp);

            ImGui::SameLine();

            ImGuiExt::VSliderAngle("##Y", size, &rotation.data()[0], 0, 360, "Y", ImGuiSliderFlags_AlwaysClamp);
            temp = fmt::format("{:.3} ", rotation[0] * 180 * std::numbers::inv_pi);
            temp += units;

            if (ImGui::IsItemHovered())
                StyledToolTip(temp);

            ImGui::SameLine();

            ImGuiExt::VSliderAngle("##Z", size, &rotation.data()[2], 0, 360, "Z", ImGuiSliderFlags_AlwaysClamp);
            temp = fmt::format("{:.3} ", rotation[2] * 180 * std::numbers::inv_pi);
            temp += units;

            if (ImGui::IsItemHovered())
                StyledToolTip(temp);

            for (u8 i = 0; i < 3; i++) {
                i32 winding = 0;
                if (rotation.data()[i] > std::numbers::pi * 2)
                    winding = std::floor(rotation.data()[i] / std::numbers::pi / 2.0);
                else if (rotation.data()[i] < 0)
                    winding = std::floor(rotation.data()[i] / std::numbers::pi / 2.0);

                rotation.data()[i] -= std::numbers::pi * winding * 2.0;
                if (rotation.data()[i] < 0.001)
                    rotation.data()[i] = 0.0F;
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            std::string label = "hex.builtin.pl_visualizer.3d.scale"_lang.operator std::string();
            label += ": %.3f";

            ImGui::PushItemWidth(availableWidth_1);

            if (scaling < minScaling)
                minScaling = scaling;
            if (scaling > maxScaling)
                maxScaling = scaling;

            ImGui::SliderFloat("##Scale", &scaling, minScaling, maxScaling, label.c_str());
            ImGui::PopItemWidth();

            ImGui::TableNextColumn();

            label = "hex.builtin.common.reset"_lang.operator std::string();
            auto labelWidth = ImGui::CalcTextSize(label.c_str()).x;
            auto spacing = (availableWidth_2 - labelWidth) / 2.0f;

            ImGui::Indent(spacing);

            ImGui::TextUnformatted(label.c_str());
            if (ImGui::IsItemHovered()) {
                std::string tip = "hex.builtin.pl_visualizer.3d.reset_description"_lang.operator std::string();
                StyledToolTip(tip);
            }
            ImGui::Unindent(spacing);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            DrawTranslateUI(availableWidth_1, translation);

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
                std::string tip = "hex.builtin.pl_visualizer.3d.reset_usage"_lang.operator std::string();
                StyledToolTip(tip);
            }
            ImGui::Unindent(spacing);
        };

        const static auto DrawLightPositionUI = [](float availableWidth, auto &lightPosition, auto &shouldUpdateSource,
                                                   auto &resetEverything) {
            if (ImGui::BeginTable("##LightPosition", 1, ImGuiTableFlags_SizingFixedFit)) {

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                std::string label = "hex.builtin.pl_visualizer.3d.lightPosition"_lang.operator std::string();
                auto labelWidth = ImGui::CalcTextSize(label.c_str()).x;

                auto indent = (availableWidth - labelWidth) / 2.0f;
                ImGui::Indent(indent);

                ImGuiExt::TextFormatted(label);
                if (ImGui::IsItemHovered()) {
                    std::string tip = "hex.builtin.pl_visualizer.3d.lightPosition_description"_lang.operator std::string();
                    StyledToolTip(tip);
                }
                ImGui::Unindent(indent);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::PushItemWidth(availableWidth);
                if (ImGui::InputFloat3("##lightDir", lightPosition.data(), "%.3f",
                                       ImGuiInputTextFlags_EnterReturnsTrue) || resetEverything) {
                    shouldUpdateSource = true;
                    previousTime = 0;
                }
                if (ImGui::IsItemHovered()) {
                    std::string tip = "hex.builtin.pl_visualizer.3d.lightPosition_usage"_lang.operator std::string();
                    StyledToolTip(tip);
                }
                ImGui::PopItemWidth();
            }
            ImGui::EndTable();
        };

        const static auto DrawProjectionUI = [](auto availableWidth, auto &isPerspective, auto &shouldResetLocally,
                                                auto &resetEverything) {
            if (ImGui::BeginTable("##Proj", 1, ImGuiTableFlags_SizingFixedFit)) {

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                static auto projectionIcon = ICON_BI_VIEW_PERSPECTIVE;
                auto label = "hex.builtin.pl_visualizer.3d.projection"_lang.operator std::string();
                auto labelWidth = ImGui::CalcTextSize(label.c_str()).x;

                float spacing = (availableWidth - labelWidth) / 2.0f;
                ImGui::Indent(spacing);

                ImGui::TextUnformatted(label.c_str());
                if (ImGui::IsItemHovered()) {
                    std::string tip;
                    if (isPerspective)
                        tip = "hex.builtin.pl_visualizer.3d.perspectiveSelected"_lang.operator std::string();
                    else
                        tip = "hex.builtin.pl_visualizer.3d.orthographicSelected"_lang.operator std::string();
                    StyledToolTip(tip);
                };
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
                        tip = "hex.builtin.pl_visualizer.3d.selectOrthographic"_lang.operator std::string();
                    else
                        tip = "hex.builtin.pl_visualizer.3d.selectPerspective"_lang.operator std::string();
                    StyledToolTip(tip);
                }
                ImGui::Unindent(spacing);
            }
            ImGui::EndTable();
        };

        const static auto DrawPrimitiveUI = [](auto availableWidth, auto &drawMode, auto &shouldResetLocally,
                                               auto &resetEverything) {
            if (ImGui::BeginTable("##Prim", 1, ImGuiTableFlags_SizingFixedFit)) {

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                auto label = "hex.builtin.pl_visualizer.3d.primitive"_lang.operator std::string();
                auto labelWidth = ImGui::CalcTextSize(label.c_str()).x;

                float spacing = (availableWidth - labelWidth) / 2.0f;
                ImGui::Indent(spacing);

                ImGui::TextUnformatted(label.c_str());
                if (ImGui::IsItemHovered()) {
                    std::string tip;
                    if (drawMode == GL_TRIANGLES)
                        tip = "hex.builtin.pl_visualizer.3d.trianglesSelected"_lang.operator std::string();
                    else
                        tip = "hex.builtin.pl_visualizer.3d.linesSelected"_lang.operator std::string();
                    StyledToolTip(tip);
                }
                ImGui::Unindent(spacing);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                static auto primitiveIcon = ICON_BI_MOD_SOLIDIFY;
                auto buttonSize = ImGui::CalcTextSize(primitiveIcon) + ImGui::GetStyle().FramePadding * 2;
                spacing = (availableWidth - buttonSize.x) / 2.0f;

                ImGui::Indent(spacing);
                bool dummy = true;
                if (ImGuiExt::DimmedIconToggle(primitiveIcon, &dummy) || resetEverything) {
                    if (drawMode == GL_LINES || resetEverything) {
                        primitiveIcon = ICON_BI_MOD_SOLIDIFY;
                        drawMode = GL_TRIANGLES;
                    } else {
                        primitiveIcon = ICON_BI_CUBE;
                        drawMode = GL_LINES;
                    }
                    shouldResetLocally = true;
                }
                if (ImGui::IsItemHovered()) {
                    std::string tip;
                    if (drawMode == GL_TRIANGLES)
                        tip = "hex.builtin.pl_visualizer.3d.line"_lang.operator std::string();
                    else
                        tip = "hex.builtin.pl_visualizer.3d.triangle"_lang.operator std::string();
                    StyledToolTip(tip);
                }
                ImGui::Unindent(spacing);
            }
            ImGui::EndTable();
        };

        const static auto DrawLightPropertiesUI = [](float  availableWidth, auto &strength) {
            if (ImGui::BeginTable("##LightProperties", 1, ImGuiTableFlags_SizingFixedFit)) {

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                auto label = "hex.builtin.pl_visualizer.3d.lightProperties"_lang.operator std::string();
                auto labelWidth = ImGui::CalcTextSize(label.c_str()).x;

                auto indent = (availableWidth - labelWidth) / 2.0f;
                ImGui::Indent(indent);

                ImGuiExt::TextFormatted(label.c_str());
                ImGui::Unindent(indent);
                ImGui::SameLine();

                float labelHeight = ImGui::CalcTextSize("Any text").y;
                auto framePadding = ImGui::GetStyle().FramePadding.x;
                auto bSize = ImVec2({((availableWidth) / 4.0f) - framePadding* 2, labelHeight});

                ImGui::Indent(framePadding);

                ImGui::InvisibleButton("##ambient", bSize);
                if (ImGui::IsItemHovered()) {
                    std::string tip = "hex.builtin.pl_visualizer.3d.ambientIntensity"_lang.operator std::string();
                    StyledToolTip(tip);
                }
                ImGui::Unindent(framePadding);

                ImGui::SameLine();

                indent = bSize.x + framePadding;
                ImGui::Indent(indent);

                ImGui::InvisibleButton("##diffuse", bSize);
                if (ImGui::IsItemHovered()) {
                    std::string tip = "hex.builtin.pl_visualizer.3d.diffuseIntensity"_lang.operator std::string();
                    StyledToolTip(tip);
                }
                ImGui::Unindent(indent);

                ImGui::SameLine();

                indent = 2 * bSize.x + framePadding;
                ImGui::Indent(indent);

                ImGui::InvisibleButton("##specular", bSize);
                if (ImGui::IsItemHovered()) {
                    std::string tip = "hex.builtin.pl_visualizer.3d.specularIntensity"_lang.operator std::string();
                    StyledToolTip(tip);
                }
                ImGui::Unindent(indent);

                ImGui::SameLine();

                indent = 3 * bSize.x + framePadding;
                ImGui::Indent(indent);

                ImGui::InvisibleButton("##shininess", bSize);
                if (ImGui::IsItemHovered()) {
                    std::string tip = "hex.builtin.pl_visualizer.3d.shininessIntensity"_lang.operator std::string();
                    StyledToolTip(tip);
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
                    std::string tip = "hex.builtin.pl_visualizer.3d.ambient"_lang.operator std::string();
                    StyledToolTip(tip);
                }
                ImGui::Unindent(0);
                ImGui::SameLine();

                ImGui::Indent(bSize.x);
                ImGui::InvisibleButton("##diffuse2", bSize);
                if (ImGui::IsItemHovered()) {
                    std::string tip = "hex.builtin.pl_visualizer.3d.diffuse"_lang.operator std::string();
                    StyledToolTip(tip);
                }
                ImGui::Unindent(bSize.x);
                ImGui::SameLine();

                ImGui::Indent(2 * bSize.x);
                ImGui::InvisibleButton("##specular2", bSize);
                if (ImGui::IsItemHovered()) {
                    std::string tip = "hex.builtin.pl_visualizer.3d.specular"_lang.operator std::string();
                    StyledToolTip(tip);
                }

                ImGui::Unindent(2 * bSize.x);
                ImGui::SameLine();

                ImGui::Indent(3 * bSize.x);
                ImGui::InvisibleButton("##shininess2", bSize);
                if (ImGui::IsItemHovered()) {
                    std::string tip = "hex.builtin.pl_visualizer.3d.shininess"_lang.operator std::string();
                    StyledToolTip(tip);
                }
                ImGui::Unindent(3 * bSize.x);
            }
            ImGui::EndTable();
        };

        const static auto DrawTextureUI = [](float availableWidth, auto texturePath) {
            if (ImGui::BeginTable("##Texture", 1, ImGuiTableFlags_SizingFixedFit)) {

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                auto label = "hex.builtin.pl_visualizer.3d.textureFile"_lang.operator std::string();
                auto labelWidth = ImGui::CalcTextSize(label.c_str()).x;

                auto indent = (availableWidth - labelWidth) / 2.0f;
                ImGui::Indent(indent);

                ImGuiExt::TextFormatted(label.c_str());
                if (ImGui::IsItemHovered()) {
                    std::string tip = "hex.builtin.pl_visualizer.3d.textureFile"_lang.operator std::string();
                    StyledToolTip(tip);
                }
                ImGui::Unindent(indent);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::PushItemWidth(availableWidth);
                ImGui::InputText("##Texture", texturePath, 256, ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::PopItemWidth();
            }
            ImGui::EndTable();
        };

        const static auto DrawAnimationSpeedUI = [](float availableWidth, auto &rpms) {
            if (ImGui::BeginTable("##AnimationSpeed", 1, ImGuiTableFlags_SizingFixedFit)) {

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                auto label = "hex.builtin.pl_visualizer.3d.animationSpeed"_lang.operator std::string();
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
        };

        const static auto DrawLightUI = [](float availableWidth, auto &drawSource, auto &resetEverything) {
            if (ImGui::BeginTable("##ligth", 1, ImGuiTableFlags_SizingFixedFit)) {

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                auto label = "hex.builtin.pl_visualizer.3d.light"_lang.operator std::string();
                auto labelWidth = ImGui::CalcTextSize(label.c_str()).x;

                auto spacing = (availableWidth - labelWidth) / 2.0f;
                ImGui::Indent(spacing);

                ImGui::TextUnformatted(label.c_str());
                if (ImGui::IsItemHovered()) {
                    std::string tip;
                    if (drawSource)
                        tip = "hex.builtin.pl_visualizer.3d.renderingLightSource"_lang.operator std::string();
                    else
                        tip = "hex.builtin.pl_visualizer.3d.notRenderingLightSource"_lang.operator std::string();
                    StyledToolTip(tip);
                }
                ImGui::Unindent(spacing);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                static auto sourceIcon = ICON_VS_LIGHTBULB;
                auto buttonSize = ImGui::CalcTextSize(sourceIcon) + ImGui::GetStyle().FramePadding * 2;

                spacing = (availableWidth - buttonSize.x) / 2.0f;
                ImGui::Indent(spacing);

                if (ImGuiExt::DimmedIconToggle(sourceIcon, &drawSource) || resetEverything) {
                    if (resetEverything)
                        drawSource = true;
                }
                if (ImGui::IsItemHovered()) {
                    std::string tip;
                    if (drawSource)
                        tip = "hex.builtin.pl_visualizer.3d.dontDrawSource"_lang.operator std::string();
                    else
                        tip = "hex.builtin.pl_visualizer.3d.drawSource"_lang.operator std::string();
                    StyledToolTip(tip);
                }
                ImGui::Unindent(spacing);
            }
            ImGui::EndTable();
        };

        const static auto DrawAnimateUI = [](float availableWidth, auto &animationOn, auto &previousTime,
                                             auto &initialTime, auto &resetEverything) {
            if (ImGui::BeginTable("##Animate", 1, ImGuiTableFlags_SizingFixedFit)) {

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                auto label = "hex.builtin.pl_visualizer.3d.animation"_lang.operator std::string();
                auto labelWidth = ImGui::CalcTextSize(label.c_str()).x;

                auto spacing = (availableWidth - labelWidth) / 2.0f;
                ImGui::Indent(spacing);

                ImGui::TextUnformatted(label.c_str());
                if (ImGui::IsItemHovered()) {
                    std::string tip;
                    if (animationOn)
                        tip = "hex.builtin.pl_visualizer.3d.animationIsRunning"_lang.operator std::string();
                    else
                        tip = "hex.builtin.pl_visualizer.3d.animationIsNotRunning"_lang.operator std::string();
                    StyledToolTip(tip);
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
                        tip = "hex.builtin.pl_visualizer.3d.dontAnimate"_lang.operator std::string();
                    else
                        tip = "hex.builtin.pl_visualizer.3d.animate"_lang.operator std::string();
                    StyledToolTip(tip);
                }
                ImGui::Unindent(spacing);
            }
            ImGui::EndTable();
        };

        const static auto DrawWindow = [](auto &texture, auto &renderingWindowSize, auto mvp, auto minSize, auto& showUI, auto UISize) {
            auto textureSize = texture.getSize();
            auto textureWidth = textureSize.x;
            auto textureHeight = textureSize.y;
            float firstColumnWidth = textureWidth;
            float secondColumnWidth = UISize.x;
            int columnCount;
            auto maxSize = ImGui::GetPlatformIO().Monitors[0].MainSize;

            if (showUI) {
                maxSize.x -= (secondColumnWidth + 40);
                maxSize.y -= (UISize.y + 40);

                if (renderingWindowSize.x > maxSize.x)
                    renderingWindowSize.x = maxSize.x;
                if (renderingWindowSize.y > maxSize.y)
                    renderingWindowSize.y = maxSize.y;
                columnCount = 2;
            } else
                columnCount = 1;



            if (ImGui::BeginTable("##3DVisualizer", columnCount, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX)) {
                constexpr static auto ColumnFlags =  ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoResize;
                float column2Width;
                ImGui::TableSetupColumn("##FirstColumn", ColumnFlags, textureWidth);

                if (showUI) {
                    column2Width = secondColumnWidth;
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
                        tip = "hex.builtin.pl_visualizer.3d.hideUI"_lang.operator std::string();
                    else
                        tip = "hex.builtin.pl_visualizer.3d.showUI"_lang.operator std::string();
                    StyledToolTip(tip);
                }

                ImGui::SameLine();
                ImGui::TextUnformatted("hex.builtin.pl_visualizer.3d.size"_lang);

                if (ImGui::IsItemHovered()) {
                    std::string tip = "hex.builtin.pl_visualizer.3d.size_usage"_lang.operator std::string();
                    StyledToolTip(tip);
                }

                ImGui::SameLine();

                auto labelWidth = ImGui::CalcTextSize("1000").x;
                ImGui::PushItemWidth(labelWidth);

                ImGui::DragFloat("##WinSizex", &renderingWindowSize.x, 1.0f, minSize, maxSize.x);

                if (ImGui::IsItemHovered()) {
                    std::string tip = "hex.builtin.pl_visualizer.3d.size_units"_lang.operator std::string();
                    StyledToolTip(tip);
                }
                ImGui::PopItemWidth();

                ImGui::SameLine();

                ImGui::PushItemWidth(labelWidth);
                ImGui::DragFloat("##WinSizey", &renderingWindowSize.y, 1.0f, minSize, maxSize.y);

                if (ImGui::IsItemHovered()) {
                    std::string tip = "hex.builtin.pl_visualizer.3d.size_units"_lang.operator std::string();
                    StyledToolTip(tip);
                }
                ImGui::PopItemWidth();

                ImGui::SameLine();

                static auto axesIcon = ICON_BI_EMPTY_ARROWS;

                if (ImGuiExt::DimmedIconToggle(axesIcon, &drawAxes))
                    shouldResetLocally = true;
                if (resetEverything) {
                    drawAxes = true;
                    shouldResetLocally = true;
                }

                if (ImGui::IsItemHovered()) {
                    std::string tip;
                    if (drawAxes)
                        tip = "hex.builtin.pl_visualizer.3d.dontDrawAxes"_lang.operator std::string();
                    else
                        tip = "hex.builtin.pl_visualizer.3d.drawAxes"_lang.operator std::string();
                    StyledToolTip(tip);
                }

                ImGui::SameLine();

                static auto gridIcon = ICON_BI_GRID;
                if (isPerspective)
                    gridIcon = ICON_BI_GRID;
                else
                    gridIcon = ICON_VS_SYMBOL_NUMBER;
                if (ImGuiExt::DimmedIconToggle(gridIcon, &drawGrid))
                    shouldResetLocally = true;

                if (resetEverything) {
                    drawGrid = true;
                    shouldResetLocally = true;
                }

                if (ImGui::IsItemHovered()) {
                    std::string tip;
                    if (drawGrid)
                        tip = "hex.builtin.pl_visualizer.3d.hideGrid"_lang.operator std::string();
                    else
                        tip = "hex.builtin.pl_visualizer.3d.renderGrid"_lang.operator std::string();
                    StyledToolTip(tip);
                }

                if (showUI) {
                    ImGui::TableNextColumn();
                    auto textSize = ImGui::CalcTextSize("hex.builtin.pl_visualizer.3d.rotation"_lang);

                    float indent = (secondColumnWidth - textSize.x) / 2.0f;
                    ImGui::Indent(indent);

                    ImGui::TextUnformatted("hex.builtin.pl_visualizer.3d.rotation"_lang);
                    ImGui::Unindent(indent);

                    if (ImGui::IsItemHovered()) {
                        std::string tip = "hex.builtin.pl_visualizer.3d.rotation_usage"_lang.operator std::string();
                        StyledToolTip(tip);
                    }
                }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImVec2 screenPos = ImGui::GetCursorScreenPos();
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

                if (ImGui::BeginChild("##image", textureSize, true,
                                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                    ImGui::Image(texture, textureSize, ImVec2(0, 1), ImVec2(1, 0));

                    if (drawAxes) {
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
                        float mouse_x = ImGui::GetMousePos().x;
                        if (std::fabs(mouse_x) >= 3.0e+38F)
                            mouse_x = 0.0F;
                        else
                            mouse_x -= screenPos.x;

                        float mouse_y = ImGui::GetMousePos().y;
                        if (std::fabs(mouse_y) >= 3.0e+38F)
                            mouse_y = 0.0F;
                        else
                            mouse_y -= screenPos.y;

                        std::string mousePos = format("x: {:.5}\ny: {:.5}", mouse_x, mouse_y);
                        ImDrawList *drawList = ImGui::GetWindowDrawList();
                        drawList->AddText(screenPos, IM_COL32(255, 255, 255, 255), mousePos.c_str());
                    }
                }

                ImGui::EndChild();
                ImGui::PopStyleVar();

                if (showUI) {

                    ImGui::TableNextColumn();

                    DrawRotationsScaleUI(firstColumnWidth, secondColumnWidth, rotation, rotate, scaling,
                                         strength, lightPosition, renderingWindowSize, minSize, resetEverything,
                                         translation, nearLimit, farLimit);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    DrawLightPositionUI(firstColumnWidth, lightPosition, shouldUpdateSource, resetEverything);

                    ImGui::TableNextColumn();

                    DrawProjectionUI(secondColumnWidth, isPerspective, shouldResetLocally, resetEverything);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    DrawLightPropertiesUI(firstColumnWidth, strength);

                    ImGui::TableNextColumn();

                    DrawLightUI(secondColumnWidth, drawSource, resetEverything);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    DrawTextureUI(firstColumnWidth, texturePath);

                    ImGui::TableNextColumn();

                    DrawPrimitiveUI(secondColumnWidth, drawMode, shouldResetLocally, resetEverything);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    DrawAnimationSpeedUI(firstColumnWidth, rpms);

                    ImGui::TableNextColumn();

                    DrawAnimateUI(secondColumnWidth, animationOn, previousTime, initialTime, resetEverything);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    DrawNearPlaneUI(firstColumnWidth, nearLimit, farLimit);
                }
            }
            ImGui::EndTable();
        };

        static IndexType indexType;
        gl::Matrix<float, 4, 4> mvp(0);

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

        ProcessInputEvents(rotation, translation, scaling, nearLimit, farLimit);

        if (shouldResetLocally) {
            shouldResetLocally = false;
            if (indicesPattern->getTypeName() == "u8" || indicesPattern->getTypeName() == "byte" ||
                indicesPattern->getTypeName() == "char" || indicesPattern->getTypeName() == "s8")

                indexType = IndexType::U8;

            else if (indicesPattern->getTypeName() == "u32" || indicesPattern->getTypeName() == "s32" ||
                     indicesPattern->getTypeName() == "unsigned")

                indexType = IndexType::U32;

            else if (indicesPattern->getTypeName() == "u16" || indicesPattern->getTypeName() == "s16" ||
                     indicesPattern->getTypeName() == "short")

                indexType = IndexType::U16;

            else
                indexType = IndexType::Invalid;

            Vectors vectors;
            LineVectors lineVectors;

            if (drawMode == GL_TRIANGLES) {
                vectors.vertices = patternToArray<float>(verticesPattern.get());
                if (indexType == IndexType::U16)
                    vectors.indices16 = patternToArray<u16>(indicesPattern.get());

                else if (indexType == IndexType::U32)
                    vectors.indices32 = patternToArray<u32>(indicesPattern.get());

                else if (indexType == IndexType::U8)
                    vectors.indices8 = patternToArray<u8>(indicesPattern.get());

                if (colorsPattern != nullptr)
                    vectors.colors = patternToArray<float>(colorsPattern.get());
                if (normalsPattern != nullptr)
                    vectors.normals = patternToArray<float>(normalsPattern.get());
                if (uvPattern1 != nullptr)
                    vectors.uv1 = patternToArray<float>(uvPattern1.get());

                LoadVectors(vectors, indexType);

                BindBuffers(buffers, vertexArray, vectors, indexType);
            } else {
                lineVectors.vertices = patternToArray<float>(verticesPattern.get());
                if (indexType == IndexType::U16)
                    lineVectors.indices16 = patternToArray<u16>(indicesPattern.get());

                else if (indexType == IndexType::U32)
                    lineVectors.indices32 = patternToArray<u32>(indicesPattern.get());

                else if (indexType == IndexType::U8)
                    lineVectors.indices8 = patternToArray<u8>(indicesPattern.get());

                if (colorsPattern != nullptr)
                    lineVectors.colors = patternToArray<float>(colorsPattern.get());

                LoadLineVectors(lineVectors, indexType);

                BindLineBuffers(lineBuffers, vertexArray, lineVectors, indexType);
            }
        }

        if (shouldUpdateSource) {
            shouldUpdateSource = false;
            sourceVectors.moveTo(lightPosition);
            sourceBuffers.moveVertices(sourceVertexArray, sourceVectors);
        }
        {
            gl::Matrix<float, 4, 4> model(0);
            gl::Matrix<float, 4, 4> scaledModel(0);
            gl::Matrix<float, 4, 4> view(0);
            gl::Matrix<float, 4, 4> projection(0);

            unsigned width = std::floor(renderingWindowSize.x);
            unsigned height = std::floor(renderingWindowSize.y);

            gl::FrameBuffer frameBuffer(width,height);
            gl::Texture renderTexture(width,height );
            frameBuffer.attachTexture(renderTexture);
            frameBuffer.bind();

            rotate = gl::getRotationMatrix<float>(rotation, true, gl::RotationSequence::ZYX);

            gl::Matrix<float, 4, 4> scale = gl::Matrix<float, 4, 4>::identity();
            gl::Matrix<float, 4, 4> scaleForVertices = gl::Matrix<float, 4, 4>::identity();
            gl::Matrix<float, 4, 4> translate = gl::Matrix<float, 4, 4>::identity();

            float totalScale;
            float viewWidth =  renderingWindowSize.x / 500.0f;
            float viewHeight = renderingWindowSize.y / 500.0f;
            glViewport(0,0 , renderTexture.getWidth(), renderTexture.getHeight());
            glDepthRangef(nearLimit, farLimit);
            glClearColor(0.00F, 0.00F, 0.00F, 0.00f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);

            if (isPerspective == 0) {
                projection = gl::GetOrthographicMatrix(viewWidth, viewHeight, nearLimit, farLimit, false);
                totalScale = scaling / (std::fabs(translation[2]));
                scale(0, 0) = totalScale;
                scale(1, 1) = totalScale;
                scale(2, 2) = totalScale;

                translate(3, 0) = translation[0] / std::fabs(translation[2]);
                translate(3, 1) = translation[1] / std::fabs(translation[2]);
                translate(3, 2) = translation[2];
            } else {
                projection = gl::GetPerspectiveMatrix(viewWidth, viewHeight, nearLimit, farLimit, false);
                totalScale = scaling;
                scale(0, 0) = totalScale;
                scale(1, 1) = totalScale;
                scale(2, 2) = totalScale;

                translate(3, 0) = translation[0];
                translate(3, 1) = translation[1];
                translate(3, 2) = translation[2];
            }
            totalScale /= (3.0f * max);
            scaleForVertices(0, 0) = totalScale;
            scaleForVertices(1, 1) = totalScale;
            scaleForVertices(2, 2) = totalScale;

            model = rotate * scale;
            scaledModel =  rotate * scaleForVertices;
            view = translate;
            mvp = model * view * projection;

            if (drawMode == GL_TRIANGLES) {
                static gl::Shader shader = gl::Shader(romfs::get("shaders/default/vertex.glsl").string(),
                                                      romfs::get("shaders/default/fragment.glsl").string());
                shader.bind();

                shader.setUniform<4>("ScaledModel", scaledModel);
                shader.setUniform<4>("Model", model);
                shader.setUniform<4>("View", view);
                shader.setUniform<4>("Projection", projection);
                shader.setUniform<3>("LightPosition", lightPosition);
                shader.setUniform<4>("Strength", strength);

                vertexArray.bind();
                LoadTexture(texturePath, ogltexture, drawTexture);
                if (drawTexture)
                    glBindTexture(GL_TEXTURE_2D, ogltexture);

                if (indexType == IndexType::U8) {

                    buffers.indices8.bind();
                    if (buffers.indices8.getSize() == 0)
                        buffers.vertices.draw(drawMode);
                    else
                        buffers.indices8.draw(drawMode);
                    buffers.indices8.unbind();

                } else if (indexType == IndexType::U16) {

                    buffers.indices16.bind();
                    if (buffers.indices16.getSize() == 0)
                        buffers.vertices.draw(drawMode);
                    else
                        buffers.indices16.draw(drawMode);
                    buffers.indices16.unbind();
                } else {

                    buffers.indices32.bind();
                    if (buffers.indices32.getSize() == 0)
                        buffers.vertices.draw(drawMode);
                    else
                        buffers.indices32.draw(drawMode);
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
                if (indexType == IndexType::U8) {

                    lineBuffers.indices8.bind();
                    if (lineBuffers.indices8.getSize() == 0)
                        lineBuffers.vertices.draw(drawMode);
                    else
                        lineBuffers.indices8.draw(drawMode);
                    lineBuffers.indices8.unbind();

                } else if (indexType == IndexType::U16) {

                    lineBuffers.indices16.bind();
                    if (lineBuffers.indices16.getSize() == 0)
                        lineBuffers.vertices.draw(drawMode);
                    else
                        lineBuffers.indices16.draw(drawMode);
                    lineBuffers.indices16.unbind();
                } else {

                    lineBuffers.indices32.bind();
                    if (lineBuffers.indices32.getSize() == 0)
                        lineBuffers.vertices.draw(drawMode);
                    else
                        lineBuffers.indices32.draw(drawMode);
                    lineBuffers.indices32.unbind();
                }
            }

            if (drawGrid || drawAxes) {
                static gl::Shader gridAxesShader = gl::Shader(
                        romfs::get("shaders/default/lineVertex.glsl").string(),
                        romfs::get("shaders/default/lineFragment.glsl").string());
                gridAxesShader.bind();

                gridAxesShader.setUniform<4>("Model", model);
                gridAxesShader.setUniform<4>("View", view);
                gridAxesShader.setUniform<4>("Projection", projection);

                if (drawGrid) {
                    gridVertexArray.bind();
                    gridBuffers.getIndices().bind();
                    gridBuffers.getIndices().draw(GL_LINES);
                    gridBuffers.getIndices().unbind();
                    gridVertexArray.unbind();
                }

                if (drawAxes) {
                    axesVertexArray.bind();
                    axesBuffers.getIndices().bind();
                    axesBuffers.getIndices().draw(GL_LINES);
                    axesBuffers.getIndices().unbind();
                    axesVertexArray.unbind();
                }
                gridAxesShader.unbind();
            }
            if (drawSource) {
                static gl::Shader sourceShader = gl::Shader(
                        romfs::get("shaders/default/lineVertex.glsl").string(),
                        romfs::get("shaders/default/lineFragment.glsl").string());
                sourceShader.bind();
                gl::Vector<float, 4> tempStrength({1.0F, 0.0F, 0.0F, 32.0F});
                sourceShader.setUniform<4>("Model", model);
                sourceShader.setUniform<4>("View", view);
                sourceShader.setUniform<4>("Projection", projection);

                sourceVertexArray.bind();
                sourceBuffers.indices.bind();
                sourceBuffers.indices.draw(GL_TRIANGLES);
                sourceBuffers.indices.unbind();
                sourceVertexArray.unbind();
                sourceShader.unbind();
            }

            if (animationOn) {
                static float radius;
                static float initialAngle;
                float currentTime = glfwGetTime() - initialTime;

                if (previousTime == 0) {
                    radius = std::sqrt(
                            lightPosition[0] * lightPosition[0] + lightPosition[2] * lightPosition[2]);
                    initialAngle = std::atan2(lightPosition[2], lightPosition[0]);
                }

                float unitConvert = rpms * std::numbers::pi / 30.0f;
                float angle = std::fmod(initialAngle + unitConvert * currentTime, 2 * std::numbers::pi);

                lightPosition[0] = radius * std::cos(angle);
                lightPosition[2] = radius * std::sin(angle);

                shouldUpdateSource = true;
                previousTime = currentTime;
            }

            vertexArray.unbind();
            frameBuffer.unbind();

            texture = ImGuiExt::Texture(renderTexture.release(), renderTexture.getWidth(),
                                     renderTexture.getHeight());

            DrawWindow(texture, renderingWindowSize, mvp, minSize, showUI, UISize);
        }
    }

}