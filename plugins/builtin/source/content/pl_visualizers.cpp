#include <hex/api/content_registry.hpp>
#include <hex/api/localization.hpp>
#include <hex/api/task.hpp>

#include <hex/helpers/disassembler.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/opengl.hpp>
#include <hex/helpers/http_requests.hpp>

#include <imgui.h>
#include <implot.h>

#if defined (OS_EMSCRIPTEN)
    #define GLFW_INCLUDE_ES3
    #include <GLES3/gl3.h>
#else
    #include <imgui_impl_opengl3_loader.h>
#endif

#include <GLFW/glfw3.h>

#include <hex/ui/imgui_imhex_extensions.h>
#include <fonts/codicons_font.h>

#include <pl/patterns/pattern.hpp>
#include <pl/patterns/pattern_padding.hpp>

#include <miniaudio.h>

#include <romfs/romfs.hpp>

#include <numeric>
#include <numbers>

#include <content/helpers/diagrams.hpp>
#include <ui/hex_editor.hpp>

#include <content/providers/memory_file_provider.hpp>

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

        void drawLinePlotVisualizer(pl::ptrn::Pattern &, pl::ptrn::IIterable &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
            static std::vector<float> values;
            auto dataPattern = arguments[0].toPattern();

            if (ImPlot::BeginPlot("##plot", ImVec2(400, 250), ImPlotFlags_NoChild | ImPlotFlags_CanvasOnly)) {
                ImPlot::SetupAxes("X", "Y", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

                if (shouldReset) {
                    values.clear();
                    values = sampleData(patternToArray<float>(dataPattern.get()), ImPlot::GetPlotSize().x * 4);
                }

                ImPlot::PlotLine("##line", values.data(), values.size());

                ImPlot::EndPlot();
            }
        }

        void drawScatterPlotVisualizer(pl::ptrn::Pattern &, pl::ptrn::IIterable &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
            static std::vector<float> xValues, yValues;

            auto xPattern = arguments[0].toPattern();
            auto yPattern = arguments[1].toPattern();

            if (ImPlot::BeginPlot("##plot", ImVec2(400, 250), ImPlotFlags_NoChild | ImPlotFlags_CanvasOnly)) {
                ImPlot::SetupAxes("X", "Y", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

                if (shouldReset) {
                    xValues.clear(); yValues.clear();
                    xValues = sampleData(patternToArray<float>(xPattern.get()), ImPlot::GetPlotSize().x * 4);
                    yValues = sampleData(patternToArray<float>(yPattern.get()), ImPlot::GetPlotSize().x * 4);
                }

                ImPlot::PlotScatter("##scatter", xValues.data(), yValues.data(), xValues.size());

                ImPlot::EndPlot();
            }
        }

        void drawImageVisualizer(pl::ptrn::Pattern &, pl::ptrn::IIterable &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
            static ImGui::Texture texture;
            static float scale = 1.0F;

            if (shouldReset) {
                auto pattern  = arguments[0].toPattern();

                auto data = pattern->getBytes();
                texture = ImGui::Texture(data.data(), data.size());
                scale = 200_scaled / texture.getSize().x;
            }

            if (texture.isValid())
                ImGui::Image(texture, texture.getSize() * scale);

            if (ImGui::IsWindowHovered()) {
                auto scrollDelta = ImGui::GetIO().MouseWheel;
                if (scrollDelta != 0.0F) {
                    scale += scrollDelta * 0.1F;
                    scale = std::clamp(scale, 0.1F, 10.0F);
                }
            }
        }

        void drawBitmapVisualizer(pl::ptrn::Pattern &, pl::ptrn::IIterable &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
            static ImGui::Texture texture;
            static float scale = 1.0F;

            if (shouldReset) {
                auto pattern  = arguments[0].toPattern();
                auto width  = arguments[1].toUnsigned();
                auto height = arguments[2].toUnsigned();

                auto data = pattern->getBytes();
                texture = ImGui::Texture(data.data(), data.size(), width, height);
            }

            if (texture.isValid())
                ImGui::Image(texture, texture.getSize() * scale);

            if (ImGui::IsWindowHovered()) {
                auto scrollDelta = ImGui::GetIO().MouseWheel;
                if (scrollDelta != 0.0F) {
                    scale += scrollDelta * 0.1F;
                    scale = std::clamp(scale, 0.1F, 10.0F);
                }
            }
        }

        void drawDisassemblyVisualizer(pl::ptrn::Pattern &, pl::ptrn::IIterable &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
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

        void draw3DVisualizer(pl::ptrn::Pattern &, pl::ptrn::IIterable &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
            auto verticesPattern = arguments[0].toPattern();
            auto indicesPattern  = arguments[1].toPattern();

            static ImGui::Texture texture;

            static gl::Vector<float, 3> translation;
            static gl::Vector<float, 3> rotation = { { 1.0F, -1.0F, 0.0F } };
            static float scaling = 0.1F;

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
                vertices = patternToArray<float>(verticesPattern.get());
                indices  = patternToArray<u32>(indicesPattern.get());

                normals.clear();
                normals.resize(vertices.size());

                for (u32 i = 0; i < normals.size(); i += 9) {

                    auto v1 = gl::Vector<float, 3>({ vertices[i]    , vertices[i + 1], vertices[i + 2] });
                    auto v2 = gl::Vector<float, 3>({ vertices[i + 3], vertices[i + 4], vertices[i + 5] });
                    auto v3 = gl::Vector<float, 3>({ vertices[i + 6], vertices[i + 7], vertices[i + 8] });

                    auto normal = ((v2 - v1).cross(v3 - v1)).normalize();
                    normals[i] = normal[0];
                    normals[i + 1] = normal[1];
                    normals[i + 2] = normal[2];
                    normals[i + 3] = normal[0];
                    normals[i + 4] = normal[1];
                    normals[i + 5] = normal[2];
                    normals[i + 6] = normal[0];
                    normals[i + 7] = normal[1];
                    normals[i + 8] = normal[2];
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

                gl::Texture renderTexture(400_scaled, 400_scaled);
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

            auto textureSize = texture.getSize();

            if (ImGui::BeginTable("##3DVisualizer", 2, ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                if (ImGui::BeginChild("##image", textureSize, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                    ImGui::Image(texture, textureSize, ImVec2(0, 1), ImVec2(1, 0));
                }
                ImGui::EndChild();
                ImGui::PopStyleVar();

                ImGui::TableNextColumn();
                ImGui::TextUnformatted("hex.builtin.pl_visualizer.3d.rotation"_lang);
                ImGui::VSliderFloat("##X", ImVec2(18_scaled, textureSize.y), &rotation.data()[0], 0, std::numbers::pi * 2, "", ImGuiSliderFlags_AlwaysClamp);
                ImGui::SameLine();
                ImGui::VSliderFloat("##Y", ImVec2(18_scaled, textureSize.y), &rotation.data()[1], 0, std::numbers::pi * 2, "", ImGuiSliderFlags_AlwaysClamp);
                ImGui::SameLine();
                ImGui::VSliderFloat("##Z", ImVec2(18_scaled, textureSize.y), &rotation.data()[2], 0, std::numbers::pi * 2, "", ImGuiSliderFlags_AlwaysClamp);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::TextUnformatted("hex.builtin.pl_visualizer.3d.scale"_lang);
                ImGui::SameLine();
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::SliderFloat("##Scale", &scaling, 0.0001F, 0.2F, "");
                ImGui::PopItemWidth();

                for (u8 i = 0; i < 3; i++) {
                    while (rotation.data()[i] > std::numbers::pi * 2)
                        rotation.data()[i] -= std::numbers::pi * 2;
                    while (rotation.data()[i] < 0)
                        rotation.data()[i] += std::numbers::pi * 2;
                }

                ImGui::TableNextColumn();

                if (ImGui::Button("hex.builtin.common.reset"_lang, ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                    translation = gl::Vector<float, 3>({ 0.0F, 0.0F, 0.0F });
                    rotation = gl::Vector<float, 3>({ 0.0F, 0.0F, 0.0F });
                    scaling = 0.1F;
                }

                ImGui::EndTable();
            }


        }

        void drawSoundVisualizer(pl::ptrn::Pattern &, pl::ptrn::IIterable &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
            auto wavePattern = arguments[0].toPattern();
            auto channels = arguments[1].toUnsigned();
            auto sampleRate = arguments[2].toUnsigned();

            static std::vector<i16> waveData, sampledData;
            static ma_device audioDevice;
            static ma_device_config deviceConfig;
            static bool shouldStop = false;
            static u64 index = 0;
            static TaskHolder resetTask;

            if (sampleRate == 0)
                throw std::logic_error(hex::format("Invalid sample rate: {}", sampleRate));
            else if (channels == 0)
                throw std::logic_error(hex::format("Invalid channel count: {}", channels));

            if (shouldReset) {
                waveData.clear();

                resetTask = TaskManager::createTask("Visualizing...", TaskManager::NoProgress, [=](Task &) {
                    ma_device_stop(&audioDevice);
                    waveData = patternToArray<i16>(wavePattern.get());
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

        void drawChunkBasedEntropyVisualizer(pl::ptrn::Pattern &, pl::ptrn::IIterable &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
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

        void drawHexVisualizer(pl::ptrn::Pattern &, pl::ptrn::IIterable &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
            static ui::HexEditor editor;
            static std::unique_ptr<MemoryFileProvider> dataProvider;

            if (shouldReset) {
                auto pattern = arguments[0].toPattern();
                std::vector<u8> data;

                dataProvider = std::make_unique<MemoryFileProvider>();
                try {
                    data = pattern->getBytes();
                } catch (const std::exception &e) {
                    dataProvider->resize(0);
                    throw;
                }

                dataProvider->resize(data.size());
                dataProvider->writeRaw(0x00, data.data(), data.size());
                dataProvider->setReadOnly(true);

                editor.setProvider(dataProvider.get());
            }

            if (ImGui::BeginChild("##editor", scaled(ImVec2(600, 400)), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                editor.draw();

                ImGui::EndChild();
            }
        }

        void drawCoordinateVisualizer(pl::ptrn::Pattern &, pl::ptrn::IIterable &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
            static ImVec2 coordinate;
            static double latitude, longitude;
            static std::string address;
            static std::mutex addressMutex;
            static TaskHolder addressTask;

            static auto mapTexture = ImGui::Texture(romfs::get("assets/common/map.jpg").span());
            static ImVec2 mapSize = scaled(ImVec2(500, 500 / mapTexture.getAspectRatio()));

            if (shouldReset) {
                std::scoped_lock lock(addressMutex);

                address.clear();
                latitude  = arguments[0].toFloatingPoint();
                longitude = arguments[1].toFloatingPoint();

                // Convert latitude and longitude to X/Y coordinates on the image
                coordinate.x = float((longitude + 180) / 360 * mapSize.x);
                coordinate.y = float((-latitude + 90)  / 180 * mapSize.y);
            }

            const auto startPos = ImGui::GetWindowPos() + ImGui::GetCursorPos();

            // Draw background image
            ImGui::Image(mapTexture, mapSize);

            // Draw Longitude / Latitude text below image
            ImGui::PushTextWrapPos(startPos.x + mapSize.x);
            ImGui::TextFormattedWrapped("{}: {:.0f}° {:.0f}' {:.4f}\" {}  |  {}: {:.0f}° {:.0f}' {:.4f}\" {}",
                                 "hex.builtin.pl_visualizer.coordinates.latitude"_lang,
                                 std::floor(std::abs(latitude)),
                                 std::floor(std::abs(latitude - std::floor(latitude)) * 60),
                                 (std::abs(latitude - std::floor(latitude)) * 60 - std::floor(std::abs(latitude - std::floor(latitude)) * 60)) * 60,
                                 latitude >= 0 ? "N" : "S",
                                 "hex.builtin.pl_visualizer.coordinates.longitude"_lang,
                                 std::floor(std::abs(longitude)),
                                 std::floor(std::abs(longitude - std::floor(longitude)) * 60),
                                 (std::abs(longitude - std::floor(longitude)) * 60 - std::floor(std::abs(longitude - std::floor(longitude)) * 60)) * 60,
                                 longitude >= 0 ? "E" : "W"
                                 );
            ImGui::PopTextWrapPos();

            if (addressTask.isRunning()) {
                ImGui::TextSpinner("hex.builtin.pl_visualizer.coordinates.querying"_lang);
            } else if (address.empty()) {
                if (ImGui::DimmedButton("hex.builtin.pl_visualizer.coordinates.query"_lang)) {
                    addressTask = TaskManager::createBackgroundTask("hex.builtin.pl_visualizer.coordinates.querying"_lang, [lat = latitude, lon = longitude](auto &) {
                        constexpr static auto ApiURL = "https://geocode.maps.co/reverse?lat={}&lon={}&format=jsonv2";

                        HttpRequest request("GET", hex::format(ApiURL, lat, lon));
                        auto response = request.execute().get();

                        if (!response.isSuccess())
                            return;

                        try {

                            auto json = nlohmann::json::parse(response.getData());
                            auto jsonAddr = json["address"];

                            std::scoped_lock lock(addressMutex);
                            if (jsonAddr.contains("village")) {
                                address = hex::format("{} {}, {} {}",
                                                      jsonAddr["village"].get<std::string>(),
                                                      jsonAddr["county"].get<std::string>(),
                                                      jsonAddr["state"].get<std::string>(),
                                                      jsonAddr["country"].get<std::string>());
                            } else if (jsonAddr.contains("city")) {
                                address = hex::format("{}, {} {}, {} {}",
                                                      jsonAddr["road"].get<std::string>(),
                                                      jsonAddr["quarter"].get<std::string>(),
                                                      jsonAddr["city"].get<std::string>(),
                                                      jsonAddr["state"].get<std::string>(),
                                                      jsonAddr["country"].get<std::string>());
                            }
                        } catch (std::exception &e) {
                            address = std::string("hex.builtin.pl_visualizer.coordinates.querying_no_address"_lang);
                        }
                    });
                }
            } else {
                ImGui::PushTextWrapPos(startPos.x + mapSize.x);
                ImGui::TextFormattedWrapped("{}", address);
                ImGui::PopTextWrapPos();
            }

            // Draw crosshair pointing to the coordinates
            {
                constexpr static u32 CrossHairColor = 0xFF00D0D0;
                constexpr static u32 BorderColor    = 0xFF000000;

                auto drawList = ImGui::GetWindowDrawList();
                drawList->AddLine(startPos + ImVec2(coordinate.x, 0), startPos + ImVec2(coordinate.x, mapSize.y), CrossHairColor, 2_scaled);
                drawList->AddLine(startPos + ImVec2(0, coordinate.y), startPos + ImVec2(mapSize.x, coordinate.y), CrossHairColor, 2_scaled);
                drawList->AddCircleFilled(startPos + coordinate, 5, CrossHairColor);
                drawList->AddCircle(startPos + coordinate, 5, BorderColor);
            }
        }

        void drawTimestampVisualizer(pl::ptrn::Pattern &, pl::ptrn::IIterable &, bool, std::span<const pl::core::Token::Literal> arguments) {
            time_t timestamp = arguments[0].toUnsigned();
            auto tm = fmt::gmtime(timestamp);
            auto date = std::chrono::year_month_day(std::chrono::year(tm.tm_year + 1900), std::chrono::month(tm.tm_mon + 1), std::chrono::day(tm.tm_mday));

            auto lastMonthDay = std::chrono::year_month_day_last(date.year(), date.month() / std::chrono::last);
            auto firstWeekDay = std::chrono::weekday(std::chrono::year_month_day(date.year(), date.month(), std::chrono::day(1)));

            const auto scale = 1_scaled * (ImHexApi::System::getFontSize() / ImHexApi::System::DefaultFontSize);

            // Draw calendar
            if (ImGui::BeginTable("##month_table", 2)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                // Draw centered month name and year
                ImGui::TextFormattedCenteredHorizontal("{:%B %Y}", tm);

                if (ImGui::BeginTable("##days_table", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_NoHostExtendX, ImVec2(160, 120) * scale)) {
                    constexpr static auto ColumnFlags = ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoHide;
                    ImGui::TableSetupColumn("M", ColumnFlags);
                    ImGui::TableSetupColumn("T", ColumnFlags);
                    ImGui::TableSetupColumn("W", ColumnFlags);
                    ImGui::TableSetupColumn("T", ColumnFlags);
                    ImGui::TableSetupColumn("F", ColumnFlags);
                    ImGui::TableSetupColumn("S", ColumnFlags);
                    ImGui::TableSetupColumn("S", ColumnFlags);

                    ImGui::TableHeadersRow();

                    ImGui::TableNextRow();

                    // Skip days before the first day of the month
                    for (u8 i = 0; i < firstWeekDay.c_encoding() - 1; ++i)
                        ImGui::TableNextColumn();

                    // Draw days
                    for (u8 i = 1; i <= u32(lastMonthDay.day()); ++i) {
                        ImGui::TableNextColumn();
                        ImGui::TextFormatted("{:02}", i);

                        if (std::chrono::day(i) == date.day())
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetCustomColorU32(ImGuiCustomCol_ToolbarRed));

                        if (std::chrono::weekday(std::chrono::year_month_day(date.year(), date.month(), std::chrono::day(i))) == std::chrono::Sunday)
                            ImGui::TableNextRow();
                    }

                    ImGui::EndTable();
                }

                ImGui::TableNextColumn();

                // Draw analog clock
                const auto size = ImVec2(120, 120) * scale;
                if (ImGui::BeginChild("##clock", size + ImVec2(0, ImGui::GetTextLineHeightWithSpacing()))) {
                    // Draw centered digital hour, minute and seconds
                    ImGui::TextFormattedCenteredHorizontal("{:%H:%M:%S}", tm);
                    auto drawList = ImGui::GetWindowDrawList();
                    const auto center = ImGui::GetWindowPos() + ImVec2(0, ImGui::GetTextLineHeightWithSpacing()) + size / 2;

                    // Draw clock face
                    drawList->AddCircle(center, size.x / 2, ImGui::GetColorU32(ImGuiCol_TextDisabled), 0);

                    auto sectionPos = [](float i) {
                        return ImVec2(std::sin(-i * 30.0F * std::numbers::pi / 180.0F + std::numbers::pi / 2), std::cos(-i * 30.0F * std::numbers::pi / 180.0F + std::numbers::pi / 2));
                    };

                    // Draw clock sections and numbers
                    for (u8 i = 0; i < 12; ++i) {
                        auto text = hex::format("{}", (((i + 2) % 12) + 1));
                        drawList->AddLine(center + sectionPos(i) * size / 2.2, center + sectionPos(i) * size / 2, ImGui::GetColorU32(ImGuiCol_TextDisabled), 1_scaled);
                        drawList->AddText(center + sectionPos(i) * size / 3 - ImGui::CalcTextSize(text.c_str()) / 2, ImGui::GetColorU32(ImGuiCol_Text), text.c_str());
                    }

                    // Draw hour hand
                    drawList->AddLine(center, center + sectionPos((tm.tm_hour + 9) % 12 + float(tm.tm_min) / 60.0) * size / 3.5, ImGui::GetColorU32(ImGuiCol_TextDisabled), 3_scaled);

                    // Draw minute hand
                    drawList->AddLine(center, center + sectionPos((float(tm.tm_min) / 5.0F) - 3)  * size / 2.5, ImGui::GetColorU32(ImGuiCol_TextDisabled), 3_scaled);

                    // Draw second hand
                    drawList->AddLine(center, center + sectionPos((float(tm.tm_sec) / 5.0F) - 3)  * size / 2.5, ImGui::GetCustomColorU32(ImGuiCustomCol_ToolbarRed), 2_scaled);
                }
                ImGui::EndChild();

                ImGui::EndTable();
            }
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
        ContentRegistry::PatternLanguage::addVisualizer("hex_viewer", drawHexVisualizer, 1);
        ContentRegistry::PatternLanguage::addVisualizer("coordinates", drawCoordinateVisualizer, 2);
        ContentRegistry::PatternLanguage::addVisualizer("timestamp", drawTimestampVisualizer, 1);
    }

}
