#include <algorithm>
#include <hex/helpers/utils.hpp>

#include <content/visualizer_helpers.hpp>

#include <fonts/vscode_icons.hpp>
#include <fonts/blender_icons.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <hex/helpers/opengl.hpp>
#include <opengl_support.h>

#include <numbers>

#include <hex/ui/imgui_imhex_extensions.h>
#include <hex/api/imhex_api.hpp>
#include <hex/api/localization_manager.hpp>

#include <romfs/romfs.hpp>
#include <numeric>

namespace hex::plugin::visualizers {

    namespace {

        enum class IndexType : u8 {
            U8,
            U16,
            U32,
            Undefined,
        };

        template<class T>
        struct Vectors {
            std::vector<float> vertices;
            std::vector<float> normals;
            std::vector<float> colors;
            std::vector<float> uv;
            std::vector<T> indices;
        };

        template <class T>
        struct LineVectors {
            std::vector<float> vertices;
            std::vector<float> colors;
            std::vector<T> indices;
        };

        template<class T>
        struct Buffers {
            gl::Buffer<float> vertices;
            gl::Buffer<float> normals;
            gl::Buffer<float> colors;
            gl::Buffer<float> uv;
            gl::Buffer<T> indices;
        };

        template<class T>
        struct LineBuffers {
            gl::Buffer<float> vertices;
            gl::Buffer<float> colors;
            gl::Buffer<T> indices;
        };

        ImVec2 s_renderingWindowSize;

        int s_drawMode = GL_TRIANGLES;
        float s_nearLimit = 0.9F;
        float s_farLimit = 100.0F;
        float s_scaling = 1.0F;
        float s_max;

        bool s_isPerspective = true;
        bool s_drawAxes = true;
        bool s_drawGrid = true;
        bool s_drawLightSource = true;
        bool s_drawTexture = false;
        bool s_shouldReset = false;

        bool s_shouldUpdateLightSource = true;
        bool s_shouldUpdateTexture = false;

        std::vector<u32> s_badIndices;

        IndexType s_indexType;

        ImGuiExt::Texture s_modelTexture;

        gl::Vector<float, 3> s_translation      = { {  0.0F, 0.0F, -3.0F } };
        gl::Vector<float, 3> s_rotation         = { {  0.0F, 0.0F,  0.0F } };
        gl::Vector<float, 3> s_lightPosition    = { { -0.7F, 0.0F,  0.0F } };
        gl::Vector<float, 4> s_lightBrightness  = { {  0.5F, 0.5F,  0.5F, 32.0F } };
        gl::Vector<float, 3> s_lightColor       = { {  1.0F, 1.0F,  1.0F } };
        gl::Matrix<float, 4, 4> s_rotate        = gl::Matrix<float, 4, 4>::identity();

        ImGuiExt::Texture s_texture;
        std::fs::path s_texturePath;

        u32 s_vertexCount;

        const auto isIndexInRange = [](auto index) {
            if (index >= s_vertexCount)
                s_badIndices.push_back(index);
            return index < s_vertexCount;
        };

        template<typename T>
        void indicesForLines(std::vector<T> &vertexIndices) {
            std::vector<u32> indices;

            u32 vertexCount = vertexIndices.size() / 3;
            indices.resize(vertexCount * 6);

            for (u32 i = 0; i < vertexCount; i += 1) {
                indices[ i * 6     ] = vertexIndices[ 3 * i     ];
                indices[(i * 6) + 1] = vertexIndices[(3 * i) + 1];

                indices[(i * 6) + 2] = vertexIndices[(3 * i) + 1];
                indices[(i * 6) + 3] = vertexIndices[(3 * i) + 2];

                indices[(i * 6) + 4] = vertexIndices[(3 * i) + 2];
                indices[(i * 6) + 5] = vertexIndices[ 3 * i     ];
            }

            vertexIndices.resize(indices.size());
            for (u32 i = 0; i < indices.size(); i += 1) {
                vertexIndices[i] = indices[i];
            }
        }

        float getBoundingBox(const std::vector<float> &vertices) {
            gl::Vector<float, 4> minWorld(std::numeric_limits<float>::infinity()), maxWorld(-std::numeric_limits<float>::infinity());
            for (u32 i = 0; i < vertices.size(); i += 3) {
                minWorld[0] = std::min(vertices[i    ], minWorld[0]);
                minWorld[1] = std::min(vertices[i + 1], minWorld[1]);
                minWorld[2] = std::min(vertices[i + 2], minWorld[2]);

                maxWorld[0] = std::max(vertices[i    ], maxWorld[0]);
                maxWorld[1] = std::max(vertices[i + 1], maxWorld[1]);
                maxWorld[2] = std::max(vertices[i + 2], maxWorld[2]);
            }

            minWorld[3] = 1;
            maxWorld[3] = 1;

            gl::Vector<float, 4> minCamera = minWorld, maxCamera = maxWorld;

            if (maxCamera[3] != 0)
                maxCamera = maxCamera * (1.0F / maxCamera[3]);

            if (minCamera[3] != 0)
                minCamera = minCamera * (1.0F / minCamera[3]);

            float maxX = std::max(std::fabs(minCamera[0]), std::fabs(maxCamera[0]));
            float maxY = std::max(std::fabs(minCamera[1]), std::fabs(maxCamera[1]));

            return std::max(maxX, maxY);
        }

        void setDefaultUVs(std::vector<float> &uv, size_t size) {
            uv.resize(size / 3 * 2);
            for (u32 i = 0; i < uv.size(); i += 2) {
                uv[i    ] = 0.0F;
                uv[i + 1] = 0.0F;
            }
        }

        void setDefaultColors(std::vector<float> &colors, size_t size, u32 color) {
            colors.resize(size / 3 * 4);

            float red   = float((color >> 0)  & 0xFF) / 255.0F;
            float green = float((color >> 8)  & 0xFF) / 255.0F;
            float blue  = float((color >> 16) & 0xFF) / 255.0F;
            float alpha = float((color >> 24) & 0xFF) / 255.0F;

            for (u32 i = 0; i < colors.size(); i += 4) {
                colors[i    ] = red;
                colors[i + 1] = green;
                colors[i + 2] = blue;
                colors[i + 3] = alpha;
            }
        }

        void setNormals(const std::vector<float> &vertices, std::vector<float> &normals) {
            for (u32 i = 0; i < normals.size(); i += 9) {

                auto v1 = gl::Vector<float, 3>({vertices[i    ], vertices[i + 1], vertices[i + 2]});
                auto v2 = gl::Vector<float, 3>({vertices[i + 3], vertices[i + 4], vertices[i + 5]});
                auto v3 = gl::Vector<float, 3>({vertices[i + 6], vertices[i + 7], vertices[i + 8]});

                auto normal = ((v2 - v1).cross(v3 - v1));
                normals[i    ] += normal[0];
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
                normals[i    ] = normal[0];
                normals[i + 1] = normal[1];
                normals[i + 2] = normal[2];
            }
        }

        void setNormalsWithIndices(const std::vector<float> &vertices, std::vector<float> &normals, const std::vector<u32> &indices) {
            for (u32 i = 0; i < indices.size(); i += 3) {
                auto idx  = indices[i    ];
                auto idx1 = indices[i + 1];
                auto idx2 = indices[i + 2];

                auto v1 = gl::Vector<float, 3>({vertices[3 * idx ], vertices[(3 * idx ) + 1], vertices[(3 * idx ) + 2]});
                auto v2 = gl::Vector<float, 3>({vertices[3 * idx1], vertices[(3 * idx1) + 1], vertices[(3 * idx1) + 2]});
                auto v3 = gl::Vector<float, 3>({vertices[3 * idx2], vertices[(3 * idx2) + 1], vertices[(3 * idx2) + 2]});

                auto weighted = ((v2 - v1).cross(v3 - v1));

                normals[ 3 * idx      ] += weighted[0];
                normals[(3 * idx)  + 1] += weighted[1];
                normals[(3 * idx)  + 2] += weighted[2];
                normals[(3 * idx1)    ] += weighted[0];
                normals[(3 * idx1) + 1] += weighted[1];
                normals[(3 * idx1) + 2] += weighted[2];
                normals[(3 * idx2)    ] += weighted[0];
                normals[(3 * idx2) + 1] += weighted[1];
                normals[(3 * idx2) + 2] += weighted[2];
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

        template<class T>
        void loadVectors(Vectors<T> &vectors, IndexType indexType) {
            s_max = getBoundingBox(vectors.vertices);

            if (s_drawTexture)
                setDefaultColors(vectors.colors, vectors.vertices.size(), 0x00000000);
            else if (vectors.colors.empty())
                setDefaultColors(vectors.colors, vectors.vertices.size(), 0xFF337FFF);

            if (vectors.uv.empty())
                setDefaultUVs(vectors.uv, vectors.vertices.size());

            if (vectors.normals.empty()) {
                vectors.normals.resize(vectors.vertices.size());

                if (vectors.indices.empty() || (indexType == IndexType::Undefined)) {
                    setNormals(vectors.vertices, vectors.normals);

                } else {
                    std::vector<u32> indices;

                    indices.resize(vectors.indices.size());
                    for (u32 i = 0; i < vectors.indices.size(); i += 1)
                        indices[i] = vectors.indices[i];

                    setNormalsWithIndices(vectors.vertices, vectors.normals, indices);
                }
            }
        }

        template<class T>
        void loadLineVectors(LineVectors<T> &lineVectors, IndexType indexType) {
            s_max = getBoundingBox(lineVectors.vertices);

            if (lineVectors.colors.empty())
                setDefaultColors(lineVectors.colors, lineVectors.vertices.size(), 0xFF337FFF);

            if (indexType != IndexType::Undefined)
                indicesForLines(lineVectors.indices);
        }

        void processKeyEvent(ImGuiKey key, float &variable, float increment, float acceleration) {
            if (ImGui::IsKeyPressed(key)) {
                auto temp = variable + (increment * acceleration);
                if (variable * temp < 0.0F)
                    variable = 0.0F;
                else
                    variable = temp;
            }
        }

        void processInputEvents(gl::Vector<float, 3> &rotation, gl::Vector<float, 3> &translation, float &scaling, float &nearLimit, float &farLimit) {
            auto accel = 1.0F;
            if (ImGui::IsKeyDown(ImGuiKey_LeftShift) ||
                ImGui::IsKeyDown(ImGuiKey_RightShift))
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

            scaling = std::max(scaling, 0.01F);

            processKeyEvent(ImGuiKey_Keypad4, translation[0], -0.1F, accel);
            processKeyEvent(ImGuiKey_Keypad6, translation[0],  0.1F, accel);
            processKeyEvent(ImGuiKey_Keypad8, translation[1],  0.1F, accel);
            processKeyEvent(ImGuiKey_Keypad2, translation[1], -0.1F, accel);
            processKeyEvent(ImGuiKey_Keypad1, translation[2],  0.1F, accel);
            processKeyEvent(ImGuiKey_Keypad7, translation[2], -0.1F, accel);
            processKeyEvent(ImGuiKey_Keypad9, nearLimit,     -0.01F, accel);
            processKeyEvent(ImGuiKey_Keypad3, nearLimit,      0.01F, accel);

            if (ImHexApi::System::isDebugBuild()) {
                processKeyEvent(ImGuiKey_KeypadDivide,   farLimit, -1.0F, accel);
                processKeyEvent(ImGuiKey_KeypadMultiply, farLimit,  1.0F, accel);
            }

            processKeyEvent(ImGuiKey_KeypadAdd, rotation[2],     -0.075F, accel);
            processKeyEvent(ImGuiKey_KeypadSubtract, rotation[2], 0.075F, accel);
            rotation[2] = std::fmod(rotation[2], 2 * std::numbers::pi_v<float>);
        }

        bool validateVector(const std::vector<float> &vector, u32 vertexCount, u32 divisor, const std::string &name,std::string &errorMessage) {
            if (!vector.empty()) {
                if (vector.size() % divisor != 0) {
                    errorMessage = hex::format("hex.visualizers.pl_visualizer.3d.error_message_count"_lang, name , std::to_string(divisor));
                    return false;
                }
            } else {
                errorMessage = hex::format("hex.visualizers.pl_visualizer.3d.error_message_not_empty"_lang, name);
                return false;
            }
            auto vectorCount = vector.size()/divisor;
            if (vectorCount != vertexCount) {
                errorMessage =  hex::format("hex.visualizers.pl_visualizer.3d.error_message_expected"_lang, std::to_string(vertexCount), std::to_string(vectorCount));
                return false;
            }
            return true;
        }

        template <class T>
        void bindBuffers(Buffers<T> &buffers, const gl::VertexArray &vertexArray, Vectors<T> vectors, IndexType indexType) {
            buffers.vertices    = {};
            buffers.normals     = {};
            buffers.colors      = {};
            buffers.uv          = {};
            buffers.indices     = {};

            vertexArray.bind();
            u32 vertexCount = vectors.vertices.size() / 3;
            std::string errorMessage;

            if (indexType != IndexType::Undefined && !vectors.indices.empty())
                buffers.indices = gl::Buffer<T>(gl::BufferType::Index, vectors.indices);


            if ((indexType == IndexType::Undefined || vectors.indices.empty()) && vertexCount % 3 != 0) {
                throw std::runtime_error(std::string("hex.visualizers.pl_visualizer.3d.error_message_vertex_count"_lang));
            } else
                buffers.vertices = gl::Buffer<float>(gl::BufferType::Vertex, vectors.vertices);


            if (validateVector(vectors.colors, vertexCount, 4,  std::string("hex.visualizers.pl_visualizer.3d.error_message_colors"_lang), errorMessage))
                buffers.colors = gl::Buffer<float>(gl::BufferType::Vertex, vectors.colors);
            else {
                throw std::runtime_error(errorMessage);
            }

            if (validateVector(vectors.normals, vertexCount, 3, std::string("hex.visualizers.pl_visualizer.3d.error_message_normals"_lang), errorMessage))
                buffers.normals = gl::Buffer<float>(gl::BufferType::Vertex, vectors.normals);
            else {
                throw std::runtime_error(errorMessage);
            }

            if (validateVector(vectors.uv, vertexCount, 2, std::string("hex.visualizers.pl_visualizer.3d.error_message_uv_coords"_lang), errorMessage))
                buffers.uv = gl::Buffer<float>(gl::BufferType::Vertex, vectors.uv);
            else {
                throw std::runtime_error(errorMessage);
            }

            vertexArray.addBuffer(0, buffers.vertices);
            vertexArray.addBuffer(1, buffers.colors, 4);
            vertexArray.addBuffer(2, buffers.normals);
            vertexArray.addBuffer(3, buffers.uv, 2);

            buffers.vertices.unbind();
            buffers.colors.unbind();
            buffers.normals.unbind();
            buffers.uv.unbind();

            if (indexType != IndexType::Undefined)
                buffers.indices.unbind();

            vertexArray.unbind();

        }

        template <class T>
        void bindLineBuffers(LineBuffers<T> &lineBuffers, const gl::VertexArray &vertexArray, const LineVectors<T> &lineVectors, IndexType indexType) {
            lineBuffers.vertices  = {};
            lineBuffers.colors    = {};
            lineBuffers.indices   = {};
            u32 vertexCount = lineVectors.vertices.size() / 3;
            vertexArray.bind();
            std::string errorMessage;

            if (indexType != IndexType::Undefined)
                lineBuffers.indices = gl::Buffer<T>(gl::BufferType::Index, lineVectors.indices);

            if ((indexType == IndexType::Undefined || lineVectors.indices.empty()) && vertexCount % 3 != 0) {
                throw std::runtime_error(std::string("hex.visualizers.pl_visualizer.3d.error_message_vertex_count"_lang));
            } else
                lineBuffers.vertices = gl::Buffer<float>(gl::BufferType::Vertex, lineVectors.vertices);

            if (validateVector(lineVectors.colors, vertexCount, 4, std::string("hex.visualizers.pl_visualizer.3d.error_message_colors"_lang), errorMessage))
                lineBuffers.colors = gl::Buffer<float>(gl::BufferType::Vertex, lineVectors.colors);
            else {
                throw std::runtime_error(errorMessage);
            }

            vertexArray.addBuffer(0, lineBuffers.vertices);
            vertexArray.addBuffer(1, lineBuffers.colors, 4);

            lineBuffers.vertices.unbind();
            lineBuffers.colors.unbind();

            if (indexType != IndexType::Undefined)
                lineBuffers.indices.unbind();

            vertexArray.unbind();

        }

        void drawWindow(const ImGuiExt::Texture &texture, ImVec2 &renderingWindowSize, const gl::Matrix<float, 4, 4> &mvp) {
            auto textureSize = texture.getSize();
            auto textureWidth = textureSize.x;
            auto textureHeight = textureSize.y;

            ImVec2 screenPos = ImGui::GetCursorScreenPos();
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

            ImGui::SetNextWindowSizeConstraints(scaled({ 350, 350 }), ImVec2(FLT_MAX, FLT_MAX));
            if (ImGui::BeginChild("##image", textureSize, ImGuiChildFlags_ResizeX | ImGuiChildFlags_ResizeY | ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                renderingWindowSize = ImGui::GetContentRegionAvail();

                ImGui::Image(texture, textureSize, ImVec2(0, 1), ImVec2(1, 0));

                if (s_drawAxes) {
                    gl::Matrix<float, 4, 4> axes = gl::Matrix<float, 4, 4>::identity();
                    axes(0, 3) = 1.0F;
                    axes(1, 3) = 1.0F;
                    axes(2, 3) = 1.0F;

                    axes = axes * mvp;
                    bool showX = axes(0, 3) > 0.0F;
                    bool showY = axes(1, 3) > 0.0F;
                    bool showZ = axes(2, 3) > 0.0F;

                    axes.updateRow(0, axes.getRow(0) * (1.0F / axes(0, 3)));
                    axes.updateRow(1, axes.getRow(1) * (1.0F / axes(1, 3)));
                    axes.updateRow(2, axes.getRow(2) * (1.0F / axes(2, 3)));

                    auto axesPositionX = (axes.getColumn(0) + 1.0F) * (textureWidth / 2.0F);
                    auto axesPositionY = (axes.getColumn(1) + 1.0F) * (-textureHeight / 2.0F) + textureHeight;

                    ImDrawList *drawList = ImGui::GetWindowDrawList();

                    if (showX)
                        drawList->AddText(ImVec2(axesPositionX[0], axesPositionY[0]) + screenPos, IM_COL32(255, 0, 0, 255), "X");
                    if (showY)
                        drawList->AddText(ImVec2(axesPositionX[1], axesPositionY[1]) + screenPos, IM_COL32(0, 255, 0, 255), "Y");
                    if (showZ)
                        drawList->AddText(ImVec2(axesPositionX[2], axesPositionY[2]) + screenPos, IM_COL32(0, 0, 255, 255), "Z");
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
            {
                ImGui::SameLine();
                {
                    ImGui::PushID(5);
                    ImGui::Dummy(ImVec2(0, 0));
                    ImGui::PopID();
                }
            }

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
                if (ImGuiExt::DimmedIconToggle(s_isPerspective ? ICON_BI_GRID : ICON_VS_SYMBOL_NUMBER, &s_drawGrid))
                    s_shouldReset = true;
                ImGui::PopID();
            }

            ImGui::SameLine();

            // Draw light source toggle
            {
                ImGui::PushID(3);
                if (ImGuiExt::DimmedIconToggle(ICON_VS_LIGHTBULB, &s_drawLightSource))
                    s_shouldReset = true;

                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                    ImGui::OpenPopup("LightSettings");
                }

                if (ImGui::BeginPopup("LightSettings")) {
                    if (ImGui::DragFloat3("hex.visualizers.pl_visualizer.3d.light_position"_lang, s_lightPosition.data(), 0.05F)) {
                        s_shouldUpdateLightSource = true;
                    }

                    ImGui::SliderFloat("hex.visualizers.pl_visualizer.3d.ambient_brightness"_lang,   &s_lightBrightness.data()[0], 0, 2);
                    ImGui::SliderFloat("hex.visualizers.pl_visualizer.3d.diffuse_brightness"_lang,   &s_lightBrightness.data()[1], 0, 2);
                    ImGui::SliderFloat("hex.visualizers.pl_visualizer.3d.specular_brightness"_lang,  &s_lightBrightness.data()[2], 0, 2);
                    ImGui::SliderFloat("hex.visualizers.pl_visualizer.3d.object_reflectiveness"_lang, &s_lightBrightness.data()[3], 0, 64);
                    if (ImGui::ColorEdit3("hex.visualizers.pl_visualizer.3d.light_color"_lang, s_lightColor.data()))
                        s_shouldUpdateLightSource = true;

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

            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();

            if (ImGuiExt::DimmedButton("hex.ui.common.reset"_lang, ImVec2(renderingWindowSize.x+5-ImGui::GetCursorPosX(), 0))) {
                s_translation      = { {  0.0F, 0.0F, -3.0F } };
                s_rotation         = { {  0.0F, 0.0F,  0.0F } };
                s_scaling            = 1.0F;
            }

            // Draw more settings
            if (ImGui::CollapsingHeader("hex.visualizers.pl_visualizer.3d.more_settings"_lang)) {
                if (ImGuiExt::InputFilePicker("hex.visualizers.pl_visualizer.3d.texture_file"_lang, s_texturePath, {}))
                    s_shouldUpdateTexture = true;
            }
        }

    }

    template <class T>
    void processRendering(std::shared_ptr<pl::ptrn::Pattern> verticesPattern, std::shared_ptr<pl::ptrn::Pattern> indicesPattern,
                          std::shared_ptr<pl::ptrn::Pattern> normalsPattern, std::shared_ptr<pl::ptrn::Pattern> colorsPattern,
                          std::shared_ptr<pl::ptrn::Pattern> uvPattern) {
        static gl::LightSourceVectors sourceVectors(20);
        static gl::VertexArray sourceVertexArray = {};
        static gl::LightSourceBuffers sourceBuffers(sourceVertexArray, sourceVectors);

        static gl::VertexArray gridVertexArray = {};
        static gl::GridVectors gridVectors(9);
        static gl::GridBuffers gridBuffers(gridVertexArray, gridVectors);

        static gl::VertexArray axesVertexArray = {};
        static gl::AxesVectors axesVectors;
        static gl::AxesBuffers axesBuffers(axesVertexArray, axesVectors);

        static gl::VertexArray vertexArray = {};

        if (s_renderingWindowSize.x <= 0 || s_renderingWindowSize.y <= 0)
            s_renderingWindowSize = {350_scaled, 350_scaled};

        gl::Matrix<float, 4, 4> mvp(0);
        static Buffers<T> buffers;
        static LineBuffers<T> lineBuffers;


        if (s_shouldReset) {
            s_shouldReset = false;
            s_shouldUpdateLightSource = true;
            if (s_drawMode == GL_TRIANGLES) {
                Vectors<T> vectors;

                vectors.vertices = patternToArray<float>(verticesPattern.get());
                std::string errorMessage;
                s_vertexCount = vectors.vertices.size() / 3;
                if (!validateVector(vectors.vertices, s_vertexCount, 3, std::string("hex.visualizers.pl_visualizer.3d.error_message_positions"_lang), errorMessage))
                    throw std::runtime_error(errorMessage);

                if (s_indexType != IndexType::Undefined) {
                    vectors.indices = patternToArray<T>(indicesPattern.get());
                    s_badIndices.clear();
                    auto indexCount = vectors.indices.size();
                    if (indexCount < 3 || indexCount % 3 != 0) {
                        throw std::runtime_error(std::string("hex.visualizers.pl_visualizer.3d.error_message_index_count"_lang));
                    }
                    auto booleans = std::views::transform(vectors.indices,isIndexInRange);
                    if (!std::accumulate(std::begin(booleans), std::end(booleans), true, std::logical_and<>())) {
                        errorMessage = std::string("hex.visualizers.pl_visualizer.3d.error_message_invalid_indices"_lang);
                        for (auto badIndex : s_badIndices)
                            errorMessage += std::to_string(badIndex) + ", ";
                        errorMessage.pop_back();
                        errorMessage.pop_back();
                        errorMessage += hex::format(std::string("hex.visualizers.pl_visualizer.3d.error_message_for_vertex_count"_lang),s_vertexCount);
                        throw std::runtime_error(errorMessage);
                    }
                }

                if (colorsPattern != nullptr)
                    vectors.colors = patternToArray<float>(colorsPattern.get());
                if (normalsPattern != nullptr)
                    vectors.normals = patternToArray<float>(normalsPattern.get());
                if (uvPattern != nullptr)
                    vectors.uv = patternToArray<float>(uvPattern.get());

                loadVectors(vectors, s_indexType);

                bindBuffers(buffers, vertexArray, vectors, s_indexType);
            } else {
                LineVectors<T> lineVectors;
                std::string errorMessage;

                lineVectors.vertices = patternToArray<float>(verticesPattern.get());
                s_vertexCount = lineVectors.vertices.size() / 3;
                if (!validateVector(lineVectors.vertices, s_vertexCount, 3, std::string("hex.visualizers.pl_visualizer.3d.error_message_positions"_lang), errorMessage))
                    throw std::runtime_error(errorMessage);

                if (s_indexType != IndexType::Undefined) {
                    lineVectors.indices = patternToArray<T>(indicesPattern.get());
                    auto indexCount = lineVectors.indices.size();
                    if (indexCount < 3 || indexCount % 3 != 0) {
                        throw std::runtime_error(std::string("hex.visualizers.pl_visualizer.3d.error_message_index_count"_lang));
                    }
                    s_badIndices.clear();
                    if (!std::ranges::all_of(lineVectors.indices,isIndexInRange)) {
                        errorMessage = std::string("hex.visualizers.pl_visualizer.3d.error_message_invalid_indices"_lang);
                        for (auto badIndex : s_badIndices)
                            errorMessage += std::to_string(badIndex) + ", ";
                        errorMessage.pop_back();
                        errorMessage.pop_back();
                        errorMessage += hex::format(std::string("hex.visualizers.pl_visualizer.3d.error_message_for_vertex_count"_lang),s_vertexCount);
                        throw std::runtime_error(errorMessage);
                    }
                }

                if (colorsPattern != nullptr)
                    lineVectors.colors = patternToArray<float>(colorsPattern.get());

                loadLineVectors(lineVectors, s_indexType);

                bindLineBuffers(lineBuffers, vertexArray, lineVectors, s_indexType);
            }
        }

        if (s_shouldUpdateLightSource) {
            s_shouldUpdateLightSource = false;
            sourceVectors.moveTo(s_lightPosition);
            sourceVectors.setColor(s_lightColor[0], s_lightColor[1], s_lightColor[2]);
            sourceBuffers.moveVertices(sourceVertexArray, sourceVectors);
            sourceBuffers.updateColors(sourceVertexArray, sourceVectors);
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
            float viewWidth =  s_renderingWindowSize.x / 500.0F;
            float viewHeight = s_renderingWindowSize.y / 500.0F;
            glViewport(0,0 , GLsizei(renderTexture.getWidth()), GLsizei(renderTexture.getHeight()));
            glDepthRangef(s_nearLimit, s_farLimit);
            glClearColor(0.00F, 0.00F, 0.00F, 0.00F);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);

            if (!s_isPerspective) {
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
            totalScale /= (3.0F * s_max);
            scaleForVertices(0, 0) = totalScale;
            scaleForVertices(1, 1) = totalScale;
            scaleForVertices(2, 2) = totalScale;

            model = s_rotate * scale;
            scaledModel = s_rotate * scaleForVertices;
            view = translate;
            mvp = model * view * projection;

            if (s_drawMode == GL_TRIANGLES) {
                static gl::Shader shader = gl::Shader(
                    romfs::get("shaders/default/vertex.glsl").string(),
                    romfs::get("shaders/default/fragment.glsl").string()
                );

                shader.bind();

                shader.setUniform("modelScale",       scaledModel);
                shader.setUniform("modelMatrix",      model);
                shader.setUniform("viewMatrix",       view);
                shader.setUniform("projectionMatrix", projection);
                shader.setUniform("lightPosition",    s_lightPosition);
                shader.setUniform("lightBrightness",  s_lightBrightness);
                shader.setUniform("lightColor",       s_lightColor);

                vertexArray.bind();
                if (s_shouldUpdateTexture) {
                    s_shouldUpdateTexture = false;
                    s_modelTexture = ImGuiExt::Texture::fromImage(s_texturePath, ImGuiExt::Texture::Filter::Nearest);
                    if (s_modelTexture.isValid()) {
                        s_drawTexture = true;
                    }
                }

                if (s_drawTexture)
                    glBindTexture(GL_TEXTURE_2D, s_modelTexture);

                buffers.indices.bind();
                if (buffers.indices.getSize() == 0)
                    buffers.vertices.draw(s_drawMode);
                else
                    buffers.indices.draw(s_drawMode);
                buffers.indices.unbind();

            } else {
                static gl::Shader lineShader = gl::Shader(
                    romfs::get("shaders/default/lineVertex.glsl").string(),
                    romfs::get("shaders/default/lineFragment.glsl").string()
                );

                lineShader.bind();
                lineShader.setUniform("modelMatrix", scaledModel);
                lineShader.setUniform("viewMatrix", view);
                lineShader.setUniform("projectionMatrix", projection);
                vertexArray.bind();

                lineBuffers.indices.bind();
                if (lineBuffers.indices.getSize() == 0)
                    lineBuffers.vertices.draw(s_drawMode);
                else
                    lineBuffers.indices.draw(s_drawMode);
                lineBuffers.indices.unbind();

            }

            if (s_drawGrid || s_drawAxes) {
                static auto gridAxesShader = gl::Shader(
                    romfs::get("shaders/default/lineVertex.glsl").string(),
                    romfs::get("shaders/default/lineFragment.glsl").string()
                );

                gridAxesShader.bind();

                gridAxesShader.setUniform("modelMatrix", model);
                gridAxesShader.setUniform("viewMatrix", view);
                gridAxesShader.setUniform("projectionMatrix", projection);

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
            if (s_drawLightSource) {
                static auto sourceShader = gl::Shader(
                    romfs::get("shaders/default/lightVertex.glsl").string(),
                    romfs::get("shaders/default/lightFragment.glsl").string()
                );
                sourceShader.bind();

                sourceShader.setUniform("modelMatrix", model);
                sourceShader.setUniform("viewMatrix", view);
                sourceShader.setUniform("projectionMatrix", projection);

                sourceVertexArray.bind();
                sourceBuffers.getIndices().bind();
                sourceBuffers.getIndices().draw(GL_TRIANGLES);
                sourceBuffers.getIndices().unbind();
                sourceVertexArray.unbind();
                sourceShader.unbind();
            }

            vertexArray.unbind();
            frameBuffer.unbind();

            s_texture = ImGuiExt::Texture::fromGLTexture(renderTexture.release(), GLsizei(renderTexture.getWidth()), GLsizei(renderTexture.getHeight()));

            drawWindow(s_texture, s_renderingWindowSize, mvp);
        }
    }

    void draw3DVisualizer(pl::ptrn::Pattern &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {

        std::shared_ptr<pl::ptrn::Pattern> verticesPattern = arguments[0].toPattern();
        std::shared_ptr<pl::ptrn::Pattern> indicesPattern = arguments[1].toPattern();
        std::shared_ptr<pl::ptrn::Pattern> normalsPattern = nullptr;
        std::shared_ptr<pl::ptrn::Pattern> colorsPattern = nullptr;
        std::shared_ptr<pl::ptrn::Pattern> uvPattern = nullptr;

        std::string textureFile;
        if (arguments.size() > 2) {
            normalsPattern = arguments[2].toPattern();
            if (arguments.size() > 3) {
                colorsPattern = arguments[3].toPattern();
                if (arguments.size() > 4) {
                    uvPattern = arguments[4].toPattern();
                    if (arguments.size() > 5)
                        textureFile = arguments[5].toString();
                }
            }
        }

        s_texturePath = textureFile;
        s_drawTexture = !textureFile.empty();
        if (shouldReset)
            s_shouldReset = true;
        processInputEvents(s_rotation, s_translation, s_scaling, s_nearLimit, s_farLimit);

        auto *iterable = dynamic_cast<pl::ptrn::IIterable*>(indicesPattern.get());
        if (iterable != nullptr && iterable->getEntryCount() > 0) {
            const auto &content = iterable->getEntry(0);
            if (content->getSize() == 1) {
                s_indexType = IndexType::U8;
                processRendering<u8>(verticesPattern, indicesPattern, normalsPattern, colorsPattern, uvPattern);
            } else if (content->getSize() == 2) {
                s_indexType = IndexType::U16;
                processRendering<u16>(verticesPattern, indicesPattern, normalsPattern, colorsPattern, uvPattern);
            } else if (content->getSize() == 4) {
                s_indexType = IndexType::U32;
                processRendering<u32>(verticesPattern, indicesPattern, normalsPattern, colorsPattern, uvPattern);
            }
        } else {
            s_indexType = IndexType::Undefined;
            processRendering<u8>(verticesPattern, indicesPattern, normalsPattern, colorsPattern, uvPattern);
        }
    }
}