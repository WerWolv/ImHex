#include "content/views/view_command_palette.hpp"

#include <hex/api/content_registry.hpp>
#include <wolv/utils/guards.hpp>

#include <cstring>

namespace hex::plugin::builtin {

    ViewCommandPalette::ViewCommandPalette() : View("hex.builtin.view.command_palette.name") {
        ShortcutManager::addGlobalShortcut(CTRLCMD + SHIFT + Keys::P, [this] {
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
            if (ImGui::InputText("##command_input", this->m_commandBuffer)) {
                this->m_lastResults = this->getCommandResults(this->m_commandBuffer);
            }
            ImGui::PopItemWidth();
            ImGui::SetItemDefaultFocus();

            if (this->m_focusInputTextBox) {
                ImGui::SetKeyboardFocusHere(-1);
                ImGui::ActivateItem(ImGui::GetID("##command_input"));

                auto textState = ImGui::GetInputTextState(ImGui::GetID("##command_input"));
                if (textState != nullptr) {
                    textState->Stb.cursor =
                    textState->Stb.select_start =
                    textState->Stb.select_end = this->m_commandBuffer.size();
                }

                this->m_focusInputTextBox = false;
            }

            if (ImGui::IsItemFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))) {
                if (!this->m_lastResults.empty()) {
                    auto &[displayResult, matchedCommand, callback] = this->m_lastResults.front();
                    callback(matchedCommand);
                }
                ImGui::CloseCurrentPopup();
            }

            if (this->m_justOpened) {
                focusInputTextBox();
                this->m_lastResults = this->getCommandResults("");
                this->m_commandBuffer.clear();
                this->m_justOpened = false;
            }

            ImGui::Separator();

            if (ImGui::BeginChild("##results", ImGui::GetContentRegionAvail(), false, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_NavFlattened)) {
                for (const auto &[displayResult, matchedCommand, callback] : this->m_lastResults) {;
                    ImGui::PushAllowKeyboardFocus(true);
                    ON_SCOPE_EXIT { ImGui::PopAllowKeyboardFocus(); };

                    if (ImGui::Selectable(displayResult.c_str(), false, ImGuiSelectableFlags_DontClosePopups)) {
                        callback(matchedCommand);
                        break;
                    }
                    if (ImGui::IsItemFocused() && (ImGui::IsKeyDown(ImGuiKey_Enter) || ImGui::IsKeyDown(ImGuiKey_KeypadEnter))) {
                        callback(matchedCommand);
                        break;
                    }
                }
                ImGui::EndChild();
            }

            ImGui::EndPopup();
        } else {
            this->m_commandPaletteOpen = false;
        }
    }

    std::vector<ViewCommandPalette::CommandResult> ViewCommandPalette::getCommandResults(const std::string &input) {
        constexpr static auto MatchCommand = [](const std::string &currCommand, const std::string &commandToMatch) -> std::pair<MatchType, std::string_view> {
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

            auto AutoComplete = [this, currCommand = command](auto) {
                this->focusInputTextBox();
                this->m_commandBuffer = currCommand;
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

        for (const auto &handler : ContentRegistry::CommandPaletteCommands::getHandlers()) {
            const auto &[type, command, queryCallback, displayCallback] = handler;

            auto processedInput = input;
            if (processedInput.starts_with(command))
                processedInput = processedInput.substr(command.length());

            for (const auto &[description, callback] : queryCallback(processedInput)) {
                if (type == ContentRegistry::CommandPaletteCommands::Type::SymbolCommand) {
                    if (auto [match, value] = MatchCommand(input, command); match != MatchType::NoMatch) {
                        results.push_back({ hex::format("{} ({})", command, description), "", callback });
                    }
                } else if (type == ContentRegistry::CommandPaletteCommands::Type::KeywordCommand) {
                    if (auto [match, value] = MatchCommand(input, command + " "); match != MatchType::NoMatch) {
                        results.push_back({ hex::format("{} ({})", command, description), "", callback });
                    }
                }
            }
        }

        return results;
    }

}