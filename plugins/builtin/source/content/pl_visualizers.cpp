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

            if (shouldReset) {
                std::vector<float> vertices;
                vertices.resize(pattern.getSize() / sizeof(float));
                pattern.getEvaluator()->readData(pattern.getOffset(), vertices.data(), vertices.size() * sizeof(float), pattern.getSection());

                std::vector<u32> indices;
                indices.resize(vertices.size() / 3);
                std::iota(indices.begin(), indices.end(), 0);

                {
                    gl::FrameBuffer frameBuffer;

                    gl::Texture renderTexture(512, 512);
                    frameBuffer.attachTexture(renderTexture);

                    frameBuffer.bind();

                    constexpr static const char *VertexShaderSource = R"glsl(
                        #version 330 core
                        layout (location = 0) in vec3 in_Position;

                        void main() {
                            gl_Position = vec4(in_Position.x, in_Position.y, in_Position.z, 1.0);
                        }
                    )glsl";

                    constexpr static const char *FragmentShaderSource = R"glsl(
                        #version 330 core
                        out vec4 out_Color;

                        void main() {
                            out_Color = vec4(1.0f, 0.5f, 0.2f, 1.0f);
                        }
                    )glsl";

                    gl::Shader shader(VertexShaderSource, FragmentShaderSource);

                    gl::VertexArray vertexArray;
                    vertexArray.bind();

                    gl::Buffer<float> vertexBuffer(gl::BufferType::Vertex, vertices);
                    gl::Buffer<u32> indexBuffer(gl::BufferType::Index, indices);

                    vertexArray.addBuffer(vertexBuffer);

                    vertexBuffer.unbind();
                    vertexArray.unbind();

                    shader.bind();
                    vertexArray.bind();

                    glViewport(0, 0, renderTexture.getWidth(), renderTexture.getHeight());
                    glClearColor(0.00F, 0.00F, 0.00F, 0.00f);
                    glClear(GL_COLOR_BUFFER_BIT);
                    indexBuffer.draw();

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