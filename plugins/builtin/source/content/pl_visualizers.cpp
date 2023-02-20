#include <hex/api/content_registry.hpp>
#include <hex/api/localization.hpp>

#include <hex/helpers/disassembler.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/opengl.hpp>

#include <imgui.h>
#include <implot.h>
#include <imgui_impl_opengl3_loader.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <fonts/codicons_font.h>

#include <pl/patterns/pattern.hpp>
#include <pl/patterns/pattern_padding.hpp>

#include <miniaudio.h>

#include <romfs/romfs.hpp>

#include <numeric>

#include <content/helpers/diagrams.hpp>

namespace hex::plugin::builtin {

    namespace {

        template<typename T>
        std::vector<T> patternToArray(pl::ptrn::Pattern *pattern){
            const auto bytes = pattern->getBytes();

            std::vector<T> result;
            result.resize(bytes.size() / sizeof(T));
            for (size_t i = 0; i < result.size(); i++)
                std::memcpy(&result[i], &bytes[i * sizeof(T)], sizeof(T));

            return result;
        }

    }

    namespace {

        void drawLinePlotVisualizer(pl::ptrn::Pattern &, pl::ptrn::Iteratable &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
            static std::vector<float> values;
            auto dataPattern = arguments[0].toPattern();

            if (ImPlot::BeginPlot("##plot", ImVec2(400, 250), ImPlotFlags_NoChild | ImPlotFlags_CanvasOnly)) {

                if (shouldReset) {
                    values.clear();
                    values = sampleData(patternToArray<float>(dataPattern), ImPlot::GetPlotSize().x * 4);
                }

                ImPlot::SetupAxes("X", "Y", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

                ImPlot::PlotLine("##line", values.data(), values.size());

                ImPlot::EndPlot();
            }
        }

        void drawScatterPlotVisualizer(pl::ptrn::Pattern &, pl::ptrn::Iteratable &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
            static std::vector<float> xValues, yValues;

            auto xPattern = arguments[0].toPattern();
            auto yPattern = arguments[1].toPattern();

            if (ImPlot::BeginPlot("##plot", ImVec2(400, 250), ImPlotFlags_NoChild | ImPlotFlags_CanvasOnly)) {

                if (shouldReset) {
                    xValues.clear(); yValues.clear();
                    xValues = sampleData(patternToArray<float>(xPattern), ImPlot::GetPlotSize().x * 4);
                    yValues = sampleData(patternToArray<float>(yPattern), ImPlot::GetPlotSize().x * 4);
                }

                ImPlot::SetupAxes("X", "Y", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

                ImPlot::PlotScatter("##scatter", xValues.data(), yValues.data(), xValues.size());

                ImPlot::EndPlot();
            }
        }

        void drawImageVisualizer(pl::ptrn::Pattern &, pl::ptrn::Iteratable &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
            static ImGui::Texture texture;
            if (shouldReset) {
                auto pattern  = arguments[0].toPattern();

                auto data = pattern->getBytes();
                texture = ImGui::Texture(data.data(), data.size());
            }

            if (texture.isValid())
                ImGui::Image(texture, texture.getSize());
        }

        void drawBitmapVisualizer(pl::ptrn::Pattern &, pl::ptrn::Iteratable &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
            static ImGui::Texture texture;
            if (shouldReset) {
                auto pattern  = arguments[0].toPattern();
                auto width  = arguments[1].toUnsigned();
                auto height = arguments[2].toUnsigned();

                auto data = pattern->getBytes();
                texture = ImGui::Texture(data.data(), data.size(), width, height);
            }

            if (texture.isValid())
                ImGui::Image(texture, texture.getSize());
        }

        void drawDisassemblyVisualizer(pl::ptrn::Pattern &, pl::ptrn::Iteratable &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
            struct Disassembly {
                u64 address;
                std::vector<u8> bytes;
                std::string instruction;
            };

            static std::vector<Disassembly> disassembly;
            if (shouldReset) {
                auto pattern  = arguments[0].toPattern();
                auto baseAddress  = arguments[1].toUnsigned();
                auto architecture = arguments[2].toUnsigned();
                auto mode         = arguments[3].toUnsigned();

                disassembly.clear();

                csh capstone;
                if (cs_open(static_cast<cs_arch>(architecture), static_cast<cs_mode>(mode), &capstone) == CS_ERR_OK) {
                    cs_option(capstone, CS_OPT_SKIPDATA, CS_OPT_ON);

                    auto data = pattern->getBytes();
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

        void draw3DVisualizer(pl::ptrn::Pattern &, pl::ptrn::Iteratable &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
            auto verticesPattern = arguments[0].toPattern();
            auto indicesPattern  = arguments[1].toPattern();

            static ImGui::Texture texture;
            static float scaling = 0.5F;
            static gl::Vector<float, 3> rotation = { { 1.0F, -1.0F, 0.0F } };
            static gl::Vector<float, 3> translation;
            static std::vector<float> vertices, normals;
            static std::vector<u32> indices;
            static gl::Shader shader;
            static gl::VertexArray vertexArray;
            static gl::Buffer<float> vertexBuffer, normalBuffer;
            static gl::Buffer<u32> indexBuffer;

            {
                auto dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
                rotation[0] += -dragDelta.y * 0.0075F;
                rotation[1] += -dragDelta.x * 0.0075F;
                ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);

                dragDelta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
                translation[0] += -dragDelta.x * 0.1F;
                translation[1] += -dragDelta.y * 0.1F;
                ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);

                auto scrollDelta = ImGui::GetIO().MouseWheel;
                scaling += scrollDelta * 0.01F;

                if (scaling < 0.01F)
                    scaling = 0.01F;
            }

            if (shouldReset) {
                vertices = patternToArray<float>(verticesPattern);
                indices  = patternToArray<u32>(indicesPattern);

                normals.clear();
                normals.resize(vertices.size());
                for (u32 i = 9; i < normals.size(); i += 9) {
                    auto v1 = gl::Vector<float, 3>({ vertices[i - 9], vertices[i - 8], vertices[i - 7] });
                    auto v2 = gl::Vector<float, 3>({ vertices[i - 6], vertices[i - 5], vertices[i - 4] });
                    auto v3 = gl::Vector<float, 3>({ vertices[i - 3], vertices[i - 2], vertices[i - 1] });

                    auto normal = ((v2 - v1).cross(v3 - v1)).normalize();
                    normals[i - 9] = normal[0];
                    normals[i - 8] = normal[1];
                    normals[i - 7] = normal[2];
                    normals[i - 6] = normal[0];
                    normals[i - 5] = normal[1];
                    normals[i - 4] = normal[2];
                    normals[i - 3] = normal[0];
                    normals[i - 2] = normal[1];
                    normals[i - 1] = normal[2];
                }

                shader = gl::Shader(romfs::get("shaders/default/vertex.glsl").string(), romfs::get("shaders/default/fragment.glsl").string());

                vertexArray = gl::VertexArray();

                vertexBuffer = {};
                normalBuffer = {};
                indexBuffer  = {};

                vertexArray.bind();

                vertexBuffer = gl::Buffer<float>(gl::BufferType::Vertex, vertices);
                normalBuffer = gl::Buffer<float>(gl::BufferType::Vertex, normals);
                indexBuffer  = gl::Buffer<u32>(gl::BufferType::Index, indices);

                vertexArray.addBuffer(0, vertexBuffer);
                vertexArray.addBuffer(1, normalBuffer);

                if (!indices.empty())
                    vertexArray.addBuffer(2, indexBuffer);

                vertexBuffer.unbind();
                normalBuffer.unbind();
                indexBuffer.unbind();
                vertexArray.unbind();
            }

            {
                gl::FrameBuffer frameBuffer;

                gl::Texture renderTexture(512, 512);
                frameBuffer.attachTexture(renderTexture);

                frameBuffer.bind();

                glEnable(GL_DEPTH_TEST);
                glEnable(GL_DEPTH_CLAMP);

                shader.bind();
                shader.setUniform("scale", scaling);
                shader.setUniform("rotation", rotation);
                shader.setUniform("translation", translation);

                vertexArray.bind();

                glViewport(0, 0, renderTexture.getWidth(), renderTexture.getHeight());
                glClearColor(0.00F, 0.00F, 0.00F, 0.00f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();
                glFrustum(-1.0F, 1.0F, -1.0F, 1.0F, 0.0000001F, 10000000.0F);

                if (indices.empty())
                    vertexBuffer.draw();
                else
                    indexBuffer.draw();

                vertexArray.unbind();
                shader.unbind();
                frameBuffer.unbind();

                texture = ImGui::Texture(renderTexture.release(), renderTexture.getWidth(), renderTexture.getHeight());
            }

            ImGui::Image(texture, texture.getSize(), ImVec2(0, 1), ImVec2(1, 0));
        }

        void drawSoundVisualizer(pl::ptrn::Pattern &, pl::ptrn::Iteratable &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
            auto wavePattern = arguments[0].toPattern();
            auto channels = arguments[1].toUnsigned();
            auto sampleRate = arguments[2].toUnsigned();

            static std::vector<i16> waveData, sampledData;
            static ma_device audioDevice;
            static ma_device_config deviceConfig;
            static bool shouldStop = false;
            static u64 index = 0;
            static TaskHolder resetTask;

            if (shouldReset) {
                waveData.clear();

                resetTask = TaskManager::createTask("Visualizing...", TaskManager::NoProgress, [=](Task &) {
                    ma_device_stop(&audioDevice);
                    waveData = patternToArray<i16>(wavePattern);
                    sampledData = sampleData(waveData, 300_scaled * 4);
                    index = 0;

                    deviceConfig = ma_device_config_init(ma_device_type_playback);
                    deviceConfig.playback.format   = ma_format_s16;
                    deviceConfig.playback.channels = channels;
                    deviceConfig.sampleRate        = sampleRate;
                    deviceConfig.pUserData         = &waveData;
                    deviceConfig.dataCallback      = [](ma_device *device, void *pOutput, const void *, ma_uint32 frameCount) {
                        if (index >= waveData.size()) {
                            index = 0;
                            shouldStop = true;
                            return;
                        }

                        ma_copy_pcm_frames(pOutput, waveData.data() + index, frameCount, device->playback.format, device->playback.channels);
                        index += frameCount;
                    };

                    ma_device_init(nullptr, &deviceConfig, &audioDevice);
                });
            }

            ImGui::BeginDisabled(resetTask.isRunning());

            ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0, 0));
            if (ImPlot::BeginPlot("##amplitude_plot", scaled(ImVec2(300, 80)), ImPlotFlags_NoChild | ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame | ImPlotFlags_NoInputs)) {
                ImPlot::SetupAxes("##time", "##amplitude", ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_NoMenus, ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_NoMenus);
                ImPlot::SetupAxesLimits(0, waveData.size(), std::numeric_limits<i16>::min(), std::numeric_limits<i16>::max(), ImGuiCond_Always);

                double dragPos = index;
                if (ImPlot::DragLineX(1, &dragPos, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                    if (dragPos < 0) dragPos = 0;
                    if (dragPos >= waveData.size()) dragPos = waveData.size() - 1;

                    index = dragPos;
                }

                ImPlot::PlotLine("##audio", sampledData.data(), sampledData.size());

                ImPlot::EndPlot();
            }
            ImPlot::PopStyleVar();

            {
                const u64 min = 0, max = waveData.size();
                ImGui::PushItemWidth(300_scaled);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                ImGui::SliderScalar("##index", ImGuiDataType_U64, &index, &min, &max, "");
                ImGui::PopStyleVar();
                ImGui::PopItemWidth();
            }

            if (shouldStop) {
                shouldStop = false;
                ma_device_stop(&audioDevice);
            }

            bool playing = ma_device_is_started(&audioDevice);

            if (ImGui::IconButton(playing ? ICON_VS_DEBUG_PAUSE : ICON_VS_PLAY, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarGreen))) {
                if (playing)
                    ma_device_stop(&audioDevice);
                else
                    ma_device_start(&audioDevice);
            }

            ImGui::SameLine();

            if (ImGui::IconButton(ICON_VS_DEBUG_STOP, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed))) {
                index = 0;
                ma_device_stop(&audioDevice);
            }

            ImGui::EndDisabled();

            ImGui::SameLine();

            if (resetTask.isRunning())
                ImGui::TextSpinner("");
            else
                ImGui::TextFormatted("{:02d}:{:02d} / {:02d}:{:02d}",
                                     (index / sampleRate) / 60, (index / sampleRate) % 60,
                                     (waveData.size() / sampleRate) / 60, (waveData.size() / sampleRate) % 60);
        }

        void drawChunkBasedEntropyVisualizer(pl::ptrn::Pattern &, pl::ptrn::Iteratable &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
            // variable used to store the result to avoid having to recalculate the result at each frame 
            static DiagramChunkBasedEntropyAnalysis analyzer;  

            // compute data    
            if (shouldReset) {
                auto pattern   = arguments[0].toPattern();
                auto chunkSize = arguments[1].toUnsigned();
                analyzer.process(pattern->getBytes(), chunkSize); 
            }
            
            // show results
            analyzer.draw(ImVec2(400, 250), ImPlotFlags_NoChild | ImPlotFlags_CanvasOnly); 
        }

    }

    void registerPatternLanguageVisualizers() {
        ContentRegistry::PatternLanguage::addVisualizer("line_plot", drawLinePlotVisualizer, 1);
        ContentRegistry::PatternLanguage::addVisualizer("scatter_plot", drawScatterPlotVisualizer, 2);
        ContentRegistry::PatternLanguage::addVisualizer("image", drawImageVisualizer, 1);
        ContentRegistry::PatternLanguage::addVisualizer("bitmap", drawBitmapVisualizer, 3);
        ContentRegistry::PatternLanguage::addVisualizer("disassembler", drawDisassemblyVisualizer, 4);
        ContentRegistry::PatternLanguage::addVisualizer("3d", draw3DVisualizer, 2);
        ContentRegistry::PatternLanguage::addVisualizer("sound", drawSoundVisualizer, 3);
        ContentRegistry::PatternLanguage::addVisualizer("chunk_entropy", drawChunkBasedEntropyVisualizer, 2);
    }

}
