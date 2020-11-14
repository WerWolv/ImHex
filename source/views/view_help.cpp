#include "views/view_help.hpp"

namespace hex {

    ViewHelp::ViewHelp() {

    }

    ViewHelp::~ViewHelp() {

    }

    void ViewHelp::drawAboutPopup() {
        ImGui::OpenPopup("About");
        ImGui::SetNextWindowSizeConstraints(ImVec2(450, 300), ImVec2(450, 300));
        if (ImGui::BeginPopupModal("About", &this->m_aboutWindowOpen, ImGuiWindowFlags_NoResize)) {

            ImGui::Text("ImHex Hex Editor");
            ImGui::Text("by WerWolv");
            ImGui::Separator();
            ImGui::NewLine();

            ImGui::Text("Source code found at"); ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.4F, 0.4F, 0.8F, 1.0F), "https://github.com/WerWolv/ImHex");
            ImGui::NewLine();

            ImGui::Separator();
            ImGui::Text("Libraries used");
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.2F, 0.2F, 0.2F, 0.3F));
            ImGui::BeginChild("##scroll");
            ImGui::NewLine();
            ImGui::BulletText("ImGui by ocornut");
            ImGui::BulletText("imgui_club by ocornut");
            ImGui::BulletText("imgui-filebrowser by AirGuanZ");
            ImGui::NewLine();
            ImGui::BulletText("GNU libmagic");
            ImGui::BulletText("OpenSSL libcrypto");
            ImGui::BulletText("GLFW3");

            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }
    }

    void ViewHelp::drawPatternHelpPopup() {
        ImGui::OpenPopup("PatternLanguage");
        ImGui::SetNextWindowSizeConstraints(ImVec2(450, 300), ImVec2(450, 300));

        constexpr static auto DrawTitle = [](const std::string &title) {
            ImGui::TextColored(ImVec4(0.6F, 0.6F, 1.0F, 1.0F), title.c_str());
        };

        constexpr static auto DrawCodeSegment = [](const std::string &id, size_t height, const std::string &code) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.2F, 0.2F, 0.2F, 0.3F));
            ImGui::NewLine();
            ImGui::BeginChild(id.c_str(), ImVec2(-1, height));

            ImGui::Text("%s", code.c_str());

            ImGui::EndChild();
            ImGui::NewLine();
            ImGui::PopStyleColor();
        };

        ImGui::SetNextWindowSizeConstraints(ImVec2(450, 300), ImVec2(2000, 1000));
        if (ImGui::BeginPopupModal("PatternLanguage", &this->m_patternHelpWindowOpen)) {
            ImGui::Text("ImHex Pattern Language Cheat Sheet");
            ImGui::Separator();
            ImGui::NewLine();

            DrawTitle("Built-in types");
            ImGui::TextWrapped(
                    "The following built-in types are available for use");
            DrawCodeSegment("built-in", 80,
                    "u8, s8\n"
                    "u16, s16\n"
                    "u32, s32\n"
                    "u64, s64\n"
                    "u128, s128\n"
                    "float, double"
                    );

            DrawTitle("Variables and Arrays");
            ImGui::TextWrapped(
                    "Normal variables as well as arrays are used to highlight and display values.");
            DrawCodeSegment("vars arrays", 30,
                    "u32 variable;\n"
                    "s8 string[16];"
            );

            DrawTitle("Structs");
            ImGui::TextWrapped(
                    "To bundle multiple variables together, a struct can be used.");
            DrawCodeSegment("struct", 70,
                "struct Header {\n"
                "   u32 magic;\n"
                "   u8 version;\n"
                "   Flags flags;\n"
                "}"
            );

            DrawTitle("Enum");
            ImGui::TextWrapped(
                    "If a value can only be a few specific values with special meaning, an enum can be used. "
                    "The underlying type has to be specified using a unsigned, built-in type after the name. "
                    "Entries can be listed with or without a value. The values start counting at zero and increase by one "
                    "for every next entry");
            DrawCodeSegment("enum", 70,
                    "enum OperatingSystem : u8 {\n"
                    "   Windows = 0x10,\n"
                    "   MacOSX,\n"
                    "   Linux\n"
                    "}"
            );

            DrawTitle("Using declarations");
            ImGui::TextWrapped(
                    "A using declaration can be used to create type aliases for already existing types. This can be "
                    "a built-in type, a struct, enum or another alias type.");
            DrawCodeSegment("using", 15,
                            "using magic_t = u32;"
            );

            DrawTitle("Comments");
            ImGui::TextWrapped(
                    "To create a comment the C // or /*  */ syntax can be used. //-style comments end at the next new line "
                    "and /*-style comments only end when at the next */.");
            DrawCodeSegment("comment", 55,
                            "// This is a single line comment\n\n"
                            "/* This is a\n"
                            "multiline comment */"
            );

            DrawTitle("Variable placement");
            ImGui::TextWrapped(
                    "In order to highlight bytes and displaying their value in the pattern data window, "
                    "a variable needs to be created and placed in memory. The following line of code creates"
                    "a unsigned 32 bit variable named data and places it at offset 0x100."
                    );
            DrawCodeSegment("var placement", 15, "u32 data @ 0x100;");
            ImGui::EndPopup();
        }
    }

    void ViewHelp::createView() {
        if (this->m_aboutWindowOpen)
            drawAboutPopup();

        if (this->m_patternHelpWindowOpen)
            drawPatternHelpPopup();
    }

    void ViewHelp::createMenu() {
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About", ""))
                this->m_aboutWindowOpen = true;
            ImGui::Separator();
            if (ImGui::MenuItem("Pattern Language Cheat Sheet", ""))
                this->m_patternHelpWindowOpen = true;
            ImGui::EndMenu();
        }
    }

}