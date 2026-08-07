#include "content/popups/hex_editor/popup_hex_editor_paste_from_source.hpp"
#include "content/views/view_hex_editor.hpp"

#include <hex/ui/imgui_imhex_extensions.h>
#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/imhex_api/hex_editor.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/literals.hpp>
#include <imgui_internal.h>
#include <fonts/vscode_icons.hpp>

namespace hex::plugin::builtin {

    PopupPasteFromSource::PopupPasteFromSource(const ImHexApi::HexEditor::ProviderRegion &selection) {
        auto &[source, dest] = m_providers;

        // Initialize source region with default values
        source.providerObjectId = ObjectIdType::SourceId;
        source.providerIndex = -1;
        source.providerValidity = false;
        source.providerSelection.address = 0;
        source.providerSelection.size = 0;
        source.providerSelection.provider = nullptr;
        source.providerSelectionToggle = false;

        // Set selected region for destination
        dest.providerObjectId = ObjectIdType::DestId;
        if (selection.getProvider() != nullptr)
        {
            dest.providerSelection.address = selection.getStartAddress();
            dest.providerSelection.size = selection.getSize();
            dest.providerSelection.provider = selection.getProvider();
            dest.providerSelectionToggle = false;
        } else {
            dest.providerIndex = -1;
            dest.providerValidity = false;
            dest.providerSelection.address = 0;
            dest.providerSelection.size = 0;
            dest.providerSelection.provider = nullptr;
            dest.providerSelectionToggle = false;
        }
    }

    UnlocalizedString PopupPasteFromSource::getTitle() const {
        return "hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.title";
    }

    bool PopupPasteFromSource::canBePinned() const {
        return true;
    }

    void PopupPasteFromSource::draw(ViewHexEditor *editor) {
        drawProviderSelectionBox();
        // ImGui::Spacing();
        drawActionButtonsBox(editor);
        // ImGui::Spacing();
        drawRegionSelectionBox(editor);
        ImGui::Spacing();
        drawPasteModeBox();
        ImGui::Spacing();
        drawPasteInfoBox();
        ImGui::Spacing();
        drawPasteDecisionBox(editor);
    }

    void PopupPasteFromSource::drawProviderSelectionBox(void) {
        auto &[source, dest] = m_providers;

        // Table for selecting source and destination provider
        if (ImGui::BeginTable("##table_provider_selection", 2, (ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchSame), ImVec2(m_getTableWidth(), 0.0f))) {
            const auto sourceColumnHeader = fmt::format("{} {}", ((ImHexApi::Provider::getCurrentProviderIndex() == source.providerIndex) ? ICON_VS_FILE_BINARY : ICON_VS_FILE),
                                                                 "hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.provider.source.title"_lang);
            const auto destColumnHeader = fmt::format("{} {}", ((ImHexApi::Provider::getCurrentProviderIndex() == dest.providerIndex) ? ICON_VS_FILE_BINARY : ICON_VS_FILE),
                                                                "hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.provider.dest.title"_lang);
            // Add two columns
            ImGui::TableSetupColumn(sourceColumnHeader.c_str());
            ImGui::TableSetupColumn(destColumnHeader.c_str());
            ImGui::TableHeadersRow();

            // First row
            ImGui::TableNextRow();

            // First column for selecting source provider
            ImGui::TableSetColumnIndex(0);
            ImGui::Spacing();
            drawProviderSelector(source);
            ImGui::Spacing();

            // Second column for selecting destination provider
            ImGui::TableSetColumnIndex(1);
            ImGui::Spacing();
            drawProviderSelector(dest);
            ImGui::Spacing();

            // Set the source/destination as current provider if column header is clicked
            const auto hoveredRow = ImGui::TableGetHoveredRow();
            const auto hoveredColumn = ImGui::TableGetHoveredColumn();
            // Source column header [0, 0]
            if ((hoveredRow == 0 && hoveredColumn == 0) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (source.providerValidity) ImHexApi::Provider::setCurrentProvider(source.providerSelection.provider);
            }
            // Destination column header [0, 1]
            if ((hoveredRow == 0 && hoveredColumn == 1) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (dest.providerValidity) ImHexApi::Provider::setCurrentProvider(dest.providerSelection.provider);
            }

            ImGui::EndTable();
        }
    }

    void PopupPasteFromSource::drawActionButtonsBox(ViewHexEditor *editor) {
        auto &[source, dest] = m_providers;

        // Table for selecting an action on selected provider/region
        if (ImGui::BeginTable("##table_action_buttons", 3, (ImGuiTableFlags_BordersOuterV | ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_PreciseWidths), ImVec2(m_getTableWidth(), 0.0f))) {
            ImGui::TableSetupColumn("hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.provider.source.title"_lang);
            ImGui::TableSetupColumn("hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.button.tip.swap.title"_lang);
            ImGui::TableSetupColumn("hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.provider.dest.title"_lang);
            // ImGui::TableHeadersRow();

            // First row
            ImGui::TableNextRow();

            // Second column: Swap action button (rendering the swap column first)
            ImGui::TableSetColumnIndex(1);
            // Read the column width (_SizingStretchSame: all columns will have same width)
            const auto availColumnWidth = (ImGui::GetContentRegionAvail().x);
            // Calculate swap button width
            const auto swapButtonWidth = (ImGui::CalcTextSize(ICON_VS_ARROW_SWAP, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0f);
            // Center the swap button
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availColumnWidth/2) - (swapButtonWidth/2));
            if (ImGuiExt::DimmedIconButton(ICON_VS_ARROW_SWAP, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                // Swap all the members of source and dest except providerObjectId
                std::swap(source.providerIndex, dest.providerIndex); 
                std::swap(source.providerValidity, dest.providerValidity);
                std::swap(source.providerSelection, dest.providerSelection);
                std::swap(source.providerSelectionToggle, dest.providerSelectionToggle);
                // Clear paste mode and hint
                m_pasteMode = PasteModeType::ModeNotSelected;
                m_pasteHint = PasteHintType::HintDefaultDescription;
            }
            ImGuiExt::InfoTooltip("hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.button.tip.swap"_lang);

            // First column: Source action buttons
            ImGui::TableSetColumnIndex(0);
            drawActionButtons(source, editor);

            // Third column: Destination action buttons
            ImGui::TableSetColumnIndex(2);
            // Calculate grouped button width
            const auto groupedButtonWidth = (ImGui::CalcTextSize(ICON_VS_DEBUG_RESTART_FRAME, nullptr, true).x +
                                             ImGui::CalcTextSize(ICON_VS_CHECKLIST, nullptr, true).x +
                                             ImGui::CalcTextSize(ICON_VS_CHEVRON_UP, nullptr, true).x +
                                             ImGui::CalcTextSize(ICON_VS_CHEVRON_DOWN, nullptr, true).x +
                                             ImGui::CalcTextSize(ICON_VS_CLEAR_ALL, nullptr, true).x +
                                             ImGui::GetStyle().FramePadding.x * 2.0f * 5.0f + // 5 frame padding
                                             ImGui::GetStyle().ItemSpacing.x * 4.0f); // 4 spacing between 5 buttons
            // Align buttons to the right
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availColumnWidth - groupedButtonWidth);
            drawActionButtons(dest, editor);

            ImGui::EndTable();
        }
    }

    void PopupPasteFromSource::drawRegionSelectionBox(ViewHexEditor *editor) {
        auto &[source, dest] = m_providers;

        // Table for provider region selection
        if (ImGui::BeginTable("##table_region_selection", 2, (ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchSame), ImVec2(m_getTableWidth(), 0.0f))) {
            // Add two columns
            ImGui::TableSetupColumn("hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.provider.source.title"_lang);
            ImGui::TableSetupColumn("hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.provider.dest.title"_lang);
            // ImGui::TableHeadersRow();

            // First row
            ImGui::TableNextRow();

            // First column for selecting source provider region
            ImGui::TableSetColumnIndex(0);
            ImGui::Spacing();
            drawRegionInput(source, editor);
            ImGui::Spacing();

            // Second column for selecting destination provider region
            ImGui::TableSetColumnIndex(1);
            ImGui::Spacing();
            drawRegionInput(dest, editor);
            ImGui::Spacing();

            ImGui::EndTable();
        }
    }

    void PopupPasteFromSource::drawPasteModeBox(void) {
        auto &[source, dest] = m_providers;
        const auto isDstSelectionMultiByte = (dest.providerSelection.size > 1);
        const auto isBothProviderValid = (dest.providerValidity && source.providerValidity);
        const auto isSrcProviderEmpty = (!source.providerValidity || (source.providerSelection.provider->getActualSize() == 0));
        const auto isDstProviderEmpty = (!dest.providerValidity || (dest.providerSelection.provider->getActualSize() == 0));

        if (!isBothProviderValid) {
            m_pasteMode = PasteModeType::ModeNotSelected;
        }

        // Table for selecting the paste mode
        if (ImGui::BeginTable("##table_paste_mode_selection", 1, (ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchSame), ImVec2(m_getTableWidth(), 0.0f))) {
            // Add two columns
            ImGui::TableSetupColumn("hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.mode_button.title"_lang);
            // ImGui::TableHeadersRow();

            // First row
            ImGui::TableNextRow();

            // First column
            ImGui::TableSetColumnIndex(0);
            // Calculate button width
            const auto modeButtonWidth = ((ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2.0f);

            ImGui::Spacing();

            // Paste Over Selection
            auto modeButtonDisable = (!isBothProviderValid || (m_modeRecommend && !isDstSelectionMultiByte) || isSrcProviderEmpty || isDstProviderEmpty);
            drawPasteModeButton("hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.mode_button.paste_sel"_lang.get(), 
                                PasteModeType::ModePasteOverSelection, PasteHintType::HintPasteOverSelection, 
                                modeButtonDisable, modeButtonWidth);

            ImGui::SameLine();

            // Replace Selection
            modeButtonDisable = (!isBothProviderValid || (m_modeRecommend && !isDstSelectionMultiByte) || isSrcProviderEmpty || isDstProviderEmpty);
            drawPasteModeButton("hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.mode_button.replace_sel"_lang.get(), 
                                PasteModeType::ModeReplaceSelection, PasteHintType::HintReplaceSelection, 
                                modeButtonDisable, modeButtonWidth);

            // Paste Everything
            modeButtonDisable = (!isBothProviderValid || (m_modeRecommend && isDstSelectionMultiByte) || isSrcProviderEmpty);
            drawPasteModeButton("hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.mode_button.paste_all"_lang.get(), 
                                PasteModeType::ModePasteEverything, PasteHintType::HintPasteEverything, 
                                modeButtonDisable, modeButtonWidth);

            ImGui::SameLine();

            // Insert Everything
            modeButtonDisable = (!isBothProviderValid || (m_modeRecommend && isDstSelectionMultiByte) || isSrcProviderEmpty || isDstProviderEmpty);
            drawPasteModeButton("hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.mode_button.insert_all"_lang.get(), 
                                PasteModeType::ModeInsertEverything, PasteHintType::HintInsertEverything, 
                                modeButtonDisable, modeButtonWidth);

            ImGui::Spacing();

            ImGui::EndTable();
        }
    }

    void PopupPasteFromSource::drawPasteInfoBox(void) {
        // Table for displaying paste info
        if (ImGui::BeginTable("##table_paste_information", 1, (ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchSame), ImVec2(m_getTableWidth(), 0.0f))) {
            // Add single column
            ImGui::TableSetupColumn("hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.info.title"_lang);
            // ImGui::TableHeadersRow();

            // First row
            const float minRowHeight = (ImGui::GetTextLineHeightWithSpacing() * 3.0f); // 3 lines of text
            ImGui::TableNextRow(ImGuiTableRowFlags_None, minRowHeight);

            // First column
            ImGui::TableSetColumnIndex(0);
            ImGui::Spacing();
            // Display paste info
            ImGui::TextColored(ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue), ICON_VS_INFO);
            ImGui::SameLine();
            switch(m_pasteHint) {
                case PasteHintType::HintPasteOverSelection: {
                    ImGui::TextWrapped("%s", "hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.info.paste_sel"_lang.get());
                    break;
                }

                case PasteHintType::HintPasteEverything: {
                    ImGui::TextWrapped("%s", "hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.info.paste_all"_lang.get());
                    break;
                }

                case PasteHintType::HintReplaceSelection: {
                    ImGui::TextWrapped("%s", "hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.info.replace_sel"_lang.get());
                    break;
                }

                case PasteHintType::HintInsertEverything: {
                    ImGui::TextWrapped("%s", "hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.info.insert_all"_lang.get());
                    break;
                }

                case PasteHintType::HintReadOnlyDestination: {
                    ImGui::TextWrapped("%s", "hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.info.read_only"_lang.get());
                    break;
                }

                case PasteHintType::HintSelectMode: {
                    ImGui::TextWrapped("%s", "hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.info.select_mode"_lang.get());
                    break;
                }

                case PasteHintType::HintPasteSuccess: {
                    ImGuiExt::TextFormattedWrappedSelectable("hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.info.paste_success"_lang, elapsedTimeFormatted());
                    break;
                }

                case PasteHintType::HintPasteFailure: {
                    ImGui::TextWrapped("%s", "hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.info.paste_fail"_lang.get());
                    break;
                }

                default: {
                    ImGui::TextWrapped("%s", "hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.description"_lang.get());
                    break;
                }
            }
            ImGui::Spacing();

            ImGui::EndTable();
        }
    }

    void PopupPasteFromSource::drawPasteDecisionBox(ViewHexEditor *editor) {
        // Table for displaying confirmation selection
        if (ImGui::BeginTable("##table_paste_decision", 2, (ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchSame), ImVec2(m_getTableWidth(), 0.0f))) {
            // Add two columns
            ImGui::TableSetupColumn("##settings");
            ImGui::TableSetupColumn("##decision");

            // First row
            ImGui::TableNextRow();

            // First column
            ImGui::TableSetColumnIndex(0);
            ImGui::Spacing();
            if (ImGuiExt::DimmedIconToggle(ICON_VS_SPARKLE_FILLED, &m_modeRecommend)) {
                // Clear paste mode and hint
                m_pasteMode = PasteModeType::ModeNotSelected;
                m_pasteHint = PasteHintType::HintDefaultDescription;
            }
            ImGuiExt::InfoTooltip("hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.toggle.tip.mode_recommend"_lang);

            ImGui::SameLine();
            ImGuiExt::DimmedIconToggle(ICON_VS_SYMBOL_NUMERIC, &m_inputBaseHex);
            ImGuiExt::InfoTooltip("hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.toggle.tip.hex_input"_lang);

            // Second column
            ImGui::TableSetColumnIndex(1);
            const auto decisionButtonWidth = ((ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2.0f);

            ImGui::Spacing();
            // Apply button
            auto &[source, dest] = m_providers;
            auto &sourceSelection = source.providerSelection;
            auto &destSelection = dest.providerSelection;
            const auto isSrcRegionOffsetsValid = (source.providerValidity && 
                                                    (sourceSelection.getStartAddress() <= sourceSelection.getEndAddress()) && 
                                                    (sourceSelection.getEndAddress() < sourceSelection.provider->getActualSize()));
            const auto isDstRegionOffsetsValid = (dest.providerValidity && 
                                                    (((destSelection.getStartAddress() <= destSelection.getEndAddress()) && 
                                                      (destSelection.getEndAddress() < destSelection.provider->getActualSize())) || 
                                                     (destSelection.provider->getActualSize() == 0))); // to allow pasting in empty provider
            const auto isApplyButtonEnabled = (isSrcRegionOffsetsValid && isDstRegionOffsetsValid);

            ImGui::BeginDisabled(!isApplyButtonEnabled);
            {
                if ((ImGuiExt::DimmedButton("hex.ui.common.apply"_lang, ImVec2(decisionButtonWidth, 0))) || 
                            (isApplyButtonEnabled && ImGui::IsWindowFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)))) {
                    // Check if destination is writable
                    if (isDstRegionOffsetsValid && dest.providerSelection.provider->isWritable()) {
                        const auto isPasteModeValid = ((m_pasteMode > PasteModeType::ModeNotSelected) && (m_pasteMode < PasteModeType::ModeCount));
                        // Execute paste operation
                        if (isPasteModeValid) {
                            // Open paste confirmation popup
                            ImGui::OpenPopup("paste_confirmation_popup");
                        } else {
                            m_pasteHint = PasteHintType::HintSelectMode;
                        }
                    } else {
                        m_pasteHint = PasteHintType::HintReadOnlyDestination;
                    }
                }

                // Draw paste confirmation popup
                drawPasteConfirmationPopup(editor);
            }
            ImGui::EndDisabled();

            ImGui::SameLine();

            // Cancel button
            if (ImGuiExt::DimmedButton("hex.ui.common.cancel"_lang, ImVec2(decisionButtonWidth, 0)) ||
                (ImGui::IsWindowFocused() && (ImGui::IsKeyPressed(ImGuiKey_Escape)))) {
                editor->closePopup();
            }
            ImGui::Spacing();

            ImGui::EndTable();
        }
    }

    void PopupPasteFromSource::drawPasteConfirmationPopup(ViewHexEditor *editor) {
        // Open popup at the center of current window
        ImGui::SetNextWindowPos((ImGui::GetWindowPos() + (ImGui::GetWindowSize() / 2.0f)), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        // Disable the popup transparency
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
        if (ImGui::BeginPopupModal("paste_confirmation_popup", nullptr, (ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize))) {
            // Table for displaying paste warning and confirmation
            if (ImGui::BeginTable("##table_paste_confirmation", 1, (ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingFixedFit))) {
                // Calculate column width
                const auto warningText = "hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.warning"_lang.get();
                const auto columnWidth = ((ImGui::CalcTextSize(ICON_VS_WARNING, nullptr, true).x + (ImGui::GetStyle().FramePadding.x * 2.0f)) +
                                          (ImGui::GetStyle().ItemSpacing.x) + (ImGui::GetStyle().CellPadding.x * 2.0f) +
                                          (ImGui::CalcTextSize(warningText, strchr(warningText, '\n')).x));
 
                // Add single column
                ImGui::TableSetupColumn("hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.warn.title"_lang, ImGuiTableColumnFlags_WidthFixed, columnWidth);
                // ImGui::TableHeadersRow();

                // First row
                ImGui::TableNextRow();

                // First column
                ImGui::TableSetColumnIndex(0);
                ImGui::Spacing();
                // Show warning symbol
                ImGui::TextColored(ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed), ICON_VS_WARNING);
                ImGui::SameLine();
                // Show warning text
                ImGui::TextWrapped(warningText);

                ImGui::Separator();

                const auto buttonWidth = ((ImGui::GetContentRegionAvail().x - (ImGui::GetStyle().ItemSpacing.x * 3.0f)) / 4.0f);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ((buttonWidth + ImGui::GetStyle().ItemSpacing.x) * 2.0f));

                // Yes
                if (ImGuiExt::DimmedButton("hex.ui.common.yes"_lang, ImVec2(buttonWidth, 0)) ||
                        (ImGui::IsWindowFocused() && (ImGui::IsKeyPressed(ImGuiKey_Y)))) {
                    const auto startTime = std::chrono::steady_clock::now();
                    const auto isPasteSuccess = executePasteOperation();
                    const auto endTime = std::chrono::steady_clock::now();
                    if (isPasteSuccess) {
                        auto &[source, dest] = m_providers;
                        // Calculate the time elapsed for paste operation
                        m_elapsedTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);
                        // Turn off selection toggles after paste success
                        source.providerSelectionToggle = false;
                        dest.providerSelectionToggle = false;
                        // Restore saved pin state
                        restorePinStatus();
                        // Jump to the end of the pasted area of destination provider in hex editor
                        dest.providerSelection.size = ((m_pasteMode != PasteModeType::ModePasteOverSelection) ? (source.providerSelection.getSize()) 
                                                                                                              : (std::min(source.providerSelection.getSize(), 
                                                                                                                          dest.providerSelection.getSize())));
                        ImHexApi::Provider::setCurrentProvider(dest.providerSelection.provider);
                        editor->setSelection(dest.providerSelection.getEndAddress(), dest.providerSelection.getStartAddress());
                        editor->jumpToSelection();
                        // Clear paste mode
                        m_pasteMode = PasteModeType::ModeNotSelected;
                        m_pasteHint = PasteHintType::HintPasteSuccess;
                    } else {
                        m_pasteHint = PasteHintType::HintPasteFailure;
                    }
                    // close popup
                    ImGui::CloseCurrentPopup();
                }

                ImGui::SameLine();

                // No
                if (ImGuiExt::DimmedButton("hex.ui.common.no"_lang, ImVec2(buttonWidth, 0)) || 
                        (ImGui::IsWindowFocused() && (ImGui::IsKeyPressed(ImGuiKey_N))) || 
                        (!ImGui::IsMouseHoveringRect(ImGui::GetWindowPos(), (ImGui::GetWindowPos() + ImGui::GetWindowSize())) && 
                         ImGui::IsMouseClicked(ImGuiMouseButton_Left))) {
                    // close popup
                    ImGui::CloseCurrentPopup();
                }
                ImGui::Spacing();

                ImGui::EndTable();
            }

            // Highlight popup border
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRect(ImGui::GetWindowPos(), ImGui::GetWindowPos() + ImGui::GetWindowSize(),
                              ImGui::GetColorU32(ImGuiCol_NavWindowingHighlight), ImGui::GetStyle().WindowRounding,
                              ImDrawFlags_None, 2_scaled);

            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
    }

    void PopupPasteFromSource::drawProviderSelector(ProviderInfo &object) {
        auto &providerObjectId = object.providerObjectId;
        auto &providerIndex = object.providerIndex;
        auto &providerValidity = object.providerValidity;
        auto &providerSelection = object.providerSelection;
        auto &providerSelectionToggle = object.providerSelectionToggle;

        // Get list of all providers
        auto providerList = ImHexApi::Provider::getProviders();
        // Erase invalid providers
        std::erase_if(providerList, [&](const prv::Provider *provider) { return (!provider->isAvailable() || !provider->isReadable()); });

        // Find the current index of the selected provider
        if (providerSelection.provider != nullptr) {
            auto itr = std::find(providerList.begin(), providerList.end(), providerSelection.provider);

            if (itr != providerList.end()) {
                // Update the provider index
                providerIndex = static_cast<i64>(std::distance(providerList.begin(), itr));

                // Clear selection region for empty provider
                if (providerSelection.provider->getActualSize() == 0) {
                    providerSelection.address = providerSelection.provider->getBaseAddress();
                    providerSelection.size = 0;
                }
            } else {
                // Clear the provider info if the provider was closed or not found
                providerIndex = -1;
                providerSelection.address = 0;
                providerSelection.size = 0;
                providerSelection.provider = nullptr;
                if(providerSelectionToggle) {
                    providerSelectionToggle = false;
                    // Restore saved pin state
                    restorePinStatus();
                }
            }
        } else {
            providerIndex = -1;
        }

        // Validity of the provider
        providerValidity = (providerIndex >= 0);

        // Get the name of the currently selected provider
        std::string providerPreview = "hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.provider.preview"_lang.get();
        if (providerValidity) {
            providerPreview = providerList[providerIndex]->getName();
        }

        // Draw combobox with all available providers
        ImGui::PushID(&providerObjectId);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("", providerPreview.c_str())) {
            for (size_t i = 0; i < providerList.size(); i++) {
                ImGui::PushID(i+1);
                if (ImGui::Selectable(providerList[i]->getName().c_str())) {
                    const bool isProviderChanged = (providerIndex != static_cast<int>(i));
                    // Set selected provider
                    providerSelection.provider = providerList[i];
                    providerIndex = static_cast<i64>(i);
                    providerValidity = (providerSelection.provider != nullptr);
                    // Clear region, paste mode and hint if a new provider is selected
                    if(isProviderChanged) {
                        if (providerValidity) {
                            providerSelection.address = providerSelection.provider->getBaseAddress();
                            providerSelection.size = 1;
                        } else {
                            providerSelection.address = 0;
                            providerSelection.size = 0;
                        }
                        if(providerSelectionToggle) {
                            providerSelectionToggle = false;
                            // Restore saved pin state
                            restorePinStatus();
                        }
                        m_pasteMode = PasteModeType::ModeNotSelected;
                        m_pasteHint = PasteHintType::HintDefaultDescription;
                    }

                    // Disable selecting same provider to avoid paste operation on same provider
                    auto &[source, dest] = m_providers;
                    // If source, reset destination object
                    if ((providerObjectId == ObjectIdType::SourceId) && (providerIndex == dest.providerIndex)) {
                        dest.providerIndex = -1;
                        dest.providerValidity = false;
                        dest.providerSelection.address = 0;
                        dest.providerSelection.size = 0;
                        dest.providerSelection.provider = nullptr;
                        if(dest.providerSelectionToggle) {
                            dest.providerSelectionToggle = false;
                            // Restore saved pin state
                            restorePinStatus();
                        }
                    }
                    // If destination, reset source object
                    if ((providerObjectId == ObjectIdType::DestId) && (providerIndex == source.providerIndex)) {
                        source.providerIndex = -1;
                        source.providerValidity = false;
                        source.providerSelection.address = 0;
                        source.providerSelection.size = 0;
                        source.providerSelection.provider = nullptr;
                        if(source.providerSelectionToggle) {
                            source.providerSelectionToggle = false;
                            // Restore saved pin state
                            restorePinStatus();
                        }
                    }
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();
    }

    void PopupPasteFromSource::drawActionButtons(ProviderInfo &object, ViewHexEditor *editor) {
        auto &providerIndex = object.providerIndex;
        auto &providerValidity = object.providerValidity;
        auto &providerSelection = object.providerSelection;
        auto &providerSelectionToggle = object.providerSelectionToggle;
        auto &providerObjectId = object.providerObjectId;
        auto &[source, dest] = m_providers;
        const auto isProviderEmpty = (!providerValidity || (providerSelection.provider->getActualSize() == 0));
        const auto wasSelectionActive = (source.providerSelectionToggle || dest.providerSelectionToggle);

        ImGui::BeginDisabled(!providerValidity);
        {
            // Select/Show input region in hex editor
            ImGui::PushID(&providerSelectionToggle);
            if (ImGuiExt::DimmedIconToggle(ICON_VS_DEBUG_RESTART_FRAME, &providerSelectionToggle)) {
                if (providerSelectionToggle) {
                    // Save original pin state only when entering selection mode
                    if (!wasSelectionActive) {
                        m_savedPinStatus = this->isPinned();
                        // Pin the popup while selecting a region
                        if (!m_savedPinStatus) {
                            this->setPinned(true);
                        }
                    }

                    // If source, turn off destination selection toggle
                    if (providerObjectId == ObjectIdType::SourceId) {
                        dest.providerSelectionToggle = false;
                    }
                    // If destination, turn off source selection toggle
                    if (providerObjectId == ObjectIdType::DestId) {
                        source.providerSelectionToggle = false;
                    }
                    // Jump to the selection in hex editor
                    ImHexApi::Provider::setCurrentProvider(providerSelection.provider);
                    if (!isProviderEmpty) {
                        editor->setSelection(providerSelection.getStartAddress(), providerSelection.getEndAddress());
                        editor->jumpToSelection();
                    }
                } else {
                    // Restore saved pin state when leaving selection mode
                    if (wasSelectionActive) {
                        restorePinStatus();
                    }
                }
            }
            ImGuiExt::InfoTooltip("hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.toggle.tip.select"_lang);
            ImGui::PopID();

            // Update the selection region for the current provider from hex editor selection if select/show region toggle is enabled
            if (providerValidity && (ImHexApi::Provider::getCurrentProviderIndex() == providerIndex) && 
                    providerSelectionToggle && !isProviderEmpty) {
                providerSelection.address = editor->getSelection().getStartAddress();
                providerSelection.size = editor->getSelection().getSize();
            }

            // Select everything
            ImGui::SameLine();
            ImGui::PushID(&providerSelection.provider+1);
            if (ImGuiExt::DimmedIconButton(ICON_VS_CHECKLIST, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                if (providerValidity) {
                    providerSelection.address = providerSelection.provider->getBaseAddress();
                    providerSelection.size = providerSelection.provider->getActualSize();
                } else {
                    providerSelection.address = 0;
                    providerSelection.size = 0;
                }
                // Turn off select/show region toggle
                source.providerSelectionToggle = false;
                dest.providerSelectionToggle = false;
                // Restore saved pin state
                restorePinStatus();
                // Jump to the selection in hex editor
                ImHexApi::Provider::setCurrentProvider(providerSelection.provider);
                setSelection(providerSelection.getStartAddress(), providerSelection.getEndAddress());
            }
            ImGuiExt::InfoTooltip("hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.button.tip.selection.all"_lang);
            ImGui::PopID();

            // Jump to begin address of selected region
            ImGui::SameLine();
            ImGui::PushID(&providerSelection.provider+2);
            if (ImGuiExt::DimmedIconButton(ICON_VS_CHEVRON_UP, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                // Turn off select/show region toggle
                source.providerSelectionToggle = false;
                dest.providerSelectionToggle = false;
                // Restore saved pin state
                restorePinStatus();
                // Jump to the selection in hex editor
                ImHexApi::Provider::setCurrentProvider(providerSelection.provider);
                setSelection(providerSelection.getStartAddress(), providerSelection.getEndAddress());
            }
            ImGuiExt::InfoTooltip("hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.button.tip.selection.begin"_lang);
            ImGui::PopID();

            // Jump to end address of selected region
            ImGui::SameLine();
            ImGui::PushID(&providerSelection.provider+3);
            if (ImGuiExt::DimmedIconButton(ICON_VS_CHEVRON_DOWN, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                // Turn off select/show region toggle
                source.providerSelectionToggle = false;
                dest.providerSelectionToggle = false;
                // Restore saved pin state
                restorePinStatus();
                // Jump to the selection in hex editor
                ImHexApi::Provider::setCurrentProvider(providerSelection.provider);
                setSelection(providerSelection.getEndAddress(), providerSelection.getStartAddress());
            }
            ImGuiExt::InfoTooltip("hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.button.tip.selection.end"_lang);
            ImGui::PopID();

            // Clear selected region
            ImGui::SameLine();
            ImGui::PushID(&providerSelection.provider+4);
            if (ImGuiExt::DimmedIconButton(ICON_VS_CLEAR_ALL, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                if (providerValidity) {
                    // providerSelection.address = providerSelection.provider->getBaseAddress();
                    providerSelection.size = 1;
                } else {
                    providerSelection.address = 0;
                    providerSelection.size = 0;
                }
                // Turn off select/show region toggle
                source.providerSelectionToggle = false;
                dest.providerSelectionToggle = false;
                // Restore saved pin state
                restorePinStatus();
                m_pasteMode = PasteModeType::ModeNotSelected;
                // Jump to the selection in hex editor
                ImHexApi::Provider::setCurrentProvider(providerSelection.provider);
                setSelection(providerSelection.getStartAddress(), providerSelection.getEndAddress());
            }
            ImGuiExt::InfoTooltip("hex.builtin.view.hex_editor.menu.edit.paste_from_source.popup.button.tip.selection.clear"_lang);
            ImGui::PopID();
        }
        ImGui::EndDisabled();

        // Handle setting and jumping to the selection in the next cycle for the action buttons except Select/Show toggle button.
        // This is because ViewHexEditor loads the new provider at the start of next cycle.
        static bool isNextDrawCycle = false;
        if (m_setSelectionTrigger) {
            // Function drawActionButtons() is called twice per update cycle (source and destination).
            // Defer until the next draw cycle to ensure ViewHexEditor has loaded the new provider.
            if (isNextDrawCycle) {
                if (providerValidity && (ImHexApi::Provider::getCurrentProviderIndex() == providerIndex)) {
                    m_setSelectionTrigger = false;
                    isNextDrawCycle = false;
                    // Jump to the selection in hex editor
                    if (!isProviderEmpty) {
                        editor->setSelection(m_setSelectionStart, m_setSelectionEnd);
                        editor->jumpToSelection();
                    }
                }
            } else {
                isNextDrawCycle = true;
            }
        }
    }

    void PopupPasteFromSource::drawRegionInput(ProviderInfo &object, ViewHexEditor *editor) {
        auto &providerIndex = object.providerIndex;
        auto &providerValidity = object.providerValidity;
        auto &providerSelection = object.providerSelection;
        auto &providerSelectionToggle = object.providerSelectionToggle;

        const auto providerStartAddress = (providerValidity ? providerSelection.provider->getBaseAddress() : 0);
        const auto providerActualSize   = (providerValidity ? providerSelection.provider->getActualSize() : 0);
        const auto providerEndAddress = ((providerActualSize == 0) ? providerStartAddress : (providerStartAddress + (providerActualSize - 1)));

        ImGui::BeginDisabled(!providerValidity);
        {
            const auto inputWidth = (ImGui::GetContentRegionAvail().x * 0.75f); // 75% of column width

            ImGui::PushID(&providerSelection.address);
            if (ImGui::BeginTabBar("select_tabs")) {

                // Region Tab Bar
                if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.select.offset.region"_lang)) {
                    u64 inputA = providerSelection.getStartAddress();
                    u64 inputB = providerSelection.getEndAddress();

                    // Begin
                    drawIntegerInputField("hex.builtin.view.hex_editor.select.offset.begin"_lang.get(), &inputA, inputWidth);
                    // End
                    drawIntegerInputField("hex.builtin.view.hex_editor.select.offset.end"_lang.get(), &inputB, inputWidth);

                    // Validate region selection start address and end address
                    if (providerActualSize == 0) {
                        providerSelection.address = providerStartAddress;
                        providerSelection.size = 0;
                    } else {
                        inputA = std::clamp(inputA, providerStartAddress, providerEndAddress);
                        inputB = std::clamp(inputB, inputA, providerEndAddress);

                        providerSelection.address = inputA;
                        providerSelection.size = (inputB - inputA) + 1;
                    }

                    ImGui::EndTabItem();
                }

                // Size Tab Bar
                if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.select.offset.size"_lang)) {
                    u64 inputA = providerSelection.getStartAddress();
                    u64 inputB = providerSelection.getSize();

                    // Begin
                    drawIntegerInputField("hex.builtin.view.hex_editor.select.offset.begin"_lang.get(), &inputA, inputWidth);
                    // Size
                    drawIntegerInputField("hex.builtin.view.hex_editor.select.offset.size"_lang.get(), &inputB, inputWidth);

                    // Validate region selection start address and size
                    if (providerActualSize == 0) {
                        providerSelection.address = providerStartAddress;
                        providerSelection.size = 0;
                    } else {
                        inputA = std::clamp(inputA, providerStartAddress, providerEndAddress);

                        const auto providerRemainingSize = providerActualSize - (inputA - providerStartAddress);
                        inputB = std::clamp<u64>(inputB, 1, providerRemainingSize);

                        providerSelection.address = inputA;
                        providerSelection.size = inputB;
                    }

                    ImGui::EndTabItem();
                }

                // Update the region selection in hex editor for the current provider if select/show region toggle is enabled
                if (providerValidity && (ImHexApi::Provider::getCurrentProviderIndex() == providerIndex) && providerSelectionToggle && (providerActualSize > 0) &&
                        (!(ImGui::IsMouseDragging(ImGuiMouseButton_Left)) && !(ImGui::IsKeyDown(ImGuiKey_LeftShift)) && !(ImGui::IsKeyDown(ImGuiKey_RightShift)))) {
                    editor->setSelection(providerSelection.getStartAddress(), providerSelection.getEndAddress());
                }

                ImGui::EndTabBar();
            }
            ImGui::PopID();
        }
        ImGui::EndDisabled();
    }

    void PopupPasteFromSource::drawPasteModeButton(const char *buttonLabel, PasteModeType buttonMode, PasteHintType buttonHint, bool buttonDisable, float buttonWidth) {
        auto buttonPushed = false;

        ImGui::BeginDisabled(buttonDisable);
        {
            if ((m_pasteMode == buttonMode) && !buttonDisable) {
                ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                buttonPushed = true;
            }
            if (ImGuiExt::DimmedButton(buttonLabel, ImVec2(buttonWidth, 0))) {
                m_pasteMode = buttonMode;
                m_pasteHint = buttonHint;
            }
            if (buttonPushed) {
                ImGui::PopStyleColor();
            }
        }
        ImGui::EndDisabled();
    }

    void PopupPasteFromSource::drawIntegerInputField(const char *inputLabel, void *inputValue, float inputWidth) {
        ImGui::SetNextItemWidth(inputWidth);
        ImGuiExt::InputIntegerPrefix(inputLabel,
                                     (m_inputBaseHex ? "0x" : ""),
                                     inputValue,
                                     ImGuiDataType_U64,
                                     (m_inputBaseHex ? "%llX" : "%lld"),
                                     ((m_inputBaseHex ? ImGuiInputTextFlags_CharsHexadecimal : ImGuiInputTextFlags_CharsDecimal) | ImGuiInputTextFlags_AutoSelectAll));
    }

    void PopupPasteFromSource::setSelection(u64 start, u64 end) {
        m_setSelectionStart = start;
        m_setSelectionEnd = end;
        m_setSelectionTrigger = true;
    }

    bool PopupPasteFromSource::executePasteOperation(void) const {
        auto &[source, dest] = m_providers;
        u64 pasteSize = 0;

        if ((source.providerSelection.provider == nullptr) || (dest.providerSelection.provider == nullptr)) {
            return false; // Invalid source or destination
        }

        // Process paste mode
        switch (m_pasteMode) {
            case PasteModeType::ModePasteOverSelection: {
                // Size of the data to be pasted is the smallest of source and destination region size
                pasteSize = std::min(source.providerSelection.getSize(), dest.providerSelection.getSize());
                break;
            }

            case PasteModeType::ModePasteEverything: {
                // Calculate available size from selected destination begin to end
                u64 availSize = (dest.providerSelection.provider->getActualSize() - dest.providerSelection.getStartAddress());

                // Size of the data to be pasted is same as selected source region size
                pasteSize = source.providerSelection.getSize();
                // Resize the provider size to accommodate selected source region data
                if (availSize < pasteSize) {
                    dest.providerSelection.provider->insertRaw(dest.providerSelection.getStartAddress(), (pasteSize - availSize));
                }
                break;
            }

            case PasteModeType::ModeReplaceSelection: {
                i64 sizeDiff = (dest.providerSelection.getSize() - source.providerSelection.getSize());

                // Size of the data to be pasted is same as selected source region size
                pasteSize = source.providerSelection.getSize();
                // Resize selected destination region to match the selected source region size
                if (sizeDiff < 0) {
                    dest.providerSelection.provider->insertRaw(dest.providerSelection.getStartAddress(), -sizeDiff);
                } else if (sizeDiff > 0) {
                    dest.providerSelection.provider->removeRaw(dest.providerSelection.getStartAddress(), sizeDiff);
                } else {
                    // Same size
                }
                break;
            }

            case PasteModeType::ModeInsertEverything: {
                // Size of the data to be pasted is same as selected source region size
                pasteSize = source.providerSelection.getSize();
                // Insert selected source region size of data bytes at destination region start address
                dest.providerSelection.provider->insertRaw(dest.providerSelection.getStartAddress(), source.providerSelection.getSize());
                break;
            }

            default: {
                return false; // Invalid paste mode
            }
        }

        // Paste the source region data over the respective (selected/resized/inserted) region of destination
        using namespace hex::literals;
        constexpr static size_t ChunkSize = 4_KiB;
        std::array<u8, ChunkSize> buffer{};

        auto sourceOffset = source.providerSelection.getStartAddress();
        auto destOffset = dest.providerSelection.getStartAddress();
        auto bytesLeft = pasteSize;

        while (bytesLeft > 0) {
            const size_t chunk = std::min<u64>(bytesLeft, ChunkSize);

            source.providerSelection.provider->readRaw(sourceOffset, buffer.data(), chunk);
            dest.providerSelection.provider->writeRaw(destOffset, buffer.data(), chunk);

            sourceOffset += chunk;
            destOffset += chunk;
            bytesLeft -= chunk;
        }

        dest.providerSelection.provider->markDataDirty();

        return true; // Paste successfull
    }

    std::string PopupPasteFromSource::elapsedTimeFormatted(void) const {
        using namespace std::literals::chrono_literals;

        if (m_elapsedTime >= 1s) {
            return fmt::format("{:.3f} s", std::chrono::duration<double>(m_elapsedTime).count());
        }
        if (m_elapsedTime >= 1ms) {
            return fmt::format("{:.3f} ms", std::chrono::duration<double, std::milli>(m_elapsedTime).count());
        }
        if (m_elapsedTime >= 1us) {
            return fmt::format("{:.3f} µs", std::chrono::duration<double, std::micro>(m_elapsedTime).count());
        }

        return fmt::format("{:.3f} ns", std::chrono::duration<double, std::nano>(m_elapsedTime).count());
    }

    void PopupPasteFromSource::restorePinStatus(void) {
        // Restore saved pin state
        this->setPinned(m_savedPinStatus);
    }
}