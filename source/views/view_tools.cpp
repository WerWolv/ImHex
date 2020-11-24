#include "views/view_tools.hpp"

#include <cxxabi.h>
#include <cstring>
#include <regex>

#include "utils.hpp"

namespace hex {

    ViewTools::ViewTools() : View("Tools") {
        this->m_mangledBuffer = new char[0xF'FFFF];

        std::memset(this->m_mangledBuffer, 0x00, 0xF'FFFF);
        this->m_demangledName = "< ??? >";

        this->m_regexInput = new char[0xF'FFFF];
        this->m_regexPattern = new char[0xF'FFFF];
        this->m_replacePattern = new char[0xF'FFFF];
        std::memset(this->m_regexInput, 0x00, 0xF'FFFF);
        std::memset(this->m_regexPattern, 0x00, 0xF'FFFF);
        std::memset(this->m_replacePattern, 0x00, 0xF'FFFF);
    }

    ViewTools::~ViewTools() {
        delete[] this->m_mangledBuffer;

        delete[] this->m_regexInput;
        delete[] this->m_regexPattern;
    }

    void ViewTools::drawDemangler() {
        if (ImGui::CollapsingHeader("Itanium demangler")) {
            if (ImGui::InputText("Mangled name", this->m_mangledBuffer, 0xF'FFFF)) {
                this->m_demangledName = demangleItaniumSymbol(this->m_mangledBuffer);
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

    void ViewTools::createView() {
        if (ImGui::Begin("Tools", &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {

            this->drawDemangler();
            this->drawASCIITable();
            this->drawRegexReplacer();
            this->drawColorPicker();

        }
        ImGui::End();
    }

    void ViewTools::createMenu() {

    }

}