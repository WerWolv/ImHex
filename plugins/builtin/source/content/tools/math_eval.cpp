#include <hex/api/localization_manager.hpp>
#include <hex/api/imhex_api.hpp>
#include <hex/providers/provider.hpp>
#include <hex/helpers/utils.hpp>

#include <wolv/math_eval/math_evaluator.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <fonts/vscode_icons.hpp>

#include <vector>
#include <string>

namespace hex::plugin::builtin {

   void drawMathEvaluator() {
        static std::vector<long double> mathHistory;
        static std::string lastMathError;
        static std::string mathInput;
        bool evaluate = false;

        static wolv::math_eval::MathEvaluator<long double> mathEvaluator = [&] {
            wolv::math_eval::MathEvaluator<long double> evaluator;

            evaluator.registerStandardVariables();
            evaluator.registerStandardFunctions();

            evaluator.setFunction(
                "clear", [&](auto args) -> std::optional<long double> {
                    std::ignore = args;

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
            if (ImGui::Button(ICON_VS_DISCARD, buttonSize)) mathInput.clear();

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

            ImGui::PushStyleColor(ImGuiCol_Button, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_DescButtonHovered));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_DescButton));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_DescButtonActive));
            if (ImGui::Button("=", buttonSize)) evaluate = true;
            ImGui::SameLine();
            ImGui::PopStyleColor(3);

            ImGui::NewLine();

            ImGui::TableNextColumn();

            if (ImGui::BeginTable("##mathHistory", 1, ImGuiTableFlags_ScrollY | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg, ImVec2(0, 300))) {
                ImGui::TableSetupColumn("hex.builtin.tools.history"_lang);
                ImGui::TableSetupScrollFreeze(0, 1);

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
                                ImGuiExt::TextFormatted("{0:.3Lf}", mathHistory[(mathHistory.size() - 1) - i]);
                                break;
                            case MathDisplayType::Scientific:
                                ImGuiExt::TextFormatted("{0:.6Lg}", mathHistory[(mathHistory.size() - 1) - i]);
                                break;
                            case MathDisplayType::Engineering:
                                ImGuiExt::TextFormatted("{0}", hex::toEngineeringString(mathHistory[(mathHistory.size() - 1) - i]).c_str());
                                break;
                            case MathDisplayType::Programmer:
                                ImGuiExt::TextFormatted("0x{0:X} ({1})",
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
                ImGui::TableSetupColumn("hex.builtin.tools.name"_lang);
                ImGui::TableSetupColumn("hex.builtin.tools.value"_lang);
                ImGui::TableSetupScrollFreeze(0, 1);

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
                            ImGuiExt::TextFormatted("{0:.3Lf}", value);
                            break;
                        case MathDisplayType::Scientific:
                            ImGuiExt::TextFormatted("{0:.6Lg}", value);
                            break;
                        case MathDisplayType::Engineering:
                            ImGuiExt::TextFormatted("{}", hex::toEngineeringString(value));
                            break;
                        case MathDisplayType::Programmer:
                            ImGuiExt::TextFormatted("0x{0:X} ({1})", u64(value), u64(value));
                            break;
                    }
                }

                ImGui::EndTable();
            }

            ImGui::EndTable();
        }

        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGuiExt::InputTextIcon("##input", ICON_VS_SYMBOL_OPERATOR, mathInput, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            ImGui::SetKeyboardFocusHere();
            evaluate = true;
        }
        ImGui::PopItemWidth();

        if (!lastMathError.empty())
            ImGuiExt::TextFormattedColored(ImColor(0xA00040FF), "hex.builtin.tools.error"_lang, lastMathError);
        else
            ImGui::NewLine();

        if (evaluate) {
            try {
                auto result = mathEvaluator.evaluate(mathInput);
                mathInput.clear();
                if (result.has_value()) {
                    mathHistory.push_back(result.value());
                    lastMathError.clear();
                } else {
                    lastMathError = mathEvaluator.getLastError().value_or("");
                }
            } catch (std::invalid_argument &e) {
                lastMathError = e.what();
            }
        }
    }

}
