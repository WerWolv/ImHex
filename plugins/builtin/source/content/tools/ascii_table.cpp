#include <hex/helpers/utils.hpp>
#include <hex/api/localization_manager.hpp>

#include <imgui.h>

#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::plugin::builtin {

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
            ImGui::TableNextColumn();
        }
        ImGui::EndTable();

        ImGui::Checkbox("hex.builtin.tools.ascii_table.octal"_lang, &asciiTableShowOctal);
    }

}

