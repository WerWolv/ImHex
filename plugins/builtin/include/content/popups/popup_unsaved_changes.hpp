#include <hex/ui/popup.hpp>

#include <hex/api/localization.hpp>

#include <functional>
#include <string>

namespace hex::plugin::builtin {

    class PopupUnsavedChanges : public Popup<PopupUnsavedChanges> {
    public:
        PopupUnsavedChanges(std::string message, std::function<void()> yesFunction, std::function<void()> noFunction)
                : hex::Popup<PopupUnsavedChanges>("hex.builtin.common.question", false),
                  m_message(std::move(message)),
                  m_yesFunction(std::move(yesFunction)), m_noFunction(std::move(noFunction)) { }

        void drawContent() override {
            ImGui::TextFormattedWrapped("{}", this->m_message.c_str());
            ImGui::NewLine();

            if (ImGui::BeginTable("##unsaved_providers", 1, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg, ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 4))) {
                for (const auto &provider : ImHexApi::Provider::impl::getClosingProviders()) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(provider->getName().c_str());
                }
                ImGui::EndTable();
            }

            ImGui::NewLine();
            ImGui::Separator();

            auto width = ImGui::GetWindowWidth();
            ImGui::SetCursorPosX(width / 9);
            if (ImGui::Button("hex.builtin.common.yes"_lang, ImVec2(width / 3, 0))) {
                this->m_yesFunction();
                this->close();
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(width / 9 * 5);
            if (ImGui::Button("hex.builtin.common.no"_lang, ImVec2(width / 3, 0))) {
                this->m_noFunction();
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
            return scaled({ 600, 300 });
        }

    private:
        std::string m_message;
        std::function<void()> m_yesFunction, m_noFunction;
    };

}