#include "views/view_command_palette.hpp"

#include <GLFW/glfw3.h>

namespace hex {

    ViewCommandPalette::ViewCommandPalette() : View("hex.view.command_palette.name") {
        this->m_commandBuffer.resize(1024, 0x00);
    }

    ViewCommandPalette::~ViewCommandPalette() {

    }

    void ViewCommandPalette::drawContent() {

        if (!this->m_commandPaletteOpen) return;

        ImGui::SetNextWindowPos(ImVec2(SharedData::windowPos.x + SharedData::windowSize.x * 0.5F, SharedData::windowPos.y), ImGuiCond_Always, ImVec2(0.5F,0.0F));
        if (ImGui::BeginPopup("hex.view.command_palette.name"_lang)) {
            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            ImGui::PushItemWidth(-1);
            if (ImGui::InputText("##nolabel", this->m_commandBuffer.data(), this->m_commandBuffer.size(), ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_EnterReturnsTrue,
            [](ImGuiInputTextCallbackData *callbackData) -> int {
                auto _this = static_cast<ViewCommandPalette*>(callbackData->UserData);
                _this->m_lastResults = _this->getCommandResults(callbackData->Buf);

                return 0;
            }, this)) {
                if (!this->m_lastResults.empty()) {
                    auto &[displayResult, matchedCommand, callback] = this->m_lastResults.front();
                    callback(matchedCommand);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopItemWidth();

            if (this->m_justOpened) {
                focusInputTextBox();
                this->m_lastResults = this->getCommandResults("");
                std::memset(this->m_commandBuffer.data(), 0x00, this->m_commandBuffer.size());
                this->m_justOpened = false;
            }

            if (this->m_focusInputTextBox) {
                ImGui::SetKeyboardFocusHere(0);
                this->m_focusInputTextBox = false;
            }

            ImGui::Separator();

            for (const auto &[displayResult, matchedCommand, callback] : this->m_lastResults) {
                if (ImGui::Selectable(displayResult.c_str(), false, ImGuiSelectableFlags_DontClosePopups))
                    callback(matchedCommand);
            }

            ImGui::EndPopup();
        } else {
            this->m_commandPaletteOpen = false;
        }

    }

    void ViewCommandPalette::drawMenu() {

    }

    bool ViewCommandPalette::handleShortcut(bool keys[512], bool ctrl, bool shift, bool alt) {
        if (ctrl && shift && keys['P']) {
            View::doLater([this] {
                ImGui::OpenPopup("hex.view.command_palette.name"_lang);
                this->m_commandPaletteOpen = true;
                this->m_justOpened = true;
            });
            return true;
        }

        return false;
    }

    std::vector<ViewCommandPalette::CommandResult> ViewCommandPalette::getCommandResults(std::string_view input) {
        constexpr auto MatchCommand = [](std::string_view currCommand, std::string_view commandToMatch) -> std::pair<MatchType, std::string_view> {
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

        std::vector<CommandResult> results;

        for (const auto &[type, command, unlocalizedDescription, displayCallback, executeCallback] : ContentRegistry::CommandPaletteCommands::getEntries()) {

            auto AutoComplete = [this, &currCommand = command](auto) {
                focusInputTextBox();
                std::strncpy(this->m_commandBuffer.data(), currCommand.data(), this->m_commandBuffer.size());
                this->m_lastResults = this->getCommandResults(currCommand);
            };

            if (type == ContentRegistry::CommandPaletteCommands::Type::SymbolCommand) {
                if (auto [match, value] = MatchCommand(input, command); match != MatchType::NoMatch) {
                    if (match != MatchType::PerfectMatch)
                        results.push_back({ command + " (" + LangEntry(unlocalizedDescription) + ")", "", AutoComplete });
                    else {
                        auto matchedCommand = input.substr(command.length()).data();
                        results.push_back({ displayCallback(matchedCommand), matchedCommand, executeCallback });
                    }
                }
            } else if (type == ContentRegistry::CommandPaletteCommands::Type::KeywordCommand) {
                if (auto [match, value] = MatchCommand(input, command + " "); match != MatchType::NoMatch) {
                    if (match != MatchType::PerfectMatch)
                        results.push_back({ command + " (" + LangEntry(unlocalizedDescription) + ")", "", AutoComplete });
                    else {
                        auto matchedCommand = input.substr(command.length() + 1).data();
                        results.push_back({ displayCallback(matchedCommand), matchedCommand, executeCallback });
                    }
                }
            }

        }

        return results;
    }

}