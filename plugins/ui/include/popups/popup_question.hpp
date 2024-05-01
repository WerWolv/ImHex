#pragma once

#include <hex/ui/popup.hpp>

#include <hex/api/localization_manager.hpp>

#include <functional>
#include <string>

namespace hex::ui {

    class PopupQuestion : public Popup<PopupQuestion> {
    public:
        PopupQuestion(std::string message, std::function<void()> yesFunction, std::function<void()> noFunction)
                : hex::Popup<PopupQuestion>("hex.ui.common.question", false),
                  m_message(std::move(message)),
                  m_yesFunction(std::move(yesFunction)), m_noFunction(std::move(noFunction)) { }

        void drawContent() override {
            ImGuiExt::TextFormattedWrapped("{}", m_message.c_str());
            ImGui::NewLine();
            ImGui::Separator();

            auto width = ImGui::GetWindowWidth();
            ImGui::SetCursorPosX(width / 9);
            if (ImGui::Button("hex.ui.common.yes"_lang, ImVec2(width / 3, 0))) {
                m_yesFunction();
                this->close();
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(width / 9 * 5);
            if (ImGui::Button("hex.ui.common.no"_lang, ImVec2(width / 3, 0))) {
                m_noFunction();
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
        std::string m_message;
        std::function<void()> m_yesFunction, m_noFunction;
    };

    class PopupCancelableQuestion : public Popup<PopupCancelableQuestion> {
    public:
        PopupCancelableQuestion(std::string message, std::function<void()> yesFunction, std::function<void()> noFunction)
                : hex::Popup<PopupCancelableQuestion>("hex.ui.common.question", false),
                  m_message(std::move(message)),
                  m_yesFunction(std::move(yesFunction)), m_noFunction(std::move(noFunction)) { }

        void drawContent() override {
            ImGuiExt::TextFormattedWrapped("{}", m_message.c_str());
            ImGui::NewLine();
            ImGui::Separator();

            auto width = ImGui::GetWindowWidth();
            ImGui::SetCursorPosX(width / 9);
            if (ImGui::Button("hex.ui.common.yes"_lang, ImVec2(width / 3, 0))) {
                m_yesFunction();
                this->close();
            }
            ImGui::SameLine();
            if (ImGui::Button("hex.ui.common.no"_lang, ImVec2(width / 3, 0))) {
                m_noFunction();
                this->close();
            }
            ImGui::SameLine();
            if (ImGui::Button("hex.ui.common.cancel"_lang, ImVec2(width / 3, 0))) {
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
        std::string m_message;
        std::function<void()> m_yesFunction, m_noFunction;
    };

}