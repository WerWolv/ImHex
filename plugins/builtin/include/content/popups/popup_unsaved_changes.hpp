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
                            std::function<void()> saveFunction, std::function<void()> discardFunction, std::function<void()> cancelFunction)
                : hex::Popup<PopupUnsavedChanges>("hex.ui.common.question", false),
                  m_message(std::move(message)),
                  m_providers(std::move(providers)),
                  m_saveFunction(std::move(saveFunction)), m_discardFunction(std::move(discardFunction)), m_cancelFunction(std::move(cancelFunction)) { }

        void drawContent() override {
            ImGuiExt::TextFormattedWrapped("{}", m_message.c_str());
            ImGui::NewLine();

            if (ImGui::BeginTable("##unsaved_providers", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp, ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 4))) {
                ImGui::TableSetupColumn("Provider", ImGuiTableColumnFlags_WidthStretch, 0.6F);
                ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoClip, ImGui::CalcTextSize("X").x * 2);
                ImGui::TableSetupColumn("Project", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoClip, ImGui::CalcTextSize("X").x * 2);
                ImGui::TableHeadersRow();

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
            ImGui::SetCursorPosX((width / 10) * 0.5);
            if (ImGui::Button("hex.ui.common.save"_lang, ImVec2(width / 4, 0))) {
                m_saveFunction();
                this->close();
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX((width / 10) * 3.75);
            if (ImGui::Button("hex.ui.common.discard"_lang, ImVec2(width / 4, 0))) {
                m_discardFunction();
                this->close();
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX((width / 10) * 7);
            if (ImGui::Button("hex.ui.common.cancel"_lang, ImVec2(width / 4, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
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
        std::function<void()> m_saveFunction, m_discardFunction, m_cancelFunction;
    };

}
