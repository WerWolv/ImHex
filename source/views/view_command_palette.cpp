#include "views/view_command_palette.hpp"

#include <GLFW/glfw3.h>

#include "helpers/math_evaluator.hpp"

namespace hex {

    ViewCommandPalette::ViewCommandPalette() : View("Command Palette") {
        this->getWindowOpenState() = true;

        this->m_commandBuffer.resize(1024, 0x00);
        this->m_lastResults = this->getCommandResults("");
    }

    ViewCommandPalette::~ViewCommandPalette() {

    }

    void ViewCommandPalette::createView() {

        auto windowPos = View::getWindowPosition();
        auto windowSize = View::getWindowSize();
        auto paletteSize = this->getMinSize();
        ImGui::SetNextWindowPos(ImVec2(windowPos.x + (windowSize.x - paletteSize.x) / 2.0F, windowPos.y), ImGuiCond_Always);
        if (ImGui::BeginPopup("Command Palette")) {
            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            ImGui::PushItemWidth(paletteSize.x - ImGui::GetStyle().WindowPadding.x * 2);
            if (ImGui::InputText("##nolabel", this->m_commandBuffer.data(), this->m_commandBuffer.size(), ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_EnterReturnsTrue,
            [](ImGuiInputTextCallbackData *callbackData) -> int {
                auto _this = static_cast<ViewCommandPalette*>(callbackData->UserData);
                _this->m_lastResults = _this->getCommandResults(callbackData->Buf);

                return 0;
            }, this)) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopItemWidth();

            if (this->m_justOpened) {
                ImGui::SetKeyboardFocusHere(0);
                this->m_justOpened = false;
            }

            ImGui::Separator();

            for (const auto &result : this->m_lastResults) {
                ImGui::TextUnformatted(result.c_str());
            }

            ImGui::EndPopup();
        }
    }

    void ViewCommandPalette::createMenu() {

    }

    bool ViewCommandPalette::handleShortcut(int key, int mods) {
        if (key == GLFW_KEY_P && mods == (GLFW_MOD_SHIFT | GLFW_MOD_CONTROL)) {
            View::doLater([this] {
                this->m_justOpened = true;
                ImGui::OpenPopup("Command Palette");
            });
            return true;
        }

        return false;
    }

    enum class MatchType {
        NoMatch,
        InfoMatch,
        PartialMatch,
        PerfectMatch
    };

    std::vector<std::string> ViewCommandPalette::getCommandResults(std::string_view command) {
        constexpr auto matchCommand = [](std::string_view currCommand, std::string_view commandToMatch) -> std::pair<MatchType, std::string_view> {
            if (currCommand.empty()) {
                return { MatchType::InfoMatch, "" };
            }
            else if (currCommand.size() <= commandToMatch.size()) {
                if (commandToMatch.starts_with(currCommand))
                    return { MatchType::PartialMatch, currCommand };
                else
                    return { MatchType::NoMatch, { } };
            }
            else {
                if (currCommand.starts_with(commandToMatch))
                    return { MatchType::PerfectMatch, currCommand.substr(commandToMatch.length()) };
                else
                    return { MatchType::NoMatch, { } };
            }
        };

        std::vector<std::string> results;

        if (auto [match, value] = matchCommand(command, "#"); match != MatchType::NoMatch) {
            if (match != MatchType::PerfectMatch)
                results.emplace_back("# (Calculator)");
            else {
                MathEvaluator evaluator;
                evaluator.registerStandardVariables();
                evaluator.registerStandardFunctions();

                auto result = evaluator.evaluate(std::string(value));

                if (result.has_value())
                    results.emplace_back(hex::format("#%s = %Lf", value.data(), result.value()));
                else
                    results.emplace_back(hex::format("#%s = ???", value.data()));
            }
        }
        if (auto [match, value] = matchCommand(command, "/find "); match != MatchType::NoMatch) {
            if (match != MatchType::PerfectMatch)
                results.emplace_back("/find (Find Command)");
            else {
                results.emplace_back(hex::format("Command: Find \"%s\"", value.data()));
            }
        }
        if (auto [match, value] = matchCommand(command, ">"); match != MatchType::NoMatch) {
            results.emplace_back("> (Command)");
        }

        return results;
    }

}