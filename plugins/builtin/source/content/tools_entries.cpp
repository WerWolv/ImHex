#include <hex/plugin.hpp>

#include <regex>

#include <llvm/Demangle/Demangle.h>
#include "math_evaluator.hpp"

namespace hex::plugin::builtin {

    namespace {

        void drawDemangler() {
            static std::vector<char> mangledBuffer(0xF'FFFF, 0x00);
            static std::string demangledName;

            if (ImGui::InputText("Mangled name", mangledBuffer.data(), 0xF'FFFF)) {
                demangledName = llvm::demangle(mangledBuffer.data());
            }

            ImGui::InputText("Demangled name", demangledName.data(), demangledName.size(), ImGuiInputTextFlags_ReadOnly);
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

            ImGui::Checkbox("Show octal", &asciiTableShowOctal);
            ImGui::NewLine();
        }

        void drawRegexReplacer() {
            static std::vector<char> regexInput(0xF'FFFF, 0x00);;
            static std::vector<char> regexPattern(0xF'FFFF, 0x00);;
            static std::vector<char> replacePattern(0xF'FFFF, 0x00);;
            static std::string regexOutput(0xF'FFFF, 0x00);;

            bool shouldInvalidate;

            shouldInvalidate = ImGui::InputText("Regex pattern", regexPattern.data(), regexPattern.size());
            shouldInvalidate = ImGui::InputText("Replace pattern", replacePattern.data(), replacePattern.size()) || shouldInvalidate;
            shouldInvalidate = ImGui::InputTextMultiline("Input", regexInput.data(), regexInput.size())  || shouldInvalidate;

            if (shouldInvalidate) {
                try {
                    regexOutput = std::regex_replace(regexInput.data(), std::regex(regexPattern.data()), replacePattern.data());
                } catch (std::regex_error&) {}
            }

            ImGui::InputTextMultiline("Output", regexOutput.data(), regexOutput.size(), ImVec2(0, 0), ImGuiInputTextFlags_ReadOnly);
            ImGui::NewLine();
        }

        void drawColorPicker() {
            static std::array<float, 4> pickedColor = { 0 };

            ImGui::SetNextItemWidth(300.0F);
            ImGui::ColorPicker4("Color Picker", pickedColor.data(),
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

            if (ImGui::InputText("Input", mathInput.data(), mathInput.size(), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
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
                ImGui::TextColored(ImColor(0xA00040FF), "Last Error: %s", lastMathError.c_str());
            else
                ImGui::NewLine();

            enum class MathDisplayType { Standard, Scientific, Engineering, Programmer } mathDisplayType;
            if (ImGui::BeginTabBar("##mathFormatTabBar")) {
                if (ImGui::BeginTabItem("Standard")) {
                    mathDisplayType = MathDisplayType::Standard;
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Scientific")) {
                    mathDisplayType = MathDisplayType::Scientific;
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Engineering")) {
                    mathDisplayType = MathDisplayType::Engineering;
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Programmer")) {
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
                    ImGui::TableSetupColumn("History");

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
                    ImGui::TableSetupColumn("Name");
                    ImGui::TableSetupColumn("Value");

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

    }

    void registerToolEntries() {
        ContentRegistry::Tools::add("Itanium/MSVC demangler",   drawDemangler);
        ContentRegistry::Tools::add("ASCII table",              drawASCIITable);
        ContentRegistry::Tools::add("Regex replacer",           drawRegexReplacer);
        ContentRegistry::Tools::add("Color picker",             drawColorPicker);
        ContentRegistry::Tools::add("Calculator",               drawMathEvaluator);
    }

}