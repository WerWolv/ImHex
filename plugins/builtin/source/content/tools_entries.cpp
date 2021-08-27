#include <hex/plugin.hpp>

#include <hex/helpers/net.hpp>

#include <regex>
#include <chrono>

#include <llvm/Demangle/Demangle.h>
#include "math_evaluator.hpp"

namespace hex::plugin::builtin {

    namespace {

        using namespace std::literals::chrono_literals;

        void drawDemangler() {
            static std::vector<char> mangledBuffer(0xF'FFFF, 0x00);
            static std::string demangledName;

            if (ImGui::InputText("hex.builtin.tools.demangler.mangled"_lang, mangledBuffer.data(), 0xF'FFFF)) {
                demangledName = llvm::demangle(mangledBuffer.data());
            }

            ImGui::InputText("hex.builtin.tools.demangler.demangled"_lang, demangledName.data(), demangledName.size(), ImGuiInputTextFlags_ReadOnly);
            ImGui::NewLine();
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

                u32 rowCount = 0;
                for (u8 i = 0; i < 0x80 / 4; i++) {
                    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);

                    ImGui::TableNextColumn();
                    ImGui::Text("%02d", i + 32 * tablePart);

                    if (asciiTableShowOctal) {
                        ImGui::TableNextColumn();
                        ImGui::Text("0o%02o", i + 32 * tablePart);
                    }

                    ImGui::TableNextColumn();
                    ImGui::Text("0x%02x", i + 32 * tablePart);

                    ImGui::TableNextColumn();
                    ImGui::Text("%s", makePrintable(i + 32 * tablePart).c_str());

                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ((rowCount % 2) == 0) ? 0xFF101010 : 0xFF303030);

                    rowCount++;
                }

                ImGui::EndTable();
                ImGui::TableNextColumn();
            }
            ImGui::EndTable();

            ImGui::Checkbox("hex.builtin.tools.ascii_table.octal"_lang, &asciiTableShowOctal);
            ImGui::NewLine();
        }

        void drawRegexReplacer() {
            static std::vector<char> regexInput(0xF'FFFF, 0x00);;
            static std::vector<char> regexPattern(0xF'FFFF, 0x00);;
            static std::vector<char> replacePattern(0xF'FFFF, 0x00);;
            static std::string regexOutput(0xF'FFFF, 0x00);;

            bool shouldInvalidate;

            shouldInvalidate = ImGui::InputText("hex.builtin.tools.regex_replacer.pattern"_lang, regexPattern.data(), regexPattern.size());
            shouldInvalidate = ImGui::InputText("hex.builtin.tools.regex_replacer.replace"_lang, replacePattern.data(), replacePattern.size()) || shouldInvalidate;
            shouldInvalidate = ImGui::InputTextMultiline("hex.builtin.tools.regex_replacer.input"_lang, regexInput.data(), regexInput.size())  || shouldInvalidate;

            if (shouldInvalidate) {
                try {
                    regexOutput = std::regex_replace(regexInput.data(), std::regex(regexPattern.data()), replacePattern.data());
                } catch (std::regex_error&) {}
            }

            ImGui::InputTextMultiline("hex.builtin.tools.regex_replacer.input"_lang, regexOutput.data(), regexOutput.size(), ImVec2(0, 0), ImGuiInputTextFlags_ReadOnly);
            ImGui::NewLine();
        }

        void drawColorPicker() {
            static std::array<float, 4> pickedColor = { 0 };

            ImGui::SetNextItemWidth(300.0F);
            ImGui::ColorPicker4("hex.builtin.tools.color"_lang, pickedColor.data(),
                                ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHex);
            ImGui::NewLine();
        }

        void drawMathEvaluator() {
            static std::vector<long double> mathHistory;
            static std::string lastMathError;
            static std::vector<char> mathInput(0xF'FFFF, 0x00);

            static MathEvaluator mathEvaluator = [&]{
                MathEvaluator evaluator;

                evaluator.registerStandardVariables();
                evaluator.registerStandardFunctions();

                evaluator.setFunction("clear", [&](auto args) -> std::optional<long double> {
                    mathHistory.clear();
                    lastMathError.clear();
                    mathEvaluator.getVariables().clear();
                    mathEvaluator.registerStandardVariables();
                    std::memset(mathInput.data(), 0x00, mathInput.size());

                    return { };
                }, 0, 0);

                evaluator.setFunction("read", [](auto args) -> std::optional<long double> {
                    u8 value = 0;

                    auto provider = SharedData::currentProvider;
                    if (provider == nullptr || !provider->isReadable() || args[0] >= provider->getActualSize())
                        return { };

                    provider->read(args[0], &value, sizeof(u8));

                    return value;
                }, 1, 1);

                evaluator.setFunction("write", [](auto args) -> std::optional<long double> {
                    auto provider = SharedData::currentProvider;
                    if (provider == nullptr || !provider->isWritable() || args[0] >= provider->getActualSize())
                        return { };

                    if (args[1] > 0xFF)
                        return { };

                    u8 value = args[1];
                    provider->write(args[0], &value, sizeof(u8));

                    return { };
                }, 2, 2);

                return std::move(evaluator);
            }();

            if (ImGui::InputText("hex.builtin.tools.input"_lang, mathInput.data(), mathInput.size(), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                ImGui::SetKeyboardFocusHere();
                std::optional<long double> result;

                try {
                    result = mathEvaluator.evaluate(mathInput.data());
                } catch (std::invalid_argument &e) {
                    lastMathError = e.what();
                }

                if (result.has_value()) {
                    mathHistory.push_back(result.value());
                    std::memset(mathInput.data(), 0x00, mathInput.size());
                    lastMathError.clear();
                }

            }

            if (!lastMathError.empty())
                ImGui::TextColored(ImColor(0xA00040FF), "hex.builtin.tools.error"_lang, lastMathError.c_str());
            else
                ImGui::NewLine();

            enum class MathDisplayType { Standard, Scientific, Engineering, Programmer } mathDisplayType;
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

            if (ImGui::BeginTable("##mathWrapper", 2)) {
                ImGui::TableSetupColumn("##results");
                ImGui::TableSetupColumn("##variables", ImGuiTableColumnFlags_WidthStretch, 0.7);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (ImGui::BeginTable("##mathHistory", 1, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg, ImVec2(0, 400))) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("hex.builtin.tools.history"_lang);

                    ImGuiListClipper clipper;
                    clipper.Begin(mathHistory.size());

                    ImGui::TableHeadersRow();
                    while (clipper.Step()) {
                        for (u64 i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                            if (i == 0)
                                ImGui::PushStyleColor(ImGuiCol_Text, ImU32(ImColor(0xA5, 0x45, 0x45)));

                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();

                            switch (mathDisplayType) {
                                case MathDisplayType::Standard:
                                    ImGui::Text("%.3Lf", mathHistory[(mathHistory.size() - 1) - i]);
                                    break;
                                case MathDisplayType::Scientific:
                                    ImGui::Text("%.6Le", mathHistory[(mathHistory.size() - 1) - i]);
                                    break;
                                case MathDisplayType::Engineering:
                                    ImGui::Text("%s", hex::toEngineeringString(mathHistory[(mathHistory.size() - 1) - i]).c_str());
                                    break;
                                case MathDisplayType::Programmer:
                                    ImGui::Text("0x%llX (%llu)",
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
                if (ImGui::BeginTable("##mathVariables", 2, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg, ImVec2(0, 400))) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("hex.builtin.tools.name"_lang);
                    ImGui::TableSetupColumn("hex.builtin.tools.value"_lang);

                    ImGui::TableHeadersRow();
                    for (const auto &[name, value] : mathEvaluator.getVariables()) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(name.c_str());

                        ImGui::TableNextColumn();
                        switch (mathDisplayType) {
                            case MathDisplayType::Standard:
                                ImGui::Text("%.3Lf", value);
                                break;
                            case MathDisplayType::Scientific:
                                ImGui::Text("%.6Le", value);
                                break;
                            case MathDisplayType::Engineering:
                                ImGui::Text("%s", hex::toEngineeringString(value).c_str());
                                break;
                            case MathDisplayType::Programmer:
                                ImGui::Text("0x%llX (%llu)", u64(value),  u64(value));
                                break;
                        }
                    }

                    ImGui::EndTable();
                }

                ImGui::EndTable();
            }
        }

        void drawBaseConverter() {
            static char buffer[4][0xFFF] = { { '0' }, { '0' }, { '0' }, { '0' } };

            static auto CharFilter = [](ImGuiInputTextCallbackData *data) -> int {
                switch (*static_cast<u32*>(data->UserData)) {
                    case 16: return std::isxdigit(data->EventChar);
                    case 10: return std::isdigit(data->EventChar);
                    case 8:  return std::isdigit(data->EventChar) && data->EventChar < '8';
                    case 2:  return data->EventChar == '0' || data->EventChar == '1';
                    default: return false;
                }
            };

            static auto ConvertBases = [](u8 base) {
                u64 number;

                errno = 0;
                switch (base) {
                    case 16: number = std::strtoull(buffer[1], nullptr, base); break;
                    case 10: number = std::strtoull(buffer[0], nullptr, base); break;
                    case 8:  number = std::strtoull(buffer[2], nullptr, base); break;
                    case 2:  number = std::strtoull(buffer[3], nullptr, base); break;
                    default: return;
                }

                auto base10String   = std::to_string(number);
                auto base16String   = hex::format("0x{0:X}", number);
                auto base8String    = hex::format("{0:#o}", number);
                auto base2String    = hex::toBinaryString(number);

                std::strncpy(buffer[0], base10String.c_str(), sizeof(buffer[0]));
                std::strncpy(buffer[1], base16String.c_str(), sizeof(buffer[1]));
                std::strncpy(buffer[2], base8String.c_str(),  sizeof(buffer[2]));
                std::strncpy(buffer[3], base2String.c_str(),  sizeof(buffer[3]));
            };

            u8 base = 10;
            if (ImGui::InputText("hex.builtin.tools.base_converter.dec"_lang, buffer[0], 20 + 1, ImGuiInputTextFlags_CallbackCharFilter, CharFilter, &base))
                ConvertBases(base);

            base = 16;
            if (ImGui::InputText("hex.builtin.tools.base_converter.hex"_lang, buffer[1], 16 + 1, ImGuiInputTextFlags_CallbackCharFilter, CharFilter, &base))
                ConvertBases(base);

            base = 8;
            if (ImGui::InputText("hex.builtin.tools.base_converter.oct"_lang, buffer[2], 22 + 1, ImGuiInputTextFlags_CallbackCharFilter, CharFilter, &base))
                ConvertBases(base);

            base = 2;
            if (ImGui::InputText("hex.builtin.tools.base_converter.bin"_lang, buffer[3], 64 + 1, ImGuiInputTextFlags_CallbackCharFilter, CharFilter, &base))
                ConvertBases(base);
        }

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
            ImGui::TextColored(WarningColor, "%s", static_cast<const char*>("hex.builtin.tools.permissions.setuid_error"_lang));
        if (setgid && !x[1])
            ImGui::TextColored(WarningColor, "%s", static_cast<const char*>("hex.builtin.tools.permissions.setgid_error"_lang));
        if (sticky && !x[2])
            ImGui::TextColored(WarningColor, "%s", static_cast<const char*>("hex.builtin.tools.permissions.sticky_error"_lang));

    }

    void drawFileUploader() {
        struct UploadedFile {
            std::string fileName, link, size;
        };

        static hex::Net net;
        static std::future<Response<std::string>> uploadProcess;
        static std::filesystem::path currFile;
        static std::vector<UploadedFile> links;

        bool uploading = uploadProcess.valid() && uploadProcess.wait_for(0s) != std::future_status::ready;

        ImGui::Header("hex.builtin.tools.file_uploader.control"_lang, true);
        if (!uploading) {
            if (ImGui::Button("hex.builtin.tools.file_uploader.upload"_lang)) {
                hex::openFileBrowser("hex.builtin.tools.file_uploader.done"_lang, DialogMode::Open, { }, [&](auto path){
                    uploadProcess = net.uploadFile("https://api.anonfiles.com/upload", path);
                    currFile = path;
                });
            }
        } else {
            if (ImGui::Button("hex.common.cancel"_lang)) {
                net.cancel();
            }
        }

        ImGui::SameLine();

        ImGui::ProgressBar(net.getProgress(), ImVec2(0, 0), uploading ? nullptr : "Done!");

        ImGui::Header("hex.builtin.tools.file_uploader.recent"_lang);

        if (ImGui::BeginTable("##links", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg, ImVec2(0, 400))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("hex.common.file"_lang);
            ImGui::TableSetupColumn("hex.common.link"_lang);
            ImGui::TableSetupColumn("hex.common.size"_lang);
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            clipper.Begin(links.size());

            while (clipper.Step()) {
                for (s32 i = clipper.DisplayEnd - 1; i >= clipper.DisplayStart; i--) {
                    auto &[fileName, link, size] = links[i];

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(fileName.c_str());

                    ImGui::TableNextColumn();
                    if (ImGui::Hyperlink(link.c_str())) {
                        if (ImGui::GetMergedKeyModFlags() == ImGuiKeyModFlags_Ctrl)
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
            if (response.code == 200) {
                try {
                    auto json = nlohmann::json::parse(response.body);
                    links.push_back({
                        currFile.filename().string(),
                        json["data"]["file"]["url"]["short"],
                        json["data"]["file"]["metadata"]["size"]["readable"]
                    });
                } catch (...) {
                    View::showErrorPopup("hex.builtin.tools.file_uploader.invalid_response"_lang);
                }
            } else if (response.code == 0) {
                // Canceled by user, no action needed
            } else View::showErrorPopup(hex::format("hex.builtin.tools.file_uploader.error"_lang, response.code));

            uploadProcess = { };
            currFile.clear();
        }
    }

    void registerToolEntries() {
        ContentRegistry::Tools::add("hex.builtin.tools.demangler",         drawDemangler);
        ContentRegistry::Tools::add("hex.builtin.tools.ascii_table",       drawASCIITable);
        ContentRegistry::Tools::add("hex.builtin.tools.regex_replacer",    drawRegexReplacer);
        ContentRegistry::Tools::add("hex.builtin.tools.color",             drawColorPicker);
        ContentRegistry::Tools::add("hex.builtin.tools.calc",              drawMathEvaluator);
        ContentRegistry::Tools::add("hex.builtin.tools.base_converter",    drawBaseConverter);
        ContentRegistry::Tools::add("hex.builtin.tools.permissions",       drawPermissionsCalculator);
        ContentRegistry::Tools::add("hex.builtin.tools.file_uploader",     drawFileUploader);
    }

}