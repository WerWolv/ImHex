#include <hex/helpers/utils.hpp>
#include <hex/api/localization_manager.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <fonts/fonts.hpp>
#include <fonts/vscode_icons.hpp>

#include <hex/helpers/logger.hpp>

#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::plugin::builtin {

    static void drawColumnASCIITable() {
        static bool asciiTableShowOctal = false;

        ImGui::SameLine();
        ImGuiExt::DimmedIconToggle("0o", &asciiTableShowOctal);
        ImGui::SetItemTooltip("%s", "hex.builtin.tools.ascii_table.octal"_lang.get());

        if (ImGui::BeginTable("##asciitable", 4, ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableSetupColumn("##1");
            ImGui::TableSetupColumn("##2");
            ImGui::TableSetupColumn("##3");
            ImGui::TableSetupColumn("##4");

            ImGui::TableNextRow();

            for (u8 tablePart = 0; tablePart < 4; tablePart++) {
                ImGui::TableNextColumn();
                if (ImGui::BeginTable("##asciitablepart", asciiTableShowOctal ? 4 : 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("dec");
                    if (asciiTableShowOctal)
                        ImGui::TableSetupColumn("oct");
                    ImGui::TableSetupColumn("hex");
                    ImGui::TableSetupColumn("char");

                    ImGui::TableHeadersRow();

                    for (u8 i = 0; i < 0x80 / 4; i++) {
                        ImGui::TableNextRow();

                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("{0:03d}", i + 32 * tablePart);

                        if (asciiTableShowOctal) {
                            ImGui::TableNextColumn();
                            ImGuiExt::TextFormatted("0o{0:03o}", i + 32 * tablePart);
                        }

                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("0x{0:02X}", i + 32 * tablePart);

                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("{0}", hex::makePrintable(i + 32 * tablePart));
                    }

                    ImGui::EndTable();
                }
            }
            ImGui::EndTable();
        }
    }

    static void drawGridASCIITable() {
        constexpr static std::array Characters = {
            "␀", "␁", "␂", "␃", "␄", "␅", "␆", "␇",
            "␈", "␉", "␊", "␋", "␌", "␍", "␎", "␏",
            "␐", "␑", "␒", "␓", "␔", "␕", "␖", "␗",
            "␘", "␙", "␚", "␛", "␜", "␝", "␞", "␟",
            "␣", "!", "\"", "#", "$", "%", "&", "'",
            "(", ")", "*", "+", ",", "-", ".", "/",
            "0", "1", "2", "3", "4", "5", "6", "7",
            "8", "9", ":", ";", "<", "=", ">", "?",
            "@", "A", "B", "C", "D", "E", "F", "G",
            "H", "I", "J", "K", "L", "M", "N", "O",
            "P", "Q", "R", "S", "T", "U", "V", "W",
            "X", "Y", "Z", "[", "\\", "]", "^", "_",
            "`", "a", "b", "c", "d", "e", "f", "g",
            "h", "i", "j", "k", "l", "m", "n", "o",
            "p", "q", "r", "s", "t", "u", "v", "w",
            "x", "y", "z", "{", "|", "}", "~", "␡"
        };

        using HighlightFunction = int(*)(int);
        constexpr static std::array<const char *, 12> HighlightFunctionNames = {
            "iscntrl", "isprint", "isspace", "isblank",
            "isgraph", "ispunct", "isalnum", "isalpha",
            "isupper", "islower", "isdigit", "isxdigit"
        };
        const static std::array<HighlightFunction, 12> HighlightFunctions = {
            std::iscntrl, std::isprint, std::isspace, std::isblank,
            std::isgraph, std::ispunct, std::isalnum, std::isalpha,
            std::isupper, std::islower, std::isdigit, std::isxdigit
        };

        static std::array<bool, 12> highlightFunctionEnabled;
        static HighlightFunction hoverHighlightFunction = nullptr;

        if (ImGui::BeginTable("##asciitable", 1 + 0x10, ImGuiTableFlags_BordersInner | ImGuiTableFlags_SizingStretchSame, ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
            ImGui::TableSetupColumn("##ascii", ImGuiTableColumnFlags_WidthFixed, ImGui::GetTextLineHeight());
            for (u8 i = 0; i < 0x10; i += 1) {
                ImGui::TableSetupColumn(fmt::format("{:X}", i).c_str());
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            for (u8 i = 0; i < 0x10; i += 1) {
                ImGui::TableNextColumn();
                ImGuiExt::TextFormatted(" {:X}", i);
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(ImGuiCol_TableHeaderBg));
            }

            for (u8 row = 0; row < 0x08; row += 1) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGuiExt::TextFormatted(" {:X}", row);
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(ImGuiCol_TableHeaderBg));

                for (u8 column = 0; column < 0x10; column += 1) {
                    ImGui::TableNextColumn();

                    const auto character = row * 0x10 + column;
                    for (size_t i = 0; i < HighlightFunctions.size(); i++) {
                        if (highlightFunctionEnabled[i] && HighlightFunctions[i](character) != 0) {
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(ImGuiCustomCol_Highlight));
                            break;
                        }
                    }
                    if (hoverHighlightFunction != nullptr && hoverHighlightFunction(character) != 0) {
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::GetColorU32(ImGuiCol_PlotHistogram, 0.25F));
                    }

                    if (ImGui::GetIO().KeyShift)
                        ImGuiExt::TextFormattedDisabled("0x{:02X}", character);
                    else
                        ImGuiExt::TextFormatted(" {}",  Characters[character]);
                }
            }

            {
                const auto hoveredRow = ImGui::TableGetHoveredRow();
                const auto hoveredColumn = ImGui::TableGetHoveredColumn();
                if (hoveredRow > 0 && hoveredColumn > 0) {
                    const auto character = (hoveredRow - 1) * 0x10 + (hoveredColumn - 1);
                    ImGui::SetNextWindowSize(ImVec2(ImGui::CalcTextSize(" bin: 0b00000000 ").x, 0), ImGuiCond_Always);
                    if (ImGui::BeginTooltip()) {
                        fonts::Default().push(2.0F);
                        ImGuiExt::TextFormattedCenteredHorizontal(" {}", Characters[character]);
                        fonts::Default().pop();

                        ImGui::Separator();

                        ImGuiExt::TextFormatted("dec: {}",       character);
                        ImGuiExt::TextFormatted("hex: 0x{:02X}", character);
                        ImGuiExt::TextFormatted("oct: 0o{:03o}", character);
                        ImGuiExt::TextFormatted("bin: 0b{:08b}", character);

                        ImGui::EndTooltip();
                    }
                }
            }

            ImGui::EndTable();
        }

        hoverHighlightFunction = nullptr;
        for (size_t i = 0; i < HighlightFunctions.size(); i++) {
            if (i % 6 != 0)
                ImGui::SameLine();

            auto &spacing = ImGui::GetStyle().ItemSpacing.x;
            ImGuiExt::DimmedButtonToggle(HighlightFunctionNames[i], &highlightFunctionEnabled[i], ImVec2((ImGui::GetWindowSize().x - spacing) / 6 - spacing, 0));
            if (ImGui::IsItemHovered()) {
                hoverHighlightFunction = HighlightFunctions[i];
            }

        }
    }

    void drawASCIITable() {
        static bool columnLayout = false;

        ImGuiExt::DimmedIconToggle(ICON_VS_SPLIT_HORIZONTAL, &columnLayout);

        if (columnLayout)
            drawColumnASCIITable();
        else
            drawGridASCIITable();
    }

}

