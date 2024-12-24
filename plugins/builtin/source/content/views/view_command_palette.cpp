#include "content/views/view_command_palette.hpp"

#include <hex/api/content_registry.hpp>
#include <wolv/utils/guards.hpp>

#include "imstb_textedit.h"

namespace hex::plugin::builtin {

    ViewCommandPalette::ViewCommandPalette() : View::Special("hex.builtin.view.command_palette.name") {
        // Add global shortcut to open the command palette
        ShortcutManager::addGlobalShortcut(CTRLCMD + SHIFT + Keys::P, "hex.builtin.view.command_palette.name", [this] {
            RequestOpenPopup::post("hex.builtin.view.command_palette.name"_lang);
            m_commandPaletteOpen = true;
            m_justOpened         = true;
        });

        EventSearchBoxClicked::subscribe([this](ImGuiMouseButton button) {
            if (button == ImGuiMouseButton_Left) {
                RequestOpenPopup::post("hex.builtin.view.command_palette.name"_lang);
                m_commandPaletteOpen = true;
                m_justOpened         = true;
            }
        });
    }

    void ViewCommandPalette::drawAlwaysVisibleContent() {
        // If the command palette is hidden, don't draw it
        if (!m_commandPaletteOpen) return;

        auto windowPos  = ImHexApi::System::getMainWindowPosition();
        auto windowSize = ImHexApi::System::getMainWindowSize();

        ImGui::SetNextWindowPos(ImVec2(windowPos.x + windowSize.x * 0.5F, windowPos.y), ImGuiCond_Always, ImVec2(0.5F, 0.0F));
        ImGui::SetNextWindowSizeConstraints(this->getMinSize(), this->getMaxSize());
        if (ImGui::BeginPopup("hex.builtin.view.command_palette.name"_lang)) {
            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindowRead());
            ImGui::BringWindowToFocusFront(ImGui::GetCurrentWindowRead());

            // Close the popup if the user presses ESC
            if (ImGui::IsKeyDown(ImGuiKey_Escape))
                ImGui::CloseCurrentPopup();


            const auto buttonColor = [](float alpha) {
                return ImU32(ImColor(ImGui::GetStyleColorVec4(ImGuiCol_ModalWindowDimBg) * ImVec4(1, 1, 1, alpha)));
            };

            // Draw the main input text box
            ImGui::PushItemWidth(-1);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, buttonColor(0.5F));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, buttonColor(0.7F));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, buttonColor(0.9F));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0_scaled);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4_scaled);

            if (ImGui::InputText("##command_input", m_commandBuffer)) {
                m_lastResults = this->getCommandResults(m_commandBuffer);
            }
            ImGui::SetItemKeyOwner(ImGuiKey_LeftAlt, ImGuiInputFlags_CondActive);

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);
            ImGui::PopItemWidth();
            ImGui::SetItemDefaultFocus();

            if (m_moveCursorToEnd) {
                auto textState = ImGui::GetInputTextState(ImGui::GetID("##command_input"));
                if (textState != nullptr) {
                    auto stb = reinterpret_cast<STB_TexteditState*>(textState->Stb);
                    stb->cursor =
                    stb->select_start =
                    stb->select_end = m_commandBuffer.size();
                }
                m_moveCursorToEnd = false;
            }

            // Handle giving back focus to the input text box
            if (m_focusInputTextBox) {
                ImGui::SetKeyboardFocusHere(-1);
                ImGui::ActivateItemByID(ImGui::GetID("##command_input"));

                m_focusInputTextBox = false;
                m_moveCursorToEnd = true;
            }

            // Execute the currently selected command when pressing enter
            if (ImGui::IsItemFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))) {
                bool closePalette = true;
                if (!m_lastResults.empty()) {
                    auto &[displayResult, matchedCommand, callback] = m_lastResults.front();

                    if (auto result = callback(matchedCommand); result.has_value()) {
                        m_commandBuffer = result.value();
                        closePalette = false;
                        m_focusInputTextBox = true;
                    }
                }

                if (closePalette) {
                    ImGui::CloseCurrentPopup();
                }
            }

            // Focus the input text box when the popup is opened
            if (m_justOpened) {
                focusInputTextBox();
                m_lastResults = this->getCommandResults("");
                m_commandBuffer.clear();
                m_justOpened = false;
            }

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y);

            ImGui::Separator();

            // Draw the results
            if (ImGui::BeginChild("##results", ImGui::GetContentRegionAvail(), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
                for (const auto &[displayResult, matchedCommand, callback] : m_lastResults) {
                    ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, false);
                    ON_SCOPE_EXIT { ImGui::PopItemFlag(); };

                    // Allow executing a command by clicking on it or selecting it with the keyboard and pressing enter
                    if (ImGui::Selectable(displayResult.c_str(), false, ImGuiSelectableFlags_NoAutoClosePopups)) {
                        if (auto result = callback(matchedCommand); result.has_value())
                            m_commandBuffer = result.value();

                        break;
                    }
                    if (ImGui::IsItemFocused() && (ImGui::IsKeyDown(ImGuiKey_Enter) || ImGui::IsKeyDown(ImGuiKey_KeypadEnter))) {
                        if (auto result = callback(matchedCommand); result.has_value())
                            m_commandBuffer = result.value();

                        break;
                    }
                }
            }
            ImGui::EndChild();

            ImGui::EndPopup();
        } else {
            m_commandPaletteOpen = false;
        }
    }

    std::vector<ViewCommandPalette::CommandResult> ViewCommandPalette::getCommandResults(const std::string &input) {
        constexpr static auto MatchCommand = [](const std::string &currCommand, const std::string &commandToMatch) -> std::pair<MatchType, std::string_view> {
            // Check if the current input matches a command
            // NoMatch means that the input doesn't match the command
            // PartialMatch means that the input partially matches the command but is not a perfect match
            // PerfectMatch means that the input perfectly matches the command

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

        // Loop over every registered command and check if the input matches it
        for (const auto &[type, command, unlocalizedDescription, displayCallback, executeCallback] : ContentRegistry::CommandPaletteCommands::impl::getEntries()) {

            auto AutoComplete = [this, currCommand = command](auto) {
                this->focusInputTextBox();
                m_commandBuffer = currCommand + " ";
                m_lastResults = this->getCommandResults(currCommand);

                return std::nullopt;
            };

            if (type == ContentRegistry::CommandPaletteCommands::Type::SymbolCommand) {
                // Handle symbol commands
                // These commands are used by entering a single symbol and then any input

                if (auto [match, value] = MatchCommand(input, command); match != MatchType::NoMatch) {
                    if (match != MatchType::PerfectMatch) {
                        results.push_back({ hex::format("{} ({})", command, Lang(unlocalizedDescription)), "", AutoComplete });
                    } else {
                        auto matchedCommand = wolv::util::trim(input.substr(command.length()));
                        results.push_back({ displayCallback(matchedCommand), matchedCommand, executeCallback });
                    }
                }
            } else if (type == ContentRegistry::CommandPaletteCommands::Type::KeywordCommand) {
                // Handle keyword commands
                // These commands are used by entering a keyword followed by a space and then any input

                if (auto [match, value] = MatchCommand(input, command + " "); match != MatchType::NoMatch) {
                    if (match != MatchType::PerfectMatch) {
                        results.push_back({ hex::format("{} ({})", command, Lang(unlocalizedDescription)), "", AutoComplete });
                    } else {
                        auto matchedCommand = wolv::util::trim(input.substr(command.length() + 1));
                        results.push_back({ displayCallback(matchedCommand), matchedCommand, executeCallback });
                    }
                }
            }
        }

        // WHen a command has been identified, show the query results for that command
        for (const auto &handler : ContentRegistry::CommandPaletteCommands::impl::getHandlers()) {
            const auto &[type, command, queryCallback, displayCallback] = handler;

            auto processedInput = input;
            if (processedInput.starts_with(command))
                processedInput = wolv::util::trim(processedInput.substr(command.length()));

            for (const auto &[description, callback] : queryCallback(processedInput)) {
                if (type == ContentRegistry::CommandPaletteCommands::Type::SymbolCommand) {
                    if (auto [match, value] = MatchCommand(input, command); match != MatchType::NoMatch) {
                        results.push_back({ hex::format("{} ({})", command, description), "", [callback](auto ... args){ callback(args...); return std::nullopt; } });
                    }
                } else if (type == ContentRegistry::CommandPaletteCommands::Type::KeywordCommand) {
                    if (auto [match, value] = MatchCommand(input, command + " "); match != MatchType::NoMatch) {
                        results.push_back({ hex::format("{} ({})", command, description), "", [callback](auto ... args){ callback(args...); return std::nullopt; } });
                    }
                }
            }
        }

        return results;
    }

}