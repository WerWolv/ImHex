#pragma once

#include <hex/ui/popup.hpp>

#include <hex/api/localization_manager.hpp>
#include <hex/providers/provider.hpp>

#include <functional>
#include <string>
#include <vector>

namespace hex::plugin::builtin {

    struct ProviderDirtyState {
        prv::Provider *provider;
        bool dataDirty;
        bool metadataDirty;
    };

    class PopupUnsavedChanges : public Popup<PopupUnsavedChanges> {
    public:
        PopupUnsavedChanges(std::string message, std::vector<ProviderDirtyState> providers,
                            std::function<void()> saveDataFunction, std::function<void()> saveProjectFunction,
                            std::function<void()> discardFunction, std::function<void()> cancelFunction)
                : hex::Popup<PopupUnsavedChanges>("hex.ui.common.question", false),
                  m_message(std::move(message)),
                  m_providers(std::move(providers)),
                  m_saveDataFunction(std::move(saveDataFunction)), m_saveProjectFunction(std::move(saveProjectFunction)),
                  m_discardFunction(std::move(discardFunction)), m_cancelFunction(std::move(cancelFunction)) { }

        void drawContent() override {
            ImGuiExt::TextFormattedWrapped("{}", m_message.c_str());
            ImGui::NewLine();

            bool anyDataDirty = false;
            bool anyMetadataDirty = false;
            for (const auto &entry : m_providers) {
                if (entry.dataDirty) anyDataDirty = true;
                if (entry.metadataDirty) anyMetadataDirty = true;
            }

            if (ImGui::BeginTable("##unsaved_providers", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp, ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 4))) {
                ImGui::TableSetupColumn("Provider", ImGuiTableColumnFlags_WidthStretch, 0.6F);
                ImGui::TableSetupColumn("hex.ui.common.data"_lang, ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoClip, ImGui::CalcTextSize("hex.ui.common.data"_lang).x);
                ImGui::TableSetupColumn("hex.ui.common.metadata"_lang, ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoClip, ImGui::CalcTextSize("hex.ui.common.metadata"_lang).x);
                ImGui::TableHeadersRow();

                // Hover tooltip for Data column header
                if (ImGui::TableGetColumnCount() > 1) {
                    ImGui::TableSetColumnIndex(1);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", "hex.ui.common.data.tooltip"_lang.get());
                }
                // Hover tooltip for Project data column header
                if (ImGui::TableGetColumnCount() > 2) {
                    ImGui::TableSetColumnIndex(2);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", "hex.ui.common.metadata.tooltip"_lang.get());
                }

                for (const auto &entry : m_providers) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(entry.provider->getName().c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(entry.dataDirty ? "X" : "");
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(entry.metadataDirty ? "X" : "");
                }
                ImGui::EndTable();
            }

            ImGui::NewLine();
            ImGui::Separator();

            auto width = ImGui::GetWindowWidth();
            float buttonWidth = width / 5;

            // Save data only (grayed out if no data is dirty)
            ImGui::SetCursorPosX((width / 10) * 0.5);
            if (!anyDataDirty) ImGui::BeginDisabled();
            if (ImGui::Button("hex.ui.common.save.data_only"_lang, ImVec2(buttonWidth, 0))) {
                m_saveDataFunction();
                this->close();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", "hex.ui.common.data.tooltip"_lang.get());
            if (!anyDataDirty) ImGui::EndDisabled();
            ImGui::SameLine();

            // Save data & project
            if (!anyMetadataDirty) ImGui::BeginDisabled();
            if (ImGui::Button("hex.ui.common.save.project"_lang, ImVec2(buttonWidth, 0))) {
                m_saveProjectFunction();
                this->close();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s", anyMetadataDirty ? "hex.ui.common.metadata.tooltip"_lang.get() : "hex.ui.common.metadata.disabled.tooltip"_lang.get());
            if (!anyMetadataDirty) ImGui::EndDisabled();
            ImGui::SameLine();

            // Discard
            if (ImGui::Button("hex.ui.common.discard"_lang, ImVec2(buttonWidth, 0))) {
                m_discardFunction();
                this->close();
            }
            ImGui::SameLine();

            // Cancel
            if (ImGui::Button("hex.ui.common.cancel"_lang, ImVec2(buttonWidth, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                m_cancelFunction();
                this->close();
            }

            ImGui::SetWindowPos((ImHexApi::System::getMainWindowSize() - ImGui::GetWindowSize()) / 2, ImGuiCond_Appearing);
        }

        [[nodiscard]] ImGuiWindowFlags getFlags() const override {
            return ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        }

        [[nodiscard]] ImVec2 getMinSize() const override {
            return scaled({ 400, 100 });
        }

        [[nodiscard]] ImVec2 getMaxSize() const override {
            return scaled({ 600, 600 });
        }

    private:
        std::string m_message;
        std::vector<ProviderDirtyState> m_providers;
        std::function<void()> m_saveDataFunction, m_saveProjectFunction, m_discardFunction, m_cancelFunction;
    };

}
