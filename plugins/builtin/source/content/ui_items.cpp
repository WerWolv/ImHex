#include <hex/plugin.hpp>

#include <codicons_font.h>
#include <imgui.h>
#include <imgui_internal.h>

namespace hex::plugin::builtin {

    void addFooterItems() {

        ContentRegistry::Interface::addFooterItem([] {
            static float framerate = 0;
            if (ImGui::HasSecondPassed()) {
                framerate = 1.0F / ImGui::GetIO().DeltaTime;
            }

            ImGui::TextUnformatted(hex::format("FPS {0:.2f}", framerate).c_str());
        });

    }

    void addToolbarItems() {
        ContentRegistry::Interface::addToolbarItem([] {
            const static auto buttonSize = ImVec2(ImGui::GetCurrentWindow()->MenuBarHeight(), ImGui::GetCurrentWindow()->MenuBarHeight());
            ImGui::ToolBarButton(ICON_VS_DISCARD, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue), buttonSize);
            ImGui::ToolBarButton(ICON_VS_REDO, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue), buttonSize);

            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

            ImGui::ToolBarButton(ICON_VS_FILE, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarGray), buttonSize);
            ImGui::ToolBarButton(ICON_VS_FOLDER_OPENED, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBrown), buttonSize);

            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

            ImGui::ToolBarButton(ICON_VS_SAVE, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue), buttonSize);
            ImGui::ToolBarButton(ICON_VS_SAVE_AS, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue), buttonSize);

            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

            ImGui::ToolBarButton(ICON_VS_COPY, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarGray), buttonSize);
            ImGui::ToolBarButton(ICON_VS_OUTPUT, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarGray), buttonSize);

            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

            ImGui::ToolBarButton(ICON_VS_BOOKMARK, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarGreen), buttonSize);
        });

    }

}