#include "content/popups/hex_editor/popup_hex_editor_paste_from_file_behaviour.hpp"
#include "content/views/view_hex_editor.hpp"

#include <hex/ui/imgui_imhex_extensions.h>
#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/imhex_api/hex_editor.hpp>
#include <hex/helpers/fmt.hpp>
#include <fonts/vscode_icons.hpp>

namespace hex::plugin::builtin {

    PopupPasteFromFileBehaviour::PopupPasteFromFileBehaviour(const ImHexApi::HexEditor::ProviderRegion &selection, 
                                                             const std::function<bool(const ImHexApi::HexEditor::ProviderRegion &src, 
                                                                                      const ImHexApi::HexEditor::ProviderRegion &dst, 
                                                                                      const u8 mode)> &pasteCallback) 
                                                            : m_pasteCallback(pasteCallback) {
        // Pin the popup window
        this->setPinned(true);

        auto &[src, dst] = m_fileProvider;

        // Initialize source file region with default values
        src.providerObjectId = ObjIdType::SrcId;
        src.providerIndex = -1;
        src.providerValidity = false;
        src.providerSelection.address = 0;
        src.providerSelection.size = 0;
        src.providerSelection.provider = nullptr;
        src.providerSelectionToggle = false;

        // Set selected region for destination file
        dst.providerObjectId = ObjIdType::DstId;
        if (selection.getProvider() != nullptr)
        {
            dst.providerSelection.address = selection.getStartAddress();
            dst.providerSelection.size = selection.getSize();
            dst.providerSelection.provider = selection.getProvider();
            dst.providerSelectionToggle = false;
        } else {
            dst.providerIndex = -1;
            dst.providerValidity = false;
            dst.providerSelection.address = 0;
            dst.providerSelection.size = 0;
            dst.providerSelection.provider = nullptr;
            dst.providerSelectionToggle = false;
        }
    }

    UnlocalizedString PopupPasteFromFileBehaviour::getTitle() const {
        return "hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.title";
    }

    bool PopupPasteFromFileBehaviour::canBePinned() const {
        return true;
    }

    void PopupPasteFromFileBehaviour::draw(ViewHexEditor *editor) {
        drawFileSelectionBox();
        // ImGui::Spacing();
        drawActionButtonsBox(editor);
        // ImGui::Spacing();
        drawRegionSelectionBox(editor);
        ImGui::Spacing();
        drawPasteModeBox();
        ImGui::Spacing();
        drawPasteInfoBox();
        ImGui::Spacing();
        drawPasteWarningBox();
        ImGui::Spacing();
        drawPasteDecisionBox(editor);
    }

    void PopupPasteFromFileBehaviour::drawFileSelectionBox(void) {
        auto &[src, dst] = m_fileProvider;

        // Table for selecting source and destination file
        if (ImGui::BeginTable("##table_1", 2, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchSame, ImVec2(m_tableWidth, 0.0f))) {
            // Add two columns
            ImGui::TableSetupColumn("hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.provider.source.title"_lang);
            ImGui::TableSetupColumn("hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.provider.dest.title"_lang);
            ImGui::TableHeadersRow();

            // First row
            ImGui::TableNextRow();

            // First column for selecting source file
            ImGui::TableSetColumnIndex(0);
            ImGui::Spacing();
            drawFileProvider(src);
            ImGui::Spacing();

            // Second column for selecting destination file
            ImGui::TableSetColumnIndex(1);
            ImGui::Spacing();
            drawFileProvider(dst);
            ImGui::Spacing();

            ImGui::EndTable();
        }
    }

    void PopupPasteFromFileBehaviour::drawActionButtonsBox(ViewHexEditor *editor) {
        auto &[src, dst] = m_fileProvider;

        // Table for selecting an action on selected file/region
        if (ImGui::BeginTable("##table_2", 3, ImGuiTableFlags_BordersOuterV | ImGuiTableFlags_SizingStretchSame, ImVec2(m_tableWidth, 0.0f))) {
            ImGui::TableSetupColumn("hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.provider.source.title"_lang);
            ImGui::TableSetupColumn("hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.button.tip.swap.title"_lang);
            ImGui::TableSetupColumn("hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.provider.dest.title"_lang);
            // ImGui::TableHeadersRow();

            // First row
            ImGui::TableNextRow();

            // Second column: Swap action button (rendering the swap column first)
            ImGui::TableSetColumnIndex(1);
            // Read the column width (_SizingStretchSame: all columns will have same width)
            auto availColumnWidth = (ImGui::GetContentRegionAvail().x);
            // Calculate swap button width
            auto swapButtonWidth = (ImGui::CalcTextSize(ICON_VS_ARROW_SWAP, nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0f);
            // Center the swap button
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (availColumnWidth/2) - (swapButtonWidth/2));
            ImGui::PushID("button_swap_files");
            if (ImGuiExt::DimmedIconButton(ICON_VS_ARROW_SWAP, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                // Swap all the members of src and dst except providerObjectId
                std::swap(src.providerIndex, dst.providerIndex); 
                std::swap(src.providerValidity, dst.providerValidity);
                std::swap(src.providerSelection, dst.providerSelection);
                std::swap(src.providerSelectionToggle, dst.providerSelectionToggle);
                // Clear paste mode and hint
                m_pasteMode = PasteModeType::ModeNotSelected;
                m_pasteHint = PasteHintType::HintDefaultDescription;
            }
            ImGuiExt::InfoTooltip("hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.button.tip.swap"_lang);
            ImGui::PopID();

            // First column: Source action buttons
            ImGui::TableSetColumnIndex(0);
            drawActionButtons(src, editor);

            // Third column: Destination action buttons
            ImGui::TableSetColumnIndex(2);
            // Calculate grouped button width
            auto groupedButtonWidth = (ImGui::CalcTextSize(ICON_VS_DEBUG_RESTART_FRAME, nullptr, true).x +
                                       ImGui::CalcTextSize(ICON_VS_CHECKLIST, nullptr, true).x +
                                       ImGui::CalcTextSize(ICON_VS_CHEVRON_UP, nullptr, true).x +
                                       ImGui::CalcTextSize(ICON_VS_CHEVRON_DOWN, nullptr, true).x +
                                       ImGui::CalcTextSize(ICON_VS_CLEAR_ALL, nullptr, true).x +
                                       ImGui::GetStyle().FramePadding.x * 2.0f * 5.0f + // 5 frame padding
                                       ImGui::GetStyle().ItemSpacing.x * 4.0f); // 4 spacing between 5 buttons
            // Align buttons to the right
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availColumnWidth - groupedButtonWidth);
            drawActionButtons(dst, editor);

            ImGui::EndTable();
        }
    }

    void PopupPasteFromFileBehaviour::drawRegionSelectionBox(ViewHexEditor *editor) {
        auto &[src, dst] = m_fileProvider;

        // Table for selecting source file data and pasting mode
        if (ImGui::BeginTable("##table_3", 2, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchSame, ImVec2(m_tableWidth, 0.0f))) {
            // Add two columns
            ImGui::TableSetupColumn("hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.provider.source.title"_lang);
            ImGui::TableSetupColumn("hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.provider.dest.title"_lang);
            // ImGui::TableHeadersRow();

            // First row
            ImGui::TableNextRow();

            // First column for selecting source file region
            ImGui::TableSetColumnIndex(0);
            ImGui::Spacing();
            drawRegionProvider(src, editor);
            ImGui::Spacing();

            // Second column for selecting destination file region
            ImGui::TableSetColumnIndex(1);
            ImGui::Spacing();
            drawRegionProvider(dst, editor);
            ImGui::Spacing();

            ImGui::EndTable();
        }
    }

    void PopupPasteFromFileBehaviour::drawPasteModeBox(void) {
        auto &[src, dst] = m_fileProvider;
        auto isDstSelectionMultiByte = (dst.providerSelection.size > 1);
        auto isBothProviderValid = (dst.providerValidity && src.providerValidity);
        auto isSrcProviderEmpty = (src.providerValidity && (src.providerSelection.provider->getActualSize() == 0));
        auto isDstProviderEmpty = (dst.providerValidity && (dst.providerSelection.provider->getActualSize() == 0));

        if (!isBothProviderValid) {
            m_pasteMode = PasteModeType::ModeNotSelected;
        }

        // Table for selecting the paste mode
        if (ImGui::BeginTable("##table_4", 1, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchSame, ImVec2(m_tableWidth, 0.0f))) {
            // Add two columns
            ImGui::TableSetupColumn("hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.mode_button.title"_lang);
            // ImGui::TableHeadersRow();

            // First row
            ImGui::TableNextRow();

            // First column
            ImGui::TableSetColumnIndex(0);
            // Calculate button width
            auto modeButtonWidth = ((ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2.0f);

            ImGui::Spacing();

            // Paste Over Selection
            auto modeButtonDisable = (!isBothProviderValid || (m_modeRecommend && !isDstSelectionMultiByte) || isSrcProviderEmpty || isDstProviderEmpty);
            drawPasteModeButton("hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.mode_button.paste_sel"_lang.get(), 
                                PasteModeType::ModePasteOverSelection, PasteHintType::HintPasteOverSelection, 
                                modeButtonDisable, modeButtonWidth);

            ImGui::SameLine();

            // Replace Selection
            modeButtonDisable = (!isBothProviderValid || (m_modeRecommend && !isDstSelectionMultiByte) || isSrcProviderEmpty || isDstProviderEmpty);
            drawPasteModeButton("hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.mode_button.replace_sel"_lang.get(), 
                                PasteModeType::ModeReplaceSelection, PasteHintType::HintReplaceSelection, 
                                modeButtonDisable, modeButtonWidth);

            // Paste Everything
            modeButtonDisable = (!isBothProviderValid || (m_modeRecommend && isDstSelectionMultiByte) || isSrcProviderEmpty);
            drawPasteModeButton("hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.mode_button.paste_all"_lang.get(), 
                                PasteModeType::ModePasteEverything, PasteHintType::HintPasteEverything, 
                                modeButtonDisable, modeButtonWidth);

            ImGui::SameLine();

            // Insert Everything
            modeButtonDisable = (!isBothProviderValid || (m_modeRecommend && isDstSelectionMultiByte) || isSrcProviderEmpty || isDstProviderEmpty);
            drawPasteModeButton("hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.mode_button.insert_all"_lang.get(), 
                                PasteModeType::ModeInsertEverything, PasteHintType::HintInsertEverything, 
                                modeButtonDisable, modeButtonWidth);

            ImGui::Spacing();

            ImGui::EndTable();
        }
    }

    void PopupPasteFromFileBehaviour::drawPasteInfoBox(void) {
        // Table for displaying paste info
        if (ImGui::BeginTable("##table_5", 1, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchSame, ImVec2(m_tableWidth, 0.0f))) {
            // Add single column
            ImGui::TableSetupColumn("hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.info.title"_lang);
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
                    ImGui::TextWrapped("%s", "hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.info.paste_sel"_lang.get());
                    break;
                }

                case PasteHintType::HintPasteEverything: {
                    ImGui::TextWrapped("%s", "hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.info.paste_all"_lang.get());
                    break;
                }

                case PasteHintType::HintReplaceSelection: {
                    ImGui::TextWrapped("%s", "hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.info.replace_sel"_lang.get());
                    break;
                }

                case PasteHintType::HintInsertEverything: {
                    ImGui::TextWrapped("%s", "hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.info.insert_all"_lang.get());
                    break;
                }

                case PasteHintType::HintReadOnlyDestination: {
                    ImGui::TextWrapped("%s", "hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.info.read_only"_lang.get());
                    break;
                }

                case PasteHintType::HintSelectMode: {
                    ImGui::TextWrapped("%s", "hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.info.select_mode"_lang.get());
                    break;
                }

                case PasteHintType::HintPasteSuccess: {
                    ImGuiExt::TextFormattedWrappedSelectable("hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.info.paste_success"_lang, elapsedTimeFormatted());
                    break;
                }

                case PasteHintType::HintPasteFailure: {
                    ImGui::TextWrapped("%s", "hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.info.paste_fail"_lang.get());
                    break;
                }

                default: {
                    ImGui::TextWrapped("%s", "hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.description"_lang.get());
                    break;
                }
            }
            ImGui::Spacing();

            ImGui::EndTable();
        }
    }

    void PopupPasteFromFileBehaviour::drawPasteWarningBox(void) {
        // Table for displaying paste warning
        if (ImGui::BeginTable("##table_6", 1, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchSame, ImVec2(m_tableWidth, 0.0f))) {
            // Add single column
            ImGui::TableSetupColumn("hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.warn.title"_lang);
            // ImGui::TableHeadersRow();

            // First row
            ImGui::TableNextRow(ImGuiTableRowFlags_None, ImGui::GetTextLineHeightWithSpacing()); // 1 line of text

            // First column
            ImGui::TableSetColumnIndex(0);
            ImGui::Spacing();
            // Show paste warning
            ImGui::TextColored(ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed), ICON_VS_WARNING);
            ImGui::SameLine();
            ImGui::TextWrapped("hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.warning"_lang);
            ImGui::Spacing();

            ImGui::EndTable();
        }
    }

    void PopupPasteFromFileBehaviour::drawPasteDecisionBox(ViewHexEditor *editor) {
        // Table for displaying confirmation selection
        if (ImGui::BeginTable("##table_7", 2, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingStretchSame, ImVec2(m_tableWidth, 0.0f))) {
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
            ImGuiExt::InfoTooltip("hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.toggle.tip.mode_recommend"_lang);

            ImGui::SameLine();
            ImGuiExt::DimmedIconToggle(ICON_VS_SYMBOL_NUMERIC, &m_inputBaseHex);
            ImGuiExt::InfoTooltip("hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.toggle.tip.hex_input"_lang);

            // Second column
            ImGui::TableSetColumnIndex(1);
            auto decisionButtonWidth = ((ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2.0f);

            ImGui::Spacing();
            // Apply button
            auto &[src, dst] = m_fileProvider;
            auto isSrcRegionOffsetsValid = (src.providerValidity && (src.providerSelection.getStartAddress() <= src.providerSelection.getEndAddress()) && 
                                                                    (src.providerSelection.getEndAddress() < src.providerSelection.provider->getActualSize()));
            auto isDstRegionOffsetsValid = (dst.providerValidity && (dst.providerSelection.getStartAddress() <= dst.providerSelection.getEndAddress()) && 
                                                                    (dst.providerSelection.getEndAddress() <= dst.providerSelection.provider->getActualSize())); // <= for enabling pasting in empty file
            auto isApplyButtonEnabled = (isSrcRegionOffsetsValid && isDstRegionOffsetsValid);

            ImGui::BeginDisabled(!isApplyButtonEnabled);
            {
                if ((ImGuiExt::DimmedButton("hex.ui.common.apply"_lang, ImVec2(decisionButtonWidth, 0))) || 
                            (isApplyButtonEnabled && ImGui::IsWindowFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)))) {
                    // Check if destination file is writable
                    if (isDstRegionOffsetsValid && dst.providerSelection.provider->isWritable()) {
                        auto isPasteModeValid = ((m_pasteMode > PasteModeType::ModeNotSelected) && (m_pasteMode < PasteModeType::ModeCount));
                        // Execute paste operation
                        if (isPasteModeValid) {
                            const auto startTime = std::chrono::steady_clock::now();
                            auto isPasteSuccess = m_pasteCallback(src.providerSelection, dst.providerSelection, static_cast<u8>(m_pasteMode));
                            const auto endTime = std::chrono::steady_clock::now();
                            if (isPasteSuccess) {
                                // Calculate the time elapsed for paste operation
                                m_elapsedTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime);
                                // Turn off selection toggles after paste success
                                src.providerSelectionToggle = false;
                                dst.providerSelectionToggle = false;
                                // Jump to the end of the pasted area of destination file in hex editor
                                dst.providerSelection.size = ((m_pasteMode != PasteModeType::ModePasteOverSelection) ? (src.providerSelection.getSize()) 
                                                                                                          : (std::min(src.providerSelection.getSize(), dst.providerSelection.getSize())));
                                ImHexApi::Provider::setCurrentProvider(dst.providerSelection.provider);
                                editor->setSelection(dst.providerSelection.getEndAddress(), dst.providerSelection.getStartAddress());
                                editor->jumpToSelection();
                                // Clear paste mode
                                m_pasteMode = PasteModeType::ModeNotSelected;
                                m_pasteHint = PasteHintType::HintPasteSuccess;
                            } else {
                                m_pasteHint = PasteHintType::HintPasteFailure;
                            }
                        } else {
                            m_pasteHint = PasteHintType::HintSelectMode;
                        }

                        if (!this->isPinned()) {
                            editor->closePopup();
                        }
                    } else {
                        m_pasteHint = PasteHintType::HintReadOnlyDestination;
                    }
                }
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

    void PopupPasteFromFileBehaviour::drawFileProvider(ProviderInfo &object) {
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
                providerIndex = static_cast<i64>(std::distance(providerList.begin(), itr));

                if (providerSelection.provider->getActualSize() == 0) {
                    providerSelection.address = 0;
                    providerSelection.size = 0;
                }
            } else {
                // Clear the provider info if the file was closed
                providerIndex = -1;
                providerValidity = false;
                providerSelection.address = 0;
                providerSelection.size = 0;
                providerSelection.provider = nullptr;
                providerSelectionToggle = false;
            }
        } else {
            providerIndex = -1;
        }

        // Validity of the provider
        providerValidity = (providerIndex > -1);

        // Get the name of the currently selected provider
        std::string providerPreview = "hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.provider.file.preview"_lang.get();
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
                    // Clear region, paste mode and hint if a new file is selected
                    if(isProviderChanged) {
                        providerSelection.address = 0;
                        providerSelection.size = 1;
                        providerSelectionToggle = false;
                        m_pasteMode = PasteModeType::ModeNotSelected;
                        m_pasteHint = PasteHintType::HintDefaultDescription;
                    }

                    // Disable selecting same files to avoid paste operation on same file (pasting/copying logic not implemented for same provider)
                    auto &[src, dst] = m_fileProvider;

                    if ((providerObjectId == ObjIdType::SrcId) && (providerIndex == dst.providerIndex)) {
                        dst.providerIndex = -1;
                        dst.providerValidity = false;
                        dst.providerSelection.address = 0;
                        dst.providerSelection.size = 0;
                        dst.providerSelection.provider = nullptr;
                        dst.providerSelectionToggle = false;
                    }
                    if ((providerObjectId == ObjIdType::DstId) && (providerIndex == src.providerIndex)) {
                        src.providerIndex = -1;
                        src.providerValidity = false;
                        src.providerSelection.address = 0;
                        src.providerSelection.size = 0;
                        src.providerSelection.provider = nullptr;
                        src.providerSelectionToggle = false;
                    }
                }
                // Show the file path in the tooltip of listed files
                drawFilePathToolTip(providerList[i]);
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();
    }

    void PopupPasteFromFileBehaviour::drawActionButtons(ProviderInfo &object, ViewHexEditor *editor) {
        auto &providerIndex = object.providerIndex;
        auto &providerValidity = object.providerValidity;
        auto &providerSelection = object.providerSelection;
        auto &providerSelectionToggle = object.providerSelectionToggle;
        auto &providerObjectId = object.providerObjectId;
        auto &[src, dst] = m_fileProvider;

        ImGui::BeginDisabled(!providerValidity);
        {
            // Select/Show input region in hex editor
            ImGui::PushID(&providerSelectionToggle);
            if (ImGuiExt::DimmedIconToggle(ICON_VS_DEBUG_RESTART_FRAME, &providerSelectionToggle)) {
                if (providerSelectionToggle) {
                    // If source, turn off destination selection toggle
                    if (providerObjectId == ObjIdType::SrcId) {
                        dst.providerSelectionToggle = false;
                    }
                    // If destination, turn off source selection toggle
                    if (providerObjectId == ObjIdType::DstId) {
                        src.providerSelectionToggle = false;
                    }
                    // Jump to the selection in hex editor
                    ImHexApi::Provider::setCurrentProvider(providerSelection.provider);
                    editor->setSelection(providerSelection.getStartAddress(), providerSelection.getEndAddress());
                    editor->jumpToSelection();
                }
            }
            ImGuiExt::InfoTooltip("hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.toggle.tip.select"_lang);
            ImGui::PopID();

            // Update the selection region for the current file from hex editor selection if select/show region toggle is enabled
            if (providerValidity && (ImHexApi::Provider::getCurrentProviderIndex() == providerIndex) && providerSelectionToggle) {
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
                }
                // Turn off select/show region toggle
                src.providerSelectionToggle = false;
                dst.providerSelectionToggle = false;
                // Jump to the selection in hex editor
                ImHexApi::Provider::setCurrentProvider(providerSelection.provider);
                setSelection(providerSelection.getStartAddress(), providerSelection.getEndAddress());
            }
            ImGuiExt::InfoTooltip("hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.button.tip.selection.all"_lang);
            ImGui::PopID();

            // Jump to begin address of selected region
            ImGui::SameLine();
            ImGui::PushID(&providerSelection.provider+2);
            if (ImGuiExt::DimmedIconButton(ICON_VS_CHEVRON_UP, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                // Turn off select/show region toggle
                src.providerSelectionToggle = false;
                dst.providerSelectionToggle = false;
                // Jump to the selection in hex editor
                ImHexApi::Provider::setCurrentProvider(providerSelection.provider);
                setSelection(providerSelection.getStartAddress(), providerSelection.getEndAddress());
            }
            ImGuiExt::InfoTooltip("hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.button.tip.selection.begin"_lang);
            ImGui::PopID();

            // Jump to end address of selected region
            ImGui::SameLine();
            ImGui::PushID(&providerSelection.provider+3);
            if (ImGuiExt::DimmedIconButton(ICON_VS_CHEVRON_DOWN, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                // Turn off select/show region toggle
                src.providerSelectionToggle = false;
                dst.providerSelectionToggle = false;
                // Jump to the selection in hex editor
                ImHexApi::Provider::setCurrentProvider(providerSelection.provider);
                setSelection(providerSelection.getEndAddress(), providerSelection.getStartAddress());
            }
            ImGuiExt::InfoTooltip("hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.button.tip.selection.end"_lang);
            ImGui::PopID();

            // Clear selected region
            ImGui::SameLine();
            ImGui::PushID(&providerSelection.provider+4);
            if (ImGuiExt::DimmedIconButton(ICON_VS_CLEAR_ALL, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                // providerSelection.address = 0;
                providerSelection.size = 1;
                // Turn off select/show region toggle
                src.providerSelectionToggle = false;
                dst.providerSelectionToggle = false;
                m_pasteMode = PasteModeType::ModeNotSelected;
                // Jump to the selection in hex editor
                ImHexApi::Provider::setCurrentProvider(providerSelection.provider);
                setSelection(providerSelection.getStartAddress(), providerSelection.getEndAddress());
            }
            ImGuiExt::InfoTooltip("hex.builtin.view.hex_editor.menu.edit.paste_from_file.popup.button.tip.selection.clear"_lang);
            ImGui::PopID();
        }
        ImGui::EndDisabled();

        // Handle setting and jumping to the selection in the next cycle for the action buttons except Select/Show toggle button, since it doesn't work immediately after changing the provider with setCurrentProvider().
        // This is because ViewHexEditor loads the new provider at the start of next cycle.
        static u8 cycleCount = 0;
        if (m_setSelectionTrigger) {
            if (cycleCount > 1) { // cycle count should reach 2 since drawActionButtons() is called twice (src & dst) in a single cycle
                if (providerValidity && (ImHexApi::Provider::getCurrentProviderIndex() == providerIndex)) {
                    m_setSelectionTrigger = false;
                    editor->setSelection(m_setSelectionStart, m_setSelectionEnd);
                    editor->jumpToSelection();
                }
            } else {
                cycleCount++;
            }
        } else {
            cycleCount = 0;
        }
    }

    void PopupPasteFromFileBehaviour::drawRegionProvider(ProviderInfo &object, ViewHexEditor *editor) {
        auto &providerIndex = object.providerIndex;
        auto &providerValidity = object.providerValidity;
        auto &providerSelection = object.providerSelection;
        auto &providerSelectionToggle = object.providerSelectionToggle;

        ImGui::BeginDisabled(!providerValidity);
        {
            auto inputWidth = (ImGui::GetContentRegionAvail().x * 0.80f); // 80% of column width

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
                    if (providerValidity) {
                        auto providerStartAddress = providerSelection.provider->getBaseAddress();
                        auto providerActualSize = providerSelection.provider->getActualSize();
                        auto providerEndAddress = (providerStartAddress + providerActualSize - 1);

                        if (inputA > providerEndAddress) {
                            inputA = providerEndAddress;
                        }
                        if (inputA <= providerStartAddress) {
                            inputA = providerStartAddress;
                        }

                        if (inputB < inputA) {
                            inputB = inputA;
                        }
                        if (inputB > providerEndAddress) {
                            inputB = providerEndAddress;
                        }
                    }

                    providerSelection.address = inputA;
                    providerSelection.size = (inputB - inputA) + 1;

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
                    if (providerValidity) {
                        auto providerStartAddress = providerSelection.provider->getBaseAddress();
                        auto providerActualSize = providerSelection.provider->getActualSize();
                        auto providerEndAddress = (providerStartAddress + providerActualSize - 1);

                        if (inputA > providerEndAddress) {
                            inputA = providerEndAddress;
                        }
                        if (inputA <= providerStartAddress) {
                            inputA = providerStartAddress;
                        }

                        auto providerAvailSize = (providerActualSize - inputA);
                        if (inputB > providerAvailSize) {
                            inputB = providerAvailSize;
                        }
                        if (inputB <= 0) {
                            inputB = 1;
                        }
                    }

                    providerSelection.address = inputA;
                    providerSelection.size = inputB;

                    ImGui::EndTabItem();
                }

                // Update the region selection in hex editor for the current file if select/show region toggle is enabled
                if (providerValidity && (ImHexApi::Provider::getCurrentProviderIndex() == providerIndex) && providerSelectionToggle &&
                        (!(ImGui::IsMouseDragging(ImGuiMouseButton_Left)) && 
                        !(ImGui::IsKeyDown(ImGuiKey_LeftShift)) && 
                        !(ImGui::IsKeyDown(ImGuiKey_RightShift)))) {
                    editor->setSelection(providerSelection.getStartAddress(), providerSelection.getEndAddress());
                }

                ImGui::EndTabBar();
            }
            ImGui::PopID();
        }
        ImGui::EndDisabled();
    }

    void PopupPasteFromFileBehaviour::drawFilePathToolTip(const prv::Provider *provider) const {
        auto *dataDescriptionProvider = dynamic_cast<const prv::IProviderDataDescription*>(provider);
        if (dataDescriptionProvider != nullptr) {
            const auto &description = dataDescriptionProvider->getDataDescription();
            if (!description.empty()) {
                auto &[_, path] = description[0]; // description index 0: file path
                if (ImGuiExt::InfoTooltip()) {
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted(path.c_str()); // only show file path in tool tip of listed files
                    ImGui::EndTooltip();
                }
            }
        }
    }

    void PopupPasteFromFileBehaviour::drawPasteModeButton(const char *buttonLabel, PasteModeType buttonMode, PasteHintType buttonHint, bool buttonDisable, float buttonWidth) {
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

    void PopupPasteFromFileBehaviour::drawIntegerInputField(const char *inputLabel, void *inputValue, float inputWidth) {
        ImGui::SetNextItemWidth(inputWidth);
        ImGuiExt::InputIntegerPrefix(inputLabel,
                                     (m_inputBaseHex ? "0x" : "0d"),
                                     inputValue,
                                     ImGuiDataType_U64,
                                     (m_inputBaseHex ? "%llX" : "%lld"),
                                     ((m_inputBaseHex ? ImGuiInputTextFlags_CharsHexadecimal : ImGuiInputTextFlags_CharsDecimal) | ImGuiInputTextFlags_AutoSelectAll));
    }

    void PopupPasteFromFileBehaviour::setSelection(u64 start, u64 end) {
        m_setSelectionStart = start;
        m_setSelectionEnd = end;
        m_setSelectionTrigger = true;
    }

    std::string PopupPasteFromFileBehaviour::elapsedTimeFormatted(void) const {
        using namespace std::chrono;

        if (m_elapsedTime >= 1s) {
            return fmt::format("{:.3f} s", duration<double>(m_elapsedTime).count());
        }

        if (m_elapsedTime >= 1ms) {
            return fmt::format("{:.3f} ms", duration<double, std::milli>(m_elapsedTime).count());
        }

        if (m_elapsedTime >= 1us) {
            return fmt::format("{:.3f} µs", duration<double, std::micro>(m_elapsedTime).count());
        }

        return fmt::format("{:.3f} ns", duration<double, std::nano>(m_elapsedTime).count());
    }
}