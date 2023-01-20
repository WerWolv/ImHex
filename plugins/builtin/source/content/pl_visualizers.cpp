#include <hex/api/content_registry.hpp>
#include <hex/api/localization.hpp>

#include <hex/helpers/disassembler.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/opengl.hpp>

#include <imgui.h>
#include <implot.h>
#include <imgui_impl_opengl3_loader.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <pl/patterns/pattern.hpp>

#include <numeric>

namespace hex::plugin::builtin {

    namespace {

        void drawLinePlotVisualizer(pl::ptrn::Pattern &, pl::ptrn::Iteratable &iteratable, bool, const std::vector<pl::core::Token::Literal> &) {
            if (ImPlot::BeginPlot("##plot", ImVec2(400, 250), ImPlotFlags_NoChild | ImPlotFlags_CanvasOnly)) {

                ImPlot::SetupAxes("X", "Y", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

                ImPlot::PlotLineG("##line", [](void *data, int idx) -> ImPlotPoint {
                    auto &iteratable = *static_cast<pl::ptrn::Iteratable *>(data);
                    return { static_cast<double>(idx), pl::core::Token::literalToFloatingPoint(iteratable.getEntry(idx)->getValue()) };
                }, &iteratable, iteratable.getEntryCount());

                ImPlot::EndPlot();
            }
        }

        void drawImageVisualizer(pl::ptrn::Pattern &pattern, pl::ptrn::Iteratable &, bool shouldReset, const std::vector<pl::core::Token::Literal> &) {
            static ImGui::Texture texture;
            if (shouldReset) {
                std::vector<u8> data;
                data.resize(pattern.getSize());
                pattern.getEvaluator()->readData(pattern.getOffset(), data.data(), data.size(), pattern.getSection());
                texture = ImGui::Texture(data.data(), data.size());
            }

            if (texture.isValid())
                ImGui::Image(texture, texture.getSize());
        }

        void drawBitmapVisualizer(pl::ptrn::Pattern &pattern, pl::ptrn::Iteratable &, bool shouldReset, const std::vector<pl::core::Token::Literal> &arguments) {
            static ImGui::Texture texture;
            if (shouldReset) {
                auto width = pl::core::Token::literalToUnsigned(arguments[1]);
                auto height = pl::core::Token::literalToUnsigned(arguments[2]);

                std::vector<u8> data;
                data.resize(width * height * 4);

                pattern.getEvaluator()->readData(pattern.getOffset(), data.data(), data.size(), pattern.getSection());
                texture = ImGui::Texture(data.data(), data.size(), width, height);
            }

            if (texture.isValid())
                ImGui::Image(texture, texture.getSize());
        }

        void drawDisassemblyVisualizer(pl::ptrn::Pattern &pattern, pl::ptrn::Iteratable &, bool shouldReset, const std::vector<pl::core::Token::Literal> &arguments) {
            struct Disassembly {
                u64 address;
                std::vector<u8> bytes;
                std::string instruction;
            };

            static std::vector<Disassembly> disassembly;
            if (shouldReset) {
                auto baseAddress = pl::core::Token::literalToUnsigned(arguments[1]);
                auto architecture = pl::core::Token::literalToUnsigned(arguments[2]);
                auto mode = pl::core::Token::literalToUnsigned(arguments[3]);

                disassembly.clear();

                csh capstone;
                if (cs_open(static_cast<cs_arch>(architecture), static_cast<cs_mode>(mode), &capstone) == CS_ERR_OK) {
                    cs_option(capstone, CS_OPT_SKIPDATA, CS_OPT_ON);

                    std::vector<u8> data;
                    data.resize(pattern.getSize());
                    pattern.getEvaluator()->readData(pattern.getOffset(), data.data(), data.size(), pattern.getSection());
                    cs_insn *instructions = nullptr;

                    size_t instructionCount = cs_disasm(capstone, data.data(), data.size(), baseAddress, 0, &instructions);
                    for (size_t i = 0; i < instructionCount; i++) {
                        disassembly.push_back({ instructions[i].address, { instructions[i].bytes, instructions[i].bytes + instructions[i].size }, hex::format("{} {}", instructions[i].mnemonic, instructions[i].op_str) });
                    }
                    cs_free(instructions, instructionCount);
                    cs_close(&capstone);
                }
            }

            if (ImGui::BeginTable("##disassembly", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY, scaled(ImVec2(0, 300)))) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("hex.builtin.common.address"_lang);
                ImGui::TableSetupColumn("hex.builtin.common.bytes"_lang);
                ImGui::TableSetupColumn("hex.builtin.common.instruction"_lang);
                ImGui::TableHeadersRow();

                for (auto &entry : disassembly) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextFormatted("0x{0:08X}", entry.address);
                    ImGui::TableNextColumn();
                    std::string bytes;
                    for (auto byte : entry.bytes)
                        bytes += hex::format("{0:02X} ", byte);
                    ImGui::TextUnformatted(bytes.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(entry.instruction.c_str());
                }

                ImGui::EndTable();
            }
        }

        void draw3DVisualizer(pl::ptrn::Pattern &pattern, pl::ptrn::Iteratable &, bool shouldReset, const std::vector<pl::core::Token::Literal> &) {
            static ImGui::Texture texture;

            if (!shouldReset) {
                std::vector<float> vertices;
                vertices.resize(pattern.getSize() / sizeof(float));
                pattern.getEvaluator()->readData(pattern.getOffset(), vertices.data(), vertices.size() * sizeof(float), pattern.getSection());

                std::vector<float> normals;
                normals.resize(vertices.size());
                for (u32 i = 0; i < normals.size(); i += 9) {
                    auto v1 = gl::Vector<float, 3>({ vertices[i + 0], vertices[i + 1], vertices[i + 2] });
                    auto v2 = gl::Vector<float, 3>({ vertices[i + 3], vertices[i + 4], vertices[i + 5] });
                    auto v3 = gl::Vector<float, 3>({ vertices[i + 6], vertices[i + 7], vertices[i + 8] });

                    auto normal = ((v2 - v1).cross(v3 - v1)).normalize();
                    normals[i + 0] = normal[0];
                    normals[i + 1] = normal[1];
                    normals[i + 2] = normal[2];
                    normals[i + 3] = normal[0];
                    normals[i + 4] = normal[1];
                    normals[i + 5] = normal[2];
                    normals[i + 6] = normal[0];
                    normals[i + 7] = normal[1];
                    normals[i + 8] = normal[2];
                }

                {
                    gl::FrameBuffer frameBuffer;

                    gl::Texture renderTexture(512, 512);
                    frameBuffer.attachTexture(renderTexture);

                    frameBuffer.bind();

                    constexpr static const char *VertexShaderSource = R"glsl(
                        #version 330 core
                        layout (location = 0) in vec3 in_Position;
                        layout (location = 1) in vec3 in_Normal;

                        uniform float time;
                        out vec3 normal;

                        mat4 rotationMatrix(vec3 axis, float angle) {
                            axis = normalize(axis);
                            float s = sin(angle);
                            float c = cos(angle);
                            float oc = 1.0 - c;

                            return mat4(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0,
                                        oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,  0.0,
                                        oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0,
                                        0.0,                                0.0,                                0.0,                                1.0);
                        }

                        void main() {
                            mat4 rotation = rotationMatrix(vec3(1.0, 0.0, 0.0), time / 2) * rotationMatrix(vec3(0.0, 1.0, 0.0), time / 3);
                            normal = (vec4(in_Normal, 1.0) * rotation).xyz;
                            gl_Position = vec4(in_Position * 0.5, 1.0) * rotation;
                        }
                    )glsl";

                    constexpr static const char *FragmentShaderSource = R"glsl(
                        #version 330 core
                        in vec3 normal;
                        out vec4 color;

                        void main() {
                            vec3 norm = normalize(normal);
                            vec3 lightDir = normalize(vec3(0, 0, -1));
                            float diff = max(dot(norm, lightDir), 0.0);
                            vec3 diffuse = diff * vec3(1.0, 1.0, 1.0);

                            color = vec4(1.0f, 0.5f, 0.2f, 1.0f) * vec4(diffuse, 1.0) + 0.1;
                        }
                    )glsl";

                    glEnable(GL_DEPTH_TEST);

                    gl::Shader shader(VertexShaderSource, FragmentShaderSource);

                    gl::VertexArray vertexArray;
                    vertexArray.bind();

                    gl::Buffer<float> vertexBuffer(gl::BufferType::Vertex, vertices);
                    gl::Buffer<float> normalBuffer(gl::BufferType::Vertex, normals);

                    vertexArray.addBuffer(0, vertexBuffer);
                    vertexArray.addBuffer(1, normalBuffer);

                    vertexBuffer.unbind();
                    vertexArray.unbind();

                    shader.bind();
                    shader.setUniform("time", glfwGetTime());

                    vertexArray.bind();

                    glViewport(0, 0, renderTexture.getWidth(), renderTexture.getHeight());
                    glClearColor(0.00F, 0.00F, 0.00F, 0.00f);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    vertexBuffer.draw();

                    vertexArray.unbind();
                    shader.unbind();
                    frameBuffer.unbind();

                    texture = ImGui::Texture(renderTexture.getTexture(), renderTexture.getWidth(), renderTexture.getHeight());
                    renderTexture.release();
                }
            }

            ImGui::Image(texture, texture.getSize(), ImVec2(0, 1), ImVec2(1, 0));
        }

    }

    void registerPatternLanguageVisualizers() {
        ContentRegistry::PatternLanguage::addVisualizer("line_plot", drawLinePlotVisualizer, 0);
        ContentRegistry::PatternLanguage::addVisualizer("image", drawImageVisualizer, 0);
        ContentRegistry::PatternLanguage::addVisualizer("bitmap", drawBitmapVisualizer, 3);
        ContentRegistry::PatternLanguage::addVisualizer("disassembler", drawDisassemblyVisualizer, 4);
        ContentRegistry::PatternLanguage::addVisualizer("3d", draw3DVisualizer, 0);
    }

}