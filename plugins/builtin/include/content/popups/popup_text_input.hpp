#include <hex/ui/popup.hpp>

#include <hex/api/localization.hpp>

#include <functional>
#include <string>

namespace hex::plugin::builtin {

    class PopupTextInput : public Popup<PopupTextInput> {
    public:
        PopupTextInput(std::string unlocalizedName, std::string message, std::function<void(std::string)> function)
            : hex::Popup<PopupTextInput>(std::move(unlocalizedName), false),
              m_message(std::move(message)), m_function(std::move(function)) { }

        void drawContent() override {
            ImGui::TextFormattedWrapped("{}", this->m_message.c_str());
            ImGui::NewLine();

            ImGui::PushItemWidth(-1);
            ImGui::SetKeyboardFocusHere();
            ImGui::InputTextIcon("##input", ICON_VS_SYMBOL_KEY, this->m_input);
            ImGui::PopItemWidth();

            ImGui::NewLine();
            ImGui::Separator();

            auto width = ImGui::GetWindowWidth();
            ImGui::SetCursorPosX(width / 9);
            if (ImGui::Button("hex.builtin.common.okay"_lang, ImVec2(width / 3, 0)) || ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter))) {
                this->m_function(this->m_input);
                this->close();
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(width / 9 * 5);
            if (ImGui::Button("hex.builtin.common.cancel"_lang, ImVec2(width / 3, 0)) || ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape))) {
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

        std::string m_message;
        std::function<void(std::string)> m_function;
    };

}