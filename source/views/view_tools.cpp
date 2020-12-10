#include "views/view_tools.hpp"

#include <cstring>
#include <regex>
#include <optional>

#include "providers/provider.hpp"
#include "helpers/utils.hpp"

namespace hex {

    ViewTools::ViewTools(hex::prv::Provider* &provider) : View("Tools"), m_dataProvider(provider) {
        this->m_mangledBuffer = new char[0xF'FFFF];
        std::memset(this->m_mangledBuffer, 0x00, 0xF'FFFF);

        this->m_regexInput = new char[0xF'FFFF];
        this->m_regexPattern = new char[0xF'FFFF];
        this->m_replacePattern = new char[0xF'FFFF];
        std::memset(this->m_regexInput, 0x00, 0xF'FFFF);
        std::memset(this->m_regexPattern, 0x00, 0xF'FFFF);
        std::memset(this->m_replacePattern, 0x00, 0xF'FFFF);


        this->m_mathInput = new char[0xFFFF];
        std::memset(this->m_mathInput, 0x00, 0xFFFF);
        this->m_mathEvaluator.registerStandardVariables();
        this->m_mathEvaluator.registerStandardFunctions();

        this->m_mathEvaluator.setFunction("clear", [this](auto args) -> std::optional<long double> {
            this->m_mathHistory.clear();
            this->m_lastMathError.clear();
            this->m_mathEvaluator.getVariables().clear();
            this->m_mathEvaluator.registerStandardVariables();
            std::memset(this->m_mathInput, 0x00, 0xFFFF);

            return { };
        }, 0, 0);

        this->m_mathEvaluator.setFunction("read", [this](auto args) -> std::optional<long double> {
            u8 value = 0;

            if (this->m_dataProvider == nullptr || !this->m_dataProvider->isReadable() || args[0] >= this->m_dataProvider->getActualSize())
                return { };

            this->m_dataProvider->read(args[0], &value, sizeof(u8));

            return value;
        }, 1, 1);

        this->m_mathEvaluator.setFunction("write", [this](auto args) -> std::optional<long double> {
            if (this->m_dataProvider == nullptr || !this->m_dataProvider->isWritable() || args[0] >= this->m_dataProvider->getActualSize())
                return { };

            if (args[1] > 0xFF)
                return { };

            u8 value = args[1];
            this->m_dataProvider->write(args[0], &value, sizeof(u8));

            return { };
        }, 2, 2);
    }

    ViewTools::~ViewTools() {
        delete[] this->m_mangledBuffer;

        delete[] this->m_regexInput;
        delete[] this->m_regexPattern;
        delete[] this->m_replacePattern;

        delete[] this->m_mathInput;
    }

    void ViewTools::drawDemangler() {
        if (ImGui::CollapsingHeader("Itanium/MSVC demangler")) {
            if (ImGui::InputText("Mangled name", this->m_mangledBuffer, 0xF'FFFF)) {
                this->m_demangledName = demangle(this->m_mangledBuffer);
            }

            ImGui::InputText("Demangled name", this->m_demangledName.data(), this->m_demangledName.size(), ImGuiInputTextFlags_ReadOnly);
            ImGui::NewLine();
        }
    }

    void ViewTools::drawASCIITable() {
        if (ImGui::CollapsingHeader("ASCII table")) {
            ImGui::BeginTable("##asciitable", 4);
            ImGui::TableSetupColumn("");
            ImGui::TableSetupColumn("");
            ImGui::TableSetupColumn("");
            ImGui::TableSetupColumn("");

            ImGui::TableNextColumn();

            for (u8 tablePart = 0; tablePart < 4; tablePart++) {
                ImGui::BeginTable("##asciitablepart", this->m_asciiTableShowOctal ? 4 : 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg);
                ImGui::TableSetupColumn("dec");
                if (this->m_asciiTableShowOctal)
                    ImGui::TableSetupColumn("oct");
                ImGui::TableSetupColumn("hex");
                ImGui::TableSetupColumn("char");

                ImGui::TableHeadersRow();

                u32 rowCount = 0;
                for (u8 i = 0; i < 0x80 / 4; i++) {
                    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);

                    ImGui::TableNextColumn();
                    ImGui::Text("%02d", i + 32 * tablePart);

                    if (this->m_asciiTableShowOctal) {
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

            ImGui::Checkbox("Show octal", &this->m_asciiTableShowOctal);
            ImGui::NewLine();
        }
    }

    void ViewTools::drawRegexReplacer() {
        if (ImGui::CollapsingHeader("Regex replacer")) {
            bool shouldInvalidate;

            shouldInvalidate = ImGui::InputText("Regex pattern", this->m_regexPattern, 0xF'FFFF);
            shouldInvalidate = ImGui::InputText("Replace pattern", this->m_replacePattern, 0xF'FFFF) || shouldInvalidate;
            shouldInvalidate = ImGui::InputTextMultiline("Input", this->m_regexInput, 0xF'FFFF)  || shouldInvalidate;

            if (shouldInvalidate) {
                try {
                    this->m_regexOutput = std::regex_replace(this->m_regexInput, std::regex(this->m_regexPattern), this->m_replacePattern);
                } catch (std::regex_error&) {}
            }

            ImGui::InputTextMultiline("Output", this->m_regexOutput.data(), this->m_regexOutput.size(), ImVec2(0, 0), ImGuiInputTextFlags_ReadOnly);
            ImGui::NewLine();


        }
    }

    void ViewTools::drawColorPicker() {
        if (ImGui::CollapsingHeader("Color picker")) {
            ImGui::SetNextItemWidth(300.0F);
            ImGui::ColorPicker4("Color Picker", this->m_pickedColor.data(),
            ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHex);
            ImGui::NewLine();
        }
    }

    void ViewTools::drawMathEvaluator() {
        if (ImGui::CollapsingHeader("Calculator")) {
            if (ImGui::InputText("Input", this->m_mathInput, 0xFFFF, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                ImGui::SetKeyboardFocusHere();
                std::optional<long double> result;

                try {
                    result = this->m_mathEvaluator.evaluate(this->m_mathInput);
                } catch (std::invalid_argument &e) {
                    this->m_lastMathError = e.what();
                }

                if (result.has_value()) {
                    this->m_mathHistory.push_back(result.value());
                    std::memset(this->m_mathInput, 0x00, 0xFFFF);
                    this->m_lastMathError.clear();
                }

            }

            if (!this->m_lastMathError.empty())
                ImGui::TextColored(ImColor(0xA00040FF), "Last Error: %s", this->m_lastMathError.c_str());
            else
                ImGui::NewLine();

            enum class MathDisplayType { Standard, Scientific, Programmer } mathDisplayType;
            if (ImGui::BeginTabBar("##mathFormatTabBar")) {
                if (ImGui::BeginTabItem("Standard")) {
                    mathDisplayType = MathDisplayType::Standard;
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Scientific")) {
                    mathDisplayType = MathDisplayType::Scientific;
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
                    clipper.Begin(this->m_mathHistory.size());

                    ImGui::TableHeadersRow();
                    while (clipper.Step()) {
                        for (u64 i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                            if (i == 0)
                                ImGui::PushStyleColor(ImGuiCol_Text, ImU32(ImColor(0xA5, 0x45, 0x45)));

                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();

                            switch (mathDisplayType) {
                                case MathDisplayType::Standard:
                                    ImGui::Text("%.3Lf", this->m_mathHistory[(this->m_mathHistory.size() - 1) - i]);
                                    break;
                                case MathDisplayType::Scientific:
                                    ImGui::Text("%.6Le", this->m_mathHistory[(this->m_mathHistory.size() - 1) - i]);
                                    break;
                                case MathDisplayType::Programmer:
                                    ImGui::Text("0x%llX (%llu)",
                                                u64(this->m_mathHistory[(this->m_mathHistory.size() - 1) - i]),
                                                u64(this->m_mathHistory[(this->m_mathHistory.size() - 1) - i]));
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
                    for (const auto &[name, value] : this->m_mathEvaluator.getVariables()) {
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

    void ViewTools::createView() {
        if (ImGui::Begin("Tools", &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {

            this->drawDemangler();
            this->drawASCIITable();
            this->drawRegexReplacer();
            this->drawMathEvaluator();
            this->drawColorPicker();

        }
        ImGui::End();
    }

    void ViewTools::createMenu() {

    }

}