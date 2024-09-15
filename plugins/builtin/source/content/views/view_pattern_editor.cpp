#include "content/views/view_pattern_editor.hpp"
#include "fonts/blendericons_font.h"

#include <hex/api/content_registry.hpp>
#include <hex/api/project_file_manager.hpp>

#include <pl/patterns/pattern.hpp>
#include <pl/core/preprocessor.hpp>
#include <pl/core/parser.hpp>
#include <pl/core/ast/ast_node_variable_decl.hpp>
#include <pl/core/ast/ast_node_builtin_type.hpp>

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

namespace hex::plugin::builtin {

    using namespace hex::literals;

    static const TextEditor::LanguageDefinition &PatternLanguage() {
        static bool initialized = false;
        static TextEditor::LanguageDefinition langDef;
        if (!initialized) {
            constexpr static std::array keywords = {
                "using", "struct", "union", "enum", "bitfield", "be", "le", "if", "else", "match", "false", "true", "this", "parent", "addressof", "sizeof", "typenameof", "$", "while", "for", "fn", "return", "break", "continue", "namespace", "in", "out", "ref", "null", "const", "unsigned", "signed", "try", "catch", "import", "as"
            };
            for (auto &k : keywords)
                langDef.mKeywords.insert(k);

            constexpr static std::array builtInTypes = {
                "u8", "u16", "u24", "u32", "u48", "u64", "u96", "u128", "s8", "s16", "s24", "s32", "s48", "s64", "s96", "s128", "float", "double", "char", "char16", "bool", "padding", "str", "auto"
            };
            for (const auto name : builtInTypes) {
                TextEditor::Identifier id;
                id.mDeclaration = "";
                langDef.mIdentifiers.insert(std::make_pair(std::string(name), id));
            }

            langDef.mTokenize = [](const char *inBegin, const char *inEnd, const char *&outBegin, const char *&outEnd, TextEditor::PaletteIndex &paletteIndex) -> bool {
                paletteIndex = TextEditor::PaletteIndex::Max;

                while (inBegin < inEnd && isascii(*inBegin) && std::isblank(*inBegin))
                    inBegin++;

                if (inBegin == inEnd) {
                    outBegin     = inEnd;
                    outEnd       = inEnd;
                    paletteIndex = TextEditor::PaletteIndex::Default;
                } else if (TokenizeCStyleIdentifier(inBegin, inEnd, outBegin, outEnd)) {
                    paletteIndex = TextEditor::PaletteIndex::Identifier;
                } else if (TokenizeCStyleNumber(inBegin, inEnd, outBegin, outEnd)) {
                    paletteIndex = TextEditor::PaletteIndex::Number;
                } else if (TokenizeCStyleCharacterLiteral(inBegin, inEnd, outBegin, outEnd)) {
                    paletteIndex = TextEditor::PaletteIndex::CharLiteral;
                } else if (TokenizeCStyleString(inBegin, inEnd, outBegin, outEnd)) {
                    paletteIndex = TextEditor::PaletteIndex::String;
                }

                return paletteIndex != TextEditor::PaletteIndex::Max;
            };

            langDef.mCommentStart      = "/*";
            langDef.mCommentEnd        = "*/";
            langDef.mSingleLineComment = "//";

            langDef.mCaseSensitive   = true;
            langDef.mAutoIndentation = true;
            langDef.mPreprocChar     = '#';

            langDef.mGlobalDocComment = "/*!";
            langDef.mDocComment      = "/**";

            langDef.mName = "Pattern Language";

            initialized = true;
        }

        return langDef;
    }

    static const TextEditor::LanguageDefinition &ConsoleLog() {
        static bool initialized = false;
        static TextEditor::LanguageDefinition langDef;
        if (!initialized) {
            langDef.mTokenize = [](const char *inBegin, const char *inEnd, const char *&outBegin, const char *&outEnd, TextEditor::PaletteIndex &paletteIndex) -> bool {
                if (std::string_view(inBegin).starts_with("D: "))
                    paletteIndex = TextEditor::PaletteIndex::Comment;
                else if (std::string_view(inBegin).starts_with("I: "))
                    paletteIndex = TextEditor::PaletteIndex::Default;
                else if (std::string_view(inBegin).starts_with("W: "))
                    paletteIndex = TextEditor::PaletteIndex::Preprocessor;
                else if (std::string_view(inBegin).starts_with("E: "))
                    paletteIndex = TextEditor::PaletteIndex::ErrorMarker;
                else
                    paletteIndex = TextEditor::PaletteIndex::Max;

                outBegin = inBegin;
                outEnd = inEnd;

                return true;
            };

            langDef.mName = "Console Log";
            langDef.mCaseSensitive   = false;
            langDef.mAutoIndentation = false;
            langDef.mCommentStart = "";
            langDef.mCommentEnd = "";
            langDef.mSingleLineComment = "";
            langDef.mDocComment = "";
            langDef.mGlobalDocComment = "";

            initialized = true;
        }
        return langDef;
    }

    int levelId;

    static void loadPatternAsMemoryProvider(const ViewPatternEditor::VirtualFile *file) {
        ImHexApi::Provider::add<prv::MemoryProvider>(file->data, wolv::util::toUTF8String(file->path.filename()));
    }

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
                ImGui::PushID(levelId);
                ImGui::SameLine();

                ImGui::TreeNodeEx(currSegment.c_str(), ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen);
                if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && ImGui::IsItemHovered() && !ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
                    ImGui::OpenPopup("##virtual_files_context_menu");
                }
                if (ImGui::BeginPopup("##virtual_files_context_menu")) {
                    if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.open_in_new_provider"_lang, nullptr, false)) {
                        loadPatternAsMemoryProvider(file);
                    }
                    ImGui::EndPopup();
                }
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered()) {
                    loadPatternAsMemoryProvider(file);
                }
                ImGui::PopID();
                levelId +=1;
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

        m_textEditor.SetLanguageDefinition(PatternLanguage());
        m_textEditor.SetShowWhitespaces(false);

        m_consoleEditor.SetLanguageDefinition(ConsoleLog());
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

    void ViewPatternEditor::setupFindReplace(TextEditor *editor) {

        // Context menu entries that open the find/replace popup
        ImGui::PushID(editor);
        static std::string findWord;
        static bool requestFocus = false;
        static u64 position = 0;
        static u64 count = 0;
        static bool updateCount = false;
        TextEditor::FindReplaceHandler *findReplaceHandler = editor->GetFindReplaceHandler();
        static bool canReplace = true;
        if (m_openFindReplacePopUp) {
            m_openFindReplacePopUp = false;
            // Place the popup at the top right of the window
            auto windowSize = ImGui::GetWindowSize();
            auto style = ImGui::GetStyle();

            // Set the scrollbar size only if it is visible
            float scrollbarSize = 0;

            // Calculate the number of lines to display in the text editor
            auto totalTextHeight =  editor->GetTotalLines() * editor->GetCharAdvance().y;

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

            if (m_focusedSubWindowName.contains(consoleView)) {
                windowPosForPopup.y = m_consoleHoverBox.Min.y;
                canReplace = false;
            } else if (m_focusedSubWindowName.contains(textEditorView))
                canReplace = true;

            ImGui::SetNextWindowPos(windowPosForPopup);
            ImGui::SetNextWindowSize(popupSize);
            ImGui::OpenPopup("##pattern_editor_find_replace");

            findReplaceHandler->resetMatches();

            // Use selection as find word if there is one, otherwise use the word under the cursor
            if (!editor->HasSelection())
                editor->SelectWordUnderCursor();

            findWord = editor->GetSelectedText();

            // Find all matches to findWord
            findReplaceHandler->FindAllMatches(editor,findWord);
            position = findReplaceHandler->FindPosition(editor,editor->GetCursorPosition(), true);
            count = findReplaceHandler->GetMatches().size();
            findReplaceHandler->SetFindWord(editor,findWord);
            requestFocus = true;
            updateCount = true;
        }

        drawFindReplaceDialog(editor, findWord, requestFocus, position, count, updateCount, canReplace);

        ImGui::PopID();
    }

    void ViewPatternEditor::drawContent() {
        auto provider = ImHexApi::Provider::get();

        if (ImHexApi::Provider::isValid() && provider->isAvailable()) {
            static float height = 0;
            static bool dragging = false;

            const auto availableSize = ImGui::GetContentRegionAvail();
            const auto windowPosition = ImGui::GetCursorScreenPos();
            auto textEditorSize = availableSize;
            textEditorSize.y *= 3.5 / 5.0;
            textEditorSize.y -= ImGui::GetTextLineHeightWithSpacing();
            textEditorSize.y += height;

            if (availableSize.y > 1)
                textEditorSize.y = std::clamp(textEditorSize.y, 1.0F, std::max(1.0F, availableSize.y - ImGui::GetTextLineHeightWithSpacing() * 3));
            const ImGuiContext& g = *GImGui;
            if (g.NavWindow != nullptr) {
                std::string name =  g.NavWindow->Name;
                if (name.contains(textEditorView) || name.contains(consoleView))
                    m_focusedSubWindowName = name;
            }
            m_textEditor.Render("hex.builtin.view.pattern_editor.name"_lang, textEditorSize, true);
            m_textEditorHoverBox = ImRect(windowPosition,windowPosition+textEditorSize);
            m_consoleHoverBox = ImRect(ImVec2(windowPosition.x,windowPosition.y+textEditorSize.y),windowPosition+availableSize);
            TextEditor::FindReplaceHandler *findReplaceHandler = m_textEditor.GetFindReplaceHandler();
            if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && ImGui::IsMouseHoveringRect(m_textEditorHoverBox.Min,m_textEditorHoverBox.Max) && !ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
                ImGui::OpenPopup("##pattern_editor_context_menu");
            }

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
                if (ImGui::MenuItem("hex.builtin.view.pattern_editor.menu.find"_lang, Shortcut(CTRLCMD + Keys::F).toString().c_str(),false, m_textEditor.HasSelection())){
                    m_replaceMode = false;
                    m_openFindReplacePopUp = true;
                }


                if (ImGui::MenuItem("hex.builtin.view.pattern_editor.menu.find_next"_lang, Shortcut(Keys::F3).toString().c_str(),false,!findReplaceHandler->GetFindWord().empty()))
                    findReplaceHandler->FindMatch(&m_textEditor,true);

                if (ImGui::MenuItem("hex.builtin.view.pattern_editor.menu.find_previous"_lang, Shortcut(SHIFT + Keys::F3).toString().c_str(),false,!findReplaceHandler->GetFindWord().empty()))
                    findReplaceHandler->FindMatch(&m_textEditor,false);

                if (ImGui::MenuItem("hex.builtin.view.pattern_editor.menu.replace"_lang, Shortcut(CTRLCMD +  Keys::H).toString().c_str(),false,!findReplaceHandler->GetReplaceWord().empty())) {
                    m_replaceMode = true;
                    m_openFindReplacePopUp = true;
                }

                if (ImGui::MenuItem("hex.builtin.view.pattern_editor.menu.replace_next"_lang,"",false,!findReplaceHandler->GetReplaceWord().empty()))
                    findReplaceHandler->Replace(&m_textEditor,true);

                if (ImGui::MenuItem("hex.builtin.view.pattern_editor.menu.replace_previous"_lang, "",false,!findReplaceHandler->GetReplaceWord().empty()))
                    findReplaceHandler->Replace(&m_textEditor,false);

                if (ImGui::MenuItem("hex.builtin.view.pattern_editor.menu.replace_all"_lang, "",false,!findReplaceHandler->GetReplaceWord().empty()))
                    findReplaceHandler->ReplaceAll(&m_textEditor);

                ImGui::EndPopup();
            }


            setupFindReplace(getEditorFromFocusedWindow());

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
                m_hasUnevaluatedChanges = true;
                m_lastEditorChangeTime = std::chrono::steady_clock::now();
                ImHexApi::Provider::markDirty();
            }

            if (m_hasUnevaluatedChanges && m_runningEvaluators == 0 && m_runningParsers == 0) {
                if ((std::chrono::steady_clock::now() - m_lastEditorChangeTime) > std::chrono::seconds(1LL)) {
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

    void ViewPatternEditor::drawFindReplaceDialog(TextEditor *textEditor, std::string &findWord, bool &requestFocus, u64 &position, u64 &count, bool &updateCount, bool canReplace) {

        TextEditor::FindReplaceHandler *findReplaceHandler = textEditor->GetFindReplaceHandler();

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
                static bool requestFocusFind = false;
                static bool requestFocusReplace = false;
                bool oldReplace = m_replaceMode;

                if (canReplace)
                    ImGuiExt::DimmedIconToggle(ICON_VS_TRIANGLE_DOWN, ICON_VS_TRIANGLE_RIGHT, &m_replaceMode);
                else
                    m_replaceMode = false;
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
                static bool enterPressedFind = false;
                ImGui::PushItemWidth(ImGui::GetFontSize() * 12);
                if (ImGui::InputTextWithHint("###findInputTextWidget", hint.c_str(), findWord, findFlags) || enter ) {
                    if (enter)
                        enterPressedFind = true;

                    updateCount = true;
                    requestFocusFind = true;
                    findReplaceHandler->SetFindWord(textEditor,findWord);
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
                    findReplaceHandler->SetFindWord(textEditor,findWord);
                    position = findReplaceHandler->FindPosition(textEditor,textEditor->GetCursorPosition(), true);
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
                    findReplaceHandler->SetMatchCase(textEditor,matchCase);
                    position = findReplaceHandler->FindPosition(textEditor,textEditor->GetCursorPosition(), true);
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
                    findReplaceHandler->SetWholeWord(textEditor,matchWholeWord);
                    position = findReplaceHandler->FindPosition(textEditor,textEditor->GetCursorPosition(), true);
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
                    findReplaceHandler->SetFindRegEx(textEditor,useRegex);
                    position = findReplaceHandler->FindPosition(textEditor,textEditor->GetCursorPosition(), true);
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
                static bool downArrowFind = false;
                if (ImGuiExt::IconButton(ICON_VS_ARROW_DOWN, ImVec4(1, 1, 1, 1)))
                    downArrowFind = true;

                ImGui::SameLine();
                static bool upArrowFind = false;
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
                    static std::string replaceWord;
                    static bool downArrowReplace = false;
                    static bool upArrowReplace = false;
                    if (ImGui::InputTextWithHint("##replaceInputTextWidget", hint.c_str(), replaceWord, replaceFlags) || downArrowReplace || upArrowReplace) {
                        findReplaceHandler->SetReplaceWord(replaceWord);
                        historyInsert(m_replaceHistory, m_replaceHistorySize, m_replaceHistoryIndex, replaceWord);

                        bool textReplaced = findReplaceHandler->Replace(textEditor,!shift && !upArrowReplace);
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
                        findReplaceHandler->ReplaceAll(textEditor);
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
                    position = findReplaceHandler->FindMatch(textEditor,!shift && !upArrowFind);
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

    void ViewPatternEditor::drawConsole(ImVec2 size) {
        auto findReplaceHandler = m_consoleEditor.GetFindReplaceHandler();
        if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && ImGui::IsMouseHoveringRect(m_consoleHoverBox.Min,m_consoleHoverBox.Max) && !ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            ImGui::OpenPopup("##console_context_menu");
        }
        const bool hasSelection = m_consoleEditor.HasSelection();
        if (ImGui::BeginPopup("##console_context_menu")) {
            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.copy"_lang, Shortcut(CTRLCMD + Keys::C).toString().c_str(), false, hasSelection)) {
                m_consoleEditor.Copy();
            }
            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.select_all"_lang, Shortcut(CTRLCMD + Keys::A).toString().c_str())) {
                m_consoleEditor.SelectAll();
            }
            ImGui::Separator();
            // Search and replace entries
            if (ImGui::MenuItem("hex.builtin.view.pattern_editor.menu.find"_lang, Shortcut(CTRLCMD + Keys::F).toString().c_str(),false, hasSelection)) {
                m_openFindReplacePopUp = true;
                m_replaceMode = false;
            }

            if (ImGui::MenuItem("hex.builtin.view.pattern_editor.menu.find_next"_lang, Shortcut(Keys::F3).toString().c_str(),false,!findReplaceHandler->GetFindWord().empty()))
                findReplaceHandler->FindMatch(&m_consoleEditor,true);

            if (ImGui::MenuItem("hex.builtin.view.pattern_editor.menu.find_previous"_lang, Shortcut(SHIFT + Keys::F3).toString().c_str(),false,!findReplaceHandler->GetFindWord().empty()))
                findReplaceHandler->FindMatch(&m_consoleEditor,false);
            ImGui::EndPopup();
        }
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
                m_consoleEditor.InsertText(m_console->at(lineCount + i));
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
                    ImGui::SetItemTooltip("%s", "hex.builtin.view.pattern_editor.sections.export"_lang.get());

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
            levelId = 1;
            drawVirtualFileTree(virtualFilePointers);

            ImGui::EndTable();
        }
    }


    void ViewPatternEditor::drawDebugger(ImVec2 size) {
        const auto &runtime = ContentRegistry::PatternLanguage::getRuntime();


        if (ImGui::BeginChild("##debugger", size, true)) {
            auto &evaluator = runtime.getInternals().evaluator;
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
                        value.resize(end);
                        //value = value.substr(0, end);
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
            auto code = file.readString();

            this->evaluatePattern(code, provider);
            m_textEditor.SetText(code);
            m_sourceCode.set(provider, code);

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

        m_runningParsers -= 1;
    }

    void ViewPatternEditor::evaluatePattern(const std::string &code, prv::Provider *provider) {
        EventPatternEvaluating::post();

        auto lock = std::scoped_lock(ContentRegistry::PatternLanguage::getRuntimeLock());

        m_runningEvaluators += 1;
        *m_executionDone = false;


        m_textEditor.SetErrorMarkers({});
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
                    std::this_thread::sleep_for(std::chrono::milliseconds(100LL));
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
                    std::this_thread::sleep_for(std::chrono::milliseconds(100LL));
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
            m_textEditor.SetText(code);
            m_sourceCode.set(ImHexApi::Provider::get(), code);
            m_hasUnevaluatedChanges = true;
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
            m_cursorPosition.get(provider) =  TextEditor::Coordinates(0, 0);
        });

        EventProviderChanged::subscribe(this, [this](prv::Provider *oldProvider, prv::Provider *newProvider) {
            if (oldProvider != nullptr) {
                m_sourceCode.set(oldProvider, m_textEditor.GetText());
                m_cursorPosition.set(m_textEditor.GetCursorPosition(),oldProvider);
            }

            if (newProvider != nullptr) {
                m_textEditor.SetText(m_sourceCode.get(newProvider));
                m_textEditor.SetCursorPosition(m_cursorPosition.get(newProvider));
            } else
                m_textEditor.SetText("");
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

    TextEditor *ViewPatternEditor::getEditorFromFocusedWindow() {
        if (m_focusedSubWindowName.contains(consoleView)) {
            return &m_consoleEditor;
        }
        if (m_focusedSubWindowName.contains(textEditorView)) {
            return &m_textEditor;
        }
        return nullptr;
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
                const auto sourceCode = tar.readString(basePath);

                m_sourceCode.set(provider, sourceCode);

                if (provider == ImHexApi::Provider::get())
                    m_textEditor.SetText(sourceCode);

                m_hasUnevaluatedChanges = true;

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
            if (m_focusedSubWindowName.contains(textEditorView)) {
                m_openFindReplacePopUp = true;
                m_replaceMode = true;
            }
        });

        ShortcutManager::addShortcut(this, Keys::F3 + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.find_next", [this] {

                if (auto editor = getEditorFromFocusedWindow(); editor != nullptr) {
                    TextEditor::FindReplaceHandler *findReplaceHandler = editor->GetFindReplaceHandler();
                    findReplaceHandler->FindMatch(editor, true);
                }
        });

        ShortcutManager::addShortcut(this, SHIFT + Keys::F3 + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.find_previous", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr) {
                TextEditor::FindReplaceHandler *findReplaceHandler = editor->GetFindReplaceHandler();
                findReplaceHandler->FindMatch(editor, false);
            }
        });

        ShortcutManager::addShortcut(this, ALT + Keys::C + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.match_case_toggle", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr) {
                TextEditor::FindReplaceHandler *findReplaceHandler = editor->GetFindReplaceHandler();
                findReplaceHandler->SetMatchCase(editor, !findReplaceHandler->GetMatchCase());
            }
        });

        ShortcutManager::addShortcut(this, ALT + Keys::R + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.regex_toggle", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr) {
                TextEditor::FindReplaceHandler *findReplaceHandler = editor->GetFindReplaceHandler();
                findReplaceHandler->SetFindRegEx(editor, !findReplaceHandler->GetFindRegEx());
            }
        });

        ShortcutManager::addShortcut(this, ALT + Keys::W + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.whole_word_toggle", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr) {
                TextEditor::FindReplaceHandler *findReplaceHandler = editor->GetFindReplaceHandler();
                findReplaceHandler->SetWholeWord(editor, !findReplaceHandler->GetWholeWord());
            }
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
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->Copy();
        });

     //   ShortcutManager::addShortcut(this, SHIFT + Keys::Insert + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.paste", [this] {
        //    m_textEditor.Paste();
    //    });

        ShortcutManager::addShortcut(this, CTRL + Keys::V + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.paste", [this] {
            if (m_focusedSubWindowName.contains(textEditorView))
                m_textEditor.Paste();
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::X + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.cut", [this] {
            if (m_focusedSubWindowName.contains(textEditorView))
                m_textEditor.Cut();
        });

      //  ShortcutManager::addShortcut(this, SHIFT + Keys::Delete + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.cut", [this] {
      //      m_textEditor.Cut();
      //  });

        ShortcutManager::addShortcut(this, CTRL + Keys::Z + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.undo", [this] {
            if (m_focusedSubWindowName.contains(textEditorView))
                m_textEditor.Undo();
        });

      //  ShortcutManager::addShortcut(this, ALT + Keys::Backspace + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.undo", [this] {
    //        m_textEditor.Undo();
      //  });

        ShortcutManager::addShortcut(this, Keys::Delete + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.delete", [this] {
            if (m_focusedSubWindowName.contains(textEditorView))
                m_textEditor.Delete();
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::Y + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.redo", [this] {
            if (m_focusedSubWindowName.contains(textEditorView))
                m_textEditor.Redo();
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::A + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_all", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->SelectAll();
        });

        ShortcutManager::addShortcut(this, SHIFT + Keys::Right + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_right", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->MoveRight(1, true, false);
        });

        ShortcutManager::addShortcut(this, CTRL + SHIFT + Keys::Right + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_word_right", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->MoveRight(1, true, true);
        });

        ShortcutManager::addShortcut(this, SHIFT + Keys::Left + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_left", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->MoveLeft(1, true, false);
        });

        ShortcutManager::addShortcut(this, CTRL + SHIFT + Keys::Left + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_word_left", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->MoveLeft(1, true, true);
        });

        ShortcutManager::addShortcut(this, SHIFT + Keys::Up + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_up", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->MoveUp(1, true);
        });

        ShortcutManager::addShortcut(this, SHIFT +Keys::PageUp + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_page_up", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->MoveUp(editor->GetPageSize()-4, true);
        });

        ShortcutManager::addShortcut(this, SHIFT + Keys::Down + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_down", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->MoveDown(1, true);
        });

        ShortcutManager::addShortcut(this, SHIFT +Keys::PageDown + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_page_down", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->MoveDown(editor->GetPageSize()-4, true);
        });

        ShortcutManager::addShortcut(this, CTRL + SHIFT + Keys::Home + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_top", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->MoveTop(true);
        });

        ShortcutManager::addShortcut(this, CTRL + SHIFT + Keys::End + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_bottom", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->MoveBottom(true);
        });

        ShortcutManager::addShortcut(this, SHIFT + Keys::Home + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_home", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->MoveHome(true);
        });

        ShortcutManager::addShortcut(this, SHIFT + Keys::End + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_end", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->MoveEnd(true);
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::Delete + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.delete_word_right", [this] {
            if (m_focusedSubWindowName.contains(textEditorView))
                m_textEditor.DeleteWordRight();
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::Backspace + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.delete_word_left", [this] {
            if (m_focusedSubWindowName.contains(textEditorView))
                m_textEditor.DeleteWordLeft();
        });

        ShortcutManager::addShortcut(this, Keys::Backspace + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.backspace", [this] {
            if (m_focusedSubWindowName.contains(textEditorView))
                m_textEditor.Backspace();
        });

        ShortcutManager::addShortcut(this, Keys::Insert + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.toggle_insert", [this] {
            if (m_focusedSubWindowName.contains(textEditorView))
                m_textEditor.SetOverwrite(!m_textEditor.IsOverwrite());
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::Right + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_word_right", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->MoveRight(1, false, true);
        });

        ShortcutManager::addShortcut(this, Keys::Right + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_right", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->MoveRight(1, false, false);
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::Left + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_word_left", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->MoveLeft(1, false, true);
        });

        ShortcutManager::addShortcut(this, Keys::Left + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_left", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->MoveLeft(1, false, false);
        });

        ShortcutManager::addShortcut(this, Keys::Up + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_up", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->MoveUp(1, false);
        });

        ShortcutManager::addShortcut(this, Keys::PageUp + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_page_up", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->MoveUp(editor->GetPageSize()-4, false);
        });

        ShortcutManager::addShortcut(this, Keys::Down + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_down", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->MoveDown(1, false);
        });

        ShortcutManager::addShortcut(this, Keys::PageDown + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_page_down", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->MoveDown(editor->GetPageSize()-4, false);
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::Home + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_top", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->MoveTop(false);
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::End + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_bottom", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->MoveBottom(false);
        });

        ShortcutManager::addShortcut(this, Keys::Home + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_home", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->MoveHome(false);
        });

        ShortcutManager::addShortcut(this, Keys::End + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_end", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->MoveEnd(false);
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