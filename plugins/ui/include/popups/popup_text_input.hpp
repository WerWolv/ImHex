#pragma once

#include <hex/ui/popup.hpp>

#include <hex/api/imhex_api.hpp>
#include <hex/api/localization_manager.hpp>

#include <functional>
#include <string>

#include <fonts/vscode_icons.hpp>

namespace hex::ui {

    class PopupTextInput : public Popup<PopupTextInput> {
    public:
        PopupTextInput(UnlocalizedString unlocalizedName, UnlocalizedString message, std::function<void(std::string)> function)
            : hex::Popup<PopupTextInput>(std::move(unlocalizedName), false),
              m_message(std::move(message)), m_function(std::move(function)) { }

        void drawContent() override {
            ImGuiExt::TextFormattedWrapped("{}", Lang(m_message));
            ImGui::NewLine();

            ImGui::PushItemWidth(-1);

            if (m_justOpened) {
                ImGui::SetKeyboardFocusHere();
                m_justOpened = false;
            }

            ImGuiExt::InputTextIcon("##input", ICON_VS_SYMBOL_KEY, m_input);
            ImGui::PopItemWidth();

            ImGui::NewLine();
            ImGui::Separator();

            auto width = ImGui::GetWindowWidth();
            ImGui::SetCursorPosX(width / 9);
            if (ImGui::Button("hex.ui.common.okay"_lang, ImVec2(width / 3, 0)) || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                m_function(m_input);
                this->close();
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(width / 9 * 5);
            if (ImGui::Button("hex.ui.common.cancel"_lang, ImVec2(width / 3, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                this->close();
            }

            ImGui::SetWindowPos((ImHexApi::System::getMainWindowSize() - ImGui::GetWindowSize()) / 2, ImGuiCond_Appearing);
        }

        [[nodiscard]] ImGuiWindowFlags getFlags() const override {
            return ImGuiWindowFlags_AlwaysAutoResize;
        }

        [[nodiscard]] ImVec2 getMinSize() const override {
            return scaled({ 400, 100 });
        }

        [[nodiscard]] ImVec2 getMaxSize() const override {
            return scaled({ 600, 300 });
        }

    private:
        std::string m_input;

        UnlocalizedString m_message;
        std::function<void(std::string)> m_function;
        bool m_justOpened = true;
    };

}