#include "views/view_tools.hpp"

#include <cxxabi.h>
#include <cstring>
#include <regex>

namespace hex {

    ViewTools::ViewTools() {
        this->m_mangledBuffer = new char[0xF'FFFF];
        this->m_demangledName = static_cast<char*>(malloc(8));

        std::memset(this->m_mangledBuffer, 0x00, 0xF'FFFF);
        strcpy(this->m_demangledName, "< ??? >");

        this->m_regexInput = new char[0xF'FFFF];
        this->m_regexPattern = new char[0xF'FFFF];
        this->m_replacePattern = new char[0xF'FFFF];
        std::memset(this->m_regexInput, 0x00, 0xF'FFFF);
        std::memset(this->m_regexPattern, 0x00, 0xF'FFFF);
        std::memset(this->m_replacePattern, 0x00, 0xF'FFFF);
    }

    ViewTools::~ViewTools() {
        delete[] this->m_mangledBuffer;
        free(this->m_demangledName);

        delete[] this->m_regexInput;
        delete[] this->m_regexPattern;
    }

    static std::string toASCIITableString(char c) {
        switch (c) {
            case 0:   return "NUL";
            case 1:   return "SOH";
            case 2:   return "STX";
            case 3:   return "ETX";
            case 4:   return "EOT";
            case 5:   return "ENQ";
            case 6:   return "ACK";
            case 7:   return "BEL";
            case 8:   return "BS";
            case 9:   return "TAB";
            case 10:  return "LF";
            case 11:  return "VT";
            case 12:  return "FF";
            case 13:  return "CR";
            case 14:  return "SO";
            case 15:  return "SI";
            case 16:  return "DLE";
            case 17:  return "DC1";
            case 18:  return "DC2";
            case 19:  return "DC3";
            case 20:  return "DC4";
            case 21:  return "NAK";
            case 22:  return "SYN";
            case 23:  return "ETB";
            case 24:  return "CAN";
            case 25:  return "EM";
            case 26:  return "SUB";
            case 27:  return "ESC";
            case 28:  return "FS";
            case 29:  return "GS";
            case 30:  return "RS";
            case 31:  return "US";
            case 32:  return "Space";
            case 127: return "DEL";
            default:  return std::string() + c;
        }
    }

    void ViewTools::drawDemangler() {
        if (ImGui::CollapsingHeader("Itanium demangler")) {
            if (ImGui::InputText("Mangled name", this->m_mangledBuffer, 0xF'FFFF)) {
                size_t length = 0;
                int status = 0;

                if (this->m_demangledName != nullptr)
                    free(this->m_demangledName);

                this->m_demangledName = abi::__cxa_demangle(this->m_mangledBuffer, nullptr, &length, &status);

                if (status != 0) {
                    this->m_demangledName = static_cast<char*>(malloc(8));
                    strcpy(this->m_demangledName, "< ??? >");
                }
            }

            ImGui::InputText("Demangled name", this->m_demangledName, strlen(this->m_demangledName), ImGuiInputTextFlags_ReadOnly);
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
                    ImGui::Text("%s", toASCIITableString(i + 32 * tablePart).c_str());

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
            ImGui::ColorPicker4("Color Picker", this->m_pickedColor.data(),
            ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHex);
            ImGui::NewLine();
        }
    }

    void ViewTools::createView() {
        if (!this->m_windowOpen)
            return;

        if (ImGui::Begin("Tools", &this->m_windowOpen, ImGuiWindowFlags_NoCollapse)) {

            this->drawDemangler();
            this->drawASCIITable();
            this->drawRegexReplacer();
            this->drawColorPicker();

        }
        ImGui::End();
    }

    void ViewTools::createMenu() {
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Tools View", "", &this->m_windowOpen);
            ImGui::EndMenu();
        }
    }

}