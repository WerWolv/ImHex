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

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <content/popups/popup_notification.hpp>
#include <nlohmann/json.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/guards.hpp>
#include <charconv>

namespace hex::plugin::builtin {

    namespace {

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
                ImGui::TextFormattedWrapped("{}", demangledName);
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

            bool changed1 = ImGui::InputTextIcon("hex.builtin.tools.regex_replacer.pattern"_lang, ICON_VS_REGEX, regexPattern);
            bool changed2 = ImGui::InputTextIcon("hex.builtin.tools.regex_replacer.replace"_lang, ICON_VS_REGEX, replacePattern);
            bool changed3 = ImGui::InputTextMultiline("hex.builtin.tools.regex_replacer.input"_lang, regexInput, ImVec2(0, 0));

            if (changed1 || changed2 || changed3) {
                try {
                    regexOutput = std::regex_replace(regexInput.data(), std::regex(regexPattern.data()), replacePattern.data());
                } catch (std::regex_error &) { }
            }

            ImGui::InputTextMultiline("hex.builtin.tools.regex_replacer.output"_lang, regexOutput.data(), regexOutput.size(), ImVec2(0, 0), ImGuiInputTextFlags_ReadOnly);
        }

        void drawColorPicker() {
            static std::array<float, 4> pickedColor = { 0 };

            ImGui::SetNextItemWidth(300_scaled);
            ImGui::ColorPicker4("hex.builtin.tools.color"_lang, pickedColor.data(), ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHSV | ImGuiColorEditFlags_DisplayHex);
        }

        void drawMathEvaluator() {
            static std::vector<long double> mathHistory;
            static std::string lastMathError;
            static std::string mathInput;
            bool evaluate = false;

            static MathEvaluator<long double> mathEvaluator = [&] {
                MathEvaluator<long double> evaluator;

                evaluator.registerStandardVariables();
                evaluator.registerStandardFunctions();

                evaluator.setFunction(
                    "clear", [&](auto args) -> std::optional<long double> {
                        hex::unused(args);

                        mathHistory.clear();
                        lastMathError.clear();
                        mathEvaluator.getVariables().clear();
                        mathEvaluator.registerStandardVariables();
                        mathInput.clear();

                        return std::nullopt;
                    },
                    0,
                    0);

                evaluator.setFunction(
                    "read", [](auto args) -> std::optional<long double> {
                        u8 value = 0;

                        auto provider = ImHexApi::Provider::get();
                        if (!ImHexApi::Provider::isValid() || !provider->isReadable() || args[0] >= provider->getActualSize())
                            return std::nullopt;

                        provider->read(args[0], &value, sizeof(u8));

                        return value;
                    },
                    1,
                    1);

                evaluator.setFunction(
                    "write", [](auto args) -> std::optional<long double> {
                        auto provider = ImHexApi::Provider::get();
                        if (!ImHexApi::Provider::isValid() || !provider->isWritable() || args[0] >= provider->getActualSize())
                            return std::nullopt;

                        if (args[1] > 0xFF)
                            return std::nullopt;

                        u8 value = args[1];
                        provider->write(args[0], &value, sizeof(u8));

                        return std::nullopt;
                    },
                    2,
                    2);

                return evaluator;
            }();

            enum class MathDisplayType : u8 {
                Standard,
                Scientific,
                Engineering,
                Programmer
            } mathDisplayType = MathDisplayType::Standard;

            if (ImGui::BeginTabBar("##mathFormatTabBar")) {
                if (ImGui::BeginTabItem("hex.builtin.tools.format.standard"_lang)) {
                    mathDisplayType = MathDisplayType::Standard;
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("hex.builtin.tools.format.scientific"_lang)) {
                    mathDisplayType = MathDisplayType::Scientific;
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("hex.builtin.tools.format.engineering"_lang)) {
                    mathDisplayType = MathDisplayType::Engineering;
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("hex.builtin.tools.format.programmer"_lang)) {
                    mathDisplayType = MathDisplayType::Programmer;
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            if (ImGui::BeginTable("##mathWrapper", 3)) {
                ImGui::TableSetupColumn("##keypad", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize);
                ImGui::TableSetupColumn("##results", ImGuiTableColumnFlags_WidthStretch, 0.666);
                ImGui::TableSetupColumn("##variables", ImGuiTableColumnFlags_WidthStretch, 0.666);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                auto buttonSize = ImVec2(3, 2) * ImGui::GetTextLineHeightWithSpacing();

                if (ImGui::Button("Ans", buttonSize)) mathInput += "ans";
                ImGui::SameLine();
                if (ImGui::Button("Pi", buttonSize)) mathInput += "pi";
                ImGui::SameLine();
                if (ImGui::Button("e", buttonSize)) mathInput += "e";
                ImGui::SameLine();
                if (ImGui::Button("CE", buttonSize)) mathInput.clear();
                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_BACKSPACE, buttonSize)) mathInput.clear();

                ImGui::SameLine();
                ImGui::NewLine();

                switch (mathDisplayType) {
                    case MathDisplayType::Standard:
                    case MathDisplayType::Scientific:
                    case MathDisplayType::Engineering:
                        if (ImGui::Button("x²", buttonSize)) mathInput += "** 2";
                        ImGui::SameLine();
                        if (ImGui::Button("1/x", buttonSize)) mathInput += "1/";
                        ImGui::SameLine();
                        if (ImGui::Button("|x|", buttonSize)) mathInput += "abs";
                        ImGui::SameLine();
                        if (ImGui::Button("exp", buttonSize)) mathInput += "e ** ";
                        ImGui::SameLine();
                        if (ImGui::Button("%", buttonSize)) mathInput += "%";
                        ImGui::SameLine();
                        break;
                    case MathDisplayType::Programmer:
                        if (ImGui::Button("<<", buttonSize)) mathInput += "<<";
                        ImGui::SameLine();
                        if (ImGui::Button(">>", buttonSize)) mathInput += ">>";
                        ImGui::SameLine();
                        if (ImGui::Button("&", buttonSize)) mathInput += "&";
                        ImGui::SameLine();
                        if (ImGui::Button("|", buttonSize)) mathInput += "|";
                        ImGui::SameLine();
                        if (ImGui::Button("^", buttonSize)) mathInput += "^";
                        ImGui::SameLine();
                        break;
                }
                ImGui::NewLine();
                if (ImGui::Button("sqrt", buttonSize)) mathInput += "sqrt";
                ImGui::SameLine();
                if (ImGui::Button("(", buttonSize)) mathInput += "(";
                ImGui::SameLine();
                if (ImGui::Button(")", buttonSize)) mathInput += ")";
                ImGui::SameLine();
                if (ImGui::Button("sign", buttonSize)) mathInput += "sign";
                ImGui::SameLine();
                if (ImGui::Button("÷", buttonSize)) mathInput += "/";
                ImGui::SameLine();
                ImGui::NewLine();
                if (ImGui::Button("xª", buttonSize)) mathInput += "**";
                ImGui::SameLine();
                if (ImGui::Button("7", buttonSize)) mathInput += "7";
                ImGui::SameLine();
                if (ImGui::Button("8", buttonSize)) mathInput += "8";
                ImGui::SameLine();
                if (ImGui::Button("9", buttonSize)) mathInput += "9";
                ImGui::SameLine();
                if (ImGui::Button("×", buttonSize)) mathInput += "*";
                ImGui::SameLine();
                ImGui::NewLine();
                if (ImGui::Button("log", buttonSize)) mathInput += "log";
                ImGui::SameLine();
                if (ImGui::Button("4", buttonSize)) mathInput += "4";
                ImGui::SameLine();
                if (ImGui::Button("5", buttonSize)) mathInput += "5";
                ImGui::SameLine();
                if (ImGui::Button("6", buttonSize)) mathInput += "6";
                ImGui::SameLine();
                if (ImGui::Button("-", buttonSize)) mathInput += "-";
                ImGui::SameLine();
                ImGui::NewLine();
                if (ImGui::Button("ln", buttonSize)) mathInput += "ln";
                ImGui::SameLine();
                if (ImGui::Button("1", buttonSize)) mathInput += "1";
                ImGui::SameLine();
                if (ImGui::Button("2", buttonSize)) mathInput += "2";
                ImGui::SameLine();
                if (ImGui::Button("3", buttonSize)) mathInput += "3";
                ImGui::SameLine();
                if (ImGui::Button("+", buttonSize)) mathInput += "+";
                ImGui::SameLine();
                ImGui::NewLine();
                if (ImGui::Button("lb", buttonSize)) mathInput += "lb";
                ImGui::SameLine();
                if (ImGui::Button("x=", buttonSize)) mathInput += "=";
                ImGui::SameLine();
                if (ImGui::Button("0", buttonSize)) mathInput += "0";
                ImGui::SameLine();
                if (ImGui::Button(".", buttonSize)) mathInput += ".";
                ImGui::SameLine();

                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetCustomColorVec4(ImGuiCustomCol_DescButtonHovered));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetCustomColorVec4(ImGuiCustomCol_DescButton));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetCustomColorVec4(ImGuiCustomCol_DescButtonActive));
                if (ImGui::Button("=", buttonSize)) evaluate = true;
                ImGui::SameLine();
                ImGui::PopStyleColor(3);

                ImGui::NewLine();

                ImGui::TableNextColumn();

                if (ImGui::BeginTable("##mathHistory", 1, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg, ImVec2(0, 300))) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("hex.builtin.tools.history"_lang);

                    ImGuiListClipper clipper;
                    clipper.Begin(mathHistory.size());

                    ImGui::TableHeadersRow();
                    while (clipper.Step()) {
                        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                            if (i == 0)
                                ImGui::PushStyleColor(ImGuiCol_Text, ImU32(ImColor(0xA5, 0x45, 0x45)));

                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();

                            switch (mathDisplayType) {
                                case MathDisplayType::Standard:
                                    ImGui::TextFormatted("{0:.3Lf}", mathHistory[(mathHistory.size() - 1) - i]);
                                    break;
                                case MathDisplayType::Scientific:
                                    ImGui::TextFormatted("{0:.6Lg}", mathHistory[(mathHistory.size() - 1) - i]);
                                    break;
                                case MathDisplayType::Engineering:
                                    ImGui::TextFormatted("{0}", hex::toEngineeringString(mathHistory[(mathHistory.size() - 1) - i]).c_str());
                                    break;
                                case MathDisplayType::Programmer:
                                    ImGui::TextFormatted("0x{0:X} ({1})",
                                        u64(mathHistory[(mathHistory.size() - 1) - i]),
                                        u64(mathHistory[(mathHistory.size() - 1) - i]));
                                    break;
                            }

                            if (i == 0)
                                ImGui::PopStyleColor();
                        }
                    }

                    clipper.End();

                    ImGui::EndTable();
                }

                ImGui::TableNextColumn();
                if (ImGui::BeginTable("##mathVariables", 2, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg, ImVec2(0, 300))) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("hex.builtin.tools.name"_lang);
                    ImGui::TableSetupColumn("hex.builtin.tools.value"_lang);

                    ImGui::TableHeadersRow();
                    for (const auto &[name, variable] : mathEvaluator.getVariables()) {
                        const auto &[value, constant] = variable;

                        if (constant)
                            continue;

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(name.c_str());

                        ImGui::TableNextColumn();
                        switch (mathDisplayType) {
                            case MathDisplayType::Standard:
                                ImGui::TextFormatted("{0:.3Lf}", value);
                                break;
                            case MathDisplayType::Scientific:
                                ImGui::TextFormatted("{0:.6Lg}", value);
                                break;
                            case MathDisplayType::Engineering:
                                ImGui::TextFormatted("{}", hex::toEngineeringString(value));
                                break;
                            case MathDisplayType::Programmer:
                                ImGui::TextFormatted("0x{0:X} ({1})", u64(value), u64(value));
                                break;
                        }
                    }

                    ImGui::EndTable();
                }

                ImGui::EndTable();
            }

            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::InputTextIcon("##input", ICON_VS_SYMBOL_OPERATOR, mathInput, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                ImGui::SetKeyboardFocusHere();
                evaluate = true;
            }
            ImGui::PopItemWidth();

            if (!lastMathError.empty())
                ImGui::TextFormattedColored(ImColor(0xA00040FF), "hex.builtin.tools.error"_lang, lastMathError);
            else
                ImGui::NewLine();

            if (evaluate) {
                try {
                    auto result = mathEvaluator.evaluate(mathInput);
                    mathInput.clear();
                    if (result.has_value()) {
                        mathHistory.push_back(result.value());
                        lastMathError.clear();
                    }
                } catch (std::invalid_argument &e) {
                    lastMathError = e.what();
                }
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

        void drawPermissionsCalculator() {
            static bool setuid, setgid, sticky;
            static bool r[3], w[3], x[3];

            ImGui::TextUnformatted("hex.builtin.tools.permissions.perm_bits"_lang);
            ImGui::Separator();

            if (ImGui::BeginTable("Permissions", 4, ImGuiTableFlags_Borders)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Special", ImGuiTableColumnFlags_NoSort);
                ImGui::TableSetupColumn("User", ImGuiTableColumnFlags_NoSort);
                ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_NoSort);
                ImGui::TableSetupColumn("Other", ImGuiTableColumnFlags_NoSort);

                ImGui::TableHeadersRow();

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::Checkbox("setuid", &setuid);
                ImGui::Checkbox("setgid", &setgid);
                ImGui::Checkbox("Sticky bit", &sticky);

                for (u8 i = 0; i < 3; i++) {
                    ImGui::TableNextColumn();

                    ImGui::PushID(i);
                    ImGui::Checkbox("Read", &r[i]);
                    ImGui::Checkbox("Write", &w[i]);
                    ImGui::Checkbox("Execute", &x[i]);
                    ImGui::PopID();
                }

                ImGui::EndTable();
            }

            ImGui::NewLine();
            ImGui::TextUnformatted("hex.builtin.tools.permissions.absolute"_lang);
            ImGui::Separator();

            auto result = hex::format("{}{}{}{}",
                (setuid << 2) | (setgid << 1) | (sticky << 0),
                (r[0] << 2) | (w[0] << 1) | (x[0] << 0),
                (r[1] << 2) | (w[1] << 1) | (x[1] << 0),
                (r[2] << 2) | (w[2] << 1) | (x[2] << 0));
            ImGui::InputText("##permissions_absolute", result.data(), result.size(), ImGuiInputTextFlags_ReadOnly);

            ImGui::NewLine();

            static const auto WarningColor = ImColor(0.92F, 0.25F, 0.2F, 1.0F);
            if (setuid && !x[0])
                ImGui::TextFormattedColored(WarningColor, "{}", "hex.builtin.tools.permissions.setuid_error"_lang);
            if (setgid && !x[1])
                ImGui::TextFormattedColored(WarningColor, "{}", "hex.builtin.tools.permissions.setgid_error"_lang);
            if (sticky && !x[2])
                ImGui::TextFormattedColored(WarningColor, "{}", "hex.builtin.tools.permissions.sticky_error"_lang);
        }

        void drawFileUploader() {
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
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("hex.builtin.common.file"_lang);
                ImGui::TableSetupColumn("hex.builtin.common.link"_lang);
                ImGui::TableSetupColumn("hex.builtin.common.size"_lang);
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
                            json["data"]["file"]["url"]["short"],
                            json["data"]["file"]["metadata"]["size"]["readable"]
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
        }

        std::string getWikipediaApiUrl() {
            auto setting = ContentRegistry::Settings::read("hex.builtin.setting.interface", "hex.builtin.setting.interface.wiki_explain_language", "en");
            return "https://" + setting + ".wikipedia.org/w/api.php?format=json&action=query&prop=extracts&explaintext&redirects=10&formatversion=2";
        }

        void drawWikiExplainer() {
            static HttpRequest request("GET", "");

            static std::string resultTitle, resultExtract;
            static std::future<HttpRequest::Result<std::string>> searchProcess;
            static bool extendedSearch = false;

            std::string searchString;

            ImGui::Header("hex.builtin.tools.wiki_explain.control"_lang, true);

            bool startSearch;

            startSearch = ImGui::InputTextIcon("##search", ICON_VS_SYMBOL_KEY, searchString, ImGuiInputTextFlags_EnterReturnsTrue);
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

                    resultTitle   = json["query"]["pages"][0]["title"].get<std::string>();
                    resultExtract = json["query"]["pages"][0]["extract"].get<std::string>();

                    if (!extendedSearch && resultExtract.ends_with(':')) {
                        extendedSearch = true;

                        request.setUrl(getWikipediaApiUrl() + "&titles="s + request.urlEncode(searchString));
                        searchProcess  = request.execute();

                        resultTitle.clear();
                        resultExtract.clear();
                    } else {
                        extendedSearch = false;
                        searchString.clear();
                    }
                } catch (...) {
                    resultTitle.clear();
                    resultExtract.clear();
                    extendedSearch = false;
                    searchProcess  = {};

                    resultTitle   = "???";
                    resultExtract = "hex.builtin.tools.wiki_explain.invalid_response"_lang.get();
                }
            }
        }


        void drawFileToolShredder() {
            static std::u8string selectedFile;
            static bool fastMode     = false;
            static TaskHolder shredderTask;

            ImGui::TextUnformatted("hex.builtin.tools.file_tools.shredder.warning"_lang);
            ImGui::NewLine();

            if (ImGui::BeginChild("settings", { 0, ImGui::GetTextLineHeightWithSpacing() * 4 }, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                ImGui::BeginDisabled(shredderTask.isRunning());
                {
                    ImGui::TextUnformatted("hex.builtin.tools.file_tools.shredder.input"_lang);
                    ImGui::SameLine();
                    ImGui::InputText("##path", selectedFile);
                    ImGui::SameLine();
                    if (ImGui::Button("...")) {
                        fs::openFileBrowser(fs::DialogMode::Open, {}, [](const auto &path) {
                            selectedFile = path.u8string();
                        });
                    }

                    ImGui::Checkbox("hex.builtin.tools.file_tools.shredder.fast"_lang, &fastMode);
                }
                ImGui::EndDisabled();
            }
            ImGui::EndChild();

            if (shredderTask.isRunning())
                ImGui::TextSpinner("hex.builtin.tools.file_tools.shredder.shredding"_lang);
            else {
                ImGui::BeginDisabled(selectedFile.empty());
                {
                    if (ImGui::Button("hex.builtin.tools.file_tools.shredder.shred"_lang)) {
                        shredderTask = TaskManager::createTask("hex.builtin.tools.file_tools.shredder.shredding", 0, [](auto &task) {
                            ON_SCOPE_EXIT {
                                selectedFile.clear();
                            };
                            wolv::io::File file(selectedFile, wolv::io::File::Mode::Write);

                            if (!file.isValid()) {
                                PopupError::open("hex.builtin.tools.file_tools.shredder.error.open"_lang);
                                return;
                            }

                            task.setMaxValue(file.getSize());

                            std::vector<std::array<u8, 3>> overwritePattern;
                            if (fastMode) {
                                /* Should be sufficient for modern disks */
                                overwritePattern.push_back({ 0x00, 0x00, 0x00 });
                                overwritePattern.push_back({ 0xFF, 0xFF, 0xFF });
                            } else {
                                /* Gutmann's method. Secure for magnetic storage */
                                std::random_device rd;
                                std::uniform_int_distribution<u8> dist(0x00, 0xFF);

                                /* Fill fixed patterns */
                                overwritePattern = {
                                        {    },
                                        {    },
                                        {},
                                        {},
                                        { 0x55, 0x55, 0x55 },
                                        { 0xAA, 0xAA, 0xAA },
                                        { 0x92, 0x49, 0x24 },
                                        { 0x49, 0x24, 0x92 },
                                        { 0x24, 0x92, 0x49 },
                                        { 0x00, 0x00, 0x00 },
                                        { 0x11, 0x11, 0x11 },
                                        { 0x22, 0x22, 0x22 },
                                        { 0x33, 0x33, 0x44 },
                                        { 0x55, 0x55, 0x55 },
                                        { 0x66, 0x66, 0x66 },
                                        { 0x77, 0x77, 0x77 },
                                        { 0x88, 0x88, 0x88 },
                                        { 0x99, 0x99, 0x99 },
                                        { 0xAA, 0xAA, 0xAA },
                                        { 0xBB, 0xBB, 0xBB },
                                        { 0xCC, 0xCC, 0xCC },
                                        { 0xDD, 0xDD, 0xDD },
                                        { 0xEE, 0xEE, 0xEE },
                                        { 0xFF, 0xFF, 0xFF },
                                        { 0x92, 0x49, 0x24 },
                                        { 0x49, 0x24, 0x92 },
                                        { 0x24, 0x92, 0x49 },
                                        { 0x6D, 0xB6, 0xDB },
                                        { 0xB6, 0xDB, 0x6D },
                                        { 0xBD, 0x6D, 0xB6 },
                                        {},
                                        {},
                                        {},
                                        {}
                                };

                                /* Fill random patterns */
                                for (u8 i = 0; i < 4; i++)
                                    overwritePattern[i] = { dist(rd), dist(rd), dist(rd) };
                                for (u8 i = 0; i < 4; i++)
                                    overwritePattern[overwritePattern.size() - 1 - i] = { dist(rd), dist(rd), dist(rd) };
                            }

                            size_t fileSize = file.getSize();

                            for (const auto &pattern : overwritePattern) {
                                for (u64 offset = 0; offset < fileSize; offset += 3) {
                                    file.writeBuffer(pattern.data(), std::min<u64>(pattern.size(), fileSize - offset));
                                    task.update(offset);
                                }

                                file.flush();
                            }

                            file.remove();

                            PopupInfo::open("hex.builtin.tools.file_tools.shredder.success"_lang);
                        });
                    }
                }
                ImGui::EndDisabled();
            }
        }

        void drawFileToolSplitter() {
            std::array sizeText = {
                (const char *)"hex.builtin.tools.file_tools.splitter.sizes.5_75_floppy"_lang,
                (const char *)"hex.builtin.tools.file_tools.splitter.sizes.3_5_floppy"_lang,
                (const char *)"hex.builtin.tools.file_tools.splitter.sizes.zip100"_lang,
                (const char *)"hex.builtin.tools.file_tools.splitter.sizes.zip200"_lang,
                (const char *)"hex.builtin.tools.file_tools.splitter.sizes.cdrom650"_lang,
                (const char *)"hex.builtin.tools.file_tools.splitter.sizes.cdrom700"_lang,
                (const char *)"hex.builtin.tools.file_tools.splitter.sizes.fat32"_lang,
                (const char *)"hex.builtin.tools.file_tools.splitter.sizes.custom"_lang
            };
            std::array<u64, sizeText.size()> sizes = {
                1200_KiB,
                1400_KiB,
                100_MiB,
                200_MiB,
                650_MiB,
                700_MiB,
                4_GiB,
                1
            };

            static std::u8string selectedFile;
            static std::u8string baseOutputPath;
            static u64 splitSize = sizes[0];
            static int selectedItem = 0;
            static TaskHolder splitterTask;

            if (ImGui::BeginChild("split_settings", { 0, ImGui::GetTextLineHeightWithSpacing() * 7 }, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                ImGui::BeginDisabled(splitterTask.isRunning());
                {
                    ImGui::InputText("##path", selectedFile);
                    ImGui::SameLine();
                    if (ImGui::Button("...##input")) {
                        fs::openFileBrowser(fs::DialogMode::Open, {}, [](const auto &path) {
                            selectedFile = path.u8string();
                        });
                    }
                    ImGui::SameLine();
                    ImGui::TextUnformatted("hex.builtin.tools.file_tools.splitter.input"_lang);

                    ImGui::InputText("##base_path", baseOutputPath);
                    ImGui::SameLine();
                    if (ImGui::Button("...##output")) {
                        fs::openFileBrowser(fs::DialogMode::Save, {}, [](const auto &path) {
                            baseOutputPath = path.u8string();
                        });
                    }
                    ImGui::SameLine();
                    ImGui::TextUnformatted("hex.builtin.tools.file_tools.splitter.output"_lang);

                    ImGui::Separator();

                    if (ImGui::Combo("###part_size", &selectedItem, sizeText.data(), sizeText.size())) {
                        splitSize = sizes[selectedItem];
                    }
                }
                ImGui::EndDisabled();
                ImGui::BeginDisabled(splitterTask.isRunning() || selectedItem != sizes.size() - 1);
                {
                    ImGui::InputScalar("###custom_size", ImGuiDataType_U64, &splitSize);
                    ImGui::SameLine();
                    ImGui::TextUnformatted("Bytes");
                }
                ImGui::EndDisabled();
            }
            ImGui::EndChild();

            ImGui::BeginDisabled(selectedFile.empty() || baseOutputPath.empty() || splitSize == 0);
            {
                if (splitterTask.isRunning())
                    ImGui::TextSpinner("hex.builtin.tools.file_tools.splitter.picker.splitting"_lang);
                else {
                    if (ImGui::Button("hex.builtin.tools.file_tools.splitter.picker.split"_lang)) {
                        splitterTask = TaskManager::createTask("hex.builtin.tools.file_tools.splitter.picker.splitting", 0, [](auto &task) {
                            ON_SCOPE_EXIT {
                                selectedFile.clear();
                                baseOutputPath.clear();
                            };

                            wolv::io::File file(selectedFile, wolv::io::File::Mode::Read);

                            if (!file.isValid()) {
                                PopupError::open("hex.builtin.tools.file_tools.splitter.picker.error.open"_lang);
                                return;
                            }

                            if (file.getSize() < splitSize) {
                                PopupError::open("hex.builtin.tools.file_tools.splitter.picker.error.size"_lang);
                                return;
                            }

                            task.setMaxValue(file.getSize());

                            u32 index = 1;
                            for (u64 offset = 0; offset < file.getSize(); offset += splitSize) {
                                task.update(offset);

                                std::fs::path path = baseOutputPath;
                                path += hex::format(".{:05}", index);

                                wolv::io::File partFile(path, wolv::io::File::Mode::Create);

                                if (!partFile.isValid()) {
                                    PopupError::open(hex::format("hex.builtin.tools.file_tools.splitter.picker.error.create"_lang, index));
                                    return;
                                }

                                constexpr static auto BufferSize = 0xFF'FFFF;
                                for (u64 partOffset = 0; partOffset < splitSize; partOffset += BufferSize) {
                                    partFile.writeVector(file.readVector(std::min<u64>(BufferSize, splitSize - partOffset)));
                                    partFile.flush();
                                }

                                index++;
                            }

                            PopupInfo::open("hex.builtin.tools.file_tools.splitter.picker.success"_lang);
                        });
                    }
                }
            }
            ImGui::EndDisabled();
        }

        void drawFileToolCombiner() {
            static std::vector<std::fs::path> files;
            static std::u8string outputPath;
            static u32 selectedIndex;
            static TaskHolder combinerTask;

            if (ImGui::BeginTable("files_table", 2, ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("file list", ImGuiTableColumnFlags_NoHeaderLabel, 10);
                ImGui::TableSetupColumn("buttons", ImGuiTableColumnFlags_NoHeaderLabel, 1);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                if (ImGui::BeginListBox("##files", { -FLT_MIN, 10 * ImGui::GetTextLineHeightWithSpacing() })) {
                    u32 index = 0;
                    for (auto &file : files) {
                        if (ImGui::Selectable(wolv::util::toUTF8String(file).c_str(), index == selectedIndex))
                            selectedIndex = index;
                        index++;
                    }

                    ImGui::EndListBox();
                }

                ImGui::TableNextColumn();

                ImGui::BeginDisabled(selectedIndex <= 0);
                {
                    if (ImGui::ArrowButton("move_up", ImGuiDir_Up)) {
                        std::iter_swap(files.begin() + selectedIndex, files.begin() + selectedIndex - 1);
                        selectedIndex--;
                    }
                }
                ImGui::EndDisabled();

                ImGui::BeginDisabled(files.empty() || selectedIndex >= files.size() - 1);
                {
                    if (ImGui::ArrowButton("move_down", ImGuiDir_Down)) {
                        std::iter_swap(files.begin() + selectedIndex, files.begin() + selectedIndex + 1);
                        selectedIndex++;
                    }
                }
                ImGui::EndDisabled();

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::BeginDisabled(combinerTask.isRunning());
                {
                    if (ImGui::Button("hex.builtin.tools.file_tools.combiner.add"_lang)) {
                        fs::openFileBrowser(fs::DialogMode::Open, {}, [](const auto &path) {
                            files.push_back(path);
                        });
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("hex.builtin.tools.file_tools.combiner.delete"_lang)) {
                        files.erase(files.begin() + selectedIndex);
                        selectedIndex--;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("hex.builtin.tools.file_tools.combiner.clear"_lang)) {
                        files.clear();
                    }
                }
                ImGui::EndDisabled();

                ImGui::EndTable();
            }

            ImGui::BeginDisabled(combinerTask.isRunning());
            {
                ImGui::InputText("##output_path", outputPath);
                ImGui::SameLine();
                if (ImGui::Button("...")) {
                    fs::openFileBrowser(fs::DialogMode::Save, {}, [](const auto &path) {
                        outputPath = path.u8string();
                    });
                }
                ImGui::SameLine();
                ImGui::TextUnformatted("hex.builtin.tools.file_tools.combiner.output"_lang);
            }
            ImGui::EndDisabled();

            ImGui::BeginDisabled(files.empty() || outputPath.empty());
            {
                if (combinerTask.isRunning())
                    ImGui::TextSpinner("hex.builtin.tools.file_tools.combiner.combining"_lang);
                else {
                    if (ImGui::Button("hex.builtin.tools.file_tools.combiner.combine"_lang)) {
                        combinerTask = TaskManager::createTask("hex.builtin.tools.file_tools.combiner.combining", 0, [](auto &task) {
                            wolv::io::File output(outputPath, wolv::io::File::Mode::Create);

                            if (!output.isValid()) {
                                PopupError::open("hex.builtin.tools.file_tools.combiner.error.open_output"_lang);
                                return;
                            }

                            task.setMaxValue(files.size());

                            u64 fileIndex = 0;
                            for (const auto &file : files) {
                                task.update(fileIndex);
                                fileIndex++;

                                wolv::io::File input(file, wolv::io::File::Mode::Read);
                                if (!input.isValid()) {
                                    PopupError::open(hex::format("hex.builtin.tools.file_tools.combiner.open_input"_lang, wolv::util::toUTF8String(file)));
                                    return;
                                }

                                constexpr static auto BufferSize = 0xFF'FFFF;
                                auto inputSize = input.getSize();
                                for (u64 inputOffset = 0; inputOffset < inputSize; inputOffset += BufferSize) {
                                    output.writeVector(input.readVector(std::min<u64>(BufferSize, inputSize - inputOffset)));
                                    output.flush();
                                }
                            }

                            files.clear();
                            selectedIndex = 0;
                            outputPath.clear();

                            PopupInfo::open("hex.builtin.tools.file_tools.combiner.success"_lang);
                        });
                    }
                }
            }
            ImGui::EndDisabled();
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
                    drawFileToolCombiner();
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }

        // Tool for converting between different number formats
        // There are three places where input can be changed; the bit checkboxes, the hex input and the decimal input.
        // The bit checkboxes and the hex input are directly related and can be converted between each other easily.
        // The decimal input is a bit more complicated. IEEE 754 floating point numbers are represented as a sign bit,
        // an exponent and a mantissa. For details see https://en.wikipedia.org/wiki/IEEE_754.
        // Workflow is as follows:
        // From the bit checkboxes determine the integer hex value. This is straightforward.
        // From the hex value determine the binary floating point value by extracting the sign, exponent and mantissa.
        // From the binary floating point value determine the decimal floating point value using third party library.
        // From the decimal floating point we reconstruct the binary floating point value using internal hardware.
        // If format is non-standard the reconstruction is done using properties of the format.
        void drawIEEE754Decoder() {

            static u128 value = 0;
            static int exponentBitCount = 8, mantissaBitCount = 23;
            long double exponentValue;
            long double mantissaValue;
            static long double resultFloat;
            i64 exponentBias;
            constexpr static auto flags = ImGuiInputTextFlags_EnterReturnsTrue;
            constexpr static auto logBaseTenOfTwo = 0.30102999566398119521373889472449L;

            enum class NumberKind {
                Normal,
                Zero,
                Denormal,
                Infinity,
                NaN,
            } numberKind;

            enum class NumberType {
                Regular,
                SignalingNaN,
                QuietNaN,
                NegativeInfinity,
                PositiveInfinity,
            } numberType;

            const static auto BitCheckbox = [](u8 bit) {
                bool checkbox = false;
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
                checkbox = (value & (u128(1) << bit)) != 0;
                ImGui::BitCheckbox("##checkbox", &checkbox);
                value = (value & ~(u128(1) << bit)) | (u128(checkbox) << bit);
                ImGui::PopStyleVar();
            };

            const static auto BitCheckboxes = [](u32 startBit, u32 count) {
                for (u32 i = 0; i < count; i++) {
                    ImGui::PushID(startBit - i);
                    BitCheckbox(startBit - i);
                    ImGui::SameLine(0, 0);
                    ImGui::PopID();
                }
            };

            ImVec4 textColor =  ImGui::GetStyleColorVec4(ImGuiCol_Text);
            const bool isThemeDark = ImLengthSqr(textColor) > 0.5f;
            const auto totalBitCount = exponentBitCount + mantissaBitCount;
            const auto signBitPosition = totalBitCount - 0;
            const auto exponentBitPosition = totalBitCount - 1;
            const auto mantissaBitPosition = totalBitCount - 1 - exponentBitCount;

            const static auto ExtractBits = [](u32 startBit, u32 count) {
                return hex::extract(startBit, startBit - (count - 1), value);
            };

            i64 signBits = ExtractBits(signBitPosition, 1);
            i64 exponentBits = ExtractBits(exponentBitPosition, exponentBitCount);
            i64 mantissaBits = ExtractBits(mantissaBitPosition, mantissaBitCount);

            static i64 inputFieldWidth = 0;
            ImGui::TextFormattedWrapped("{}", "hex.builtin.tools.ieee754.description"_lang);
            ImGui::NewLine();

            static i64 displayMode = ContentRegistry::Settings::read("hex.builtin.tools.ieee754.settings", "display_mode", 0);
            i64 displayModeTemp = displayMode;
            ImGui::RadioButton("hex.builtin.tools.ieee754.settings.display_mode.detailed"_lang, reinterpret_cast<int *>(&displayMode), 0);
            ImGui::SameLine();

            ImGui::RadioButton("hex.builtin.tools.ieee754.settings.display_mode.simplified"_lang, reinterpret_cast<int *>(&displayMode), 1);
            if (displayModeTemp != displayMode) {
                ContentRegistry::Settings::write("hex.builtin.tools.ieee754.settings", "display_mode", displayMode);
                displayModeTemp = displayMode;
            }

            auto tableFlags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoKeepColumnsVisible | ImGuiTableFlags_ScrollX;
            if (ImGui::BeginTable("##outer", 8, tableFlags, ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 5.5 ))) {
                ImGui::TableSetupColumn("hex.builtin.tools.ieee754.result.title"_lang);
                ImGui::TableSetupColumn("##equals");
                ImGui::TableSetupColumn("hex.builtin.tools.ieee754.sign"_lang);
                ImGui::TableSetupColumn("##times");
                ImGui::TableSetupColumn("hex.builtin.tools.ieee754.exponent"_lang);
                ImGui::TableSetupColumn("##times");
                ImGui::TableSetupColumn("hex.builtin.tools.ieee754.mantissa"_lang);
                ImGui::TableHeadersRow();
                ImGui::TableNextRow();

                // Row for bit labels. Due to font size constrains each bit cannot have its own label.
                // Instead, we label each 4 bits and then use the bit position to determine the bit label.
                // Result
                ImGui::TableNextColumn();
                // Equals
                ImGui::TableNextColumn();
                // Sign bit label is always shown
                ImGui::TableNextColumn();

                std::string labelString = fmt::format("{}",totalBitCount+1);
                auto columnWidth = ImGui::GetColumnWidth();
                auto checkBoxWidth = columnWidth - 20_scaled;
                auto labelWidth = ImGui::CalcTextSize(labelString.c_str()).x;
                auto indentSize = 20_scaled + checkBoxWidth /2 - labelWidth / 2;
                ImGui::Indent(indentSize);
                ImGui::TextUnformatted(labelString.c_str());
                ImGui::Unindent(indentSize);

                // Times
                ImGui::TableNextColumn();
                // Exponent
                ImGui::TableNextColumn();

                if (exponentBitCount < 4) { // In this case we always label one box
                    labelString = "xx";
                    checkBoxWidth = ImGui::CalcTextSize(labelString.c_str()).x;
                    auto centerBox = exponentBitCount == 3 || exponentBitCount == 2 ? 1 : 0;

                    auto labelIndex = totalBitCount - centerBox;
                    labelString = fmt::format("{}", labelIndex);
                    labelWidth = ImGui::CalcTextSize(labelString.c_str()).x;
                    indentSize = checkBoxWidth * centerBox;
                    auto centeredIndentSize = indentSize + checkBoxWidth / 2 - labelWidth / 2;

                    if (centeredIndentSize == 0) // Fix for imgui reposition bug only happens if first checkbox has label
                        centeredIndentSize -= 1;
                    ImGui::Indent(centeredIndentSize);
                    ImGui::TextUnformatted(labelString.c_str());
                    ImGui::Unindent(centeredIndentSize);

                } else {
                    columnWidth = ImGui::GetColumnWidth();
                    checkBoxWidth = columnWidth / exponentBitCount;

                    auto labelIndex = totalBitCount - totalBitCount % 4;
                    labelString = fmt::format("{}", labelIndex);
                    labelWidth = ImGui::CalcTextSize(labelString.c_str()).x;
                    indentSize = (totalBitCount % 4) * checkBoxWidth;// indent size for checkbox
                    auto centeredIndentSize = indentSize + checkBoxWidth / 2 - labelWidth / 2;

                    if (centeredIndentSize == 0) // Fix for imgui reposition bug only happens if first checkbox has label
                        centeredIndentSize -= 1;
                    ImGui::Indent(centeredIndentSize);
                    ImGui::TextUnformatted(labelString.c_str());
                    ImGui::Unindent(centeredIndentSize);

                    auto boxesLeft = exponentBitCount - (totalBitCount % 4);
                    auto labelsLeft = boxesLeft / 4;
                    if (boxesLeft % 4 == 0) // If we have a multiple of 4 boxes left the last label belongs to mantissa
                        labelsLeft--;

                    for (i64 i = 0; i < labelsLeft; i++) {
                        ImGui::SameLine();

                        labelIndex -= 4;
                        labelString = fmt::format("{}", labelIndex);
                        labelWidth = ImGui::CalcTextSize(labelString.c_str()).x;
                        indentSize += 4 * checkBoxWidth;

                        ImGui::Indent(indentSize + checkBoxWidth / 2 - labelWidth / 2);
                        ImGui::TextFormatted("{}", labelIndex);
                        ImGui::Unindent(indentSize + checkBoxWidth / 2 - labelWidth / 2);
                    }
                }

                // Times
                ImGui::TableNextColumn();
                // Mantissa
                ImGui::TableNextColumn();

                if (mantissaBitCount < 4) { // in this case we always label one
                    labelString = "xx";
                    checkBoxWidth = ImGui::CalcTextSize(labelString.c_str()).x;
                    auto centerBox = mantissaBitCount == 3 || mantissaBitCount == 2 ? 1 : 0;

                    auto labelIndex = mantissaBitCount - centerBox;
                    labelString = fmt::format("{}", labelIndex);
                    labelWidth = ImGui::CalcTextSize(labelString.c_str()).x;
                    indentSize = checkBoxWidth * centerBox;
                    auto centeredIndentSize = indentSize + checkBoxWidth / 2 - labelWidth / 2;

                    if (centeredIndentSize == 0) // Fix for imgui reposition bug only happens if first checkbox has label
                        centeredIndentSize -= 1;
                    ImGui::Indent(centeredIndentSize);
                    ImGui::TextUnformatted(labelString.c_str());
                    ImGui::Unindent(centeredIndentSize);

                } else {
                    columnWidth = ImGui::GetColumnWidth();
                    checkBoxWidth = columnWidth / mantissaBitCount;

                    auto labelIndex = mantissaBitCount - mantissaBitCount % 4;
                    labelString = fmt::format("{}", labelIndex);
                    labelWidth = ImGui::CalcTextSize(labelString.c_str()).x;
                    indentSize = (mantissaBitCount % 4) * checkBoxWidth;// indent size for checkbox
                    auto centeredIndentSize = indentSize + checkBoxWidth / 2 - labelWidth / 2;

                    if (centeredIndentSize == 0) // Fix for imgui reposition bug only happens if first checkbox has label
                        centeredIndentSize -= 1;
                    ImGui::Indent(centeredIndentSize);
                    ImGui::TextUnformatted(labelString.c_str());
                    ImGui::Unindent(centeredIndentSize);

                    auto labelsLeft = labelIndex / 4;
                    if (labelIndex % 4 == 0) // If we have a multiple of 4 boxes left the last label is not printed
                        labelsLeft--;

                    for (i64 i = 0; i < labelsLeft; i++) {
                        ImGui::SameLine();

                        labelIndex -= 4;
                        labelString = fmt::format("{}", labelIndex);
                        labelWidth = ImGui::CalcTextSize(labelString.c_str()).x;
                        indentSize += 4 * checkBoxWidth;

                        ImGui::Indent(indentSize + checkBoxWidth / 2 - labelWidth / 2);
                        ImGui::TextFormatted("{}", labelIndex);
                        ImGui::Unindent(indentSize + checkBoxWidth / 2 - labelWidth / 2);
                    }
                }
                ImGui::TableNextRow();
                // Row for bit checkboxes
                // Result
                ImGui::TableNextColumn();

                u64 mask = ((u128(1) << (totalBitCount + 1)) - 1);
                std::string maskString = hex::format("0x{:X}  ", mask);

                auto style = ImGui::GetStyle();
                inputFieldWidth = std::fmax(inputFieldWidth, ImGui::CalcTextSize(maskString.c_str()).x + style.FramePadding.x * 2.0f);
                ImGui::PushItemWidth(inputFieldWidth);

                u64 newValue = value & mask;
                if (ImGui::InputHexadecimal("##hex", &newValue, flags))
                    value = newValue;
                ImGui::PopItemWidth();

                // Equals
                ImGui::TableNextColumn();
                ImGui::Text("=");

                // Sign
                ImGui::TableNextColumn();



                ImVec4 signColor = ImGui::GetCustomColorVec4(ImGuiCustomCol_IEEEToolSign);
                ImVec4 expColor = ImGui::GetCustomColorVec4(ImGuiCustomCol_IEEEToolExp);
                ImVec4 mantColor = ImGui::GetCustomColorVec4(ImGuiCustomCol_IEEEToolMantissa);
                ImVec4 black = ImVec4(0.0, 0.0, 0.0, 1.0);

                ImGui::Indent(20_scaled);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, signColor);
                ImGui::PushStyleColor(ImGuiCol_Border, black);

                BitCheckboxes(signBitPosition, 1);

                ImGui::PopStyleColor();
                ImGui::PopStyleColor();
                ImGui::Unindent(20_scaled);

                // Times
                ImGui::TableNextColumn();
                // Exponent
                ImGui::TableNextColumn();

                ImGui::PushStyleColor(ImGuiCol_FrameBg, expColor);
                ImGui::PushStyleColor(ImGuiCol_Border, black);

                BitCheckboxes(exponentBitPosition, exponentBitCount);

                ImGui::PopStyleColor();
                ImGui::PopStyleColor();

                // Times
                ImGui::TableNextColumn();
                // Mantissa
                ImGui::TableNextColumn();

                ImGui::PushStyleColor(ImGuiCol_FrameBg, mantColor);
                ImGui::PushStyleColor(ImGuiCol_Border, black);

                BitCheckboxes(mantissaBitPosition, mantissaBitCount);

                ImGui::PopStyleColor();
                ImGui::PopStyleColor();

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                exponentBias = (u128(1) << (exponentBitCount - 1)) - 1;

                long double signValue = signBits == 0 ? 1.0 : -1.0;

                if (exponentBits == 0) {// Zero or denormal
                    if ((exponentBias - 1) > 128) // result doesn't fit in 128 bits
                        exponentValue = std::pow(2.0L, static_cast<long double>(-exponentBias + 1));
                    else {
                        if (exponentBias == 0) {
                            if (mantissaBits == 0) // exponent is zero
                                exponentValue = 1.0;
                            else // exponent is one
                            exponentValue = 2.0;
                        }
                        else
                            exponentValue = 1.0 / static_cast<long double>(u128(1) << (exponentBias - 1));
                    }
                }
                else { // Normal
                    if (std::abs(exponentBits - exponentBias) > 128) // result doesn't fit in 128 bits
                        exponentValue = std::pow(2.0L, static_cast<long double>(exponentBits - exponentBias));
                    else {//result fits in 128 bits
                        if (exponentBits > exponentBias) // exponent is positive
                            exponentValue = static_cast<long double>(u128(1) << (exponentBits - exponentBias));
                        else if (exponentBits < exponentBias) // exponent is negative
                            exponentValue = 1.0 / static_cast<long double>(u128(1) << (exponentBias - exponentBits));
                        else exponentValue = 1.0; // exponent is zero
                    }
                }

                mantissaValue = static_cast<long double>(mantissaBits) / static_cast<long double>(u128(1) << (mantissaBitCount));
                if (exponentBits != 0)
                    mantissaValue += 1.0;

                // Check if all exponent bits are set.
                if (std::popcount(static_cast<u64>(exponentBits)) == static_cast<i64>(exponentBitCount)) {
                    if (mantissaBits == 0) { // if fraction is zero number is infinity.
                        if (signBits == 0) {

                            numberType = NumberType::PositiveInfinity;
                            resultFloat = std::numeric_limits<long double>::infinity();

                        } else {

                            numberType = NumberType::NegativeInfinity;
                            resultFloat = -std::numeric_limits<long double>::infinity();

                        }
                        numberKind = NumberKind::Infinity;
                    } else { // otherwise number is NaN.
                        if (mantissaBits & (u128(1) << (mantissaBitCount - 1))) {

                            numberType = NumberType::QuietNaN;
                            resultFloat = std::numeric_limits<long double>::quiet_NaN();

                        } else {

                            numberType = NumberType::SignalingNaN;
                            resultFloat = std::numeric_limits<long double>::signaling_NaN();

                        }
                        numberKind = NumberKind::NaN;
                    }
                    // if all exponent bits are zero, but we have a non-zero fraction
                    // then the number is denormal which are smaller than regular numbers
                    // but not as precise.
                } else if (exponentBits == 0 && mantissaBits != 0) {

                    numberKind = NumberKind::Denormal;
                    numberType = NumberType::Regular;
                    resultFloat = signValue * exponentValue * mantissaValue;

                } else {

                    numberKind = NumberKind::Normal;
                    numberType = NumberType::Regular;
                    resultFloat = signValue * exponentValue *  mantissaValue;

                }

                i64 precision;
                if (numberKind == NumberKind::Denormal)
                    precision = std::ceil(1+mantissaBitCount * logBaseTenOfTwo);
                else
                    precision = std::ceil(1+(mantissaBitCount + 1) * logBaseTenOfTwo);

                // For C++ from_chars is better than strtold.
                // the main problem is that from_chars will not process special numbers
                // like inf and nan, so we handle them manually
                static std::string decimalFloatingPointNumberString;
                static std::string_view decimalStrView;
                // use qnan for quiet NaN and snan for signaling NaN
                if (numberKind == NumberKind::NaN) {
                    if (numberType == NumberType::QuietNaN)
                        decimalFloatingPointNumberString = fmt::format("qnan");
                    else
                        decimalFloatingPointNumberString = fmt::format("snan");
                } else
                    decimalFloatingPointNumberString = fmt::format("{:.{}}", resultFloat, precision);

                auto style1 = ImGui::GetStyle();
                inputFieldWidth = std::fmax(inputFieldWidth, ImGui::CalcTextSize(decimalFloatingPointNumberString.c_str()).x + 2 * style1.FramePadding.x);
                ImGui::PushItemWidth(inputFieldWidth);
                std::string specialNumbers[] = {"inf", "Inf", "INF", "nan", "Nan", "NAN", "qnan", "Qnan", "QNAN", "snan", "Snan", "SNAN"};

                // We allow any input in order to accept infinities and NaNs, all invalid entries
                // are detected by from_chars. You can also enter -0 or -inf.
                std::from_chars_result res;
                if (ImGui::InputText("##resultFloat", decimalFloatingPointNumberString, flags)) {
                    // Always obtain sign first.
                    if (decimalFloatingPointNumberString[0] == '-') {
                        signBits = 1; // and remove it from the string.
                        decimalFloatingPointNumberString.erase(0, 1);
                    } else //important to switch from - to +.
                        signBits = 0;

                    std::string inputType;
                    bool matchFound = false;
                    i32 i;// detect and use special numbers.
                    for (i = 0; i < 12; i++) {
                        if (decimalFloatingPointNumberString == specialNumbers[i]) {
                            i32 specialNumberIndex = i / 3; // use lower case version
                            inputType = specialNumbers[3 * specialNumberIndex];
                            matchFound = true;
                            break;
                        }
                    }

                    if (!matchFound)
                        inputType = "regular";

                    if (inputType == "regular") {
                        decimalStrView = decimalFloatingPointNumberString;
                        res = std::from_chars(decimalStrView.data(), decimalStrView.data() + decimalStrView.size(), resultFloat);
                        if (res.ec != std::errc()) { // this is why we use from_chars
                            inputType = "invalid";
                        }
                    } else if (inputType == "inf") {
                        resultFloat = std::numeric_limits<long double>::infinity();
                        resultFloat *= (signBits == 1 ? -1 : 1);

                    } else if (inputType == "nan")
                        resultFloat = std::numeric_limits<long double>::quiet_NaN();

                    else if (inputType == "qnan")
                        resultFloat = std::numeric_limits<long double>::quiet_NaN();

                    else if (inputType == "snan")
                        resultFloat = std::numeric_limits<long double>::signaling_NaN();

                    long double log2Result;

                    if (inputType != "invalid") {
                        // deal with zero first so we can use log2.
                        if (resultFloat == 0.0) {
                            if (signBits == 1)
                                resultFloat = -0.0;
                            else
                                resultFloat = 0.0;
                            numberKind = NumberKind::Zero;
                            numberType = NumberType::Regular;
                            exponentBits = 0;
                            mantissaBits = 0;

                        } else {

                            log2Result = std::log2(resultFloat);
                            // 2^(bias+1)-2^(bias-prec) is the largest number that can be represented.
                            // If the number entered is larger than this then the input is set to infinity.
                            if (resultFloat > (std::pow(2.0L, exponentBias + 1) - std::pow(2.0L, exponentBias - mantissaBitCount)) || inputType == "inf") {

                                resultFloat = std::numeric_limits<long double>::infinity();
                                numberKind = NumberKind::Infinity;
                                numberType = signBits == 1 ? NumberType::NegativeInfinity : NumberType::PositiveInfinity;
                                exponentBits = (u128(1) << exponentBitCount) - 1;
                                mantissaBits = 0;

                            } else if (-std::rint(log2Result) > exponentBias + mantissaBitCount - 1) {

                                // 1/2^(bias-1+prec) is the smallest number that can be represented.
                                // If the number entered is smaller than this then the input is set to zero.
                                if (signBits == 1)
                                    resultFloat = -0.0;
                                else
                                    resultFloat = 0.0;

                                numberKind = NumberKind::Zero;
                                numberType = NumberType::Regular;
                                exponentBits = 0;
                                mantissaBits = 0;

                            } else if (inputType == "snan") {

                                resultFloat = std::numeric_limits<long double>::signaling_NaN();
                                numberType = NumberType::SignalingNaN;
                                numberKind = NumberKind::NaN;
                                exponentBits = (u128(1) << exponentBitCount) - 1;
                                mantissaBits = 1;

                            } else if (inputType == "qnan") {

                                resultFloat = std::numeric_limits<long double>::quiet_NaN();
                                numberType = NumberType::QuietNaN;
                                numberKind = NumberKind::NaN;
                                exponentBits = (u128(1) << exponentBitCount) - 1;
                                mantissaBits = (u128(1) << (mantissaBitCount - 1));

                            } else if (static_cast<i64>(std::floor(log2Result)) + exponentBias <= 0) {

                                numberKind = NumberKind::Denormal;
                                numberType = NumberType::Regular;
                                exponentBits = 0;
                                auto mantissaExp = log2Result + exponentBias + mantissaBitCount - 1;
                                mantissaBits = static_cast<i64>(std::round(std::pow(2.0L, mantissaExp)));

                            } else {

                                numberType = NumberType::Regular;
                                numberKind = NumberKind::Normal;
                                i64 unBiasedExponent = static_cast<i64>(std::floor(log2Result));
                                exponentBits = unBiasedExponent + exponentBias;
                                mantissaValue = resultFloat * std::pow(2.0L, -unBiasedExponent) - 1;
                                mantissaBits = static_cast<i64>(std::round( static_cast<long double>(u128(1) << (mantissaBitCount)) * mantissaValue));

                            }
                        }
                        // Put the bits together.
                        value = (signBits << (totalBitCount)) | (exponentBits << (totalBitCount - exponentBitCount)) | mantissaBits;
                    }
                }
                ImGui::PopItemWidth();

                if (displayMode == 0) {

                    unsigned signColorU32 = ImGui::GetCustomColorU32(ImGuiCustomCol_IEEEToolSign);
                    unsigned expColorU32 = ImGui::GetCustomColorU32(ImGuiCustomCol_IEEEToolExp);
                    unsigned mantColorU32 = ImGui::GetCustomColorU32(ImGuiCustomCol_IEEEToolMantissa);

                    // this has the effect of dimming the color of the numbers so user doesn't try
                    // to interact with them.
                    if (isThemeDark)
                        textColor = textColor - ImVec4(0.3f, 0.3f, 0.3f, 0.0f);
                    else
                        textColor = textColor + ImVec4(0.3f, 0.3f, 0.3f, 0.0f);

                    ImGui::TableNextColumn();

                    ImGui::Text("=");

                    // Sign
                    ImGui::TableNextColumn();

                    ImGui::PushStyleColor(ImGuiCol_Text, textColor);

                    ImGui::Indent(20_scaled);
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, signColorU32);
                    if (signBits == 1)
                        ImGui::Text("-1");
                    else
                        ImGui::Text("+1");
                    ImGui::Unindent(20_scaled);

                    //times
                    ImGui::TableNextColumn();
                    ImGui::Text("x");
                    ImGui::TableNextColumn();

                    // Exponent
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, expColorU32);

                    ImGui::Indent(20_scaled);
                    if (numberKind == NumberKind::NaN) {
                        if (numberType == NumberType::QuietNaN) {
                            ImGui::Text("qNaN");
                        } else {
                            ImGui::Text("sNaN");
                        }
                    } else if (numberKind == NumberKind::Infinity) {
                        ImGui::Text("Inf");
                    } else if (numberKind == NumberKind::Zero) {
                        ImGui::Text("0");
                    } else if (numberKind == NumberKind::Denormal) {
                        ImGui::TextFormatted("2^{0}", 1 - exponentBias);
                    } else {
                        ImGui::TextFormatted("2^{0}", exponentBits - exponentBias);
                    }
                    ImGui::Unindent(20_scaled);

                    //times
                    ImGui::TableNextColumn();
                    ImGui::Text("x");
                    ImGui::TableNextColumn();

                    // Mantissa
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, mantColorU32);
                    ImGui::Indent(20_scaled);
                    ImGui::TextFormatted("{:.{}}", mantissaValue,precision);
                    ImGui::Unindent(20_scaled);

                    ImGui::PopStyleColor();
                }

                ImGui::EndTable();

            }// we are done. The rest selects the format  if user interacts with the widgets.
            // If precision and exponent match one of the IEEE 754 formats the format is highlighted
            // and remains highlighted until user changes to a different format. Matching formats occur when
            // the user clicks on one of the selections or if the slider values match the format in question.
            // when a new format is selected it may have a smaller number of digits than
            // the previous selection. Since the largest of the hexadecimal and the decimal
            // representation widths sets both field widths to the same value we need to
            // reset it here when a new choice is set.
            if (ImGui::SliderInt("hex.builtin.tools.ieee754.exponent_size"_lang, &exponentBitCount, 1, 63 - mantissaBitCount))
                inputFieldWidth = 0;
            if (ImGui::SliderInt("hex.builtin.tools.ieee754.mantissa_size"_lang, &mantissaBitCount, 1, 63 - exponentBitCount))
                inputFieldWidth = 0;
            ImGui::Separator();

            auto color = ImGui::GetColorU32(ImGuiCol_ButtonActive);

            bool needsPop = false;
            if (exponentBitCount == 5 && mantissaBitCount == 10) {
                ImGui::PushStyleColor(ImGuiCol_Button, color);
                needsPop = true;
            }
            if (ImGui::Button("hex.builtin.tools.ieee754.half_precision"_lang)) {
                exponentBitCount = 5;
                mantissaBitCount = 10;
                inputFieldWidth = 0;
            }
            if (needsPop) ImGui::PopStyleColor();

            ImGui::SameLine();

            needsPop = false;
            if (exponentBitCount == 8 && mantissaBitCount == 23) {
                ImGui::PushStyleColor(ImGuiCol_Button, color);
                needsPop = true;
            }
            if (ImGui::Button("hex.builtin.tools.ieee754.single_precision"_lang)) {
                exponentBitCount = 8;
                mantissaBitCount = 23;
                inputFieldWidth = 0;
            }
            if (needsPop) ImGui::PopStyleColor();

            ImGui::SameLine();

            needsPop = false;
            if (exponentBitCount == 11 && mantissaBitCount == 52) {
                ImGui::PushStyleColor(ImGuiCol_Button, color);
                needsPop = true;
            }
            if (ImGui::Button("hex.builtin.tools.ieee754.double_precision"_lang)) {
                exponentBitCount = 11;
                mantissaBitCount = 52;
                inputFieldWidth = 0;
            }
            if (needsPop) ImGui::PopStyleColor();

            ImGui::SameLine();

            needsPop = false;
            if (ImGui::Button("hex.builtin.tools.ieee754.clear"_lang))
                value = 0;//this will reset all interactive widgets to zero.

            ImGui::Separator();

            ImGui::NewLine();

        }


        void drawInvariantMultiplicationDecoder() {
            static u64 divisor = 1;
            static u64 multiplier = 1;
            static u64 numBits = 32;

            ImGui::TextFormattedWrapped("{}", "hex.builtin.tools.invariant_multiplication.description"_lang);

            ImGui::NewLine();

            if (ImGui::BeginChild("##calculator", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 5), true)) {
                static const u64 min = 1, max = 64;
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
    }

    void registerToolEntries() {
        ContentRegistry::Tools::add("hex.builtin.tools.demangler", drawDemangler);
        ContentRegistry::Tools::add("hex.builtin.tools.ascii_table", drawASCIITable);
        ContentRegistry::Tools::add("hex.builtin.tools.regex_replacer", drawRegexReplacer);
        ContentRegistry::Tools::add("hex.builtin.tools.color", drawColorPicker);
        ContentRegistry::Tools::add("hex.builtin.tools.calc", drawMathEvaluator);
        ContentRegistry::Tools::add("hex.builtin.tools.base_converter", drawBaseConverter);
        ContentRegistry::Tools::add("hex.builtin.tools.byte_swapper", drawByteSwapper);
        ContentRegistry::Tools::add("hex.builtin.tools.permissions", drawPermissionsCalculator);
        ContentRegistry::Tools::add("hex.builtin.tools.file_uploader", drawFileUploader);
        ContentRegistry::Tools::add("hex.builtin.tools.wiki_explain", drawWikiExplainer);
        ContentRegistry::Tools::add("hex.builtin.tools.file_tools", drawFileTools);
        ContentRegistry::Tools::add("hex.builtin.tools.ieee754", drawIEEE754Decoder);
        ContentRegistry::Tools::add("hex.builtin.tools.invariant_multiplication", drawInvariantMultiplicationDecoder);
    }

}
