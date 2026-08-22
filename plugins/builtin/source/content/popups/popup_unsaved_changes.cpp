#include "content/popups/popup_unsaved_changes.hpp"

#include <fonts/vscode_icons.hpp>
#include <hex/api/imhex_api/system.hpp>
#include <hex/helpers/scaling.hpp>

namespace hex::plugin::builtin {

    PopupUnsavedChanges::PopupUnsavedChanges(std::vector<ProviderDirtyState> providers,
                        std::function<void()> saveDataFunction, std::function<void()> saveProjectFunction,
                        std::function<void()> discardFunction, std::function<void()> cancelFunction)
            : hex::Popup<PopupUnsavedChanges>("hex.ui.common.question"_unlocalized, ICON_VS_SAVE_AS, false),
              m_providers(std::move(providers)),
              m_saveDataFunction(std::move(saveDataFunction)), m_saveProjectFunction(std::move(saveProjectFunction)),
              m_discardFunction(std::move(discardFunction)), m_cancelFunction(std::move(cancelFunction)) { }

    void PopupUnsavedChanges::drawContent() {
        ImGuiExt::TextFormattedWrapped("{}", "hex.builtin.popup.unsaved_changes.desc"_lang.get());
        ImGui::NewLine();

        bool anyDataDirty = false;
        bool anyMetadataDirty = false;
        for (const auto &entry : m_providers) {
            if (entry.dataDirty) anyDataDirty = true;
            if (entry.metadataDirty) anyMetadataDirty = true;
        }

        if (ImGui::BeginTable("##unsaved_providers", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp, ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 4))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Provider", ImGuiTableColumnFlags_WidthStretch, 0.6F);
            ImGui::TableSetupColumn("hex.ui.common.data"_lang, ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoClip, ImGui::CalcTextSize("hex.ui.common.data"_lang).x);
            ImGui::TableSetupColumn("hex.ui.common.metadata"_lang, ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoClip, ImGui::CalcTextSize("hex.ui.common.metadata"_lang).x);

            // Manually render header row so IsItemHovered works on each cell
            ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
            for (int col = 0; col < ImGui::TableGetColumnCount(); col++) {
                ImGui::TableNextColumn();
                ImGui::TableHeader(ImGui::TableGetColumnName(col));
                if (ImGui::IsItemHovered()) {
                    if (col == 1)
                        ImGui::SetTooltip("%s", "hex.ui.common.data.tooltip"_lang.get());
                    else if (col == 2)
                        ImGui::SetTooltip("%s", "hex.ui.common.metadata.tooltip"_lang.get());
                }
            }

            for (const auto &entry : m_providers) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(entry.provider->getName().c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(entry.dataDirty ? ICON_VS_CHECK : "");
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(entry.metadataDirty ? ICON_VS_CHECK : "");
            }
            ImGui::EndTable();
        }

        ImGui::NewLine();
        ImGui::Separator();

        const auto tableWidth = ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2;
        if (ImGui::BeginTable("##buttons", 2, ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_SizingFixedSame, ImVec2(tableWidth, 0))) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            const auto buttonWidth = (tableWidth / 2) - ImGui::GetStyle().FramePadding.x;

            // Save data only (grayed out if no data is dirty)
            if (!anyDataDirty) ImGui::BeginDisabled();
            if (ImGuiExt::DimmedButton("hex.ui.common.save.data_only"_lang, ImVec2(buttonWidth, 0))) {
                m_saveDataFunction();
                this->close();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s", "hex.ui.common.data.tooltip"_lang.get());
            if (!anyDataDirty) ImGui::EndDisabled();

            ImGui::TableNextColumn();

            // Save data & project
            if (!anyMetadataDirty) ImGui::BeginDisabled();
            if (ImGuiExt::DimmedButton("hex.ui.common.save.project"_lang, ImVec2(buttonWidth, 0))) {
                m_saveProjectFunction();
                this->close();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s", anyMetadataDirty ? "hex.ui.common.metadata.tooltip"_lang.get() : "hex.ui.common.metadata.disabled.tooltip"_lang.get());
            if (!anyMetadataDirty) ImGui::EndDisabled();

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            // Discard
            if (ImGuiExt::DimmedButton("hex.ui.common.discard"_lang, ImVec2(buttonWidth, 0))) {
                m_discardFunction();
                this->close();
            }

            ImGui::TableNextColumn();

            // Cancel
            if (ImGuiExt::DimmedButton("hex.ui.common.cancel"_lang, ImVec2(buttonWidth, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                m_cancelFunction();
                this->close();
            }

            ImGui::EndTable();
        }

        ImGui::SetWindowPos((ImHexApi::System::getMainWindowSize() - ImGui::GetWindowSize()) / 2, ImGuiCond_Appearing);
    }

    ImGuiWindowFlags PopupUnsavedChanges::getFlags() const {
        return ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    }

    ImVec2 PopupUnsavedChanges::getMinSize() const {
        return scaled({ 400, 100 });
    }

    ImVec2 PopupUnsavedChanges::getMaxSize() const {
        return scaled({ 600, 600 });
    }

}
