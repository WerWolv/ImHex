#include "content/views/view_pattern_editor.hpp"
#include "fonts/blendericons_font.h"

#include <hex/api/content_registry.hpp>
#include <hex/api/project_file_manager.hpp>
#include <pl/pattern_language.hpp>
#include <pl/patterns/pattern.hpp>
#include <pl/core/preprocessor.hpp>
#include <pl/core/parser.hpp>
#include <pl/core/lexer.hpp>
#include <pl/core/validator.hpp>
#include "pl/core/tokens.hpp"
#include <pl/core/ast/ast_node_compound_statement.hpp>
#include <pl/core/ast/ast_node_lvalue_assignment.hpp>
#include <pl/core/ast/ast_node_variable_decl.hpp>
#include <pl/core/ast/ast_node_builtin_type.hpp>
#include <pl/core/ast/ast_node_struct.hpp>
#include <pl/core/ast/ast_node_union.hpp>
#include <pl/core/ast/ast_node_enum.hpp>
#include <pl/core/ast/ast_node_bitfield.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/magic.hpp>
#include <hex/helpers/binary_pattern.hpp>
#include <hex/helpers/default_paths.hpp>

#include <hex/providers/memory_provider.hpp>

#include <hex/helpers/fmt.hpp>
#include <fmt/chrono.h>

#include <popups/popup_question.hpp>
#include <toasts/toast_notification.hpp>

#include <chrono>

#include <wolv/io/file.hpp>
#include <wolv/io/fs.hpp>
#include <wolv/utils/guards.hpp>
#include <wolv/utils/lock.hpp>

#include <content/global_actions.hpp>

class Definition;
namespace hex::plugin::builtin {

    using namespace hex::literals;
    static void drawVirtualFileTree(const std::vector<const ViewPatternEditor::VirtualFile*> &virtualFiles, u32 level = 0) {
        ImGui::PushID(level + 1);
        ON_SCOPE_EXIT { ImGui::PopID(); };

        std::map<std::string, std::vector<const ViewPatternEditor::VirtualFile*>> currFolderEntries;
        for (const auto &file : virtualFiles) {
            const auto &path = file->path;

            auto currSegment = wolv::io::fs::toNormalizedPathString(*std::next(path.begin(), level));
            if (std::distance(path.begin(), path.end()) == ptrdiff_t(level + 1)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                ImGui::TextUnformatted(ICON_VS_FILE);
                ImGui::SameLine();

                ImGui::TreeNodeEx(currSegment.c_str(), ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered()) {
                    ImHexApi::Provider::add<prv::MemoryProvider>(file->data, wolv::util::toUTF8String(file->path.filename()));
                }

                continue;
            }

            currFolderEntries[currSegment].emplace_back(file);
        }

        int id = 1;
        for (const auto &[segment, entries] : currFolderEntries) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            if (level == 0) {
                ImGui::TextUnformatted(ICON_VS_DATABASE);
            } else {
                ImGui::TextUnformatted(ICON_VS_FOLDER);
            }

            ImGui::PushID(id);

            ImGui::SameLine();
            if (ImGui::TreeNodeEx(segment.c_str(), ImGuiTreeNodeFlags_SpanFullWidth)) {
                drawVirtualFileTree(entries, level + 1);
                ImGui::TreePop();
            }

            ImGui::PopID();
            id += 1;
        }
    }

    ViewPatternEditor::ViewPatternEditor() : View::Window("hex.builtin.view.pattern_editor.name", ICON_VS_SYMBOL_NAMESPACE) {
        m_editorRuntime = std::make_unique<pl::PatternLanguage>();
        ContentRegistry::PatternLanguage::configureRuntime(*m_editorRuntime, nullptr);


        m_textEditor.SetShowWhitespaces(false);

        m_consoleEditor.SetShowWhitespaces(false);
        m_consoleEditor.SetReadOnly(true);
        m_consoleEditor.SetShowCursor(false);
        m_consoleEditor.SetShowLineNumbers(false);

        this->registerEvents();
        this->registerMenuItems();
        this->registerHandlers();
    }

    ViewPatternEditor::~ViewPatternEditor() {
        RequestPatternEditorSelectionChange::unsubscribe(this);
        RequestSetPatternLanguageCode::unsubscribe(this);
        RequestRunPatternCode::unsubscribe(this);
        EventFileLoaded::unsubscribe(this);
        EventProviderChanged::unsubscribe(this);
        EventProviderClosed::unsubscribe(this);
        RequestAddVirtualFile::unsubscribe(this);
    }

    void ViewPatternEditor::drawContent() {
        auto provider = ImHexApi::Provider::get();

        if (ImHexApi::Provider::isValid() && provider->isAvailable()) {
            static float height = 0;
            static bool dragging = false;

            const auto availableSize = ImGui::GetContentRegionAvail();
            auto textEditorSize = availableSize;
            textEditorSize.y *= 3.5 / 5.0;
            textEditorSize.y -= ImGui::GetTextLineHeightWithSpacing();
            textEditorSize.y += height;

            if (availableSize.y > 1)
                textEditorSize.y = std::clamp(textEditorSize.y, 1.0F, std::max(1.0F, availableSize.y - ImGui::GetTextLineHeightWithSpacing() * 3));
            m_textHighlighter.highlightSourceCode();

            m_textEditor.Render("hex.builtin.view.pattern_editor.name"_lang, textEditorSize, true);
            TextEditor::FindReplaceHandler *findReplaceHandler = m_textEditor.GetFindReplaceHandler();
            if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && !ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
                ImGui::OpenPopup("##pattern_editor_context_menu");
            }

            bool clickedMenuFind = false;
            bool clickedMenuReplace = false;
            if (ImGui::BeginPopup("##pattern_editor_context_menu")) {
                // no shortcut for this
                if (ImGui::MenuItem("hex.builtin.menu.file.import.pattern_file"_lang, nullptr, false))
                    m_importPatternFile();
                if (ImGui::MenuItem("hex.builtin.menu.file.export.pattern_file"_lang, nullptr, false))
                    m_exportPatternFile();

                ImGui::Separator();

                const bool hasSelection = m_textEditor.HasSelection();
                if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.cut"_lang, Shortcut(CTRLCMD + Keys::X).toString().c_str(), false, hasSelection)) {
                    m_textEditor.Cut();
                }
                if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.copy"_lang, Shortcut(CTRLCMD + Keys::C).toString().c_str(), false, hasSelection)) {
                    m_textEditor.Copy();
                }
                if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.paste"_lang, Shortcut(CTRLCMD + Keys::V).toString().c_str())) {
                    m_textEditor.Paste();
                }

                ImGui::Separator();

                if (ImGui::MenuItem("hex.builtin.menu.edit.undo"_lang, Shortcut(CTRLCMD + Keys::Z).toString().c_str(), false, m_textEditor.CanUndo())) {
                    m_textEditor.Undo();
                }
                if (ImGui::MenuItem("hex.builtin.menu.edit.redo"_lang, Shortcut(CTRLCMD + Keys::Y).toString().c_str(), false, m_textEditor.CanRedo())) {
                    m_textEditor.Redo();
                }

                ImGui::Separator();
                // Search and replace entries
                if (ImGui::MenuItem("hex.builtin.view.pattern_editor.menu.find"_lang, Shortcut(CTRLCMD + Keys::F).toString().c_str(),false,this->m_textEditor.HasSelection()))
                    clickedMenuFind = true;

                if (ImGui::MenuItem("hex.builtin.view.pattern_editor.menu.find_next"_lang, Shortcut(Keys::F3).toString().c_str(),false,!findReplaceHandler->GetFindWord().empty()))
                    findReplaceHandler->FindMatch(&m_textEditor,true);

                if (ImGui::MenuItem("hex.builtin.view.pattern_editor.menu.find_previous"_lang, Shortcut(SHIFT + Keys::F3).toString().c_str(),false,!findReplaceHandler->GetFindWord().empty()))
                    findReplaceHandler->FindMatch(&m_textEditor,false);

                if (ImGui::MenuItem("hex.builtin.view.pattern_editor.menu.replace"_lang, Shortcut(CTRLCMD +  Keys::H).toString().c_str(),false,!findReplaceHandler->GetReplaceWord().empty()))
                    clickedMenuReplace = true;

                if (ImGui::MenuItem("hex.builtin.view.pattern_editor.menu.replace_next"_lang,"",false,!findReplaceHandler->GetReplaceWord().empty()))
                    findReplaceHandler->Replace(&m_textEditor,true);

                if (ImGui::MenuItem("hex.builtin.view.pattern_editor.menu.replace_previous"_lang, "",false,!findReplaceHandler->GetReplaceWord().empty()))
                    findReplaceHandler->Replace(&m_textEditor,false);

                if (ImGui::MenuItem("hex.builtin.view.pattern_editor.menu.replace_all"_lang, "",false,!findReplaceHandler->GetReplaceWord().empty()))
                    findReplaceHandler->ReplaceAll(&m_textEditor);

                ImGui::EndPopup();
            }

            // Context menu entries that open the find/replace popup
            ImGui::PushID(&this->m_textEditor);
            if (clickedMenuFind) {
                m_replaceMode = false;
                m_openFindReplacePopUp = true;
            }

            if (clickedMenuReplace) {
                m_replaceMode = true;
                m_openFindReplacePopUp = true;
            }

            // shortcuts to open the find/replace popup

            static std::string findWord;
            static bool requestFocus = false;
            static u64 position = 0;
            static u64 count = 0;
            static bool updateCount = false;

            if (m_openFindReplacePopUp) {
                m_openFindReplacePopUp = false;
                // Place the popup at the top right of the window
                auto windowSize = ImGui::GetWindowSize();
                auto style = ImGui::GetStyle();

                // Set the scrollbar size only if it is visible
                float scrollbarSize = 0;

                // Calculate the number of lines to display in the text editor
                auto totalTextHeight =  m_textEditor.GetTotalLines() * m_textEditor.GetCharAdvance().y;

                // Compare it to the window height
                if (totalTextHeight > windowSize.y)
                    scrollbarSize = style.ScrollbarSize;

                auto labelSize = ImGui::CalcTextSize(ICON_VS_WHOLE_WORD);

                // Six icon buttons
                auto popupSize =  ImVec2({(labelSize.x + style.FramePadding.x * 2.0F) * 6.0F,
                                           labelSize.y + style.FramePadding.y * 2.0F + style.WindowPadding.y + 3 });

                // 2 * 11 spacings in between elements
                popupSize.x += style.FramePadding.x * 22.0F;

                // Text input fields are set to 12 characters wide
                popupSize.x += ImGui::GetFontSize() * 12.0F;

                // IndexOfCount text
                popupSize.x +=  ImGui::CalcTextSize("2000 of 2000").x + 2.0F;
                popupSize.x += scrollbarSize;

                // Position of popup relative to parent window
                ImVec2 windowPosForPopup = ImGui::GetWindowPos();

                // Not the window height but the content height
                windowPosForPopup.y += popupSize.y;

                // Add remaining window height
                popupSize.y += style.WindowPadding.y + 1;

                // Move to the right so as not to overlap the scrollbar
                windowPosForPopup.x += windowSize.x - popupSize.x;
                findReplaceHandler->SetFindWindowPos(windowPosForPopup);

                if (m_replaceMode) {
                    // Remove one window padding
                    popupSize.y -= style.WindowPadding.y;
                    // Add the replace window height
                    popupSize.y *= 2;
                }

                ImGui::SetNextWindowPos(windowPosForPopup);
                ImGui::SetNextWindowSize(popupSize);
                ImGui::OpenPopup("##pattern_editor_find_replace");

                findReplaceHandler->resetMatches();

                // Use selection as find word if there is one, otherwise use the word under the cursor
                if (!this->m_textEditor.HasSelection())
                    this->m_textEditor.SelectWordUnderCursor();

                findWord = this->m_textEditor.GetSelectedText();

                // Find all matches to findWord
                findReplaceHandler->FindAllMatches(&m_textEditor,findWord);
                position = findReplaceHandler->FindPosition(&m_textEditor,m_textEditor.GetCursorPosition(), true);
                count = findReplaceHandler->GetMatches().size();
                findReplaceHandler->SetFindWord(&m_textEditor,findWord);
                requestFocus = true;
                updateCount = true;
            }

            drawFindReplaceDialog(findWord, requestFocus, position, count, updateCount);

            ImGui::PopID();

            ImGui::Button("##settings_drag_bar", ImVec2(ImGui::GetContentRegionAvail().x, 2_scaled));
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0)) {
                if (ImGui::IsItemHovered())
                    dragging = true;
            } else {
                dragging = false;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            }

            if (dragging) {
                height += ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0).y;
                ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
            }

            auto settingsSize = ImGui::GetContentRegionAvail();
            settingsSize.y -= ImGui::GetTextLineHeightWithSpacing() * 2.5F;

            if (ImGui::BeginTabBar("##settings")) {
                if (ImGui::BeginTabItem("hex.builtin.view.pattern_editor.console"_lang)) {
                    this->drawConsole(settingsSize);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("hex.builtin.view.pattern_editor.env_vars"_lang)) {
                    this->drawEnvVars(settingsSize, *m_envVarEntries);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("hex.builtin.view.pattern_editor.settings"_lang)) {
                    this->drawVariableSettings(settingsSize, *m_patternVariables);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("hex.builtin.view.pattern_editor.sections"_lang)) {
                    this->drawSectionSelector(settingsSize, *m_sections);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("hex.builtin.view.pattern_editor.virtual_files"_lang)) {
                    this->drawVirtualFiles(settingsSize, *m_virtualFiles);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("hex.builtin.view.pattern_editor.debugger"_lang)) {
                    this->drawDebugger(settingsSize);
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);

            {
                auto &runtime = ContentRegistry::PatternLanguage::getRuntime();
                if (runtime.isRunning()) {
                    if (m_breakpointHit) {
                        if (ImGuiExt::IconButton(ICON_VS_DEBUG_CONTINUE, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarYellow)))
                            m_breakpointHit = false;
                        ImGui::SameLine();
                        if (ImGuiExt::IconButton(ICON_VS_DEBUG_STEP_INTO, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarYellow))) {
                            runtime.getInternals().evaluator->pauseNextLine();
                            m_breakpointHit = false;
                        }
                    } else {
                        if (ImGuiExt::IconButton(ICON_VS_DEBUG_STOP, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed)))
                            runtime.abort();
                    }
                } else {
                    if (ImGuiExt::IconButton(ICON_VS_DEBUG_START, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarGreen)) || m_triggerEvaluation) {
                        m_triggerEvaluation = false;
                        this->evaluatePattern(m_textEditor.GetText(), provider);
                    }
                }


                ImGui::PopStyleVar();

                ImGui::SameLine();
                if (m_runningEvaluators > 0) {
                    if (m_breakpointHit) {
                        ImGuiExt::TextFormatted("hex.builtin.view.pattern_editor.breakpoint_hit"_lang, runtime.getInternals().evaluator->getPauseLine().value_or(0));
                    } else {
                        ImGuiExt::TextSpinner("hex.builtin.view.pattern_editor.evaluating"_lang);
                    }

                    ImGui::SameLine();
                    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                    ImGui::SameLine();

                    const auto padding = ImGui::GetStyle().FramePadding.y;

                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2());
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2());
                    if (ImGui::BeginChild("##read_cursor", ImGui::GetContentRegionAvail() + ImVec2(0, padding), true)) {
                        const auto startPos = ImGui::GetCursorScreenPos();
                        const auto size    = ImGui::GetContentRegionAvail();

                        const auto dataBaseAddress = runtime.getInternals().evaluator->getDataBaseAddress();
                        const auto dataSize = runtime.getInternals().evaluator->getDataSize();

                        const auto insertPos = [&, this](u64 address, u32 color) {
                            const auto progress = float(address - dataBaseAddress) / float(dataSize);

                            m_accessHistory[m_accessHistoryIndex] = { progress, color };
                            m_accessHistoryIndex = (m_accessHistoryIndex + 1) % m_accessHistory.size();
                        };

                        insertPos(runtime.getLastReadAddress(),         ImGuiExt::GetCustomColorU32(ImGuiCustomCol_ToolbarBlue));
                        insertPos(runtime.getLastWriteAddress(),        ImGuiExt::GetCustomColorU32(ImGuiCustomCol_ToolbarRed));
                        insertPos(runtime.getLastPatternPlaceAddress(), ImGuiExt::GetCustomColorU32(ImGuiCustomCol_ToolbarGreen));

                        const auto drawList = ImGui::GetWindowDrawList();
                        for (const auto &[progress, color] : m_accessHistory) {
                            if (progress <= 0) continue;

                            const auto linePos = startPos + ImVec2(size.x * progress, 0);

                            drawList->AddLine(linePos, linePos + ImVec2(0, size.y), color, 2_scaled);
                        }
                    }
                    ImGui::EndChild();
                    ImGui::PopStyleVar(2);

                } else {
                    if (ImGui::Checkbox("hex.builtin.view.pattern_editor.auto"_lang, &m_runAutomatically)) {
                        if (m_runAutomatically)
                            m_hasUnevaluatedChanges = true;
                    }

                    ImGui::SameLine();
                    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                    ImGui::SameLine();

                    if (const auto max = runtime.getMaximumPatternCount(); max >= std::numeric_limits<u64>::max()) {
                        ImGuiExt::TextFormatted("{}", runtime.getCreatedPatternCount());
                    } else {
                        ImGuiExt::TextFormatted("{} / {}",
                                             runtime.getCreatedPatternCount(),
                                             runtime.getMaximumPatternCount());
                    }
                }
            }

            if (m_textEditor.IsTextChanged()) {
                m_textEditor.SetTextChanged(false);
                m_hasUnevaluatedChanges = true;

                m_textHighlighter.m_needsToUpdateColors = false;

                m_lastEditorChangeTime = std::chrono::steady_clock::now();

                ImHexApi::Provider::markDirty();
            }

            if (m_hasUnevaluatedChanges && m_runningEvaluators == 0 && m_runningParsers == 0) {
                if ((std::chrono::steady_clock::now() - m_lastEditorChangeTime) > std::chrono::seconds(1)) {
                    m_hasUnevaluatedChanges = false;

                    auto code = m_textEditor.GetText();
                    EventPatternEditorChanged::post(code);

                    TaskManager::createBackgroundTask("hex.builtin.task.parsing_pattern"_lang, [this, code = std::move(code), provider](auto &){
                        this->parsePattern(code, provider);

                        if (m_runAutomatically)
                            m_triggerAutoEvaluate = true;
                    });
                }
            }

            if (m_triggerAutoEvaluate.exchange(false)) {
                this->evaluatePattern(m_textEditor.GetText(), provider);
            }
        }

        if (m_dangerousFunctionCalled && !ImGui::IsPopupOpen(ImGuiID(0), ImGuiPopupFlags_AnyPopup)) {
            ui::PopupQuestion::open("hex.builtin.view.pattern_editor.dangerous_function.desc"_lang,
                [this] {
                    m_dangerousFunctionsAllowed = DangerousFunctionPerms::Allow;
                }, [this] {
                    m_dangerousFunctionsAllowed = DangerousFunctionPerms::Deny;
                }
            );

            m_dangerousFunctionCalled = false;
        }

        View::discardNavigationRequests();
    }

    void ViewPatternEditor::historyInsert(std::array<std::string,256> &history, u32 &size, u32 &index, const std::string &value) {
        for (u64 i = 0; i < size; i++) {
            if (history[i] == value)
                return;
        }

        if (size < 256){
            history[size] = value;
            size++;
        } else {
            index = (index - 1) % 256;
            history[index] = value;
            index = (index + 1) % 256;
        }
    }

    void ViewPatternEditor::drawFindReplaceDialog(std::string &findWord, bool &requestFocus, u64 &position, u64 &count, bool &updateCount) {
        static std::string replaceWord;

        TextEditor::FindReplaceHandler *findReplaceHandler = m_textEditor.GetFindReplaceHandler();
        static bool requestFocusReplace = false;
        static bool requestFocusFind = false;

        static bool downArrowFind = false;
        static bool upArrowFind = false;
        static bool downArrowReplace = false;
        static bool upArrowReplace = false;
        static bool enterPressedFind = false;

        bool enter     = ImGui::IsKeyPressed(ImGuiKey_Enter, false)         || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
        bool upArrow   = ImGui::IsKeyPressed(ImGuiKey_UpArrow, false)       || ImGui::IsKeyPressed(ImGuiKey_Keypad8, false);
        bool downArrow = ImGui::IsKeyPressed(ImGuiKey_DownArrow, false)     || ImGui::IsKeyPressed(ImGuiKey_Keypad2, false);
        bool shift     = ImGui::IsKeyDown(ImGuiKey_LeftShift)               || ImGui::IsKeyDown(ImGuiKey_RightShift);
        bool alt       = ImGui::IsKeyDown(ImGuiKey_LeftAlt)                 || ImGui::IsKeyDown(ImGuiKey_RightAlt);

        if (ImGui::BeginPopup("##pattern_editor_find_replace")) {

            if (ImGui::BeginTable("##pattern_editor_find_replace_table", 2,
                                  ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerH)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                bool oldReplace = m_replaceMode;
                ImGuiExt::DimmedIconToggle(ICON_VS_TRIANGLE_DOWN, ICON_VS_TRIANGLE_RIGHT, &m_replaceMode);
                if (oldReplace != m_replaceMode) {
                    if (m_replaceMode)
                        requestFocusReplace = true;
                    else
                        requestFocusFind = true;
                }

                ImGui::TableNextColumn();

                static int findFlags = ImGuiInputTextFlags_None;
                if (requestFocus && m_findHistoryIndex == m_findHistorySize)
                    findFlags |= ImGuiInputTextFlags_AutoSelectAll;
                else
                    findFlags &= ~ImGuiInputTextFlags_AutoSelectAll;

                if (m_findHistoryIndex != m_findHistorySize && requestFocusFind ) {
                    findFlags |= ImGuiInputTextFlags_ReadOnly;
                } else
                    findFlags &= ~ImGuiInputTextFlags_ReadOnly;

                std::string hint = "hex.builtin.view.pattern_editor.find_hint"_lang.operator std::string();
                if (m_findHistorySize > 0) {
                    hint += " (";
                    hint += ICON_BI_DATA_TRANSFER_BOTH;
                    hint += "hex.builtin.view.pattern_editor.find_hint_history"_lang.operator std::string();
                }

                ImGui::PushItemWidth(ImGui::GetFontSize() * 12);
                if (ImGui::InputTextWithHint("###findInputTextWidget", hint.c_str(), findWord, findFlags) || enter ) {
                    if (enter) {
                        enterPressedFind = true;
                        enter = false;
                    }
                    updateCount = true;
                    requestFocusFind = true;
                    findReplaceHandler->SetFindWord(&m_textEditor,findWord);
                }

                if ((!m_replaceMode && requestFocus) || requestFocusFind) {
                    ImGui::SetKeyboardFocusHere(-1);
                    requestFocus = false;
                    requestFocusFind = false;
                }

                if (ImGui::IsItemActive() && (upArrow || downArrow) && m_findHistorySize > 0) {

                    if (upArrow)
                        m_findHistoryIndex = (m_findHistoryIndex + m_findHistorySize - 1) % m_findHistorySize;

                    if (downArrow)
                        m_findHistoryIndex = (m_findHistoryIndex + 1) % m_findHistorySize;

                    findWord = m_findHistory[m_findHistoryIndex];
                    findReplaceHandler->SetFindWord(&m_textEditor,findWord);
                    position = findReplaceHandler->FindPosition(&m_textEditor,m_textEditor.GetCursorPosition(), true);
                    count = findReplaceHandler->GetMatches().size();
                    updateCount = true;
                    requestFocusFind = true;
                }
                ImGui::PopItemWidth();

                ImGui::SameLine();

                bool matchCase = findReplaceHandler->GetMatchCase();

                // Allow Alt-C to toggle case sensitivity
                bool altCPressed = ImGui::IsKeyPressed(ImGuiKey_C, false) && alt;
                if (altCPressed || ImGuiExt::DimmedIconToggle(ICON_VS_CASE_SENSITIVE, &matchCase)) {
                    if (altCPressed)
                        matchCase = !matchCase;
                    findReplaceHandler->SetMatchCase(&m_textEditor,matchCase);
                    position = findReplaceHandler->FindPosition(&m_textEditor,m_textEditor.GetCursorPosition(), true);
                    count = findReplaceHandler->GetMatches().size();
                    updateCount = true;
                    requestFocusFind = true;
                }

                ImGui::SameLine();

                bool matchWholeWord = findReplaceHandler->GetWholeWord();

                // Allow Alt-W to toggle whole word
                bool altWPressed = ImGui::IsKeyPressed(ImGuiKey_W, false) && alt;
                if (altWPressed || ImGuiExt::DimmedIconToggle(ICON_VS_WHOLE_WORD, &matchWholeWord)) {
                    if (altWPressed)
                        matchWholeWord = !matchWholeWord;
                    findReplaceHandler->SetWholeWord(&m_textEditor,matchWholeWord);
                    position = findReplaceHandler->FindPosition(&m_textEditor,m_textEditor.GetCursorPosition(), true);
                    count = findReplaceHandler->GetMatches().size();
                    updateCount = true;
                    requestFocusFind = true;
                }

                ImGui::SameLine();

                bool useRegex = findReplaceHandler->GetFindRegEx();

                // Allow Alt-R to toggle regex
                bool altRPressed = ImGui::IsKeyPressed(ImGuiKey_R, false) && alt;
                if (altRPressed || ImGuiExt::DimmedIconToggle(ICON_VS_REGEX, &useRegex)) {
                    if (altRPressed)
                        useRegex = !useRegex;
                    findReplaceHandler->SetFindRegEx(&m_textEditor,useRegex);
                    position = findReplaceHandler->FindPosition(&m_textEditor,m_textEditor.GetCursorPosition(), true);
                    count = findReplaceHandler->GetMatches().size();
                    updateCount = true;
                    requestFocusFind = true;
                }

                static std::string counterString;

                auto totalSize = ImGui::CalcTextSize("2000 of 2000");
                ImVec2 buttonSize;

                if (updateCount) {
                    updateCount = false;

                    if (count == 0 || position == std::numeric_limits<u64>::max())
                        counterString = "hex.builtin.view.pattern_editor.no_results"_lang.operator std::string();
                    else {
                        if (position > 1999)
                            counterString = "?";
                        else
                            counterString = hex::format("{} ", position);
                        counterString += "hex.builtin.view.pattern_editor.of"_lang.operator const char *();
                        if (count > 1999)
                            counterString += "1999+";
                        else
                            counterString += hex::format(" {}", count);
                    }
                }
                auto resultSize = ImGui::CalcTextSize(counterString.c_str());
                if (totalSize.x > resultSize.x)
                    buttonSize = ImVec2(totalSize.x + 2 - resultSize.x, resultSize.y);
                else
                    buttonSize = ImVec2(2, resultSize.y);

                ImGui::SameLine();
                ImGui::TextUnformatted(counterString.c_str());

                ImGui::SameLine();
                ImGui::InvisibleButton("##find_result_padding", buttonSize);

                ImGui::SameLine();
                if (ImGuiExt::IconButton(ICON_VS_ARROW_DOWN, ImVec4(1, 1, 1, 1)))
                    downArrowFind = true;

                ImGui::SameLine();
                if (ImGuiExt::IconButton(ICON_VS_ARROW_UP, ImVec4(1, 1, 1, 1)))
                    upArrowFind = true;

                if (m_replaceMode) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();

                    static int replaceFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;
                    if (m_replaceHistoryIndex != m_replaceHistorySize && requestFocusReplace) {
                        replaceFlags |= ImGuiInputTextFlags_ReadOnly;
                    } else
                        replaceFlags &= ~ImGuiInputTextFlags_ReadOnly;

                    hint = "hex.builtin.view.pattern_editor.replace_hint"_lang.operator std::string();
                    if (m_replaceHistorySize > 0) {
                        hint += " (";
                        hint += ICON_BI_DATA_TRANSFER_BOTH;
                        hint += "hex.builtin.view.pattern_editor.replace_hint_history"_lang.operator std::string();
                    }

                    ImGui::PushItemWidth(ImGui::GetFontSize() * 12);
                    if (ImGui::InputTextWithHint("##replaceInputTextWidget", hint.c_str(), replaceWord, replaceFlags) || downArrowReplace || upArrowReplace) {
                        findReplaceHandler->SetReplaceWord(replaceWord);
                        historyInsert(m_replaceHistory, m_replaceHistorySize, m_replaceHistoryIndex, replaceWord);

                        bool textReplaced = findReplaceHandler->Replace(&m_textEditor,!shift && !upArrowReplace);
                        if (textReplaced) {
                            if (count > 0) {
                                if (position == count)
                                    position -= 1;
                                count -= 1;
                                if (count == 0)
                                    requestFocusFind = true;
                                else
                                    requestFocusReplace = true;
                            } else
                                requestFocusFind = true;
                            updateCount = true;
                        }

                        downArrowReplace = false;
                        upArrowReplace = false;

                        if (enterPressedFind) {
                            enterPressedFind = false;
                            requestFocusFind = false;
                        }
                    }

                    if (requestFocus || requestFocusReplace) {
                        ImGui::SetKeyboardFocusHere(-1);
                        requestFocus = false;
                        requestFocusReplace = false;
                    }

                    if (ImGui::IsItemActive() && (upArrow || downArrow) && m_replaceHistorySize > 0) {
                        if (upArrow)
                            m_replaceHistoryIndex = (m_replaceHistoryIndex + m_replaceHistorySize - 1) % m_replaceHistorySize;
                        if (downArrow)
                            m_replaceHistoryIndex = (m_replaceHistoryIndex + 1) % m_replaceHistorySize;

                        replaceWord = m_replaceHistory[m_replaceHistoryIndex];
                        findReplaceHandler->SetReplaceWord(replaceWord);

                        requestFocusReplace = true;
                    }

                    ImGui::PopItemWidth();

                    ImGui::SameLine();
                    if (ImGuiExt::IconButton(ICON_VS_FOLD_DOWN, ImVec4(1, 1, 1, 1)))
                        downArrowReplace = true;

                    ImGui::SameLine();
                    if (ImGuiExt::IconButton(ICON_VS_FOLD_UP, ImVec4(1, 1, 1, 1)))
                        upArrowReplace = true;

                    ImGui::SameLine();
                    if (ImGuiExt::IconButton(ICON_VS_REPLACE_ALL, ImVec4(1, 1, 1, 1))) {
                        findReplaceHandler->SetReplaceWord(replaceWord);
                        historyInsert(m_replaceHistory,m_replaceHistorySize, m_replaceHistoryIndex, replaceWord);
                        findReplaceHandler->ReplaceAll(&m_textEditor);
                        count = 0;
                        position = 0;
                        requestFocusFind = true;
                        updateCount = true;
                    }
                    findReplaceHandler->SetFindWindowSize(ImGui::GetWindowSize());
                } else
                    findReplaceHandler->SetFindWindowSize(ImGui::GetWindowSize());

                if ((ImGui::IsKeyPressed(ImGuiKey_F3, false)) || downArrowFind || upArrowFind || enterPressedFind) {
                    historyInsert(m_findHistory, m_findHistorySize, m_findHistoryIndex, findWord);
                    position = findReplaceHandler->FindMatch(&m_textEditor,!shift && !upArrowFind);
                    count = findReplaceHandler->GetMatches().size();
                    updateCount = true;
                    downArrowFind = false;
                    upArrowFind = false;
                    requestFocusFind = true;
                    enterPressedFind = false;
                }

                ImGui::EndTable();
            }
            // Escape key to close the popup
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
    }

    i32 ViewPatternEditor::TextHighlighter::escapeCharCount(const std::string &str) {
        int count = 0;
        for (auto c : str) {
            if (c == '\"' || c == '\\' || c == '\'' || c == '\0' || c == '\t' ||
                c == '\n' || c == '\r' || c == '\a' || c == '\b' || c == '\f')
                count++;
        }
        return count;
    }

    // very simple parsing to look for type declarations
    std::optional<std::shared_ptr<pl::core::ast::ASTNode>> ViewPatternEditor::TextHighlighter::parseChildren(const std::shared_ptr<pl::core::ast::ASTNode> &node, const std::string &typeName) {
        std::vector<std::shared_ptr<pl::core::ast::ASTNode>> children;

        if (auto compoundStatement = dynamic_cast<pl::core::ast::ASTNodeCompoundStatement *>(node.get()); compoundStatement != nullptr) {
            children = compoundStatement->getStatements();
            for (auto child: children) {
                auto result = parseChildren(child, typeName);
                if (result.has_value())
                    return result;
            }
        } else if (auto typeDecl = dynamic_cast<pl::core::ast::ASTNodeTypeDecl *>(node.get());
               typeDecl != nullptr && typeDecl->getName() == typeName) {
            return typeDecl->getType();
        }
        return std::nullopt;
    }

    // If typeName is a UDT it returns the node.
    std::optional<std::shared_ptr<pl::core::ast::ASTNode>> ViewPatternEditor::TextHighlighter::findType(const std::string &typeName, IdentifierType &identifierType) {
        using Struct = pl::core::ast::ASTNodeStruct;
        using Union = pl::core::ast::ASTNodeUnion;
        using Enum = pl::core::ast::ASTNodeEnum;
        using Bitfield = pl::core::ast::ASTNodeBitfield;
        using TypeDecl = pl::core::ast::ASTNodeTypeDecl;

        if (typeName.empty())
            return std::nullopt;
        std::optional<std::shared_ptr<pl::core::ast::ASTNode>> node;

        if (m_types.contains(typeName))
            node = m_types[typeName]->getType();
        else
            return std::nullopt;

        if (auto structNode = dynamic_cast<Struct *>(node->get()); structNode != nullptr) {
            identifierType = IdentifierType::UDT;
            return node;
        } else if (auto unionNode = dynamic_cast<Union *>(node->get()); unionNode != nullptr) {
            identifierType = IdentifierType::UDT;
            return node;
        } else if (auto enumNode = dynamic_cast<Enum *>(node->get()); enumNode != nullptr) {
            identifierType = IdentifierType::UDT;
            return node;
        } else if (auto bitfieldNode = dynamic_cast<Bitfield *>(node->get()); bitfieldNode != nullptr) {
            identifierType = IdentifierType::UDT;
            return node;
        } else if (auto typeDeclNode = dynamic_cast<TypeDecl *>(node->get()); typeDeclNode != nullptr) {
            identifierType = IdentifierType::Typedef;
            return node;
        } else
            return std::nullopt;
    }

    // If memberName is a member of UDTName it returns true and sets the type of the member.
    bool ViewPatternEditor::TextHighlighter::isMemberOf(const std::string &memberName, const std::string &UDTName, IdentifierType &type) {
        using Struct = pl::core::ast::ASTNodeStruct;
        using Union = pl::core::ast::ASTNodeUnion;
        using Enum = pl::core::ast::ASTNodeEnum;
        using Bitfield = pl::core::ast::ASTNodeBitfield;
        IdentifierType identifierType;

        if (memberName.empty() || UDTName.empty())
            return false;

        auto optional = findType(UDTName, identifierType);
        if (!optional.has_value())
            return false;
        auto typeDecl = optional.value();

        if (auto structNode = dynamic_cast<Struct *>(typeDecl.get()); structNode != nullptr) {
            for (auto member: structNode->getMembers()) {

                if (auto variableDecl = dynamic_cast<pl::core::ast::ASTNodeVariableDecl *>(member.get());
                        variableDecl != nullptr && variableDecl->getName() == memberName) {
                    auto placementOffset = variableDecl->getPlacementOffset().get();

                    if (placementOffset != nullptr)
                        type = IdentifierType::PatternPlacedVariable;
                     else
                        type = IdentifierType::PatternVariable;
                    return true;
                }

                if (auto variableAssignment = dynamic_cast<pl::core::ast::ASTNodeLValueAssignment *>(member.get());
                        variableAssignment != nullptr &&
                        variableAssignment->getLValueName() == memberName) {
                    type = IdentifierType::PatternLocalVariable;
                    return true;
                }
            }
        } else if (auto unionNode = dynamic_cast<Union *>(typeDecl.get()); unionNode != nullptr) {
            for (auto member: unionNode->getMembers()) {

                if (auto variableDecl = dynamic_cast<pl::core::ast::ASTNodeVariableDecl *>(member.get());
                        variableDecl != nullptr && variableDecl->getName() == memberName) {
                    auto placementOffset = variableDecl->getPlacementOffset().get();

                    if (placementOffset != nullptr)
                        type = IdentifierType::PatternPlacedVariable;
                    else
                        type = IdentifierType::PatternVariable;
                    return true;
                }

                if (auto variableAssignment = dynamic_cast<pl::core::ast::ASTNodeLValueAssignment *>(member.get());
                        variableAssignment != nullptr &&
                        variableAssignment->getLValueName() == memberName) {
                    type = IdentifierType::PatternLocalVariable;
                    return true;
                }
            }
        } else if (auto enumNode = dynamic_cast<Enum *>(typeDecl.get()); enumNode != nullptr) {
            for (const auto &[name, offset]: enumNode->getEntries()) {

                if (name == memberName) {
                    type = IdentifierType::PatternVariable;
                    return true;
                }
            }
        } else if (auto bitfieldNode = dynamic_cast<Bitfield *>(typeDecl.get()); bitfieldNode != nullptr) {
            for (auto member: bitfieldNode->getEntries()) {

                if (auto variableDecl = dynamic_cast<pl::core::ast::ASTNodeVariableDecl *>(member.get());
                        variableDecl != nullptr && variableDecl->getName() == memberName) {
                    type = IdentifierType::PatternVariable;
                    return true;
                }
            }
        }
        return false;
    }

    using namespace pl::core;
    using Identifier = Token::Identifier;
    using Keyword = Token::Keyword;
    using Separator = Token::Separator;
    using Operator = Token::Operator;
    using Comment = Token::Comment;
    using DocComment = Token::DocComment;
    using Literal = Token::Literal;
    using ValueType = Token::ValueType;


    bool ViewPatternEditor::TextHighlighter::getIdentifierName(std::string &identifierName) {
        auto keyword = getValue<Keyword>(0);
        auto identifier = getValue<Identifier>(0);

        if (identifier != nullptr) {
            identifierName = identifier->get();
            return true;
        } else if (keyword != nullptr) {

            if (peek(tkn::Keyword::Parent)) {
                identifierName = "Parent";
                return true;
            }

            if (peek(tkn::Keyword::This)) {
                identifierName = "This";
                return true;
            }
        }
        return false;
    }

    // Returns a chain of identifiers like a.b.c or a::b::c
    bool ViewPatternEditor::TextHighlighter::getFullName(std::string &identifierName) {

        if (getTokenId(m_curr->location) < 1)
            return (getIdentifierName(identifierName));

        auto curr = m_curr;
        next(-1);
        identifierName.clear();
        rewindIdentifierName();
        std::vector<Identifier *> identifiers;

        if (!forwardIdentifierName(identifierName,curr, identifiers)) {
            m_curr = curr;
            return false;
        }

        m_curr = curr;
        return true;
    }

    // Rewind to the beginning of the identifier name when identifiers are separated by scope resolution or dot
    void ViewPatternEditor::TextHighlighter::rewindIdentifierName() {
        while (getTokenId(m_curr->location) > 2 &&
               (peek(tkn::Operator::ScopeResolution) || peek(tkn::Separator::Dot)) ) {

            if (peek(tkn::Separator::Dot)) {
                skipArray(200, false);
            }

            if (!peek(tkn::Literal::Identifier,-1) && !peek(tkn::Keyword::Parent,-1) && !peek(tkn::Keyword::This,-1))
                break;
            next(-2);
        }
        next();
    }

    // If optional curr is set, forwardIdentifierName will only parse until curr is reached, otherwise it will parse
    // until the end of the identifier name is reached.
    bool ViewPatternEditor::TextHighlighter::forwardIdentifierName(std::string &identifierName,std::optional<TokenIter> curr, std::vector<Identifier *> &identifiers) {
        bool useCurr = curr.has_value();
        auto tokenRef = Token(tkn::Keyword::Reference);
        auto tokenPointer = Token(tkn::Operator::Star);
        Identifier *identifier;
        TokenIter restoreCurr;

        if (useCurr)
            restoreCurr = curr.value();
        else
            restoreCurr = m_curr;

        std::string current;
        identifier = (Identifier *)getValue<Identifier>(0);
        identifiers.push_back(identifier);

        if (!getIdentifierName(current))
            return false;

        identifierName += current;
        bool whileLoop;

        if (useCurr)
            whileLoop = (m_curr != restoreCurr);
        skipArray(200,true);

        if (!useCurr)
            whileLoop = (peek(tkn::Operator::ScopeResolution,1) || peek(tkn::Separator::Dot,1));

        while (whileLoop) {
            next();

            if (peek(tkn::Operator::ScopeResolution))
                identifierName += "::";
            else if (peek(tkn::Separator::Dot))
                identifierName += ".";
            else {
                m_curr = restoreCurr;
                return false;
            }

            if (!useCurr || m_curr != restoreCurr)
                next();

            if (getIdentifierName(current)) {
                identifiers.push_back((Identifier *)getValue<Identifier>(0));
                identifierName += current;

                if (useCurr)
                    whileLoop = (m_curr != restoreCurr);
                skipArray(200,true);

                if (!useCurr)
                    whileLoop = (peek(tkn::Operator::ScopeResolution,1) || peek(tkn::Separator::Dot,1));
            } else {
                m_curr = restoreCurr;
                return false;
            }
        }
        m_curr = restoreCurr;
        return true;
    }

    // Adds namespace if it exists
    bool ViewPatternEditor::TextHighlighter::getQualifiedName(std::string &identifierName) {
        std::string shortName;
        std::string qualifiedName;

        if (!getFullName(identifierName))
            return false;

        std::string nameSpace;
        if (m_functionDefinitions.contains(identifierName) || m_UDTDefinitions.contains(identifierName))
            return true;

        for (auto [name,definition] : m_UDTDefinitions) {
            findNamespace(nameSpace, definition.tokenIndex);

            if (!nameSpace.empty() && !identifierName.contains(nameSpace)) {
                qualifiedName = nameSpace + "::" + identifierName;

                if (name == qualifiedName) {
                    identifierName = qualifiedName;
                    return true;
                }
            }

            if (name == identifierName) {
                identifierName = name;
                return true;
            }
        }

        if (identifierName.empty())
            return false;
        return true;
    }

    // Finds the token range of a function, namespace or UDT
    bool ViewPatternEditor::TextHighlighter::getTokenRange(std::vector<Token> keywords,ViewPatternEditor::TextHighlighter::UnorderedBlocks &tokenRange, ViewPatternEditor::TextHighlighter::OrderedBlocks &tokenRangeInv, bool fullName, bool invertMap, VariableScopes *blocks) {
        std::stack<i32> tokenStack;
        if (getTokenId(m_curr->location) < 1)
            return false;

        std::string name;
        if (fullName) {
            if (!getFullName(name))
                return false;
        } else {
            if (!getIdentifierName(name))
                return false;
            std::string nameSpace;
            std::string qualifiedName = name;
            findNamespace(nameSpace,getTokenId(m_curr->location));
            if (!nameSpace.empty())
                name = nameSpace + "::" + name;
        }

        i32 tokenCount = m_tokens.size();
        auto saveCurr = m_curr-1;
        skipTemplate(200);
        next();
        if (sequence(tkn::Operator::Colon)) {
            while (peek(tkn::Literal::Identifier)) {
                auto identifier = (Identifier *) getValue<Identifier>(0);
                if (identifier == nullptr)
                    break;
                auto identifierName = identifier->get();
                if (std::ranges::find(m_inheritances[name], identifierName) == m_inheritances[name].end())
                    m_inheritances[name].push_back(identifierName);
                skipTemplate(200);
                next(2);
            }
        }

        m_curr = saveCurr;
        i32 index1 = getTokenId(m_curr->location);
        bool result =  true;
        for (auto keyword : keywords)
            result = result && !peek(keyword);
        if (result)
            return false;
        u32 nestedLevel=0;
        next();
        auto limit = m_startToken + tokenCount;
        while (m_curr != limit) {

            if (sequence(tkn::Separator::LeftBrace)) {
                auto tokenId = getTokenId(m_curr[-1].location);
                tokenStack.push(tokenId);
                nestedLevel++;
            } else if (sequence(tkn::Separator::RightBrace)) {
                nestedLevel--;

                if (tokenStack.empty())
                    return false;
                Interval range(tokenStack.top(), getTokenId(m_curr[-1].location));
                tokenStack.pop();

                if (nestedLevel == 0) {
                    range.end -= 1;
                    if (blocks != nullptr)
                        blocks->operator[](name).insert(range);
                    skipAttribute();
                    break;
                }
                if (blocks != nullptr)
                    blocks->operator[](name).insert(range);
            } else if (sequence(tkn::Separator::EndOfProgram))
                return false;
            else
                next();
        }
        i32 index2 = getTokenId(m_curr->location);

        if (index2 > index1 && index2 < tokenCount) {

           if(invertMap) {
              tokenRangeInv[Interval(index1, index2)] = name;
           } else {
               tokenRange[name]=Interval(index1, index2);
           }
            if (blocks != nullptr)
                blocks->operator[](name).insert(Interval(index1,index2));
            return true;
        }
        return false;
    }

    // Searches through tokens and loads all the ranges of one kind. First namespaces are searched.
    void ViewPatternEditor::TextHighlighter::getAllTokenRanges(IdentifierType IdentifierTypeToSearch) {

        if (m_tokens.empty())
            return;

        std::string name;
        Identifier *identifier;
        IdentifierType identifierType;
        std::string UDTName;
        m_curr = m_startToken = m_originalPosition = m_partOriginalPosition = TokenIter(m_tokens.begin(), m_tokens.end());

        for (;!peek(tkn::Separator::EndOfProgram);next()) {
            auto curr = m_curr;

            if (peek(tkn::Literal::Identifier)) {
                identifier = (Identifier *) getValue<Identifier>(0);
                identifierType = identifier->getType();
                name = identifier->get();

                if (identifierType == IdentifierTypeToSearch) {
                    switch (identifierType) {
                        case IdentifierType::Function:          getTokenRange({tkn::Keyword::Function},m_functionTokenRange, m_namespaceTokenRange,false, false, &m_functionBlocks);
                            break;
                        case IdentifierType::NameSpace:
                            if (std::ranges::find(m_nameSpaces, name) == m_nameSpaces.end())
                                m_nameSpaces.push_back(name);
                                                                getTokenRange({tkn::Keyword::Namespace},m_functionTokenRange, m_namespaceTokenRange, true, true, nullptr);
                            break;
                        case IdentifierType::UDT:
                            if (std::ranges::find(m_UDTs, name) == m_UDTs.end())
                                m_UDTs.push_back(name);
                                                                getTokenRange({tkn::Keyword::Struct,tkn::Keyword::Union,tkn::Keyword::Enum,tkn::Keyword::Bitfield},m_UDTTokenRange,m_namespaceTokenRange, false, false, &m_UDTBlocks);
                            break;
                        case IdentifierType::Typedef:
                            if (std::ranges::find(m_typeDefs, name) == m_typeDefs.end())
                                m_typeDefs.push_back(name);
                            break;
                        case IdentifierType::TemplateArgument:  findScope(UDTName, m_UDTTokenRange);
                            break;
                        case IdentifierType::Attribute:         linkAttribute();
                            break;
                        case IdentifierType::GlobalVariable:
                        case IdentifierType::PlacedVariable:
                        case IdentifierType::PatternVariable:
                        case IdentifierType::PatternLocalVariable:
                        case IdentifierType::PatternPlacedVariable:
                        case IdentifierType::FunctionVariable:
                        case IdentifierType::FunctionParameter:
                        case IdentifierType::Macro:
                        case IdentifierType::Unknown:
                        case IdentifierType::FunctionUnknown:
                        case IdentifierType::MemberUnknown:
                        case IdentifierType::ScopeResolutionUnknown:
                            break;
                    }
                }
            }
            m_curr = curr;
        }
    }

    void ViewPatternEditor::TextHighlighter::skipDelimiters(i32 maxSkipCount, Token delimiter[2], i8 increment) {
        auto curr = m_curr;
        i32 skipCount = 0;
        i32 depth = 0;
        i32 tokenId = getTokenId(m_curr->location);

        if (tokenId == -1 || tokenId >= (i32) m_tokens.size())
            return;
        i32 skipCountLimit = increment > 0 ? std::min(maxSkipCount, (i32) m_tokens.size() - 1 - tokenId) : std::min(maxSkipCount, tokenId);
        next(increment);
        if (peek(delimiter[0])) {
            next(increment);
            while (skipCount < skipCountLimit) {
                if (peek(delimiter[1])) {
                    if (depth == 0)
                        return;
                    depth--;
                } else if (peek(delimiter[0]))
                    depth++;
                next(increment);
                skipCount++;
            }
        }
        m_curr = curr;
    }

    void ViewPatternEditor::TextHighlighter::skipTemplate(i32 maxSkipCount, bool forward) {
        Token delimiters[2];
        if (forward) {
            delimiters[0] = Token(tkn::Operator::BoolLessThan);
            delimiters[1] = Token(tkn::Operator::BoolGreaterThan);
        } else {
            delimiters[0] = Token(tkn::Operator::BoolGreaterThan);
            delimiters[1] = Token(tkn::Operator::BoolLessThan);
        }
        skipDelimiters(maxSkipCount, delimiters, forward ? 1 : -1);
    }

    void ViewPatternEditor::TextHighlighter::skipArray(i32 maxSkipCount, bool forward) {
        Token delimiters[2];
        if (forward) {
            delimiters[0] = Token(tkn::Separator::LeftBracket);
            delimiters[1] = Token(tkn::Separator::RightBracket);
        } else {
            delimiters[0] = Token(tkn::Separator::RightBracket);
            delimiters[1] = Token(tkn::Separator::LeftBracket);
        }
        skipDelimiters(maxSkipCount, delimiters, forward ? 1 : -1);
    }

    // Used to skip references,pointers,...
    void ViewPatternEditor::TextHighlighter::skipToken(Token token, i8 step) {
        if (peek(token,step))
            next(step);
    }

    void ViewPatternEditor::TextHighlighter::skipAttribute() {

        if (sequence(tkn::Separator::LeftBracket,tkn::Separator::LeftBracket)) {
            while (!sequence(tkn::Separator::RightBracket,tkn::Separator::RightBracket))
                next();
        }
    }

    // Takes an identifier chain resolves the type of the end from the rest iteratively.
    bool ViewPatternEditor::TextHighlighter::resolveIdentifierType(Definition &result) {
        auto curr = m_curr + 1;
        Identifier *identifier;
        next(-1);
        rewindIdentifierName();

        std::string nameSpace;
        std::string currentName;
        std::string variableParentType;

        if (identifier = (Identifier *) getValue<Identifier>(0); identifier != nullptr)
            currentName = identifier->get();
        skipArray(200, true);

        if (peek(tkn::Separator::Dot, 1))
            variableParentType = findIdentifierTypeStr(currentName);

        while (peek(tkn::Separator::Dot, 1) || peek(tkn::Operator::ScopeResolution, 1)) {

            if (peek(tkn::Separator::Dot, 1)) {
                next(2);

                if (identifier = (Identifier *) getValue<Identifier>(0); identifier != nullptr)
                    currentName = identifier->get();
                skipArray(200, true);

                if ((m_curr + 1) == curr)
                    break;

                if (peek(tkn::Separator::Dot, 1))
                    variableParentType = findIdentifierTypeStr(currentName, variableParentType);
            } else if (peek(tkn::Operator::ScopeResolution, 1)) {

                if (std::ranges::find(m_nameSpaces, currentName) != m_nameSpaces.end()) {
                    nameSpace += currentName + "::";
                    next(2);

                    if (identifier = (Identifier *) getValue<Identifier>(0); identifier != nullptr)
                        variableParentType = identifier->get();

                    if (peek(tkn::Operator::ScopeResolution, 1))
                        currentName = variableParentType;
                } else if (std::ranges::find(m_UDTs, currentName) != m_UDTs.end()) {
                    next(2);
                    variableParentType = currentName;

                    if (!nameSpace.empty() && !variableParentType.contains(nameSpace))
                        variableParentType = nameSpace + variableParentType;
                    else if (findNamespace(nameSpace) && !variableParentType.contains(nameSpace))
                        variableParentType = nameSpace + "::" + variableParentType;

                    if (identifier = (Identifier *) getValue<Identifier>(0); identifier != nullptr)
                        currentName = identifier->get();

                    if ((m_curr + 1) == curr)
                        break;

                    if (peek(tkn::Operator::ScopeResolution, 1))
                        variableParentType = findIdentifierTypeStr(currentName, variableParentType);
                } else
                    next();
            }
        }

        if (findIdentifierDefinition(result, currentName, variableParentType)) {
            m_curr = curr - 1;
            return true;
        } else if (m_types.contains(variableParentType) && identifier != nullptr) {
            IdentifierType type;
            if (isMemberOf(currentName, variableParentType, type)) {
                result.idType = type;
                result.typeStr = variableParentType;
                m_curr = curr - 1;
                return true;
            }
        }

        m_curr = curr - 1;
        return false;
    }

    // If contex then find it otherwise check if it belongs in map
    bool ViewPatternEditor::TextHighlighter::findOrContains(std::string &context, UnorderedBlocks tokenRange, VariableMap variableMap) {
        if (context.empty())
            return findScope(context, tokenRange);
        else
            return variableMap.contains(context);
    }

    bool ViewPatternEditor::TextHighlighter::findIdentifierDefinition(Definition &result, const std::string &optionalIdentifierName, std::string optionalName) {
        auto curr = m_curr;
        Scopes blocks;
        auto tokenId = getTokenId(m_curr->location);
        std::vector<Definition> definitions;
        std::string name = optionalName;
        result.idType = IdentifierType::Unknown;
        std::string identifierName = optionalIdentifierName;
        if (optionalIdentifierName == "")
            getFullName(identifierName);

        if (findOrContains(name,m_UDTTokenRange,m_UDTVariables) && m_UDTVariables[name].contains(identifierName)) {
            definitions = m_UDTVariables[name][identifierName];
            blocks = m_UDTBlocks[name];
        } else if (findOrContains(name,m_functionTokenRange,m_functionVariables) && m_functionVariables[name].contains(identifierName)) {
            definitions = m_functionVariables[name][identifierName];
            blocks = m_functionBlocks[name];
        } else if (m_globalVariables.contains(identifierName)) {
            definitions = m_globalVariables[identifierName];
            blocks = m_globalBlocks;
        }

        for (auto block: blocks) {
            for (auto definition: definitions) {
                if (block.contains(definition.tokenIndex) && block.contains(tokenId)) {
                    result = definition;
                    m_curr = curr;
                    return true;
                } else if (result.idType < definition.idType)
                    result = definition;
            }
        }

        if (result.idType != IdentifierType::Unknown) {
            m_curr = curr;
            return true;
        }

        m_curr = curr;
        return false;
    }

    bool ViewPatternEditor::TextHighlighter::findIdentifierType(IdentifierType &type) {
        type = IdentifierType::Unknown;
        auto identifier = (Identifier *) getValue<Identifier>(0);
        Definition result;

        if (identifier == nullptr)
            return false;
        std::string identifierName = identifier->get();
        std::string fullName;

        if (!getQualifiedName(fullName))
            return false;
        std::vector<std::string> vectorString;
        std::string shortName;
        std::string separator;

        if (peek(tkn::Separator::Dot, -1))
            separator = ".";
        else if (peek(tkn::Operator::ScopeResolution, -1))
            separator = "::";

        if ( findType(fullName, type).has_value()) {
            identifier->setType(type, true);
            return true;
        }

        if (separator.empty()) {

            if (findIdentifierDefinition(result, identifierName))
                type = result.idType;

            if (type != IdentifierType::Unknown) {
                identifier->setType(type, true);
                return true;
            }
            return false;
        }

        if (resolveIdentifierType(result)) {
            type = result.idType;
            identifier->setType(result.idType, true);
            return true;
        }

        vectorString = hex::splitString(fullName, separator);
        if (vectorString.back() == identifierName && vectorString.size() > 1) {
            vectorString.pop_back();
            shortName = vectorString.back();
            fullName = hex::combineStrings(vectorString, separator);

            if (findIdentifierDefinition(result, fullName))
                type = result.idType;
            else if (findIdentifierDefinition(result, shortName)) {
                fullName = result.typeStr;

                if (findIdentifierDefinition(result, fullName))
                    type = result.idType;
            }

            if (type != IdentifierType::Unknown || isMemberOf(identifierName, fullName, type)) {
                identifier->setType(type, true);
                return true;
            }
        }

        return false;
    }

    using Definition = ViewPatternEditor::TextHighlighter::Definition;
    bool ViewPatternEditor::TextHighlighter::findScopeResolutionType(IdentifierType &type) {

        auto identifier = (Identifier *) getValue<Identifier>(0);
        if (identifier == nullptr)
            return false;

        std::string variableName;
        if (!getIdentifierName(variableName))
            return false;

        if (std::ranges::find(m_nameSpaces, variableName) != m_nameSpaces.end()) {
            type = IdentifierType::NameSpace;
            identifier->setType(type,true);
            return true;
        }

        if (std::ranges::find(m_UDTs, variableName) != m_UDTs.end()) {
            type = IdentifierType::UDT;
            identifier->setType(type,true);
            return true;
        }

        u32 currentLine = m_curr->location.line-1;
        u32 startingLineTokenIndex = m_firstTokenIdOfLine[currentLine];
        if (startingLineTokenIndex == 0xFFFFFFFFu || startingLineTokenIndex > m_tokens.size())
            return false;

        if (auto *keyword = std::get_if<Keyword>(&m_tokens[startingLineTokenIndex].value);
            keyword != nullptr && *keyword == Keyword::Import) {
            type = IdentifierType::NameSpace;
            identifier->setType(type,true);
            return true;
        }

        i32 index = getTokenId(m_curr->location);
        if (index < (i32)m_tokens.size() - 1 && index > 2) {
            auto nextToken = m_curr[1];
            auto *separator = std::get_if<Token::Separator>(&nextToken.value);
            auto *operatortk = std::get_if<Token::Operator>(&nextToken.value);
            if ((separator != nullptr && *separator == Separator::Semicolon) ||
                (operatortk != nullptr && *operatortk == Operator::BoolLessThan)) {
                auto previousToken = m_curr[-1];
                auto prevprevToken = m_curr[-2];
                operatortk = std::get_if<Operator>(&previousToken.value);
                auto *identifier2 = std::get_if<Identifier>(&prevprevToken.value);
                if (operatortk != nullptr && identifier2 != nullptr && *operatortk == Operator::ScopeResolution) {
                    if (identifier2->getType() == IdentifierType::UDT) {
                        type = IdentifierType::PatternLocalVariable;
                        identifier->setType(type,true);
                        return true;
                    } else if (identifier2->getType() == IdentifierType::NameSpace) {
                        type = IdentifierType::UDT;
                        identifier->setType(type,true);
                        return true;
                    }
                }
            }
        }
        return false;
    }

    // finds the name of the tpken range that the given or the current token index is in.
    bool ViewPatternEditor::TextHighlighter::findScope(std::string &name, const UnorderedBlocks &map, i32 optionalTokenId) {
        auto tokenId = optionalTokenId == -1 ? getTokenId(m_curr->location) : optionalTokenId;
        for (auto [scopeName,range] : map) {
            if (range.contains(tokenId)) {
                name = scopeName;
                return true;
            }
        }
        return false;
    }


    // Finds the namespace of the given or the current token index.
    bool ViewPatternEditor::TextHighlighter::findNamespace(std::string &nameSpace, i32 optionalTokenId) {
        nameSpace = "";

        for (auto [interval,name] : m_namespaceTokenRange) {
            i32 tokenId = optionalTokenId == -1 ? getTokenId(m_curr->location) : optionalTokenId;

            if (tokenId > interval.start && tokenId < interval.end) {
                if (nameSpace == "")
                    nameSpace = name;
                else
                    nameSpace = name + "::" + nameSpace;
            }
        }
        if (nameSpace != "")
            return true;
        return false;
    }

    //The context is the name of the function or UDT that the variable is in.
    std::string ViewPatternEditor::TextHighlighter::findIdentifierTypeStr(const std::string &identifierName, std::string context) {
        Definition result;
        findIdentifierDefinition(result, identifierName, context);
        return result.typeStr;
    }

    // Creates a map from the attribute function to the type of the argument it takes.
    void ViewPatternEditor::TextHighlighter::linkAttribute() {
        auto curr = m_curr;
     //   while (!peek(tkn::Separator::LeftParenthesis) && !peek(tkn::Separator::Semicolon))
     //       next();
      //  next(-1);
        bool qualifiedAttribute = false;
        while (sequence(tkn::Literal::Identifier,tkn::Operator::ScopeResolution))
            qualifiedAttribute = true;

        if (qualifiedAttribute) {
            auto identifier = (Identifier *)getValue<Identifier>(0);
            if (identifier != nullptr)
                identifier->setType(IdentifierType::Attribute, true);
            m_curr = curr;
            identifier = (Identifier *)getValue<Identifier>(0);
            if (identifier != nullptr)
                identifier->setType(IdentifierType::NameSpace, true);
        } else
            m_curr = curr;

        std::string functionName;
        next();

        if (sequence(tkn::Separator::LeftParenthesis,tkn::Literal::String)) {

            functionName = getValue<Literal>(-1)->toString(false);
            if (!functionName.contains("::")) {

                std::string namespaceName;
                if (findNamespace(namespaceName))
                    functionName = namespaceName + "::" + functionName;
            } else{

                auto vectorString = hex::splitString(functionName, "::");
                vectorString.pop_back();
                for (auto nameSpace : vectorString) {
                    if (std::ranges::find(m_nameSpaces, nameSpace) == m_nameSpaces.end())
                        m_nameSpaces.push_back(nameSpace);
                }
            }
        } else
            return;

        u32 line = m_curr->location.line;
        i32 tokenIndex;

        while (!peek(tkn::Separator::Semicolon,-1)){
            if (line = previousLine(line); line > m_firstTokenIdOfLine.size())
                return;

            if (tokenIndex = m_firstTokenIdOfLine[line]; !isTokenIdValid(tokenIndex))
                return;

            restart();
            next(tokenIndex);
            while(peek(tkn::Literal::Comment,-1) || peek(tkn::Literal::DocComment,-1))
                next(-1);
        }

        while(peek(tkn::Literal::Comment) || peek(tkn::Literal::DocComment))
            next();

        Identifier *identifier;
        std::string UDTName;
        while (sequence(tkn::Literal::Identifier,tkn::Operator::ScopeResolution)) {
            identifier = (Identifier *) getValue<Identifier>(-2);
            UDTName += identifier->get() + "::";
        }

        if (sequence(tkn::Literal::Identifier)) {
            identifier = (Identifier *) getValue<Identifier>(-1);
            UDTName += identifier->get();
            if (!UDTName.contains("::")) {
                std::string namespaceName;
                if (findNamespace(namespaceName))
                    UDTName = namespaceName + "::" + UDTName;
            }
            if (m_types.contains(UDTName))
                m_attributeFunctionArgumentType[functionName] = UDTName;
        } else if (sequence(tkn::ValueType::Any)) {
            auto valueType = getValue<ValueType>(-1);
            m_attributeFunctionArgumentType[functionName] = Token::getTypeName(*valueType);
        } else {
            if (findScope(UDTName, m_UDTTokenRange) && !UDTName.empty())
                m_attributeFunctionArgumentType[functionName] = UDTName;
        }
    }

    // This function assumes that the first variable in the link that concatenates sequences including the Parent keyword started with Parent and was removed. Uses a function to find
    // all the parents of a variable, If there are subsequent elements in the link that are Parent then for each parent it finds all the grandparents and puts them in a vector called
    // parentTypes. It stops when a link that's not Parent is found amd only returns the last generation of parents.
     bool ViewPatternEditor::TextHighlighter::findAllParentTypes(std::vector<std::string> &parentTypes, std::vector<Identifier *> &identifiers, std::string &optionalFullName) {
        auto fullName = optionalFullName;

        if (optionalFullName.empty())
            forwardIdentifierName(fullName, std::nullopt, (std::vector<Identifier *> &) identifiers);

        auto nameParts = hex::splitString(fullName, ".");
        std::vector<std::string> grandpaTypes;
        findParentTypes(parentTypes);

        if (parentTypes.empty())
            return false;

        auto currentName=nameParts[0];
        nameParts.erase(nameParts.begin());
        auto identifier = identifiers[0];
        identifiers.erase(identifiers.begin());

        while (currentName == "Parent" && !nameParts.empty()) {
            for (auto parentType: parentTypes)
                findParentTypes(grandpaTypes, parentType);

            currentName = nameParts[0];
            nameParts.erase(nameParts.begin());
            identifier = identifiers[0];
            identifiers.erase(identifiers.begin());
            parentTypes = grandpaTypes;
            grandpaTypes.clear();
        }

        nameParts.insert(nameParts.begin(),currentName);
        identifiers.insert(identifiers.begin(),identifier);
        optionalFullName = hex::combineStrings(nameParts, ".");

        return true;
    }

    // Searches for parents through every custom type,i.e. for structs that have members
    // of the same type as the one being searched and places them in a vector called parentTypes.
    bool ViewPatternEditor::TextHighlighter::findParentTypes(std::vector<std::string> &parentTypes,const std::string &optionalUDTName) {
        std::string UDTName;

        if (optionalUDTName.empty()) {
            if (!findScope(UDTName, m_UDTTokenRange))
                return false;
        } else
            UDTName = optionalUDTName;

        bool found = false;
        for (auto [name,variables] : m_UDTVariables) {
            for (auto [variableName,definitions] : variables) {
                for (auto definition : definitions) {
                    if (definition.typeStr == UDTName) {
                        if (std::ranges::find(parentTypes, name) == parentTypes.end()) {
                            parentTypes.push_back(name);
                            found = true;
                        }
                    }
                }
            }
        }
        return found;
    }

    // this function searches all the parents recursively until it can match the variable name at the end of the chain
    // and selects its type to colour the variable because the search only occurs pn type declarations which we know
    // the types of. Once the end link is found then all the previous links are also assigned the types that were found
    // for them during the search.
    bool ViewPatternEditor::TextHighlighter::tryParentType(  const std::string &parentType, std::string &variableName,
                                                             std::optional<Definition> &result, std::vector<Identifier *> &identifiers) {

        auto vectorString = hex::splitString(variableName, ".");
        auto count = vectorString.size();
        Identifier *identifier = identifiers[0];
        auto UDTName=parentType;
        auto currentName = vectorString[0];

        if (m_UDTVariables.contains(UDTName) && m_UDTVariables[UDTName].contains(currentName)) {
            auto definitions = m_UDTVariables[UDTName][currentName];
            for (auto definition : definitions) {
                UDTName = definition.typeStr;

                if (count == 1) {
                    identifier->setType(definition.idType, true);
                    result = definition;
                    return true;
                }

                vectorString.erase(vectorString.begin());
                variableName = hex::combineStrings(vectorString, ".");
                identifier = identifiers[0];
                identifiers.erase(identifiers.begin());
                if (tryParentType(UDTName, variableName, result,identifiers) ) {
                    identifier->setType(definition.idType, true);
                    return true;
                }

                identifiers.insert(identifiers.begin(),identifier);
                variableName += "." + currentName;
                return false;
            }
            return false;
        } else
            return false;
        return false;
    }

    // Handles Parent keyword.
    std::optional<Definition> ViewPatternEditor::TextHighlighter::setChildrenTypes(std::string &optionalFullName, std::vector<Identifier *> &optionalIdentifiers) {
        auto curr = m_curr;
        std::string fullName;
        std::vector<Identifier *> identifiers;
        std::vector<Definition> definitions;
        std::optional<Definition> result;

        if (!optionalFullName.empty()) {
            fullName = optionalFullName;
            identifiers = optionalIdentifiers;
        } else
            forwardIdentifierName(fullName,std::nullopt,identifiers);

        std::vector<std::string> parentTypes;
        auto vectorString = hex::splitString(fullName, ".");
        if (vectorString[0] == "Parent") {
            vectorString.erase(vectorString.begin());
            fullName = hex::combineStrings(vectorString, ".");
            identifiers.erase(identifiers.begin());
            if (!findAllParentTypes(parentTypes, identifiers, fullName)) {
                m_curr = curr;
                return std::nullopt;
            }
        } else
             return std::nullopt;

        for (auto parentType : parentTypes) {
            if (tryParentType(parentType, fullName, result, identifiers)) {
                if (result.has_value())
                    definitions.push_back(result.value());
            } else {
                m_curr = curr;
                return std::nullopt;
            }
        }
        // Todo: Are all definitions supposed to be the same? If not, which one should be used?
        // for now, use the first one.
        if (!definitions.empty())
            result = definitions[0];
        m_curr = curr;
        return result;
    }

    // Third paletteIndex called from processLineTokens to process variable identifiers that were
    // labeled as unknown.
    TextEditor::PaletteIndex ViewPatternEditor::TextHighlighter::getPaletteIndex(IdentifierType type) {
        switch (type) {
            case IdentifierType::NameSpace:             return TextEditor::PaletteIndex::NameSpace;
            case IdentifierType::UDT:                   return TextEditor::PaletteIndex::UserDefinedType;
            case IdentifierType::FunctionVariable:      return TextEditor::PaletteIndex::FunctionVariable;
            case IdentifierType::FunctionParameter:     return TextEditor::PaletteIndex::FunctionParameter;
            case IdentifierType::PatternLocalVariable:  return TextEditor::PaletteIndex::PatternLocalVariable;
            case IdentifierType::PatternVariable:       return TextEditor::PaletteIndex::PatternVariable;
            case IdentifierType::PatternPlacedVariable: return TextEditor::PaletteIndex::PatternPlacedVariable;
            case IdentifierType::TemplateArgument:      return TextEditor::PaletteIndex::TemplateArgument;
            case IdentifierType::GlobalVariable:        return TextEditor::PaletteIndex::GlobalVariable;
            case IdentifierType::PlacedVariable:        return TextEditor::PaletteIndex::PlacedVariable;
            case IdentifierType::Typedef:               return TextEditor::PaletteIndex::TypeDef;
            default:                                    return TextEditor::PaletteIndex::UnkIdentifier;
        }
    }

    // Second paletteIndex called from processLineTokens to process literals
    TextEditor::PaletteIndex ViewPatternEditor::TextHighlighter::getPaletteIndex(Literal *literal) {

        if (literal->isFloatingPoint() || literal->isSigned() || literal->isUnsigned()) return TextEditor::PaletteIndex::NumericLiteral;
        else if (literal->isCharacter() || literal->isBoolean())                        return TextEditor::PaletteIndex::CharLiteral;
        else if (literal->isString())                                                   return TextEditor::PaletteIndex::StringLiteral;
        else                                                                            return TextEditor::PaletteIndex::UnkIdentifier;
    }

    // Second paletteIndex called from processLineTokens to process identifiers
    TextEditor::PaletteIndex ViewPatternEditor::TextHighlighter::getPaletteIndex(Identifier *identifier) {
        IdentifierType identifierType = identifier->getType();
        std::string functionName;
        switch (identifierType) {
            case IdentifierType::Macro:                 return TextEditor::PaletteIndex::PreprocIdentifier;
            case IdentifierType::Attribute:             return TextEditor::PaletteIndex::Attribute;
            case IdentifierType::Function:              return TextEditor::PaletteIndex::Function;
            case IdentifierType::NameSpace:             return TextEditor::PaletteIndex::NameSpace;
            case IdentifierType::UDT:
                if (std::ranges::find(m_typeDefs,identifier->get()) != m_typeDefs.end()) {
                    identifier->setType(IdentifierType::Typedef,true);
                    return TextEditor::PaletteIndex::TypeDef;
                }
                                                        return TextEditor::PaletteIndex::UserDefinedType;
            case IdentifierType::FunctionVariable:      return TextEditor::PaletteIndex::FunctionVariable;
            case IdentifierType::FunctionParameter:     return TextEditor::PaletteIndex::FunctionParameter;
            case IdentifierType::PatternLocalVariable:
                if (peek(tkn::Separator::Comma,1)) {
                    identifier->setType(IdentifierType::PatternVariable, true);
                    return TextEditor::PaletteIndex::PatternVariable;
                }
                                                        return TextEditor::PaletteIndex::PatternLocalVariable;
            case IdentifierType::PatternVariable:       return TextEditor::PaletteIndex::PatternVariable;
            case IdentifierType::PatternPlacedVariable: return TextEditor::PaletteIndex::PatternPlacedVariable;
            case IdentifierType::TemplateArgument:      return TextEditor::PaletteIndex::TemplateArgument;
            case IdentifierType::GlobalVariable:        return TextEditor::PaletteIndex::GlobalVariable;
            case IdentifierType::PlacedVariable:        return TextEditor::PaletteIndex::PlacedVariable;
            case IdentifierType::Typedef:               return TextEditor::PaletteIndex::TypeDef;
            case IdentifierType::FunctionUnknown:
            case IdentifierType::MemberUnknown:
            case IdentifierType::ScopeResolutionUnknown:
            case IdentifierType::Unknown:
                if (findIdentifierType(identifierType))
                    return getPaletteIndex(identifierType);
                if (findScopeResolutionType(identifierType))
                    return getPaletteIndex(identifierType);
                                                        return TextEditor::PaletteIndex::UnkIdentifier;
            default:                                    return TextEditor::PaletteIndex::UnkIdentifier;
        }
    }

    // First paletteIndex called from processLineTokens
    TextEditor::PaletteIndex ViewPatternEditor::TextHighlighter::getPaletteIndex() {
        auto &token = m_curr[0];
        TextEditor::PatternLanguageTypes type;
        Comment *comment;
        DocComment *docComment;
        Identifier *identifier;
        Literal *literal;
        std::vector<Identifier *> identifiers;
        std::string fullName;
        switch (token.type) {
            using enum TextEditor::PatternLanguageTypes;
            case Token::Type::Keyword:
                if (peek(tkn::Keyword::Parent,0))
                    setChildrenTypes(fullName,identifiers);
                type = Keyword;
                break;
            case Token::Type::ValueType: type = ValueType;
                break;
            case Token::Type::Operator: type = Operator;
                break;
            case Token::Type::Integer:
                if (literal = (Literal *)std::get_if<Literal>(&token.value); literal != nullptr)
                    return getPaletteIndex(literal);
                type = Integer;
                break;
            case Token::Type::String:   type = String;
                break;
            case Token::Type::Identifier:
                if (identifier =(Token::Identifier *) std::get_if<Token::Identifier>(&token.value); identifier != nullptr)
                    return getPaletteIndex(identifier);
                type = Identifier;
                break;
            case Token::Type::Separator:    type = Separator;
                break;
            case Token::Type::Comment:
                if (comment =(Token::Comment *) std::get_if<Token::Comment>(&token.value); comment != nullptr)
                    type = comment->singleLine ? Comment : BlockComment;
                else
                    type = Comment;
                break;
            case Token::Type::DocComment:
                if (docComment = (Token::DocComment *)std::get_if<Token::DocComment>(&token.value); docComment != nullptr)
                    type = docComment->singleLine ? DocComment : docComment->global ? DocGlobalComment : DocBlockComment;
                else
                    type = DocComment;
                break;
            case Token::Type::Directive:    type = Directive;
                break;
            default:
                type = Default;
        }
        return m_viewPatternEditor.m_textEditor.getPaletteIndex<TextEditor::PatternLanguageTypes>(type);
    }

    // Render the compilation errors using squiggly lines
    void ViewPatternEditor::TextHighlighter::renderErrors() {
        const auto processMessage = [](const auto &message) {
            auto lines = wolv::util::splitString(message, "\n");

            std::ranges::transform(lines, lines.begin(), [](auto line) {
                if (line.size() >= 128)
                    line = wolv::util::trim(line);

                return hex::limitStringLength(line, 128);
            });

            return wolv::util::combineStrings(lines, "\n");
        };
        TextEditor::ErrorMarkers errorMarkers;

        if (!m_compileErrors.empty()) {
            for (const auto &error : m_compileErrors) {
                auto key = TextEditor::Coordinates(error.getLocation().line, error.getLocation().column);
                if (!errorMarkers.contains(key) || errorMarkers[key].first < error.getLocation().length)
                    errorMarkers[key] = std::make_pair(error.getLocation().length, processMessage(error.getMessage()));
            }
        }
        m_viewPatternEditor.m_textEditor.SetErrorMarkers(errorMarkers);
    }

    // creates a map from variable names to a vector of token indices
    // od every instance of the variable name in the code.
    void ViewPatternEditor::TextHighlighter::loadInstances() {

        m_startToken = m_originalPosition = m_partOriginalPosition = TokenIter(m_tokens.begin(), m_tokens.end());
        for (m_curr = m_startToken; !peek(tkn::Separator::EndOfProgram); next()) {

            if (peek(pl::core::tkn::Literal::Identifier)) {
                std::string name;
                if (!getQualifiedName(name))
                    continue;
                auto id = getTokenId(m_curr->location);
                auto &instances = m_instances[name];
                if (std::ranges::find(instances, id) == instances.end())
                    instances.push_back(id);
            }
        }
    }

    // Get the location of a given token index
    pl::core::Location ViewPatternEditor::TextHighlighter::getLocation(i32 tokenId) {
        pl::core::Location location;
        if (tokenId >= (i32)m_tokens.size())
            return location;
        return m_tokens[tokenId].location;
    }

    // Get the token index for a given location.
    i32 ViewPatternEditor::TextHighlighter::getTokenId(pl::core::Location location) {
        if (location.source->source != "<Source Code>")
            return -1;
        auto line1 = location.line-1;
        auto line2 = nextLine(line1);
        i32 tokenStart = m_firstTokenIdOfLine[line1];
        i32 tokenEnd = m_firstTokenIdOfLine[line2]-1;

        if (tokenEnd >= (i32)m_tokens.size())
            tokenEnd = m_tokens.size()-1;

        if (tokenStart == -1 || tokenEnd == -1 || tokenStart >= (i32)m_tokens.size())
            return -1;

        for (i32 i = tokenStart; i <= tokenEnd; i++) {
            if (m_tokens[i].location.column >= location.column)
                return i;
        }
        return -1;
    }

    // Transfer identifier types from the parsed tokens to the un-preprocessed tokens. Token indices won't match,
    // but the locations will.
    void ViewPatternEditor::TextHighlighter::getIdentifierTypes(const std::optional<std::vector<pl::core::Token>> &tokens) {
        using IdentifierType::Unknown;
        using IdentifierType::MemberUnknown;
        using IdentifierType::FunctionUnknown;
        using IdentifierType::ScopeResolutionUnknown;

        if (!tokens.has_value())
            return;

        std::map<std::string,IdentifierType> includedNames;
        std::map<std::string,IdentifierType> sourceNames;

        m_curr = m_startToken = m_originalPosition = m_partOriginalPosition = TokenIter(tokens.value().begin(), tokens.value().end());

        for (auto curr = m_curr;!peek(tkn::Separator::EndOfProgram); next()) {
            if (auto tokenIdentifier = std::get_if<Identifier>(&m_curr->value); tokenIdentifier != nullptr) {

                if (auto type = getValue<Identifier>(0)->getType(); type != Unknown && type != MemberUnknown && type != FunctionUnknown && type != ScopeResolutionUnknown) {
                    auto name = getValue<Identifier>(0)->get();
                    i32 tokenId = getTokenId(m_curr->location);

                    if (tokenId == -1) {

                        if (type == IdentifierType::NameSpace && (std::ranges::find(m_nameSpaces, name) == m_nameSpaces.end()))
                            m_nameSpaces.push_back(name);

                        if (!includedNames.contains(name))
                            includedNames[name] = type;

                    } else if (auto m_tokensIdentifier = std::get_if<Identifier>(&m_tokens[tokenId].value); m_tokensIdentifier != nullptr) {

                        m_tokensIdentifier->setType(type, true);
                        if (!sourceNames.contains(name))
                            sourceNames[name] = type;
                    }
                }
            } else if (auto keyword = std::get_if<Keyword>(&m_curr->value); keyword != nullptr && *keyword == Keyword::Namespace) {
                curr = m_curr;
                next();
                auto namespaceIdentifier = std::get_if<Identifier>(&m_curr->value);
                while (namespaceIdentifier != nullptr) {
                    auto name = namespaceIdentifier->get();

                    if (std::ranges::find(m_nameSpaces, name) == m_nameSpaces.end())
                        m_nameSpaces.push_back(name);
                    next();

                    if (auto operatortk = std::get_if<Operator>(&m_curr->value); operatortk == nullptr || *operatortk != Operator::ScopeResolution)
                        break;
                    next();
                    namespaceIdentifier = std::get_if<Identifier>(&m_curr->value);
                }
                m_curr = curr;
            }
        }

        for (auto [name,type] : includedNames) {
            if (!sourceNames.contains(name) && m_instances.contains(name)) {
                auto &instances = m_instances[name];
                for (auto instance : instances) {

                    if (auto identifier = std::get_if<Identifier>(&m_tokens[instance].value); identifier != nullptr) {

                        if (identifier->getType() == Unknown || identifier->getType() == MemberUnknown || identifier->getType() == FunctionUnknown || identifier->getType() == ScopeResolutionUnknown)
                            identifier->setType(type, true);
                    }
                }
            }
        }
    }

    void ViewPatternEditor::TextHighlighter::appendInheritances() {
        for (auto [name,inheritances] : m_inheritances) {
            for (auto inheritance : inheritances) {
                auto definitions = m_UDTVariables[inheritance];
                for (auto [variableName,variableDefinitions] : definitions) {
                    for (auto variableDefinition : variableDefinitions)
                        m_UDTVariables[name][variableName].push_back(variableDefinition);
                }
            }
        }
    }

    // Get the string of the argument type. This works on function arguments and user defined template arguments.
    std::string ViewPatternEditor::TextHighlighter::getArgumentTypeName( i32 rangeStart, Token delimiter2) {
        auto curr = m_curr;
        auto tokenRef = Token(tkn::Keyword::Reference);
        i32 parameterIndex = getArgumentNumber(rangeStart, getTokenId(m_curr->location));
        Token delimiter;
        std::string typeStr;

        if (parameterIndex > 0)
            delimiter = tkn::Separator::Comma;
        else
            delimiter = delimiter2;

        while (!peek(delimiter))
            next(-1);
        skipToken(tokenRef);
        next();
        if (peek(tkn::ValueType::Any))
            typeStr = Token::getTypeName(*getValue<Token::ValueType>(0));
        else if (peek(tkn::Literal::Identifier))
            typeStr = getValue<Token::Identifier>(0)->get();

        m_curr = curr;
        return typeStr;
    }

    bool ViewPatternEditor::TextHighlighter::isTokenIdValid(i32 tokenId) {
        return tokenId >= 0 && tokenId < (i32)m_tokens.size();
    }

    // Find the string of the variable type. This works on function
    // variables, local, placed and regular pattern variables.
    std::string ViewPatternEditor::TextHighlighter::getVariableTypeName() {
        auto curr = m_curr;
        auto tokenPointer = Token(tkn::Operator::Star);
        auto varTokenId = getTokenId(m_curr->location);

        if (!isTokenIdValid(varTokenId) )
            return "";

        std::string typeStr;
        restart();
        next(varTokenId);
        skipToken(tokenPointer,-1);

        while (peek(tkn::Separator::Comma,-1))
            next(-2);

        if (peek(tkn::ValueType::Any,-1))
            typeStr = Token::getTypeName(*getValue<Token::ValueType>(-1));
        else {
            skipTemplate(200, false);
            next(-1);
            if (peek(tkn::Literal::Identifier)) {
                typeStr = getValue<Token::Identifier>(0)->get();
                next(-1);
            }
            std::string nameSpace;
            while (peek(tkn::Operator::ScopeResolution)) {
                next(-1);
                nameSpace = getValue<Token::Identifier>(0)->get() + "::" + nameSpace;
                next(-1);
            }
            typeStr = nameSpace + typeStr;

            if (m_types.contains(typeStr)) {
                m_curr = curr;
                return typeStr;
            }
            std::vector<std::string> candidates;
            for (auto [name,typeDecl] : m_types) {
                auto vectorString = hex::splitString(name, "::");
                if (typeStr == vectorString.back())
                    candidates.push_back(name);
            }

            if (candidates.size() == 1) {
                m_curr = curr;
                return candidates[0];
            }
        }
        m_curr = curr;
        return typeStr;
    }

    // Definitions of global variables and placed variables.
    void ViewPatternEditor::TextHighlighter::loadGlobalDefinitions(
            Scopes tokenRangeSet, std::vector<IdentifierType> identifierTypes,
            Variables &variables) {
        for (auto range : tokenRangeSet) {
            restart();
            auto limit = m_curr + range.end;
            next(range.start);
            while (m_curr != limit) {

                if (peek(tkn::Literal::Identifier)) {
                    auto identifier = (Identifier *) getValue<Token::Identifier>(0);
                    auto identifierType = identifier->getType();
                    auto identifierName = identifier->get();

                    if (std::ranges::find(identifierTypes,identifierType) != identifierTypes.end()) {
                        std::string typeStr = getVariableTypeName();
                        variables[identifierName].push_back(Definition(identifierType,typeStr,getTokenId(m_curr->location),  m_curr->location));
                    }
                }
                next();
            }
        }
    }

    // Definitions of variables and arguments in functions and user defined types.
    void ViewPatternEditor::TextHighlighter::loadVariableDefinitions(UnorderedBlocks tokenRangeMap, Token delimiter,
                                                                     std::vector<IdentifierType> identifierTypes, bool isArgument, VariableMap &variableMap) {
        for (auto [name,range] : tokenRangeMap) {
            restart();
            auto limit = m_curr + range.end;
            next(range.start);
            Keyword *keyword = std::get_if<Keyword>(&m_tokens[range.start].value);
            while (m_curr != limit) {

                if (peek(tkn::Literal::Identifier)) {
                    auto identifier = (Identifier *) getValue<Token::Identifier>(0);
                    auto identifierType = identifier->getType();
                    auto identifierName = identifier->get();

                    if (std::ranges::find(identifierTypes,identifierType) != identifierTypes.end()) {
                        std::string typeStr;

                        if (keyword != nullptr && (*keyword == Keyword::Enum || *keyword == Keyword::Bitfield)) {
                            typeStr = name;
                        } else if (isArgument) {
                            typeStr = getArgumentTypeName(range.start, delimiter);
                        } else {
                            typeStr = getVariableTypeName();
                        }
                        variableMap[name][identifierName].push_back(Definition(identifierType,typeStr, getTokenId(m_curr->location),  m_curr->location));
                    }
                }
                next();
            }
        }
    }

    // Definitions of user defined types and functions.
    void ViewPatternEditor::TextHighlighter::loadTypeDefinitions(
            UnorderedBlocks tokenRangeMap, std::vector<IdentifierType> identifierTypes, Definitions &types) {
        for (auto [name,range] : tokenRangeMap) {
            restart();
            auto limit = m_curr + range.end;
            next(range.start);
            while (m_curr != limit) {

                if (peek(tkn::Literal::Identifier)) {
                    auto identifier = (Identifier *)getValue<Token::Identifier>(0);
                    auto identifierType = identifier->getType();

                    if (std::ranges::find(identifierTypes,identifierType) != identifierTypes.end())
                        types[name] = ParentDefinition(identifierType, getTokenId(m_curr->location), m_curr->location);
                }
                next();
            }
        }
    }

    // Once types are loaded from parsed tokens we can create
    // maps of variable names to their definitions.
    void ViewPatternEditor::TextHighlighter::getDefinitions() {
        using IdentifierType::PatternLocalVariable;
        using IdentifierType::PatternVariable;
        using IdentifierType::PatternPlacedVariable;
        using IdentifierType::GlobalVariable;
        using IdentifierType::PlacedVariable;
        using IdentifierType::FunctionVariable;
        using IdentifierType::FunctionParameter;
        using IdentifierType::TemplateArgument;
        using IdentifierType::UDT;
        using IdentifierType::Function;


        loadTypeDefinitions(m_UDTTokenRange, {UDT}, m_UDTDefinitions);
        loadVariableDefinitions(m_UDTTokenRange, tkn::Operator::BoolLessThan, {TemplateArgument}, true, m_UDTVariables);
        loadVariableDefinitions(m_UDTTokenRange, tkn::Operator::BoolLessThan, {PatternLocalVariable, PatternVariable, PatternPlacedVariable}, false, m_UDTVariables);

        appendInheritances();

        loadTypeDefinitions(m_functionTokenRange,{Function},m_functionDefinitions);
        loadVariableDefinitions(m_functionTokenRange,tkn::Separator::LeftParenthesis,{FunctionParameter},true,m_functionVariables);
        loadVariableDefinitions(m_functionTokenRange,tkn::Separator::LeftParenthesis,{FunctionVariable},false,m_functionVariables);

        loadGlobalDefinitions(m_globalTokenRange,{GlobalVariable,PlacedVariable},m_globalVariables);
    }

    // Load the source code into the text highlighter, splits
    // the text into lines and creates a lookup table for the
    // first token id of each line.
    void ViewPatternEditor::TextHighlighter::loadText() {
        m_lines.clear();

        if (m_text.empty())
            m_text = m_viewPatternEditor.m_textEditor.GetText();

        m_lines = hex::splitString(m_text, "\n");
        m_lines.push_back("");
        m_firstTokenIdOfLine.clear();
        m_firstTokenIdOfLine.resize(m_lines.size(), -1);

        i32 tokenId = 0;
        i32 tokenCount = m_tokens.size();
        i32 index;

        if (tokenCount > 0) {
            index = m_tokens[0].location.line - 1;
            m_firstTokenIdOfLine[index] = 0;
        }
        i32 count = m_lines.size();
        for (i32 currentLine=0; currentLine < count; currentLine++) {
            for (index = m_tokens[tokenId].location.line - 1; index <= currentLine && tokenId+1 < tokenCount ; tokenId++)
                index = m_tokens[tokenId+1].location.line - 1;

            if (index > currentLine)
                m_firstTokenIdOfLine[index] = tokenId;
        }

        if (m_firstTokenIdOfLine.back() != tokenCount)
             m_firstTokenIdOfLine.push_back(tokenCount);
    }

    // Some tokens span many lines and some lines have no tokens. This
    // function helps to find the next line number in the inner loop.
    u32 ViewPatternEditor::TextHighlighter::nextLine(u32 line) {
        auto currentTokenId = m_firstTokenIdOfLine[line];
        u32 i = 1;
        while(line + i < m_lines.size() && (m_firstTokenIdOfLine[line + i] == currentTokenId || m_firstTokenIdOfLine[line + i] == (i32)0xFFFFFFFF))
            i++;
        return i + line;
    }

    u32 ViewPatternEditor::TextHighlighter::previousLine(u32 line) {
        auto currentTokenId = m_firstTokenIdOfLine[line];
        u32 i = 1;
        while(line - i < m_lines.size() && (m_firstTokenIdOfLine[line - i] == currentTokenId || m_firstTokenIdOfLine[line - i] == (i32)0xFFFFFFFF))
            i++;
        return line - i;
    }

    // global token ranges are the complement (aka inverse) of the union
    // of the UDT and function token ranges
    void ViewPatternEditor::TextHighlighter::invertGlobalTokenRange() {
        std::set<Interval> ranges;
        auto size = m_globalTokenRange.size();

        if (size == 0) {
            ranges.insert(Interval(0, m_tokens.size()));
        } else {
            auto it = m_globalTokenRange.begin();
            auto it2 = std::next(it);
            if (it->start != 0)
                ranges.insert(Interval(0, it->start));
            while (it2 != m_globalTokenRange.end()) {

                if (it->end < it2->start)
                    ranges.insert(Interval(it->end, it2->start));
                else
                    ranges.insert(Interval(it->start, it2->end));
                it = it2;
                it2 = std::next(it);
            }

            if (it->end < (i32) m_tokens.size())
                ranges.insert(Interval(it->end, m_tokens.size()));
        }
        m_globalTokenRange = ranges;
    }

    // 0 for 1st argument, 1 for 2nd argument, etc. Obtained counting commas.
    i32  ViewPatternEditor::TextHighlighter::getArgumentNumber(i32 start,i32 arg) {
        i32 count = 0;
        restart();
        auto limit = m_curr + arg;
        next(start);
        while (m_curr != limit) {

            if (peek(tkn::Separator::Comma))
                count++;
            next();
        }

        return count;
    }

    // The inverse of getArgumentNumber.
    void ViewPatternEditor::TextHighlighter::getTokenIdForArgument(i32 start, i32 argNumber) {
        restart();
        next(start+2);
        i32 count = 0;
        while (count < argNumber) {

            if (peek(tkn::Separator::Comma))
                count++;
            next();
        }
    }

    // Changes auto type string in definitions to the actual type string.
    void ViewPatternEditor::TextHighlighter::resolveAutos( VariableMap &variableMap, UnorderedBlocks &tokenRange) {
        auto curr = m_curr;
        std::string UDTName;
        for (auto &[name, variables]: variableMap) {
            for (auto &[variableName, definitions]: variables) {
                for (auto &definition: definitions) {

                    if (definition.typeStr == "auto") {
                        auto argumentIndex = getArgumentNumber(tokenRange[name].start, definition.tokenIndex);

                        if (tokenRange == m_UDTTokenRange || !m_attributeFunctionArgumentType.contains(name) ||
                            m_attributeFunctionArgumentType[name].empty()) {
                            auto instances = m_instances[name];
                            for (auto instance: instances) {

                                if (std::abs(definition.tokenIndex - instance) <= 5)
                                    continue;
                                std::string fullName;
                                std::vector<Identifier *> identifiers;
                                getTokenIdForArgument(instance, argumentIndex);
                                auto save = m_curr;
                                forwardIdentifierName(fullName, std::nullopt, identifiers);
                                m_curr = save;

                                if (fullName.starts_with("Parent.")) {
                                    auto fixedDefinition = setChildrenTypes(fullName, identifiers);

                                    if (fixedDefinition.has_value() &&
                                        m_UDTDefinitions.contains(fixedDefinition->typeStr)) {
                                        definition.tokenIndex = fixedDefinition->tokenIndex;
                                        definition.typeStr = fixedDefinition->typeStr;
                                        continue;
                                    }
                                } else if (fullName.contains(".")) {
                                    Definition definitionTemp;
                                    resolveIdentifierType(definitionTemp);
                                    definition.typeStr = definitionTemp.typeStr;
                                    definition.tokenIndex = definitionTemp.tokenIndex;
                                } else {
                                    auto typeName = findIdentifierTypeStr(fullName);
                                    definition.typeStr = typeName;
                                }
                            }
                        } else {
                            UDTName = m_attributeFunctionArgumentType[name];
                            if (m_UDTDefinitions.contains(UDTName)) {
                                definition.tokenIndex = m_UDTDefinitions[UDTName].tokenIndex;
                                definition.typeStr = UDTName;
                                continue;
                            }
                        }
                    }
                }
            }
        }
        m_curr = curr;
    }

    void ViewPatternEditor::TextHighlighter::fixAutos() {
        resolveAutos(m_functionVariables, m_functionTokenRange);
        resolveAutos(m_UDTVariables, m_UDTTokenRange);
    }

    // Calculates the union of all the UDT and function token ranges
    // and inverts the result.
    void ViewPatternEditor::TextHighlighter::getGlobalTokenRanges() {
        std::set<Interval> ranges;
        for (auto [name,range] : m_UDTTokenRange)
            ranges.insert(range);
        for (auto [name,range] : m_functionTokenRange)
            ranges.insert(range);

        if (ranges.empty()) {
            invertGlobalTokenRange();
            return;
        }
        auto it = ranges.begin();
        auto next = std::next(it);
        while (next != ranges.end()) {

            if (next->start - it->end < 2) {
                Interval &range = const_cast<Interval &>(*it);
                range.end = next->end;
                ranges.erase(next);
                next = std::next(it);
            } else {
                it++;
                next = std::next(it);
            }
        }
        m_globalTokenRange = ranges;
        invertGlobalTokenRange();
    }

    // A block is a range of tokens enclosed by braces.
    void ViewPatternEditor::TextHighlighter::loadGlobalBlocks() {
        auto curr = m_curr;
        std::stack<i32> tokenStack;
        for (auto tokenRange : m_globalTokenRange) {
            m_globalBlocks.insert(tokenRange);
            restart();
            auto limit = m_curr + tokenRange.end;
            next(tokenRange.start);

            while (m_curr != limit) {
                if (peek(tkn::Separator::LeftBrace)) {
                    auto tokenId = getTokenId(m_curr->location);
                    tokenStack.push(tokenId);
                } else if (peek(tkn::Separator::RightBrace)) {
                    if (tokenStack.empty())
                        break;
                    Interval range(tokenStack.top(), getTokenId(m_curr->location));
                    tokenStack.pop();
                    m_globalBlocks.insert(range);
                } else if (peek(tkn::Separator::EndOfProgram)) {
                    break;
                }
                next();
            }
        }
        m_curr = curr;
    }

    // Parser labels global variables that are not placed as
    // function variables.
    void ViewPatternEditor::TextHighlighter::fixGlobalVariables() {
        for (auto range : m_globalTokenRange) {
            restart();
            auto limit = m_curr + range.end;
            next(range.start);
            while (m_curr != limit) {

                if (sequence(tkn::Literal::Identifier)) {
                    auto identifier = (Identifier *)getValue<Token::Identifier>(-1);
                    if (identifier->getType() == IdentifierType::FunctionVariable)
                        identifier->setType(IdentifierType::GlobalVariable, true);
                } else
                    next();
            }
        }
    }

    // Parser only sets identifier types for variable definitions.
    // creates a map of all the defined function variable types and names
    // and uses it to set the type of all variable instances.
    void ViewPatternEditor::TextHighlighter::fixFunctionVariables() {
        fixAutos();

        for (auto range : m_functionTokenRange) {
            std::map<Identifier *,IdentifierType > functionVariables;
            for (auto [function,variables]  : m_functionVariables) {
                // variables can have multiple definitions
                for (auto [variable, definitions]: variables) {
                    for (auto definition: definitions) {
                        std::string variableType = definition.typeStr;
                        // 1 is for the variable type 2 is for the variable itself
                        if (m_UDTDefinitions.contains(variableType)) {
                            auto index1 = m_UDTDefinitions[variableType].tokenIndex;
                            auto index2 = definition.tokenIndex;
                            auto token1 = m_tokens[index1];
                            auto token2 = m_tokens[index2];
                            Identifier *identifier1 = std::get_if<Identifier>(&token1.value);
                            Identifier *identifier2 = std::get_if<Identifier>(&token2.value);

                            if (identifier1 != nullptr)
                                functionVariables[identifier1] = identifier1->getType();
                            if (identifier2 != nullptr)
                                functionVariables[identifier2] = identifier2->getType();
                        }
                    }
                }
            }
            // scan token range for variables in map and set the identifier type
            restart();
            auto limit = m_curr + range.second.end;
            for (next(range.second.start); m_curr != limit; next()) {

                if (peek(tkn::Literal::Identifier)) {

                    auto identifier = (Identifier *)getValue<Token::Identifier>(0);
                    auto identifierType = identifier->getType();
                    if (functionVariables.contains(identifier) && ((identifierType == IdentifierType::Unknown) ||
                        (identifierType == IdentifierType::FunctionUnknown || identifierType == IdentifierType::MemberUnknown ||
                         identifierType == IdentifierType::ScopeResolutionUnknown)))

                        identifier->setType(functionVariables[identifier],true);
                }
            }
        }
    }

    // Process the tokens one line of text at a time.
    void ViewPatternEditor::TextHighlighter::processLineTokens() {
        fixFunctionVariables();
        // This is needed to avoid iterator out of bounds errors.
        auto tokenEnd = TokenIter(m_tokens.end(), m_tokens.end());

        // Possibly skip the lines on top if they have no tokens
        u32 firstLine = 0;
        while (m_firstTokenIdOfLine[firstLine] == -1 && firstLine < m_lines.size())
            firstLine++;

        for (auto line = firstLine; line < m_lines.size(); line = nextLine(line)) {

            restart();
            auto limit = m_curr + m_firstTokenIdOfLine[nextLine(line)];
            for (next(m_firstTokenIdOfLine[line]); m_curr != limit && m_curr != tokenEnd; next()) {

                auto location = m_curr->location;
                u32 column = location.column-1;
                TextEditor::Coordinates start = {(i32) line, (i32) column};
                TextEditor::PaletteIndex paletteIndexCopy, paletteIndex;
                paletteIndex = paletteIndexCopy = getPaletteIndex();

                u32 i = 0;
                if (m_excludedLocations.size() > 0) {
                    auto isExcluded = m_excludedLocations[1].location < location;
                    while (isExcluded.value_or(false) && i + 2 < m_excludedLocations.size()) {
                        i += 2;
                        isExcluded = m_excludedLocations[i + 1].location < location;
                    }
                    auto isLessThanMax = location <= m_excludedLocations[i + 1].location;
                    auto isGreaterThanMin = m_excludedLocations[i].location <= location;

                    if (isLessThanMax.value_or(false) && isGreaterThanMin.value_or(false))
                        paletteIndex = TextEditor::PaletteIndex::PreprocessorDeactivated;
                }

                u32 escapeCount = 0;
                if (paletteIndexCopy == TextEditor::PaletteIndex::StringLiteral) {
                    Literal literal = std::get<Literal>(m_curr->value);
                    std::string str = literal.toString();
                    escapeCount = escapeCharCount(str);
                }

                TextEditor::Coordinates end = m_viewPatternEditor.m_textEditor.add(start, location.length+escapeCount);
                m_viewPatternEditor.m_textEditor.setColorRange(start, end, paletteIndex);
            }
        }
    }

    // cleats all the data structures used by highlighter so that previous runs
    // do not affect the current.
    void ViewPatternEditor::TextHighlighter::clear() {
        m_typeDefs.clear();
        m_UDTs.clear();
        m_globalVariables.clear();
        m_functionVariables.clear();
        m_UDTVariables.clear();
        m_attributeFunctionArgumentType.clear();
        m_functionTokenRange.clear();
        m_UDTTokenRange.clear();
        m_namespaceTokenRange.clear();
        m_globalTokenRange.clear();
        m_instances.clear();
        m_UDTDefinitions.clear();
        m_functionDefinitions.clear();
        m_nameSpaces.clear();
        m_types.clear();
        m_UDTBlocks.clear();
        m_functionBlocks.clear();
    }

    // Only update if needed. Must wait for the parser to finish first.
    void ViewPatternEditor::TextHighlighter::highlightSourceCode() {

        if (m_needsToUpdateColors && (m_viewPatternEditor.m_runningParsers + m_viewPatternEditor.m_runningEvaluators == 0)) {

            m_text = m_viewPatternEditor.m_textEditor.GetText();
            if (m_text.empty() || m_text == "\n")
                return;

            m_needsToUpdateColors = false;
            clear();
            m_tokens = patternLanguage->getInternals().preprocessor.get()->getResult();
            if (m_tokens.empty())
                return;
            m_globalTokenRange.insert(Interval(0, m_tokens.size()));
            loadText();
            m_ast = patternLanguage->getAST();
            // Namespaces from included files.
            m_nameSpaces = patternLanguage->getInternals().preprocessor.get()->getNamespaces();
            auto tokens = patternLanguage->getInternals().preprocessor.get()->getOutput();
            m_types = patternLanguage->getInternals().parser.get()->getTypes();
            m_excludedLocations = patternLanguage->getInternals().preprocessor->getExcludedLocations();

            // The order of the following functions is important.
            getIdentifierTypes(tokens);
            getAllTokenRanges(IdentifierType::NameSpace);
            getAllTokenRanges(IdentifierType::UDT);
            getAllTokenRanges(IdentifierType::Function);
            getAllTokenRanges(IdentifierType::Attribute);
            getAllTokenRanges(IdentifierType::Typedef);
            getAllTokenRanges(IdentifierType::TemplateArgument);
            getGlobalTokenRanges();
            loadGlobalBlocks();
            fixGlobalVariables();
            getDefinitions();
            loadInstances();

            m_viewPatternEditor.m_textEditor.ClearErrorMarkers();
            m_compileErrors = patternLanguage->getCompileErrors();
            if (!m_compileErrors.empty())
                renderErrors();
            else
                m_viewPatternEditor.m_textEditor.ClearErrorMarkers();

            processLineTokens();
        }
    }

    TextEditor::PaletteIndex ViewPatternEditor::getPaletteIndex(char type) {
        TextEditor::ConsoleTypes consoleType = TextEditor::ConsoleTypes::Information;
        switch (type) {
            case 'I':
                consoleType = TextEditor::ConsoleTypes::Information;
                break;
            case 'W':
                consoleType = TextEditor::ConsoleTypes::Warning;
                break;
            case 'E':
                consoleType = TextEditor::ConsoleTypes::Error;
                break;
            case 'D':
                consoleType = TextEditor::ConsoleTypes::Debug;
                break;
        }
        return m_textEditor.getPaletteIndex<TextEditor::ConsoleTypes>(consoleType);
    }

    void ViewPatternEditor::drawConsole(ImVec2 size) {
        if (m_consoleNeedsUpdate) {
            std::scoped_lock lock(m_logMutex);

            auto lineCount = m_consoleEditor.GetTextLines().size() - 1;
            if (m_console->size() < lineCount) {
                m_consoleEditor.SetText("");
                lineCount = 0;
            }

            m_consoleEditor.SetCursorPosition({ int(lineCount + 1), 0 });

            const auto linesToAdd = m_console->size() - lineCount;
            for (size_t i = 0; i < linesToAdd; i += 1) {
                char type = m_console->at(lineCount + i).front();
                m_consoleEditor.InsertColoredText(m_console->at(lineCount + i),getPaletteIndex(type));
                m_consoleEditor.InsertText("\n");
            }

            m_consoleNeedsUpdate = false;
        }

        m_consoleEditor.Render("##console", size, true);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y + 1_scaled);
    }

    void ViewPatternEditor::drawEnvVars(ImVec2 size, std::list<EnvVar> &envVars) {
        static u32 envVarCounter = 1;

        if (ImGui::BeginChild("##env_vars", size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
            if (ImGui::BeginTable("##env_vars_table", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerH)) {
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.1F);
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.4F);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.38F);
                ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthStretch, 0.12F);

                int index = 0;
                for (auto iter = envVars.begin(); iter != envVars.end(); ++iter) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    auto &[id, name, value, type] = *iter;

                    ImGui::PushID(index++);
                    ON_SCOPE_EXIT { ImGui::PopID(); };

                    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                    constexpr static std::array Types = { "I", "F", "S", "B" };
                    if (ImGui::BeginCombo("", Types[static_cast<int>(type)])) {
                        for (size_t i = 0; i < Types.size(); i++) {
                            if (ImGui::Selectable(Types[i]))
                                type = static_cast<EnvVarType>(i);
                        }

                        ImGui::EndCombo();
                    }
                    ImGui::PopItemWidth();

                    ImGui::TableNextColumn();

                    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                    ImGui::InputText("###name", name);
                    ImGui::PopItemWidth();

                    ImGui::TableNextColumn();

                    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                    switch (type) {
                        using enum EnvVarType;
                        case Integer:
                            {
                                i64 displayValue = hex::get_or<i128>(value, 0);
                                ImGui::InputScalar("###value", ImGuiDataType_S64, &displayValue);
                                value = i128(displayValue);
                                break;
                            }
                        case Float:
                            {
                                auto displayValue = hex::get_or<double>(value, 0.0);
                                ImGui::InputDouble("###value", &displayValue);
                                value = displayValue;
                                break;
                            }
                        case Bool:
                            {
                                auto displayValue = hex::get_or<bool>(value, false);
                                ImGui::Checkbox("###value", &displayValue);
                                value = displayValue;
                                break;
                            }
                        case String:
                            {
                                auto displayValue = hex::get_or<std::string>(value, "");
                                ImGui::InputText("###value", displayValue);
                                value = displayValue;
                                break;
                            }
                    }
                    ImGui::PopItemWidth();

                    ImGui::TableNextColumn();

                    if (ImGuiExt::IconButton(ICON_VS_ADD, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                        envVars.insert(std::next(iter), { envVarCounter++, "", i128(0), EnvVarType::Integer });
                    }

                    ImGui::SameLine();

                    ImGui::BeginDisabled(envVars.size() <= 1);
                    {
                        if (ImGuiExt::IconButton(ICON_VS_REMOVE, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                            const bool isFirst = iter == envVars.begin();
                            const bool isLast  = std::next(iter) == envVars.end();
                            envVars.erase(iter);

                            if (isFirst)
                                iter = envVars.begin();
                            if (isLast)
                                iter = std::prev(envVars.end());
                        }
                    }
                    ImGui::EndDisabled();
                }

                ImGui::EndTable();
            }
        }
        ImGui::EndChild();
    }

    void ViewPatternEditor::drawVariableSettings(ImVec2 size, std::map<std::string, PatternVariable> &patternVariables) {
        if (ImGui::BeginChild("##settings", size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
            if (patternVariables.empty()) {
                ImGuiExt::TextFormattedCentered("hex.builtin.view.pattern_editor.no_in_out_vars"_lang);
            } else {
                if (ImGui::BeginTable("##in_out_vars_table", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersOuterH | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.25F);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.75F);

                    for (auto &[name, variable] : patternVariables) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        ImGui::TextUnformatted(name.c_str());

                        ImGui::TableNextColumn();

                        ImGui::PushItemWidth(-1);
                        if (variable.outVariable) {
                            ImGuiExt::TextFormattedSelectable("{}", variable.value.toString(true).c_str());
                        } else if (variable.inVariable) {
                            const std::string label { "##" + name };

                            if (pl::core::Token::isSigned(variable.type)) {
                                i64 value = hex::get_or<i128>(variable.value, 0);
                                if (ImGui::InputScalar(label.c_str(), ImGuiDataType_S64, &value))
                                    m_hasUnevaluatedChanges = true;
                                variable.value = i128(value);
                            } else if (pl::core::Token::isUnsigned(variable.type)) {
                                u64 value = hex::get_or<u128>(variable.value, 0);
                                if (ImGui::InputScalar(label.c_str(), ImGuiDataType_U64, &value))
                                    m_hasUnevaluatedChanges = true;
                                variable.value = u128(value);
                            } else if (pl::core::Token::isFloatingPoint(variable.type)) {
                                auto value = hex::get_or<double>(variable.value, 0.0);
                                if (ImGui::InputScalar(label.c_str(), ImGuiDataType_Double, &value))
                                    m_hasUnevaluatedChanges = true;
                                variable.value = value;
                            } else if (variable.type == pl::core::Token::ValueType::Boolean) {
                                auto value = hex::get_or<bool>(variable.value, false);
                                if (ImGui::Checkbox(label.c_str(), &value))
                                    m_hasUnevaluatedChanges = true;
                                variable.value = value;
                            } else if (variable.type == pl::core::Token::ValueType::Character) {
                                std::array<char, 2> buffer = { hex::get_or<char>(variable.value, '\x00') };
                                if (ImGui::InputText(label.c_str(), buffer.data(), buffer.size()))
                                    m_hasUnevaluatedChanges = true;
                                variable.value = buffer[0];
                            } else if (variable.type == pl::core::Token::ValueType::String) {
                                std::string buffer = hex::get_or<std::string>(variable.value, "");
                                if (ImGui::InputText(label.c_str(), buffer))
                                    m_hasUnevaluatedChanges = true;
                                variable.value = buffer;
                            }
                        }
                        ImGui::PopItemWidth();
                    }

                    ImGui::EndTable();
                }
            }
        }
        ImGui::EndChild();
    }

    void ViewPatternEditor::drawSectionSelector(ImVec2 size, const std::map<u64, pl::api::Section> &sections) {
        auto &runtime = ContentRegistry::PatternLanguage::getRuntime();

        if (ImGui::BeginTable("##sections_table", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, size)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("hex.ui.common.name"_lang, ImGuiTableColumnFlags_WidthStretch, 0.5F);
            ImGui::TableSetupColumn("hex.ui.common.size"_lang, ImGuiTableColumnFlags_WidthStretch, 0.5F);
            ImGui::TableSetupColumn("##button", ImGuiTableColumnFlags_WidthFixed, 50_scaled);

            ImGui::TableHeadersRow();

            if (TRY_LOCK(ContentRegistry::PatternLanguage::getRuntimeLock())) {
                for (auto &[id, section] : sections) {
                    if (section.name.empty())
                        continue;

                    ImGui::PushID(id);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    ImGui::TextUnformatted(section.name.c_str());
                    ImGui::TableNextColumn();
                    ImGuiExt::TextFormatted("{} | 0x{:02X}", hex::toByteString(section.data.size()), section.data.size());
                    ImGui::TableNextColumn();
                    if (ImGuiExt::DimmedIconButton(ICON_VS_OPEN_PREVIEW, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                        auto dataProvider = std::make_shared<prv::MemoryProvider>(section.data);
                        auto hexEditor = auto(m_sectionHexEditor);

                        hexEditor.setBackgroundHighlightCallback([this, id, &runtime](u64 address, const u8 *, size_t) -> std::optional<color_t> {
                            if (m_runningEvaluators != 0)
                                return std::nullopt;
                            if (!ImHexApi::Provider::isValid())
                                return std::nullopt;

                            std::optional<ImColor> color;
                            for (const auto &pattern : runtime.getPatternsAtAddress(address, id)) {
                                if (pattern->getVisibility() != pl::ptrn::Visibility::Visible)
                                    continue;

                                if (color.has_value())
                                    color = ImAlphaBlendColors(*color, pattern->getColor());
                                else
                                    color = pattern->getColor();
                            }

                            return color;
                        });

                        auto patternProvider = ImHexApi::Provider::get();


                        m_sectionWindowDrawer[patternProvider] = [this, id, patternProvider, dataProvider, hexEditor, patternDrawer = std::make_shared<ui::PatternDrawer>(), &runtime] mutable {
                            hexEditor.setProvider(dataProvider.get());
                            hexEditor.draw(480_scaled);
                            patternDrawer->setSelectionCallback([&](const pl::ptrn::Pattern *pattern) {
                                hexEditor.setSelection(Region { pattern->getOffset(), pattern->getSize() });
                            });

                            const auto &patterns = [&, this] -> const auto& {
                                if (patternProvider->isReadable() && *m_executionDone) {
                                    return runtime.getPatterns(id);
                                } else {
                                    static const std::vector<std::shared_ptr<pl::ptrn::Pattern>> empty;
                                    return empty;
                                }
                            }();

                            if (*m_executionDone)
                                patternDrawer->draw(patterns, &runtime, 150_scaled);
                        };
                    }
                    ImGui::SetItemTooltip("%s", "hex.builtin.view.pattern_editor.sections.view"_lang.get());

                    ImGui::SameLine();

                    if (ImGuiExt::DimmedIconButton(ICON_VS_SAVE_AS, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                        fs::openFileBrowser(fs::DialogMode::Save, {}, [id, &runtime](const auto &path) {
                            wolv::io::File file(path, wolv::io::File::Mode::Create);
                            if (!file.isValid()) {
                                ui::ToastError::open("hex.builtin.popup.error.create"_lang);
                                return;
                            }

                            file.writeVector(runtime.getSection(id));
                        });
                    }
                    ImGui::SetItemTooltip("%s", (const char*)"hex.builtin.view.pattern_editor.sections.export"_lang.get());

                    ImGui::PopID();
                }
            }

            ImGui::EndTable();
        }
    }

    void ViewPatternEditor::drawVirtualFiles(ImVec2 size, const std::vector<VirtualFile> &virtualFiles) const {
        std::vector<const VirtualFile*> virtualFilePointers;

        for (const auto &file : virtualFiles)
            virtualFilePointers.emplace_back(&file);

        if (ImGui::BeginTable("Virtual File Tree", 1, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, size)) {
            ImGui::TableSetupColumn("##path", ImGuiTableColumnFlags_WidthStretch);

            drawVirtualFileTree(virtualFilePointers);

            ImGui::EndTable();
        }
    }


    void ViewPatternEditor::drawDebugger(ImVec2 size) {
        const auto &runtime = ContentRegistry::PatternLanguage::getRuntime();
        auto &evaluator = runtime.getInternals().evaluator;

        if (ImGui::BeginChild("##debugger", size, true)) {
            const auto &breakpoints = evaluator->getBreakpoints();
            const auto line = m_textEditor.GetCursorPosition().mLine + 1;

            if (!breakpoints.contains(line)) {
                if (ImGuiExt::IconButton(ICON_VS_DEBUG_BREAKPOINT, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed))) {
                    evaluator->addBreakpoint(line);
                    m_textEditor.SetBreakpoints(breakpoints);
                }
                ImGuiExt::InfoTooltip("hex.builtin.view.pattern_editor.debugger.add_tooltip"_lang);
            } else {
                if (ImGuiExt::IconButton(ICON_VS_DEBUG_BREAKPOINT_UNVERIFIED, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed))) {
                    evaluator->removeBreakpoint(line);
                    m_textEditor.SetBreakpoints(breakpoints);
                }
                ImGuiExt::InfoTooltip("hex.builtin.view.pattern_editor.debugger.remove_tooltip"_lang);
            }

            ImGui::SameLine();

            if (*m_breakpointHit) {
                auto displayValue = [&](const auto &parent, size_t index) {
                    return hex::format("{0} {1} [{2}]",
                                        "hex.builtin.view.pattern_editor.debugger.scope"_lang,
                                        evaluator->getScopeCount() - index - 1,
                                        parent == nullptr ?
                                        "hex.builtin.view.pattern_editor.debugger.scope.global"_lang :
                                        hex::format("0x{0:08X}", parent->getOffset())
                    );
                };

                if (evaluator->getScopeCount() > 0) {
                    ImGui::SetNextItemWidth(-1);
                    const auto &currScope = evaluator->getScope(-m_debuggerScopeIndex);
                    if (ImGui::BeginCombo("##scope", displayValue(currScope.parent, m_debuggerScopeIndex).c_str())) {
                        for (size_t i = 0; i < evaluator->getScopeCount(); i++) {
                            auto &scope = evaluator->getScope(-i);

                            if (ImGui::Selectable(displayValue(scope.parent, i).c_str(), i == size_t(m_debuggerScopeIndex))) {
                                m_debuggerScopeIndex = i;
                                m_resetDebuggerVariables = true;
                            }
                        }

                        ImGui::EndCombo();
                    }
                }

                if (m_resetDebuggerVariables) {
                    const auto pauseLine = evaluator->getPauseLine();

                    (*m_debuggerDrawer)->reset();
                    m_resetDebuggerVariables = false;
                    m_textEditor.SetCursorPosition(TextEditor::Coordinates(pauseLine.value_or(0) - 1, 0));

                    if (pauseLine.has_value())
                        m_textEditor.SetCursorPosition({ int(pauseLine.value() - 1), 0 });
                }

                const auto &currScope = evaluator->getScope(-m_debuggerScopeIndex);
                (*m_debuggerDrawer)->draw(*currScope.scope, &runtime, size.y - ImGui::GetTextLineHeightWithSpacing() * 4);
            }
        }
        ImGui::EndChild();
    }

    void ViewPatternEditor::drawAlwaysVisibleContent() {
        auto provider = ImHexApi::Provider::get();

        auto open = m_sectionWindowDrawer.contains(provider);
        if (open) {
            ImGui::SetNextWindowSize(scaled(ImVec2(600, 700)), ImGuiCond_Appearing);
            if (ImGui::Begin("hex.builtin.view.pattern_editor.section_popup"_lang, &open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                m_sectionWindowDrawer[provider]();
            }
            ImGui::End();
        }

        if (!open && m_sectionWindowDrawer.contains(provider)) {
            ImHexApi::HexEditor::setSelection(Region::Invalid());
            m_sectionWindowDrawer.erase(provider);
        }

        if (!m_lastEvaluationProcessed) {
            if (!m_lastEvaluationResult) {
                const auto processMessage = [](const auto &message) {
                    auto lines = wolv::util::splitString(message, "\n");

                    std::ranges::transform(lines, lines.begin(), [](auto line) {
                        if (line.size() >= 128)
                            line = wolv::util::trim(line);

                        return hex::limitStringLength(line, 128);
                    });

                    return wolv::util::combineStrings(lines, "\n");
                };

                TextEditor::ErrorMarkers errorMarkers;
                if (!m_callStack->empty()) {
                    for (const auto &frame : *m_callStack | std::views::reverse) {
                        auto location = frame->getLocation();
                        std::string message;
                        if (location.source->source == pl::api::Source::DefaultSource) {
                            if (m_lastEvaluationError->has_value())
                                message = processMessage((*m_lastEvaluationError)->message);
                            auto key = TextEditor::Coordinates(location.line, location.column);
                            errorMarkers[key] = std::make_pair(location.length, message);
                        }
                    }
                }

                if (!m_lastCompileError->empty()) {
                    for (const auto &error : *m_lastCompileError) {
                       auto source = error.getLocation().source;
                        if (source != nullptr && source->source == pl::api::Source::DefaultSource) {
                            auto key = TextEditor::Coordinates(error.getLocation().line, error.getLocation().column);
                            if (!errorMarkers.contains(key) ||errorMarkers[key].first < error.getLocation().length)
                                    errorMarkers[key] = std::make_pair(error.getLocation().length,processMessage(error.getMessage()));
                        }
                    }
                }

                m_textEditor.SetErrorMarkers(errorMarkers);
            } else {
                for (auto &[name, variable] : *m_patternVariables) {
                    if (variable.outVariable && m_lastEvaluationOutVars->contains(name))
                        variable.value = m_lastEvaluationOutVars->at(name);
                }

                EventHighlightingChanged::post();
            }

            m_lastEvaluationProcessed = true;
            *m_executionDone = true;
        }

        if (m_shouldAnalyze) {
            m_shouldAnalyze = false;

            m_analysisTask = TaskManager::createBackgroundTask("hex.builtin.task.analyzing_data"_lang, [this, provider](const Task &task) {
                if (!m_autoLoadPatterns)
                    return;

                pl::PatternLanguage runtime;
                ContentRegistry::PatternLanguage::configureRuntime(runtime, provider);

                bool foundCorrectType = false;

                auto mimeType = magic::getMIMEType(provider, 0, 100_KiB, true);
                runtime.addPragma("MIME", [&mimeType, &foundCorrectType](const pl::PatternLanguage &runtime, const std::string &value) {
                    hex::unused(runtime);

                    if (!magic::isValidMIMEType(value))
                        return false;

                    if (value == mimeType) {
                        foundCorrectType = true;
                        return true;
                    }
                    return !std::ranges::all_of(value, isspace) && !value.ends_with('\n') && !value.ends_with('\r');
                });

                // Format: [ AA BB CC DD ] @ 0x12345678
                runtime.addPragma("magic", [provider, &foundCorrectType](pl::PatternLanguage &, const std::string &value) -> bool {
                    const auto pattern = [value = value] mutable -> std::optional<BinaryPattern> {
                        value = wolv::util::trim(value);

                        if (value.empty())
                            return std::nullopt;

                        if (!value.starts_with('['))
                            return std::nullopt;

                        value = value.substr(1);

                        const auto end = value.find(']');
                        if (end == std::string::npos)
                            return std::nullopt;

                        value = value.substr(0, end);
                        value = wolv::util::trim(value);

                        return BinaryPattern(value);
                    }();

                    const auto address = [value = value] mutable -> std::optional<u64> {
                        value = wolv::util::trim(value);

                        if (value.empty())
                            return std::nullopt;

                        const auto start = value.find('@');
                        if (start == std::string::npos)
                            return std::nullopt;

                        value = value.substr(start + 1);
                        value = wolv::util::trim(value);

                        size_t end = 0;
                        auto result = std::stoull(value, &end, 0);
                        if (end != value.length())
                            return std::nullopt;

                        return result;
                    }();

                    if (!address)
                        return false;
                    if (!pattern)
                        return false;

                    std::vector<u8> bytes(pattern->getSize());
                    if (bytes.empty())
                        return false;

                    provider->read(*address, bytes.data(), bytes.size());

                    if (pattern->matches(bytes))
                        foundCorrectType = true;

                    return true;
                });

                std::string author;
                runtime.addPragma("author", [&author](pl::PatternLanguage &, const std::string &value) -> bool {
                    author = value;
                    return true;
                });

                std::string description;
                runtime.addPragma("description", [&description](pl::PatternLanguage &, const std::string &value) -> bool {
                    description = value;
                    return true;
                });

                m_possiblePatternFiles.get(provider).clear();

                bool popupOpen = false;
                std::error_code errorCode;
                for (const auto &dir : paths::Patterns.read()) {
                    for (auto &entry : std::fs::recursive_directory_iterator(dir, errorCode)) {
                        task.update();

                        foundCorrectType = false;
                        if (!entry.is_regular_file())
                            continue;

                        wolv::io::File file(entry.path(), wolv::io::File::Mode::Read);
                        if (!file.isValid())
                            continue;

                        author.clear();
                        description.clear();

                        auto result = runtime.preprocessString(file.readString(), pl::api::Source::DefaultSource);
                        if (!result.has_value()) {
                            log::warn("Failed to preprocess file {} during MIME analysis", entry.path().string());
                        }

                        if (foundCorrectType) {
                            {
                                std::scoped_lock lock(m_possiblePatternFilesMutex);

                                m_possiblePatternFiles.get(provider).emplace_back(
                                    entry.path(),
                                    std::move(author),
                                    std::move(description)
                                );
                            }

                            if (!popupOpen) {
                                PopupAcceptPattern::open(this);
                                popupOpen = true;
                            }
                        }

                        runtime.reset();
                    }
                }
            });
        }
    }


    void ViewPatternEditor::drawPatternTooltip(pl::ptrn::Pattern *pattern) {
        ImGui::PushID(pattern);
        {
            const bool shiftHeld = ImGui::GetIO().KeyShift;
            ImGui::ColorButton(pattern->getVariableName().c_str(), ImColor(pattern->getColor()));
            ImGui::SameLine(0, 10);
            ImGuiExt::TextFormattedColored(TextEditor::GetPalette()[u32(TextEditor::PaletteIndex::KnownIdentifier)], "{} ", pattern->getFormattedName());
            ImGui::SameLine(0, 5);
            ImGuiExt::TextFormatted("{}", pattern->getDisplayName());
            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
            ImGuiExt::TextFormatted("{: <{}} ", hex::limitStringLength(pattern->getFormattedValue(), 64), shiftHeld ? 40 : 0);

            if (shiftHeld) {
                ImGui::Indent();
                if (ImGui::BeginTable("##extra_info", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoClip)) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGuiExt::TextFormatted("{} ", "hex.ui.common.path"_lang);
                    ImGui::TableNextColumn();

                    std::string path;
                    {
                        std::vector<std::string> pathSegments;
                        const pl::ptrn::Pattern *entry = pattern;
                        while (entry != nullptr) {
                            pathSegments.push_back(entry->getVariableName());
                            entry = entry->getParent();
                        }

                        for (const auto &segment : pathSegments | std::views::reverse) {
                            if (!segment.starts_with('['))
                                path += '.';

                            path += segment;
                        }

                        if (path.starts_with('.'))
                            path = path.substr(1);
                    }

                    ImGui::Indent();
                    ImGui::PushTextWrapPos(500_scaled);
                    ImGuiExt::TextFormattedWrapped("{}", path);
                    ImGui::PopTextWrapPos();
                    ImGui::Unindent();

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGuiExt::TextFormatted("{} ", "hex.ui.common.type"_lang);
                    ImGui::TableNextColumn();
                    ImGui::Indent();
                    ImGuiExt::TextFormatted("{}", pattern->getFormattedName());
                    ImGui::Unindent();

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGuiExt::TextFormatted("{} ", "hex.ui.common.address"_lang);
                    ImGui::TableNextColumn();
                    ImGui::Indent();
                    ImGuiExt::TextFormatted("0x{:08X}", pattern->getOffset());
                    ImGui::Unindent();

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGuiExt::TextFormatted("{} ", "hex.ui.common.size"_lang);
                    ImGui::TableNextColumn();
                    ImGui::Indent();
                    ImGuiExt::TextFormatted("{}", hex::toByteString(pattern->getSize()));
                    ImGui::Unindent();

                    {
                        const auto provider = ImHexApi::Provider::get();
                        const auto baseAddress = provider != nullptr ? provider->getBaseAddress() : 0x00;
                        const auto parent = pattern->getParent();
                        const auto parentAddress = parent == nullptr ? baseAddress : parent->getOffset();
                        const auto parentSize = parent == nullptr ? baseAddress : parent->getSize();
                        const auto patternAddress = pattern->getOffset();

                        if (patternAddress >= parentAddress && patternAddress + pattern->getSize() <= parentAddress + parentSize) {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGuiExt::TextFormatted("{} ", "hex.builtin.view.pattern_editor.tooltip.parent_offset"_lang);
                            ImGui::TableNextColumn();
                            ImGui::Indent();
                            ImGuiExt::TextFormatted("0x{:02X}", pattern->getOffset() - parentAddress);
                            ImGui::Unindent();
                        }
                    }

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGuiExt::TextFormatted("{} ", "hex.ui.common.endian"_lang);
                    ImGui::TableNextColumn();
                    ImGui::Indent();
                    ImGuiExt::TextFormatted("{}", pattern->getEndian() == std::endian::little ? "hex.ui.common.little"_lang : "hex.ui.common.big"_lang);
                    ImGui::Unindent();

                    if (const auto &comment = pattern->getComment(); !comment.empty()) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("{} ", "hex.ui.common.comment"_lang);
                        ImGui::TableNextColumn();
                        ImGui::Indent();
                        ImGuiExt::TextFormattedWrapped("\"{}\"", comment);
                        ImGui::Unindent();
                    }

                    ImGui::EndTable();
                }
                ImGui::Unindent();
            }
        }

        ImGui::PopID();
    }


    void ViewPatternEditor::loadPatternFile(const std::fs::path &path, prv::Provider *provider) {
        wolv::io::File file(path, wolv::io::File::Mode::Read);
        if (file.isValid()) {

            auto code = preprocessText(file.readString());
            this->evaluatePattern(code, provider);
            m_textEditor.SetText(code);
            m_sourceCode.set(provider, code);

            m_textHighlighter.m_needsToUpdateColors = false;
            TaskManager::createBackgroundTask("hex.builtin.task.parsing_pattern"_lang, [this, code, provider](auto&) { this->parsePattern(code, provider); });
        }
    }

    void ViewPatternEditor::parsePattern(const std::string &code, prv::Provider *provider) {
        m_runningParsers += 1;

        ContentRegistry::PatternLanguage::configureRuntime(*m_editorRuntime, nullptr);
        const auto &ast = m_editorRuntime->parseString(code, pl::api::Source::DefaultSource);

        auto &patternVariables = m_patternVariables.get(provider);
        auto oldPatternVariables = std::move(patternVariables);

        if (ast.has_value()) {
            for (auto &node : *ast) {
                if (const auto variableDecl = dynamic_cast<pl::core::ast::ASTNodeVariableDecl *>(node.get())) {
                    const auto type = variableDecl->getType().get();
                    if (type == nullptr) continue;

                    const auto builtinType = dynamic_cast<pl::core::ast::ASTNodeBuiltinType *>(type->getType().get());
                    if (builtinType == nullptr)
                        continue;

                    const PatternVariable variable = {
                        .inVariable  = variableDecl->isInVariable(),
                        .outVariable = variableDecl->isOutVariable(),
                        .type        = builtinType->getType(),
                        .value       = oldPatternVariables.contains(variableDecl->getName()) ? oldPatternVariables[variableDecl->getName()].value : pl::core::Token::Literal()
                    };

                    if (variable.inVariable || variable.outVariable) {
                        if (!patternVariables.contains(variableDecl->getName()))
                            patternVariables[variableDecl->getName()] = variable;
                    }
                }
            }
        } else {
            patternVariables = std::move(oldPatternVariables);
        }

        m_textHighlighter.m_needsToUpdateColors = true;
        m_runningParsers -= 1;
    }

    void ViewPatternEditor::evaluatePattern(const std::string &code, prv::Provider *provider) {
        EventPatternEvaluating::post();

        auto lock = std::scoped_lock(ContentRegistry::PatternLanguage::getRuntimeLock());

        m_runningEvaluators += 1;
        *m_executionDone = false;

        m_console->clear();
        m_consoleNeedsUpdate = true;

        m_sectionWindowDrawer.clear();
        m_consoleEditor.SetText("");
        m_virtualFiles->clear();

        m_accessHistory = {};
        m_accessHistoryIndex = 0;

        EventHighlightingChanged::post();

        TaskManager::createTask("hex.builtin.view.pattern_editor.evaluating"_lang, TaskManager::NoProgress, [this, code, provider](auto &task) {
            auto lock = std::scoped_lock(ContentRegistry::PatternLanguage::getRuntimeLock());

            auto &runtime = ContentRegistry::PatternLanguage::getRuntime();
            ContentRegistry::PatternLanguage::configureRuntime(runtime, provider);
            runtime.getInternals().evaluator->setBreakpointHitCallback([this]{
                m_debuggerScopeIndex = 0;
                *m_breakpointHit = true;
                m_resetDebuggerVariables = true;
                while (*m_breakpointHit) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            });

            task.setInterruptCallback([this, &runtime] {
                m_breakpointHit = false;
                runtime.abort();
            });

            std::map<std::string, pl::core::Token::Literal> envVars;
            for (const auto &[id, name, value, type] : *m_envVarEntries)
                envVars.insert({ name, value });

            std::map<std::string, pl::core::Token::Literal> inVariables;
            for (auto &[name, variable] : *m_patternVariables) {
                if (variable.inVariable)
                    inVariables[name] = variable.value;
            }

            runtime.setDangerousFunctionCallHandler([this]{
                m_dangerousFunctionCalled = true;

                while (m_dangerousFunctionsAllowed == DangerousFunctionPerms::Ask) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }

                return m_dangerousFunctionsAllowed == DangerousFunctionPerms::Allow;
            });

            runtime.setLogCallback([this](auto level, auto message) {
                std::scoped_lock lock(m_logMutex);

                for (auto line : wolv::util::splitString(message, "\n")) {
                    if (!wolv::util::trim(line).empty()) {
                        switch (level) {
                            using enum pl::core::LogConsole::Level;

                            case Debug:     line = hex::format("D: {}", line); break;
                            case Info:      line = hex::format("I: {}", line); break;
                            case Warning:   line = hex::format("W: {}", line); break;
                            case Error:     line = hex::format("E: {}", line); break;
                            default: break;
                        }
                    }

                    m_console->emplace_back(line);
                    m_consoleNeedsUpdate = true;
                }
            });

            ON_SCOPE_EXIT {
                runtime.getInternals().evaluator->setDebugMode(false);
                *m_lastEvaluationOutVars = runtime.getOutVariables();
                *m_sections              = runtime.getSections();

                m_runningEvaluators -= 1;

                m_lastEvaluationProcessed = false;

                std::scoped_lock lock(m_logMutex);
                m_console->emplace_back(
                   hex::format("I: Evaluation took {}", std::chrono::duration<double>(runtime.getLastRunningTime()))
                );
                m_consoleNeedsUpdate = true;
            };


            m_lastEvaluationResult = runtime.executeString(code, pl::api::Source::DefaultSource, envVars, inVariables);
            if (!m_lastEvaluationResult) {
                *m_lastEvaluationError = runtime.getEvalError();
                *m_lastCompileError    = runtime.getCompileErrors();
                *m_callStack           = reinterpret_cast<const std::vector<const pl::core::ast::ASTNode *> &>(runtime.getInternals().evaluator->getCallStack());
            }

            TaskManager::doLater([code] {
                EventPatternExecuted::post(code);
            });
        });
    }

    std::string ViewPatternEditor::preprocessText(const std::string &code) {
        std::string result = wolv::util::replaceStrings(code, "\r\n", "\n");
        result = wolv::util::replaceStrings(result, "\r", "\n");
        result = wolv::util::replaceStrings(result, "\t", "    ");
        while (result.ends_with('\n'))
            result.pop_back();
        return result;
    }

    void ViewPatternEditor::registerEvents() {
        RequestPatternEditorSelectionChange::subscribe(this, [this](u32 line, u32 column) {
            const TextEditor::Coordinates coords = { int(line) - 1, int(column) };
            m_textEditor.SetCursorPosition(coords);
        });

        RequestLoadPatternLanguageFile::subscribe(this, [this](const std::fs::path &path) {
            this->loadPatternFile(path, ImHexApi::Provider::get());
        });

        RequestRunPatternCode::subscribe(this, [this] {
            m_triggerAutoEvaluate = true;
        });

        RequestSavePatternLanguageFile::subscribe(this, [this](const std::fs::path &path) {
            wolv::io::File file(path, wolv::io::File::Mode::Create);
            file.writeString(wolv::util::trim(m_textEditor.GetText()));
        });

        RequestSetPatternLanguageCode::subscribe(this, [this](const std::string &code) {
            const std::string preprocessed = preprocessText(code);
            m_textEditor.SetText(preprocessed);
            m_sourceCode.set(ImHexApi::Provider::get(), preprocessed);
            m_hasUnevaluatedChanges = true;
            m_textHighlighter.m_needsToUpdateColors = false;
        });

        ContentRegistry::Settings::onChange("hex.builtin.setting.general", "hex.builtin.setting.general.sync_pattern_source", [this](const ContentRegistry::Settings::SettingsValue &value) {
            m_sourceCode.enableSync(value.get<bool>(false));
        });
        ContentRegistry::Settings::onChange("hex.builtin.setting.general", "hex.builtin.setting.general.auto_load_patterns", [this](const ContentRegistry::Settings::SettingsValue &value) {
            m_autoLoadPatterns = value.get<bool>(true);
        });

        EventProviderOpened::subscribe(this, [this](prv::Provider *provider) {
            m_shouldAnalyze.get(provider) = true;
            m_envVarEntries->emplace_back(0, "", i128(0), EnvVarType::Integer);

            m_debuggerDrawer.get(provider) = std::make_unique<ui::PatternDrawer>();
        });

        EventProviderChanged::subscribe(this, [this](prv::Provider *oldProvider, prv::Provider *newProvider) {
            if (oldProvider != nullptr)
                m_sourceCode.set(oldProvider, m_textEditor.GetText());

            if (newProvider != nullptr)
               m_textEditor.SetText(m_sourceCode.get(newProvider));
            m_hasUnevaluatedChanges = true;
            m_textHighlighter.m_needsToUpdateColors = false;
        });

        EventProviderClosed::subscribe(this, [this](prv::Provider *) {
            if (ImHexApi::Provider::getProviders().empty()) {
                m_textEditor.SetText("");
            }
        });

        RequestAddVirtualFile::subscribe(this, [this](const std::fs::path &path, const std::vector<u8> &data, Region region) {
            m_virtualFiles->emplace_back(path, data, region);
        });
    }

    static void createNestedMenu(const std::vector<std::string> &menus, const std::function<void()> &function) {
        if (menus.empty())
            return;

        if (menus.size() == 1) {
            if (ImGui::MenuItem(menus.front().c_str()))
                function();
        } else {
            if (ImGui::BeginMenu(menus.front().c_str())) {
                createNestedMenu({ menus.begin() + 1, menus.end() }, function);
                ImGui::EndMenu();
            }
        }
    }

    void ViewPatternEditor::appendEditorText(const std::string &text) {
        m_textEditor.SetCursorPosition(TextEditor::Coordinates { m_textEditor.GetTotalLines(), 0 });
        m_textEditor.InsertText(hex::format("\n{0}", text));
        m_triggerEvaluation = true;
    }

    void ViewPatternEditor::appendVariable(const std::string &type) {
        const auto &selection = ImHexApi::HexEditor::getSelection();

        appendEditorText(hex::format("{0} {0}_at_0x{1:02X} @ 0x{1:02X};", type, selection->getStartAddress()));
        AchievementManager::unlockAchievement("hex.builtin.achievement.patterns", "hex.builtin.achievement.patterns.place_menu.name");
    }

    void ViewPatternEditor::appendArray(const std::string &type, size_t size) {
        const auto &selection = ImHexApi::HexEditor::getSelection();

        appendEditorText(hex::format("{0} {0}_array_at_0x{1:02X}[0x{2:02X}] @ 0x{1:02X};", type, selection->getStartAddress(), (selection->getSize() + (size - 1)) / size));
    }

    void ViewPatternEditor::registerMenuItems() {
        /* Import Pattern */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.import", "hex.builtin.menu.file.import.pattern" }, ICON_VS_FILE_CODE, 4050, Shortcut::None,
                                                m_importPatternFile, ImHexApi::Provider::isValid);

        /* Export Pattern */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.export", "hex.builtin.menu.file.export.pattern" }, ICON_VS_FILE_CODE, 7050, Shortcut::None,
                                                m_exportPatternFile, [this] {
                                                    return !wolv::util::trim(m_textEditor.GetText()).empty() && ImHexApi::Provider::isValid();
                                                }
        );

        constexpr static std::array<std::pair<const char *, size_t>, 21>  Types = {{
           { "u8", 1 }, { "u16", 2 }, { "u24", 3 }, { "u32", 4 }, { "u48", 6 }, { "u64", 8 }, { "u96", 12 }, { "u128", 16 },
           { "s8", 1 }, { "s16", 2 }, { "s24", 3 }, { "s32", 4 }, { "s48", 6 }, { "s64", 8 }, { "s96", 12 }, { "s128", 16 },
           { "float", 4 }, { "double", 8 },
           { "bool", 1 }, { "char", 1 }, { "char16", 2 }
        }};

        /* Place pattern... */
        ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.edit", "hex.builtin.view.pattern_editor.menu.edit.place_pattern" }, ICON_VS_LIBRARY, 3000,
            [&, this] {
                if (ImGui::BeginMenu("hex.builtin.view.pattern_editor.menu.edit.place_pattern.builtin"_lang)) {
                    if (ImGui::BeginMenu("hex.builtin.view.pattern_editor.menu.edit.place_pattern.builtin.single"_lang)) {
                        for (const auto &[type, size] : Types) {
                            if (ImGui::MenuItem(type))
                                appendVariable(type);
                        }
                        ImGui::EndMenu();
                    }

                    if (ImGui::BeginMenu("hex.builtin.view.pattern_editor.menu.edit.place_pattern.builtin.array"_lang)) {
                        for (const auto &[type, size] : Types) {
                            if (ImGui::MenuItem(type))
                                appendArray(type, size);
                        }
                        ImGui::EndMenu();
                    }

                    ImGui::EndMenu();
                }

                const auto &types = m_editorRuntime->getInternals().parser->getTypes();
                const bool hasPlaceableTypes = std::ranges::any_of(types, [](const auto &type) { return !type.second->isTemplateType(); });

                if (ImGui::BeginMenu("hex.builtin.view.pattern_editor.menu.edit.place_pattern.custom"_lang, hasPlaceableTypes)) {
                    const auto &selection = ImHexApi::HexEditor::getSelection();

                    for (const auto &[typeName, type] : types) {
                        if (type->isTemplateType())
                            continue;

                        createNestedMenu(hex::splitString(typeName, "::"), [&, this] {
                            std::string variableName;
                            for (const char c : hex::replaceStrings(typeName, "::", "_"))
                                variableName += static_cast<char>(std::tolower(c));
                            variableName += hex::format("_at_0x{:02X}", selection->getStartAddress());

                            appendEditorText(hex::format("{0} {1} @ 0x{2:02X};", typeName, variableName, selection->getStartAddress()));
                        });
                    }

                    ImGui::EndMenu();
                }
            }, [this] {
                return ImHexApi::Provider::isValid() && ImHexApi::HexEditor::isSelectionValid() && m_runningParsers == 0;
            });
    }

    void ViewPatternEditor::registerHandlers() {
        ContentRegistry::FileHandler::add({ ".hexpat", ".pat" }, [](const std::fs::path &path) -> bool {
            wolv::io::File file(path, wolv::io::File::Mode::Read);

            if (file.isValid()) {
                RequestSetPatternLanguageCode::post(file.readString());
                return true;
            } else {
                return false;
            }
        });

        ContentRegistry::Settings::onChange("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.pattern_parent_highlighting", [this](const ContentRegistry::Settings::SettingsValue &value) {
            m_parentHighlightingEnabled = bool(value.get<int>(false));
        });

        ImHexApi::HexEditor::addBackgroundHighlightingProvider([this](u64 address, const u8 *data, size_t size, bool) -> std::optional<color_t> {
            hex::unused(data, size);

            if (m_runningEvaluators != 0)
                return std::nullopt;

            const auto &runtime = ContentRegistry::PatternLanguage::getRuntime();

            std::optional<ImColor> color;

            if (TRY_LOCK(ContentRegistry::PatternLanguage::getRuntimeLock())) {
                for (const auto &patternColor : runtime.getColorsAtAddress(address)) {
                    if (color.has_value())
                        color = ImAlphaBlendColors(*color, patternColor);
                    else
                        color = patternColor;
                }
            }

            return color;
        });

        ImHexApi::HexEditor::addHoverHighlightProvider([this](const prv::Provider *, u64 address, size_t size) {
            std::set<Region> result;
            if (!m_parentHighlightingEnabled)
                return result;

            const auto &runtime = ContentRegistry::PatternLanguage::getRuntime();

            const auto hoveredRegion = Region { address, size };
            for (const auto &pattern : runtime.getPatternsAtAddress(hoveredRegion.getStartAddress())) {
                const pl::ptrn::Pattern * checkPattern = pattern;
                if (auto parent = checkPattern->getParent(); parent != nullptr)
                    checkPattern = parent;

                result.emplace(checkPattern->getOffset(), checkPattern->getSize());
            }

            return result;
        });

        ImHexApi::HexEditor::addTooltipProvider([this](u64 address, const u8 *data, size_t size) {
            hex::unused(data, size);

            if (TRY_LOCK(ContentRegistry::PatternLanguage::getRuntimeLock())) {
                const auto &runtime = ContentRegistry::PatternLanguage::getRuntime();

                auto patterns = runtime.getPatternsAtAddress(address);
                if (!patterns.empty() && !std::ranges::all_of(patterns, [](const auto &pattern) { return pattern->getVisibility() == pl::ptrn::Visibility::Hidden; })) {
                    ImGui::BeginTooltip();

                    for (const auto &pattern : patterns) {
                        if (pattern->getVisibility() != pl::ptrn::Visibility::Visible)
                            continue;

                        const auto tooltipColor = (pattern->getColor() & 0x00FF'FFFF) | 0x7000'0000;
                        ImGui::PushID(pattern);
                        if (ImGui::BeginTable("##tooltips", 1, ImGuiTableFlags_RowBg | ImGuiTableFlags_NoClip)) {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();

                            this->drawPatternTooltip(pattern);

                            ImGui::PushStyleColor(ImGuiCol_TableRowBg, tooltipColor);
                            ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, tooltipColor);
                            ImGui::EndTable();
                            ImGui::PopStyleColor(2);
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndTooltip();
                }
            }
        });

        ProjectFile::registerPerProviderHandler({
            .basePath = "pattern_source_code.hexpat",
            .required = false,
            .load = [this](prv::Provider *provider, const std::fs::path &basePath, const Tar &tar) {
                const auto sourceCode = preprocessText(tar.readString(basePath));

                m_sourceCode.set(provider, sourceCode);
                m_console.get(provider);
                if (provider == ImHexApi::Provider::get())
                    m_textEditor.SetText(sourceCode);

                m_hasUnevaluatedChanges = true;
                m_textHighlighter.m_needsToUpdateColors = false;
                return true;
            },
            .store = [this](prv::Provider *provider, const std::fs::path &basePath, const Tar &tar) {
                if (provider == ImHexApi::Provider::get())
                    m_sourceCode.set(provider, m_textEditor.GetText());

                const auto &sourceCode = m_sourceCode.get(provider);

                tar.writeString(basePath, wolv::util::trim(sourceCode));
                return true;
            }
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::F + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.find", [this] {
            m_openFindReplacePopUp = true;
            m_replaceMode = false;
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::H + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.replace", [this] {
            m_openFindReplacePopUp = true;
            m_replaceMode = true;
        });

        ShortcutManager::addShortcut(this, Keys::F3 + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.find_next", [this] {
            TextEditor::FindReplaceHandler *findReplaceHandler = m_textEditor.GetFindReplaceHandler();
            findReplaceHandler->FindMatch(&m_textEditor,true);
        });

        ShortcutManager::addShortcut(this, SHIFT + Keys::F3 + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.find_previous", [this] {
            TextEditor::FindReplaceHandler *findReplaceHandler = m_textEditor.GetFindReplaceHandler();
            findReplaceHandler->FindMatch(&m_textEditor,false);
        });

        ShortcutManager::addShortcut(this, ALT + Keys::C + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.match_case_toggle", [this] {
            TextEditor::FindReplaceHandler *findReplaceHandler = m_textEditor.GetFindReplaceHandler();
            findReplaceHandler->SetMatchCase(&m_textEditor,!findReplaceHandler->GetMatchCase());
        });

        ShortcutManager::addShortcut(this, ALT + Keys::R + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.regex_toggle", [this] {
            TextEditor::FindReplaceHandler *findReplaceHandler = m_textEditor.GetFindReplaceHandler();
            findReplaceHandler->SetFindRegEx(&m_textEditor,!findReplaceHandler->GetFindRegEx());
        });

        ShortcutManager::addShortcut(this, ALT + Keys::W + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.whole_word_toggle", [this] {
            TextEditor::FindReplaceHandler *findReplaceHandler = m_textEditor.GetFindReplaceHandler();
            findReplaceHandler->SetWholeWord(&m_textEditor,!findReplaceHandler->GetWholeWord());
        });

        ShortcutManager::addShortcut(this, ALT + Keys::S + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.save_project", [] {
            hex::plugin::builtin::saveProject();
        });

        ShortcutManager::addShortcut(this, ALT + Keys::O + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.open_project", [] {
            hex::plugin::builtin::openProject();
        });

        ShortcutManager::addShortcut(this, ALT + SHIFT + Keys::S + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.save_project_as", [] {
            hex::plugin::builtin::saveProjectAs();
        });

     //   ShortcutManager::addShortcut(this, CTRL + Keys::Insert + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.copy", [this] {
     //       m_textEditor.Copy();
     //   });

        ShortcutManager::addShortcut(this, CTRL + Keys::C + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.copy", [this] {
            m_textEditor.Copy();
        });

     //   ShortcutManager::addShortcut(this, SHIFT + Keys::Insert + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.paste", [this] {
        //    m_textEditor.Paste();
    //    });

        ShortcutManager::addShortcut(this, CTRL + Keys::V + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.paste", [this] {
            m_textEditor.Paste();
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::X + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.cut", [this] {
            m_textEditor.Cut();
        });

      //  ShortcutManager::addShortcut(this, SHIFT + Keys::Delete + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.cut", [this] {
      //      m_textEditor.Cut();
      //  });

        ShortcutManager::addShortcut(this, CTRL + Keys::Z + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.undo", [this] {
            m_textEditor.Undo();
        });

      //  ShortcutManager::addShortcut(this, ALT + Keys::Backspace + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.undo", [this] {
    //        m_textEditor.Undo();
      //  });

        ShortcutManager::addShortcut(this, Keys::Delete + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.delete", [this] {
            m_textEditor.Delete();
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::Y + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.redo", [this] {
            m_textEditor.Redo();
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::A + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_all", [this] {
            m_textEditor.SelectAll();
        });

        ShortcutManager::addShortcut(this, SHIFT + Keys::Right + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_right", [this] {
            m_textEditor.MoveRight(1, true, false);
        });

        ShortcutManager::addShortcut(this, CTRL + SHIFT + Keys::Right + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_word_right", [this] {
            m_textEditor.MoveRight(1, true, true);
        });

        ShortcutManager::addShortcut(this, SHIFT + Keys::Left + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_left", [this] {
            m_textEditor.MoveLeft(1, true, false);
        });

        ShortcutManager::addShortcut(this, CTRL + SHIFT + Keys::Left + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_word_left", [this] {
            m_textEditor.MoveLeft(1, true, true);
        });

        ShortcutManager::addShortcut(this, SHIFT + Keys::Up + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_up", [this] {
            m_textEditor.MoveUp(1, true);
        });

        ShortcutManager::addShortcut(this, SHIFT +Keys::PageUp + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_page_up", [this] {
            m_textEditor.MoveUp(m_textEditor.GetPageSize()-4, true);
        });

        ShortcutManager::addShortcut(this, SHIFT + Keys::Down + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_down", [this] {
            m_textEditor.MoveDown(1, true);
        });

        ShortcutManager::addShortcut(this, SHIFT +Keys::PageDown + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_page_down", [this] {
            m_textEditor.MoveDown(m_textEditor.GetPageSize()-4, true);
        });

        ShortcutManager::addShortcut(this, CTRL + SHIFT + Keys::Home + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_top", [this] {
            m_textEditor.MoveTop(true);
        });

        ShortcutManager::addShortcut(this, CTRL + SHIFT + Keys::End + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_bottom", [this] {
            m_textEditor.MoveBottom(true);
        });

        ShortcutManager::addShortcut(this, SHIFT + Keys::Home + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_home", [this] {
            m_textEditor.MoveHome(true);
        });

        ShortcutManager::addShortcut(this, SHIFT + Keys::End + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_end", [this] {
            m_textEditor.MoveEnd(true);
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::Delete + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.delete_word_right", [this] {
            m_textEditor.DeleteWordRight();
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::Backspace + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.delete_word_left", [this] {
            m_textEditor.DeleteWordLeft();
        });

        ShortcutManager::addShortcut(this, Keys::Backspace + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.backspace", [this] {
            m_textEditor.Backspace();
        });

        ShortcutManager::addShortcut(this, Keys::Insert + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.toggle_insert", [this] {
            m_textEditor.SetOverwrite(!m_textEditor.IsOverwrite());
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::Right + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_word_right", [this] {
            m_textEditor.MoveRight(1, false, true);
        });

        ShortcutManager::addShortcut(this, Keys::Right + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_right", [this] {
            m_textEditor.MoveRight(1, false, false);
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::Left + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_word_left", [this] {
            m_textEditor.MoveLeft(1, false, true);
        });

        ShortcutManager::addShortcut(this, Keys::Left + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_left", [this] {
            m_textEditor.MoveLeft(1, false, false);
        });

        ShortcutManager::addShortcut(this, Keys::Up + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_up", [this] {
            m_textEditor.MoveUp(1, false);
        });

        ShortcutManager::addShortcut(this, Keys::PageUp + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_page_up", [this] {
            m_textEditor.MoveUp(m_textEditor.GetPageSize()-4, false);
        });

        ShortcutManager::addShortcut(this, Keys::Down + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_down", [this] {
            m_textEditor.MoveDown(1, false);
        });

        ShortcutManager::addShortcut(this, Keys::PageDown + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_page_down", [this] {
            m_textEditor.MoveDown(m_textEditor.GetPageSize()-4, false);
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::Home + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_top", [this] {
            m_textEditor.MoveTop(false);
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::End + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_bottom", [this] {
            m_textEditor.MoveBottom(false);
        });

        ShortcutManager::addShortcut(this, Keys::Home + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_home", [this] {
            m_textEditor.MoveHome(false);
        });

        ShortcutManager::addShortcut(this, Keys::End + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_end", [this] {
            m_textEditor.MoveEnd(false);
        });

        ShortcutManager::addShortcut(this, Keys::F8 + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.add_breakpoint", [this] {
            const auto line = m_textEditor.GetCursorPosition().mLine + 1;
            const auto &runtime = ContentRegistry::PatternLanguage::getRuntime();

            auto &evaluator = runtime.getInternals().evaluator;
            auto &breakpoints = evaluator->getBreakpoints();

            if (breakpoints.contains(line)) {
                evaluator->removeBreakpoint(line);
            } else {
                evaluator->addBreakpoint(line);
            }

            m_textEditor.SetBreakpoints(breakpoints);
        });

        /* Trigger evaluation */
        ShortcutManager::addGlobalShortcut(Keys::F5 + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.run_pattern", [this] {
            m_triggerAutoEvaluate = true;
        });

        /* Continue debugger */
        ShortcutManager::addGlobalShortcut(SHIFT + Keys::F9 + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.continue_debugger", [this] {
            const auto &runtime = ContentRegistry::PatternLanguage::getRuntime();
            if (runtime.isRunning())
                m_breakpointHit = false;
        });

        /* Step debugger */
        ShortcutManager::addGlobalShortcut(SHIFT + Keys::F7 + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.step_debugger", [this] {
            const auto &runtime = ContentRegistry::PatternLanguage::getRuntime();
            if (runtime.isRunning()) {
                runtime.getInternals().evaluator->pauseNextLine();
                m_breakpointHit = false;
            }
        });

        // Generate pattern code report
        ContentRegistry::Reports::addReportProvider([this](prv::Provider *provider) -> std::string {
            const auto &patternCode = m_sourceCode.get(provider);

            if (wolv::util::trim(patternCode).empty())
                return "";

            std::string result;

            result += "## Pattern Code\n\n";
            result += "```cpp\n";
            result += patternCode;
            result += "\n```\n\n";

            return result;
        });
    }

}