#include <hex/api/content_registry.hpp>
#include <hex/api/imhex_api.hpp>

#include <hex/helpers/http_requests.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/literals.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/api/localization.hpp>

#include <hex/ui/view.hpp>

#include <algorithm>
#include <chrono>
#include <random>
#include <regex>

#include <llvm/Demangle/Demangle.h>
#include <content/helpers/math_evaluator.hpp>
#include <content/tools_entries.hpp>

#include <imgui.h>
#include <implot.h>

#include <hex/ui/imgui_imhex_extensions.h>
#include <content/popups/popup_notification.hpp>
#include <nlohmann/json.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/guards.hpp>
#include <wolv/net/socket_client.hpp>
#include <wolv/net/socket_server.hpp>

namespace hex::plugin::builtin {

    namespace impl {

        using namespace std::literals::string_literals;
        using namespace std::literals::chrono_literals;
        using namespace hex::literals;

        void drawDemangler() {
            static std::string mangledName, demangledName;

            if (ImGui::InputTextWithHint("hex.builtin.tools.demangler.mangled"_lang, "Itanium, MSVC, Dlang & Rust", mangledName)) {
                demangledName = llvm::demangle(mangledName);

                if (demangledName == mangledName) {
                    demangledName = "???";
                }
            }

            ImGui::Header("hex.builtin.tools.demangler.demangled"_lang);
            if (ImGui::BeginChild("demangled", ImVec2(0, 200_scaled), true)) {
                ImGui::TextFormattedWrappedSelectable("{}", demangledName);
            }
            ImGui::EndChild();
        }

        void drawASCIITable() {
            static bool asciiTableShowOctal = false;

            ImGui::BeginTable("##asciitable", 4);
            ImGui::TableSetupColumn("");
            ImGui::TableSetupColumn("");
            ImGui::TableSetupColumn("");
            ImGui::TableSetupColumn("");

            ImGui::TableNextColumn();

            for (u8 tablePart = 0; tablePart < 4; tablePart++) {
                ImGui::BeginTable("##asciitablepart", asciiTableShowOctal ? 4 : 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg);
                ImGui::TableSetupColumn("dec");
                if (asciiTableShowOctal)
                    ImGui::TableSetupColumn("oct");
                ImGui::TableSetupColumn("hex");
                ImGui::TableSetupColumn("char");

                ImGui::TableHeadersRow();

                for (u8 i = 0; i < 0x80 / 4; i++) {
                    ImGui::TableNextRow();

                    ImGui::TableNextColumn();
                    ImGui::TextFormatted("{0:03d}", i + 32 * tablePart);

                    if (asciiTableShowOctal) {
                        ImGui::TableNextColumn();
                        ImGui::TextFormatted("0o{0:03o}", i + 32 * tablePart);
                    }

                    ImGui::TableNextColumn();
                    ImGui::TextFormatted("0x{0:02X}", i + 32 * tablePart);

                    ImGui::TableNextColumn();
                    ImGui::TextFormatted("{0}", hex::makePrintable(i + 32 * tablePart));
                }

                ImGui::EndTable();
                ImGui::TableNextColumn();
            }
            ImGui::EndTable();

            ImGui::Checkbox("hex.builtin.tools.ascii_table.octal"_lang, &asciiTableShowOctal);
        }

        void drawRegexReplacer() {
            static auto regexInput     = [] { std::string s; s.reserve(0xFFF); return s; }();
            static auto regexPattern   = [] { std::string s; s.reserve(0xFFF); return s; }();
            static auto replacePattern = [] { std::string s; s.reserve(0xFFF); return s; }();
            static auto regexOutput    = [] { std::string s; s.reserve(0xFFF); return s; }();

            ImGui::PushItemWidth(-150_scaled);
            bool changed1 = ImGui::InputTextIcon("hex.builtin.tools.regex_replacer.pattern"_lang, ICON_VS_REGEX, regexPattern);
            bool changed2 = ImGui::InputTextIcon("hex.builtin.tools.regex_replacer.replace"_lang, ICON_VS_REGEX, replacePattern);
            bool changed3 = ImGui::InputTextMultiline("hex.builtin.tools.regex_replacer.input"_lang, regexInput, ImVec2(0, 0));

            if (changed1 || changed2 || changed3) {
                try {
                    regexOutput = std::regex_replace(regexInput.data(), std::regex(regexPattern.data()), replacePattern.data());
                } catch (std::regex_error &) { }
            }

            ImGui::InputTextMultiline("hex.builtin.tools.regex_replacer.output"_lang, regexOutput.data(), regexOutput.size(), ImVec2(0, 0), ImGuiInputTextFlags_ReadOnly);

            ImGui::PopItemWidth();
        }

        void drawColorPicker() {
            static std::array<float, 4> pickedColor = { 0 };
            static std::string rgba8;

            struct BitValue {
                int   bits;
                float color;
                float saturationMultiplier;
                char  name;
                u8    index;
            };

            static std::array bitValues = {
                BitValue{ 8, 0.00F, 1.0F, 'R', 0 },
                BitValue{ 8, 0.33F, 1.0F, 'G', 1 },
                BitValue{ 8, 0.66F, 1.0F, 'B', 2 },
                BitValue{ 8, 0.00F, 0.0F, 'A', 3 }
            };

            if (ImGui::BeginTable("##color_picker_table", 3, ImGuiTableFlags_BordersInnerV)) {
                ImGui::TableSetupColumn(" Color Picker", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 300_scaled);
                ImGui::TableSetupColumn(" Components", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 105_scaled);
                ImGui::TableSetupColumn(" Formats", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize);

                ImGui::TableHeadersRow();

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                // Draw main color picker widget
                ImVec2 startCursor, endCursor;
                {
                    ImGui::PushItemWidth(-1);
                    startCursor = ImGui::GetCursorPos();
                    ImGui::ColorPicker4("hex.builtin.tools.color"_lang, pickedColor.data(), ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHSV | ImGuiColorEditFlags_DisplayHex);
                    endCursor = ImGui::GetCursorPos();
                    ImGui::ColorButton("##color_button", ImColor(pickedColor[0], pickedColor[1], pickedColor[2], pickedColor[3]), ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoDragDrop | ImGuiColorEditFlags_AlphaPreviewHalf, ImVec2(300_scaled, 0));
                    ImGui::PopItemWidth();
                }

                ImGui::TableNextColumn();

                const auto colorName = hex::format("{}{}{}{}", bitValues[0].name, bitValues[1].name, bitValues[2].name, bitValues[3].name);

                // Draw color bit count sliders
                {
                    ImGui::Indent();

                    static auto drawBitsSlider = [&](BitValue *bitValue) {
                        // Change slider color
                        ImGui::PushStyleColor(ImGuiCol_FrameBg,         ImColor::HSV(bitValue->color, 0.5f * bitValue->saturationMultiplier, 0.5f).Value);
                        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,  ImColor::HSV(bitValue->color, 0.6f * bitValue->saturationMultiplier, 0.5f).Value);
                        ImGui::PushStyleColor(ImGuiCol_FrameBgActive,   ImColor::HSV(bitValue->color, 0.7f * bitValue->saturationMultiplier, 0.5f).Value);
                        ImGui::PushStyleColor(ImGuiCol_SliderGrab,      ImColor::HSV(bitValue->color, 0.9f * bitValue->saturationMultiplier, 0.9f).Value);

                        // Draw slider
                        ImGui::PushID(&bitValue->bits);
                        auto format = hex::format("%d\n{}", bitValue->name);
                        ImGui::VSliderInt("##slider", ImVec2(18_scaled, (endCursor - startCursor).y - 3_scaled), &bitValue->bits, 0, 16, format.c_str(), ImGuiSliderFlags_AlwaysClamp);
                        ImGui::PopID();

                        ImGui::PopStyleColor(4);
                    };

                    // Force sliders closer together
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

                    // Draw a slider for each color component
                    for (u32 index = 0; auto &bitValue : bitValues) {
                        // Draw slider
                        drawBitsSlider(&bitValue);

                        // Configure drag and drop source and target
                        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
                            // Set the current slider index as the payload
                            ImGui::SetDragDropPayload("BIT_VALUE", &index, sizeof(u32));

                            // Draw a color button to show the color being dragged
                            ImGui::ColorButton("##color_button", ImColor::HSV(bitValue.color, 0.5f * bitValue.saturationMultiplier, 0.5f).Value);

                            ImGui::EndDragDropSource();
                        }
                        if (ImGui::BeginDragDropTarget()) {
                            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("BIT_VALUE"); payload != nullptr) {
                                auto otherIndex = *static_cast<const u32 *>(payload->Data);

                                // Swap the currently hovered slider with the one being dragged
                                std::swap(bitValues[index], bitValues[otherIndex]);
                            }
                            ImGui::EndDragDropTarget();
                        }

                        ImGui::SameLine();

                        index += 1;
                    }

                    ImGui::NewLine();

                    // Draw color name below sliders
                    ImGui::TextFormatted("{}", colorName);

                    ImGui::PopStyleVar();

                    ImGui::Unindent();
                }

                ImGui::TableNextColumn();

                // Draw encoded color values
                {
                    // Calculate int and float representations of the selected color
                    std::array<u32, 4> intColor = {};
                    std::array<float, 4> floatColor = {};
                    for (u32 index = 0; auto &bitValue : bitValues) {
                        intColor[index] = u64(std::round(static_cast<long double>(pickedColor[bitValue.index]) * std::numeric_limits<u32>::max())) >> (32 - bitValue.bits);
                        floatColor[index] = pickedColor[bitValue.index];

                        index += 1;
                    }

                    // Draw a table with the color values
                    if (ImGui::BeginTable("##value_table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoHostExtendX , ImVec2(0, 0))) {
                        ImGui::TableSetupColumn("name",  ImGuiTableColumnFlags_WidthFixed);
                        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                        const static auto drawValue = [](const char *name, auto formatter) {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();

                            // Draw name of the formatting
                            ImGui::TextUnformatted(name);

                            ImGui::TableNextColumn();

                            // Draw value
                            ImGui::PushID(name);
                            ImGui::TextFormattedSelectable("{}", formatter());
                            ImGui::PopID();
                        };

                        const u32 bitCount = bitValues[0].bits + bitValues[1].bits + bitValues[2].bits + bitValues[3].bits;

                        // Draw the different representations

                        drawValue("HEX", [&] {
                            u64 hexValue = 0;
                            for (u32 index = 0; auto &bitValue : bitValues) {
                                hexValue <<= bitValue.bits;
                                hexValue |= u64(intColor[index]) & hex::bitmask(bitValue.bits);
                                index += 1;
                            }

                            return hex::format("#{0:0{1}X}", hexValue, bitCount / 4);
                        });

                        drawValue(colorName.c_str(), [&] {
                            return hex::format("{}({}, {}, {}, {})", colorName, intColor[0], intColor[1], intColor[2], intColor[3]);
                        });

                        drawValue("Vector4f", [&] {
                            return hex::format("{{ {:.2}F, {:.2}F, {:.2}F, {:.2}F }}", floatColor[0], floatColor[1], floatColor[2], floatColor[3]);
                        });

                        drawValue("Percentage", [&] {
                            return hex::format("{{ {}%, {}%, {}%, {}% }}", u32(floatColor[0] * 100), u32(floatColor[1] * 100), u32(floatColor[2] * 100), u32(floatColor[3] * 100));
                        });

                        ImGui::EndTable();
                    }
                }

                ImGui::EndTable();
            }
        }

        void drawGraphingCalculator() {
            static std::array<long double, 1000> x, y;
            static std::string mathInput;
            static ImPlotRect limits;
            static double prevPos = 0;
            static long double stepSize = 0.1;

            if (ImPlot::BeginPlot("Function", ImVec2(-1, 0), ImPlotFlags_NoTitle | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMouseText | ImPlotFlags_NoFrame)) {
                ImPlot::SetupAxesLimits(-10, 10, -5, 5, ImPlotCond_Once);

                limits = ImPlot::GetPlotLimits(ImAxis_X1, ImAxis_Y1);

                ImPlot::PlotLine("f(x)", x.data(), y.data(), x.size());
                ImPlot::EndPlot();
            }

            ImGui::PushItemWidth(-1);
            ImGui::InputTextIcon("##graphing_math_input", ICON_VS_SYMBOL_OPERATOR, mathInput, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
            ImGui::PopItemWidth();

            if ((prevPos != limits.X.Min && (ImGui::IsMouseReleased(ImGuiMouseButton_Left) || ImGui::GetIO().MouseWheel != 0)) || (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter))) {
                MathEvaluator<long double> evaluator;

                y = {};

                u32 i = 0;
                evaluator.setFunction("y", [&](auto args) -> std::optional<long double> {
                    i32 index = i + args[0];
                    if (index < 0 || u32(index) >= y.size())
                        return 0;
                    else
                        return y[index];
                }, 1, 1);

                evaluator.registerStandardVariables();
                evaluator.registerStandardFunctions();

                stepSize = (limits.X.Size()) / x.size();

                for (i = 0; i < x.size(); i++) {
                    evaluator.setVariable("x", limits.X.Min + i * stepSize);
                    x[i] = limits.X.Min + i * stepSize;
                    y[i] = evaluator.evaluate(mathInput).value_or(0);

                    if (y[i] < limits.Y.Min)
                        limits.Y.Min = y[i];
                    if (y[i] > limits.Y.Max)
                        limits.X.Max = y[i];

                }

                limits.X.Max = limits.X.Min + x.size() * stepSize;
                prevPos = limits.X.Min;
            }

        }

        void drawBaseConverter() {
            static std::array<std::string, 4> buffers;

            static auto ConvertBases = [](u8 base) {
                u64 number;

                switch (base) {
                    case 16:
                        number = std::strtoull(buffers[1].c_str(), nullptr, base);
                        break;
                    case 10:
                        number = std::strtoull(buffers[0].c_str(), nullptr, base);
                        break;
                    case 8:
                        number = std::strtoull(buffers[2].c_str(), nullptr, base);
                        break;
                    case 2:
                        number = std::strtoull(buffers[3].c_str(), nullptr, base);
                        break;
                    default:
                        return;
                }

                buffers[0] = std::to_string(number);
                buffers[1] = hex::format("0x{0:X}", number);
                buffers[2]  = hex::format("{0:#o}", number);
                buffers[3]  = hex::toBinaryString(number);
            };

            if (ImGui::InputTextIcon("hex.builtin.tools.base_converter.dec"_lang, ICON_VS_SYMBOL_NUMERIC, buffers[0]))
                ConvertBases(10);

            if (ImGui::InputTextIcon("hex.builtin.tools.base_converter.hex"_lang, ICON_VS_SYMBOL_NUMERIC, buffers[1]))
                ConvertBases(16);

            if (ImGui::InputTextIcon("hex.builtin.tools.base_converter.oct"_lang, ICON_VS_SYMBOL_NUMERIC, buffers[2]))
                ConvertBases(8);

            if (ImGui::InputTextIcon("hex.builtin.tools.base_converter.bin"_lang, ICON_VS_SYMBOL_NUMERIC, buffers[3]))
                ConvertBases(2);
        }

        void drawByteSwapper() {
            static std::string input, buffer, output;

            if (ImGui::InputTextIcon("hex.builtin.tools.input"_lang, ICON_VS_SYMBOL_NUMERIC, input, ImGuiInputTextFlags_CharsHexadecimal)) {
                auto nextAlignedSize = std::max<size_t>(2, std::bit_ceil(input.size()));

                buffer.clear();
                buffer.resize(nextAlignedSize - input.size(), '0');
                buffer += input;

                output.clear();
                for (u32 i = 0; i < buffer.size(); i += 2) {
                    output += buffer[buffer.size() - i - 2];
                    output += buffer[buffer.size() - i - 1];
                }
            }

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().DisabledAlpha);
            ImGui::InputTextIcon("hex.builtin.tools.output"_lang, ICON_VS_SYMBOL_NUMERIC, output, ImGuiInputTextFlags_ReadOnly);
            ImGui::PopStyleVar();
        }

        /*void drawFileUploader() {
            struct UploadedFile {
                std::string fileName, link, size;
            };

            static HttpRequest request("POST", "https://api.anonfiles.com/upload");
            static std::future<HttpRequest::Result<std::string>> uploadProcess;
            static std::fs::path currFile;
            static std::vector<UploadedFile> links;

            bool uploading = uploadProcess.valid() && uploadProcess.wait_for(0s) != std::future_status::ready;

            ImGui::Header("hex.builtin.tools.file_uploader.control"_lang, true);
            if (!uploading) {
                if (ImGui::Button("hex.builtin.tools.file_uploader.upload"_lang)) {
                    fs::openFileBrowser(fs::DialogMode::Open, {}, [&](auto path) {
                        uploadProcess = request.uploadFile(path);
                        currFile      = path;
                    });
                }
            } else {
                if (ImGui::Button("hex.builtin.common.cancel"_lang)) {
                    request.cancel();
                }
            }

            ImGui::SameLine();

            ImGui::ProgressBar(request.getProgress(), ImVec2(0, 0), uploading ? nullptr : "Done!");

            ImGui::Header("hex.builtin.tools.file_uploader.recent"_lang);

            if (ImGui::BeginTable("##links", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg, ImVec2(0, 200))) {
                ImGui::TableSetupColumn("hex.builtin.common.file"_lang);
                ImGui::TableSetupColumn("hex.builtin.common.link"_lang);
                ImGui::TableSetupColumn("hex.builtin.common.size"_lang);
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableHeadersRow();

                ImGuiListClipper clipper;
                clipper.Begin(links.size());

                while (clipper.Step()) {
                    for (i32 i = clipper.DisplayEnd - 1; i >= clipper.DisplayStart; i--) {
                        auto &[fileName, link, size] = links[i];

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fileName.c_str());

                        ImGui::TableNextColumn();
                        if (ImGui::Hyperlink(link.c_str())) {
                            if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
                                hex::openWebpage(link);
                            else
                                ImGui::SetClipboardText(link.c_str());
                        }

                        ImGui::InfoTooltip("hex.builtin.tools.file_uploader.tooltip"_lang);

                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(size.c_str());
                    }
                }

                clipper.End();

                ImGui::EndTable();
            }

            if (uploadProcess.valid() && uploadProcess.wait_for(0s) == std::future_status::ready) {
                auto response = uploadProcess.get();
                if (response.getStatusCode() == 200) {
                    try {
                        auto json = nlohmann::json::parse(response.getData());
                        links.push_back({
                            wolv::util::toUTF8String(currFile.filename()),
                            json.at("data").at("file").at("url").at("short"),
                            json.at("data").at("file").at("metadata").at("size").at("readable")
                        });
                    } catch (...) {
                        PopupError::open("hex.builtin.tools.file_uploader.invalid_response"_lang);
                    }
                } else if (response.getStatusCode() == 0) {
                    // Canceled by user, no action needed
                } else PopupError::open(hex::format("hex.builtin.tools.file_uploader.error"_lang, response.getStatusCode()));

                uploadProcess = {};
                currFile.clear();
            }
        }*/

        std::string getWikipediaApiUrl() {
            std::string setting = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.wiki_explain_language", "en").get<std::string>();
            return "https://" + setting + ".wikipedia.org/w/api.php?format=json&action=query&prop=extracts&explaintext&redirects=10&formatversion=2";
        }

        void drawWikiExplainer() {
            static HttpRequest request("GET", "");

            static std::string resultTitle, resultExtract;
            static std::future<HttpRequest::Result<std::string>> searchProcess;
            static bool extendedSearch = false;
            static std::string searchString;

            ImGui::Header("hex.builtin.tools.wiki_explain.control"_lang, true);

            bool startSearch = ImGui::InputTextIcon("##search", ICON_VS_SYMBOL_KEY, searchString, ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::SameLine();

            ImGui::BeginDisabled((searchProcess.valid() && searchProcess.wait_for(0s) != std::future_status::ready) || searchString.empty());
            startSearch = ImGui::Button("hex.builtin.tools.wiki_explain.search"_lang) || startSearch;
            ImGui::EndDisabled();

            if (startSearch && !searchString.empty()) {
                request.setUrl(getWikipediaApiUrl() + "&exintro"s + "&titles="s + request.urlEncode(searchString));
                searchProcess = request.execute();
            }

            ImGui::Header("hex.builtin.tools.wiki_explain.results"_lang);

            if (ImGui::BeginChild("##summary", ImVec2(0, 300), true)) {
                if (!resultTitle.empty() && !resultExtract.empty()) {
                    ImGui::HeaderColored(resultTitle.c_str(), ImGui::GetCustomColorVec4(ImGuiCustomCol_Highlight), true);
                    ImGui::TextFormattedWrapped("{}", resultExtract.c_str());
                }
            }
            ImGui::EndChild();

            if (searchProcess.valid() && searchProcess.wait_for(0s) == std::future_status::ready) {
                try {
                    auto response = searchProcess.get();
                    if (response.getStatusCode() != 200) throw std::runtime_error("Invalid response");

                    auto json = nlohmann::json::parse(response.getData());

                    resultTitle   = json.at("query").at("pages").at(0).at("title").get<std::string>();
                    resultExtract = json.at("query").at("pages").at(0).at("extract").get<std::string>();

                    if (!extendedSearch && resultExtract.ends_with(':')) {
                        extendedSearch = true;

                        request.setUrl(getWikipediaApiUrl() + "&titles="s + request.urlEncode(searchString));
                        searchProcess  = request.execute();

                        resultTitle.clear();
                        resultExtract.clear();
                        searchString.clear();
                    } else {
                        extendedSearch = false;
                        searchString.clear();
                    }
                } catch (...) {
                    searchString.clear();
                    resultTitle.clear();
                    resultExtract.clear();
                    extendedSearch = false;
                    searchProcess  = {};

                    resultTitle   = "???";
                    resultExtract = "hex.builtin.tools.wiki_explain.invalid_response"_lang.get();
                }
            }
        }

        void drawFileTools() {
            if (ImGui::BeginTabBar("file_tools_tabs")) {
                if (ImGui::BeginTabItem("hex.builtin.tools.file_tools.shredder"_lang)) {
                    drawFileToolShredder();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("hex.builtin.tools.file_tools.splitter"_lang)) {
                    drawFileToolSplitter();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("hex.builtin.tools.file_tools.combiner"_lang)) {
                    impl::drawFileToolCombiner();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }

        void drawInvariantMultiplicationDecoder() {
            static u64 divisor = 1;
            static u64 multiplier = 1;
            static u64 numBits = 32;

            ImGui::TextFormattedWrapped("{}", "hex.builtin.tools.invariant_multiplication.description"_lang);

            ImGui::NewLine();

            if (ImGui::BeginChild("##calculator", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 5), true)) {
                constexpr static u64 min = 1, max = 64;
                ImGui::SliderScalar("hex.builtin.tools.invariant_multiplication.num_bits"_lang, ImGuiDataType_U64, &numBits, &min, &max);
                ImGui::NewLine();

                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_TableRowBgAlt));
                if (ImGui::BeginChild("##calculator", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() + 12_scaled), true)) {
                    ImGui::PushItemWidth(100_scaled);

                    ImGui::TextUnformatted("X /");
                    ImGui::SameLine();
                    if (ImGui::InputScalar("##divisor", ImGuiDataType_U64, &divisor)) {
                        if (divisor == 0)
                            divisor = 1;

                        multiplier = ((1LLU << (numBits + 1)) / divisor) + 1;
                    }

                    ImGui::SameLine();

                    ImGui::TextUnformatted(" <=> ");

                    ImGui::SameLine();
                    ImGui::TextUnformatted("( X *");
                    ImGui::SameLine();
                    if (ImGui::InputHexadecimal("##multiplier", &multiplier)) {
                        if (multiplier == 0)
                            multiplier = 1;
                        divisor = ((1LLU << (numBits + 1)) / multiplier) + 1;
                    }

                    ImGui::SameLine();
                    ImGui::TextFormatted(") >> {}", numBits + 1);

                    ImGui::PopItemWidth();
                }
                ImGui::EndChild();
                ImGui::PopStyleColor();

                ImGui::PopStyleVar();
            }
            ImGui::EndChild();
        }

        void drawEuclidianAlgorithm() {
            static u64 a, b;

            static i64 gcdResult = 0;
            static i64 lcmResult = 0;
            static i64 p = 0, q = 0;
            static bool overflow = false;

            constexpr static auto extendedGcd = []<typename T>(T a, T b) -> std::pair<T, T> {
                T x = 1, y = 0;

                T xLast = 0, yLast = 1;

                while (b > 0) {
                    T quotient = a / b;

                    std::tie(x, xLast)  = std::tuple { xLast, x - quotient * xLast };
                    std::tie(y, yLast)  = std::tuple { yLast, y - quotient * yLast };
                    std::tie(a, b)      = std::tuple { b, a - quotient * b };
                }

                return { x, y };
            };

            ImGui::TextFormattedWrapped("{}", "hex.builtin.tools.euclidean_algorithm.description"_lang);

            ImGui::NewLine();

            if (ImGui::BeginBox()) {
                bool hasChanged = false;
                hasChanged = ImGui::InputScalar("A", ImGuiDataType_U64, &a) || hasChanged;
                hasChanged = ImGui::InputScalar("B", ImGuiDataType_U64, &b) || hasChanged;

                // Update results when input changed
                if (hasChanged) {

                    // Detect overflow
                    const u64 multiplicationResult = a * b;
                    if (a != 0 && multiplicationResult / a != b) {
                        gcdResult = 0;
                        lcmResult = 0;
                        p = 0;
                        q = 0;

                        overflow = true;
                    } else {
                        gcdResult       = std::gcd<i128, i128>(a, b);
                        lcmResult       = std::lcm<i128, i128>(a, b);
                        std::tie(p, q)  = extendedGcd(a, b);

                        overflow = false;
                    }
                }

                ImGui::Separator();

                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().DisabledAlpha);
                ImGui::InputScalar("gcd(A, B)", ImGuiDataType_S64, &gcdResult, nullptr, nullptr, "%llu", ImGuiInputTextFlags_ReadOnly);

                ImGui::Indent();
                ImGui::TextFormatted(ICON_VS_ARROW_RIGHT " a \u00D7 p  +  b \u00D7 q  =  ({0}) \u00D7 ({1})  +  ({2}) \u00D7 ({3})", a, p, b, q);
                ImGui::Unindent();

                ImGui::InputScalar("lcm(A, B)", ImGuiDataType_S64, &lcmResult, nullptr, nullptr, "%llu", ImGuiInputTextFlags_ReadOnly);
                ImGui::PopStyleVar();

                ImGui::EndBox();
            }

            if (overflow)
                ImGui::TextFormattedColored(ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed), "{}", "hex.builtin.tools.euclidean_algorithm.overflow"_lang);
            else
                ImGui::NewLine();

        }
    }

    void registerToolEntries() {
        ContentRegistry::Tools::add("hex.builtin.tools.demangler", impl::drawDemangler);
        ContentRegistry::Tools::add("hex.builtin.tools.ascii_table", impl::drawASCIITable);
        ContentRegistry::Tools::add("hex.builtin.tools.regex_replacer", impl::drawRegexReplacer);
        ContentRegistry::Tools::add("hex.builtin.tools.color", impl::drawColorPicker);
        ContentRegistry::Tools::add("hex.builtin.tools.calc", impl::drawMathEvaluator);
        ContentRegistry::Tools::add("hex.builtin.tools.graphing", impl::drawGraphingCalculator);
        ContentRegistry::Tools::add("hex.builtin.tools.base_converter", impl::drawBaseConverter);
        ContentRegistry::Tools::add("hex.builtin.tools.byte_swapper", impl::drawByteSwapper);
        ContentRegistry::Tools::add("hex.builtin.tools.permissions", impl::drawPermissionsCalculator);
        // ContentRegistry::Tools::add("hex.builtin.tools.file_uploader", impl::drawFileUploader);
        ContentRegistry::Tools::add("hex.builtin.tools.wiki_explain", impl::drawWikiExplainer);
        ContentRegistry::Tools::add("hex.builtin.tools.file_tools", impl::drawFileTools);
        ContentRegistry::Tools::add("hex.builtin.tools.ieee754", impl::drawIEEE754Decoder);
        ContentRegistry::Tools::add("hex.builtin.tools.invariant_multiplication", impl::drawInvariantMultiplicationDecoder);
        ContentRegistry::Tools::add("hex.builtin.tools.tcp_client_server", impl::drawTCPClientServer);
        ContentRegistry::Tools::add("hex.builtin.tools.euclidean_algorithm", impl::drawEuclidianAlgorithm);
    }

}
