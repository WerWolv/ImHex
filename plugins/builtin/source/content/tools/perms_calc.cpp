#include <hex/helpers/fmt.hpp>
#include <hex/api/localization_manager.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::plugin::builtin {

    void drawPermissionsCalculator() {
        static bool setuid, setgid, sticky;
        static bool r[3], w[3], x[3];

        ImGuiExt::Header("hex.builtin.tools.permissions.perm_bits"_lang, true);

        if (ImGui::BeginTable("Permissions", 4, ImGuiTableFlags_Borders)) {
            ImGui::TableSetupColumn("Special", ImGuiTableColumnFlags_NoSort);
            ImGui::TableSetupColumn("User", ImGuiTableColumnFlags_NoSort);
            ImGui::TableSetupColumn("Group", ImGuiTableColumnFlags_NoSort);
            ImGui::TableSetupColumn("Other", ImGuiTableColumnFlags_NoSort);
            ImGui::TableSetupScrollFreeze(0, 1);

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

        ImGuiExt::Header("hex.builtin.tools.permissions.absolute"_lang);

        auto result = hex::format("{}{}{}{}",
            (setuid << 2) | (setgid << 1) | (sticky << 0),
            (r[0] << 2) | (w[0] << 1) | (x[0] << 0),
            (r[1] << 2) | (w[1] << 1) | (x[1] << 0),
            (r[2] << 2) | (w[2] << 1) | (x[2] << 0));
        ImGui::InputText("##permissions_absolute", result.data(), result.size(), ImGuiInputTextFlags_ReadOnly);

        ImGui::NewLine();

        constexpr static auto WarningColor = ImColor(0.92F, 0.25F, 0.2F, 1.0F);
        if (setuid && !x[0])
            ImGuiExt::TextFormattedColored(WarningColor, "{}", "hex.builtin.tools.permissions.setuid_error"_lang);
        if (setgid && !x[1])
            ImGuiExt::TextFormattedColored(WarningColor, "{}", "hex.builtin.tools.permissions.setgid_error"_lang);
        if (sticky && !x[2])
            ImGuiExt::TextFormattedColored(WarningColor, "{}", "hex.builtin.tools.permissions.sticky_error"_lang);
    }

}
