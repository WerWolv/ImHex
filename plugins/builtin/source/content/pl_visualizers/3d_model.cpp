#include <hex/helpers/utils.hpp>

#include <content/pl_visualizers/visualizer_helpers.hpp>

#include <fonts/codicons_font.h>
#include <fonts/blendericons_font.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <hex/helpers/opengl.hpp>
#include <opengl_support.h>
#include <GLFW/glfw3.h>

#include <numbers>

#include <hex/ui/imgui_imhex_extensions.h>
#include <hex/api/imhex_api.hpp>
#include <hex/api/localization_manager.hpp>

#include <romfs/romfs.hpp>

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

        IndexType s_indexType;

        ImGuiExt::Texture s_modelTexture;

        gl::Vector<float, 3> s_translation      = { {  0.0F, 0.0F, -3.0F } };
        gl::Vector<float, 3> s_rotation         = { {  0.0F, 0.0F,  0.0F } };
        gl::Vector<float, 3> s_lightPosition    = { { -0.7F, 0.0F,  0.0F } };
        gl::Vector<float, 4> s_lightBrightness  = { {  0.5F, 0.5F,  0.5F, 32.0F } };
        gl::Vector<float, 3> s_lightColor       = { {  1.0F, 1.0F,  1.0f } };
        gl::Matrix<float, 4, 4> s_rotate        = gl::Matrix<float, 4, 4>::identity();

        ImGuiExt::Texture s_texture;
        std::fs::path s_texturePath;

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


        float getBoundingBox(const std::vector<float> &vertices) {
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

            return std::max(maxx, maxy);
        }

        void setDefaultColors(std::vector<float> &colors, float size, u32 color) {
            colors.resize(size / 3 * 4);

            float red   = float((color >> 0)  & 0xFF) / 255.0F;
            float green = float((color >> 8)  & 0xFF) / 255.0F;
            float blue  = float((color >> 16) & 0xFF) / 255.0F;
            float alpha = float((color >> 24) & 0xFF) / 255.0F;

            for (u32 i = 0; i < colors.size(); i += 4) {
                colors[i] = red;
                colors[i + 1] = green;
                colors[i + 2] = blue;
                colors[i + 3] = alpha;
            }
        }

        void setNormals(const std::vector<float> &vertices, std::vector<float> &normals) {
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

        void setNormalsWithIndices(const std::vector<float> &vertices, std::vector<float> &normals, const std::vector<u32> &indices) {
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

        void loadVectors(Vectors &vectors, IndexType indexType) {
            s_max = getBoundingBox(vectors.vertices);

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

        void loadLineVectors(LineVectors &lineVectors, IndexType indexType) {
            s_max = getBoundingBox(lineVectors.vertices);

            if (lineVectors.colors.empty())
                setDefaultColors(lineVectors.colors, lineVectors.vertices.size(), 0xFF337FFF);

            std::vector<u32> indices;
            if (indexType == IndexType::U8)
                indicesForLines(lineVectors.indices8);
            else if (indexType == IndexType::U16)
                indicesForLines(lineVectors.indices16);
            else
                indicesForLines(lineVectors.indices32);
        }

        void processKeyEvent(ImGuiKey key, float &variable, float incr, float accel) {
            if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(key))) {
                auto temp = variable + incr * accel;
                if (variable * temp < 0.0F)
                    variable = 0.0F;
                else
                    variable = temp;
            }
        }

        void processInputEvents(gl::Vector<float, 3> &rotation, gl::Vector<float, 3> &translation, float &scaling, float &nearLimit, float &farLimit) {
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


        void bindBuffers(Buffers &buffers, const gl::VertexArray &vertexArray, Vectors vectors, IndexType indexType) {
            buffers.vertices    = {};
            buffers.normals     = {};
            buffers.colors      = {};
            buffers.uv1         = {};
            buffers.indices8    = {};
            buffers.indices16   = {};
            buffers.indices32   = {};

            vertexArray.bind();
            buffers.vertices = gl::Buffer<float>(gl::BufferType::Vertex, vectors.vertices);
            buffers.colors   = gl::Buffer<float>(gl::BufferType::Vertex, vectors.colors);
            buffers.normals  = gl::Buffer<float>(gl::BufferType::Vertex, vectors.normals);

            if (indexType == IndexType::U8)
                buffers.indices8 = gl::Buffer<u8>(gl::BufferType::Index, vectors.indices8);
            else if (indexType == IndexType::U16)
                buffers.indices16 = gl::Buffer<u16>(gl::BufferType::Index, vectors.indices16);
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

        void bindLineBuffers(LineBuffers &lineBuffers, const gl::VertexArray &vertexArray, const LineVectors &lineVectors, IndexType indexType) {
            lineBuffers.vertices  = {};
            lineBuffers.colors    = {};
            lineBuffers.indices8  = {};
            lineBuffers.indices16 = {};
            lineBuffers.indices32 = {};

            vertexArray.bind();
            lineBuffers.vertices = gl::Buffer<float>(gl::BufferType::Vertex, lineVectors.vertices);
            lineBuffers.colors = gl::Buffer<float>(gl::BufferType::Vertex, lineVectors.colors);

            if (indexType == IndexType::U8)
                lineBuffers.indices8 = gl::Buffer<u8>(gl::BufferType::Index, lineVectors.indices8);
            else if (indexType == IndexType::U16)
                lineBuffers.indices16 = gl::Buffer<u16>(gl::BufferType::Index, lineVectors.indices16);
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

        void drawWindow(const ImGuiExt::Texture &texture, ImVec2 &renderingWindowSize, const gl::Matrix<float, 4, 4> &mvp) {
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
                    if (ImGui::DragFloat3("hex.builtin.pl_visualizer.3d.light_position"_lang, s_lightPosition.data(), 0.05F)) {
                        s_shouldUpdateLightSource = true;
                    }

                    ImGui::SliderFloat("hex.builtin.pl_visualizer.3d.ambient_brightness"_lang,   &s_lightBrightness.data()[0], 0, 2);
                    ImGui::SliderFloat("hex.builtin.pl_visualizer.3d.diffuse_brightness"_lang,   &s_lightBrightness.data()[1], 0, 2);
                    ImGui::SliderFloat("hex.builtin.pl_visualizer.3d.specular_brightness"_lang,  &s_lightBrightness.data()[2], 0, 2);
                    ImGui::SliderFloat("hex.builtin.pl_visualizer.3d.object_reflectiveness"_lang, &s_lightBrightness.data()[3], 0, 64);
                    if (ImGui::ColorEdit3("hex.builtin.pl_visualizer.3d.light_color"_lang, s_lightColor.data()))
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

            if (ImGuiExt::DimmedButton("hex.builtin.common.reset"_lang, ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                s_translation      = { {  0.0F, 0.0F, -3.0F } };
                s_rotation         = { {  0.0F, 0.0F,  0.0F } };
                s_scaling            = 1.0F;
            }

            // Draw more settings
            if (ImGui::CollapsingHeader("hex.builtin.pl_visualizer.3d.more_settings"_lang)) {
                if (ImGuiExt::InputFilePicker("hex.builtin.pl_visualizer.3d.texture_file"_lang, s_texturePath, {}))
                    s_shouldUpdateTexture = true;
            }
        }

    }

    void draw3DVisualizer(pl::ptrn::Pattern &, pl::ptrn::IIterable &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
        static gl::LightSourceVectors sourceVectors(20);
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

                shader.setUniform("modelScale",      scaledModel);
                shader.setUniform("modelMatrix",     model);
                shader.setUniform("viewMatrix",      view);
                shader.setUniform("projectionMatrix",projection);
                shader.setUniform("lightPosition",      s_lightPosition);
                shader.setUniform("lightBrightness",    s_lightBrightness);
                shader.setUniform("lightColor",         s_lightColor);

                vertexArray.bind();
                if (s_shouldUpdateTexture) {
                    s_shouldUpdateTexture = false;
                    s_modelTexture = ImGuiExt::Texture(s_texturePath, ImGuiExt::Texture::Filter::Nearest);
                }

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
                lineShader.setUniform("modelMatrix", scaledModel);
                lineShader.setUniform("viewMatrix", view);
                lineShader.setUniform("projectionMatrix", projection);
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
                static auto gridAxesShader = gl::Shader(
                        romfs::get("shaders/default/lineVertex.glsl").string(),
                        romfs::get("shaders/default/lineFragment.glsl").string());
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
                        romfs::get("shaders/default/lightFragment.glsl").string());
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

            s_texture = ImGuiExt::Texture(renderTexture.release(), GLsizei(renderTexture.getWidth()), GLsizei(renderTexture.getHeight()));

            drawWindow(s_texture, s_renderingWindowSize, mvp);
        }
    }

}