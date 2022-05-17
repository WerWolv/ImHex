#include "content/views/view_command_palette.hpp"

#include <hex/api/content_registry.hpp>

#include <cstring>

namespace hex::plugin::builtin {

    ViewCommandPalette::ViewCommandPalette() : View("hex.builtin.view.command_palette.name") {
        this->m_commandBuffer = std::vector<char>(1024, 0x00);

        ShortcutManager::addGlobalShortcut(CTRL + SHIFT + Keys::P, [this] {
            EventManager::post<RequestOpenPopup>("hex.builtin.view.command_palette.name"_lang);
            this->m_commandPaletteOpen = true;
            this->m_justOpened         = true;
        });
    }

    void ViewCommandPalette::drawContent() {

        if (!this->m_commandPaletteOpen) return;

        auto windowPos  = ImHexApi::System::getMainWindowPosition();
        auto windowSize = ImHexApi::System::getMainWindowSize();

        ImGui::SetNextWindowPos(ImVec2(windowPos.x + windowSize.x * 0.5F, windowPos.y), ImGuiCond_Always, ImVec2(0.5F, 0.0F));
        if (ImGui::BeginPopup("hex.builtin.view.command_palette.name"_lang)) {
            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            ImGui::PushItemWidth(-1);
            if (ImGui::InputText(
                    "##command_input", this->m_commandBuffer.data(), this->m_commandBuffer.size(), ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_EnterReturnsTrue, [](ImGuiInputTextCallbackData *callbackData) -> int {
                        auto _this           = static_cast<ViewCommandPalette *>(callbackData->UserData);
                        _this->m_lastResults = _this->getCommandResults(callbackData->Buf);

                        return 0;
                    },
                    this)) {
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
                auto textState = ImGui::GetInputTextState(ImGui::GetID("##command_input"));
                if (textState != nullptr) {
                    textState->Stb.cursor = strlen(this->m_commandBuffer.data());
                }

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

    std::vector<ViewCommandPalette::CommandResult> ViewCommandPalette::getCommandResults(const std::string &input) {
        constexpr auto MatchCommand = [](const std::string &currCommand, const std::string &commandToMatch) -> std::pair<MatchType, std::string_view> {
            if (currCommand.empty()) {
                return { MatchType::InfoMatch, "" };
            } else if (currCommand.size() <= commandToMatch.size()) {
                if (commandToMatch.starts_with(currCommand))
                    return { MatchType::PartialMatch, currCommand };
                else
                    return { MatchType::NoMatch, {} };
            } else {
                if (currCommand.starts_with(commandToMatch))
                    return { MatchType::PerfectMatch, currCommand.substr(commandToMatch.length()) };
                else
                    return { MatchType::NoMatch, {} };
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
                        auto matchedCommand = input.substr(command.length());
                        results.push_back({ displayCallback(matchedCommand), matchedCommand, executeCallback });
                    }
                }
            } else if (type == ContentRegistry::CommandPaletteCommands::Type::KeywordCommand) {
                if (auto [match, value] = MatchCommand(input, command + " "); match != MatchType::NoMatch) {
                    if (match != MatchType::PerfectMatch)
                        results.push_back({ command + " (" + LangEntry(unlocalizedDescription) + ")", "", AutoComplete });
                    else {
                        auto matchedCommand = input.substr(command.length() + 1);
                        results.push_back({ displayCallback(matchedCommand), matchedCommand, executeCallback });
                    }
                }
            }
        }

        return results;
    }

}