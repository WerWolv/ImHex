#include "content/views/view_pattern_editor.hpp"
#include <fonts/blender_icons.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/api/project_file_manager.hpp>

#include <hex/api/events/events_provider.hpp>
#include <hex/api/events/events_gui.hpp>
#include <hex/api/events/requests_interaction.hpp>

#include <pl/patterns/pattern.hpp>
#include <pl/core/lexer.hpp>
#include <pl/core/ast/ast_node_variable_decl.hpp>
#include <pl/core/ast/ast_node_builtin_type.hpp>

#include <hex/helpers/fs.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/magic.hpp>
#include <hex/helpers/binary_pattern.hpp>
#include <hex/helpers/default_paths.hpp>
#include <banners/banner_button.hpp>

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
#include <fonts/fonts.hpp>
#include <hex/api/events/requests_gui.hpp>
#include <hex/helpers/menu_items.hpp>

namespace hex::plugin::builtin {

    using namespace hex::literals;

    static const ui::TextEditor::LanguageDefinition &PatternLanguage() {
        static bool initialized = false;
        static ui::TextEditor::LanguageDefinition langDef;
        if (!initialized) {
            constexpr static std::array keywords = {
                "using", "struct", "union", "enum", "bitfield", "be", "le", "if", "else", "match", "false", "true", "this", "parent", "addressof", "sizeof", "typenameof", "while", "for", "fn", "return", "break", "continue", "namespace", "in", "out", "ref", "null", "const", "unsigned", "signed", "try", "catch", "import", "as", "from"
            };
            for (auto &k : keywords)
                langDef.m_keywords.insert(k);

            constexpr static std::array builtInTypes = {
                "u8", "u16", "u24", "u32", "u48", "u64", "u96", "u128", "s8", "s16", "s24", "s32", "s48", "s64", "s96", "s128", "float", "double", "char", "char16", "bool", "padding", "str", "auto"
            };
            for (const auto name : builtInTypes) {
                ui::TextEditor::Identifier id;
                id.m_declaration = "";
                langDef.m_identifiers.insert(std::make_pair(std::string(name), id));
            }
            constexpr static std::array directives = {
                    "include", "define", "ifdef", "ifndef", "endif", "undef", "pragma", "error"
            };
            for (const auto name : directives) {
                ui::TextEditor::Identifier id;
                id.m_declaration = "";
                langDef.m_preprocIdentifiers.insert(std::make_pair(std::string(name), id));
            }
            langDef.m_tokenize = [](std::string::const_iterator inBegin, std::string::const_iterator inEnd, std::string::const_iterator &outBegin, std::string::const_iterator &outEnd, ui::TextEditor::PaletteIndex &paletteIndex) -> bool {
                paletteIndex = ui::TextEditor::PaletteIndex::Max;

                while (inBegin < inEnd && isascii(*inBegin) && std::isblank(*inBegin))
                    ++inBegin;

                if (inBegin == inEnd) {
                    outBegin     = inEnd;
                    outEnd       = inEnd;
                    paletteIndex = ui::TextEditor::PaletteIndex::Default;
                } else if (ui::tokenizeCStyleIdentifier(inBegin, inEnd, outBegin, outEnd)) {
                    paletteIndex = ui::TextEditor::PaletteIndex::Identifier;
                } else if (ui::tokenizeCStyleNumber(inBegin, inEnd, outBegin, outEnd)) {
                    paletteIndex = ui::TextEditor::PaletteIndex::NumericLiteral;
                } else if (ui::tokenizeCStyleCharacterLiteral(inBegin, inEnd, outBegin, outEnd)) {
                    paletteIndex = ui::TextEditor::PaletteIndex::CharLiteral;
                } else if (ui::tokenizeCStyleString(inBegin, inEnd, outBegin, outEnd)) {
                    paletteIndex = ui::TextEditor::PaletteIndex::StringLiteral;
                } else if (ui::tokenizeCStyleSeparator(inBegin, inEnd, outBegin, outEnd)) {
                    paletteIndex = ui::TextEditor::PaletteIndex::Separator;
                } else if (ui::tokenizeCStyleOperator(inBegin, inEnd, outBegin, outEnd)) {
                    paletteIndex = ui::TextEditor::PaletteIndex::Operator;
                }
                return paletteIndex != ui::TextEditor::PaletteIndex::Max;
            };

            langDef.m_commentStart      = "/*";
            langDef.m_commentEnd        = "*/";
            langDef.m_singleLineComment = "//";

            langDef.m_caseSensitive   = true;
            langDef.m_autoIndentation = true;
            langDef.m_preprocChar     = '#';

            langDef.m_globalDocComment    = "/*!";
            langDef.m_blockDocComment     = "/**";
            langDef.m_docComment          = "///";

            langDef.m_name = "Pattern Language";

            initialized = true;
        }

        return langDef;
    }

    static const ui::TextEditor::LanguageDefinition &ConsoleLog() {
        static bool initialized = false;
        static ui::TextEditor::LanguageDefinition langDef;
        if (!initialized) {
            langDef.m_tokenize = [](std::string::const_iterator inBegin, std::string::const_iterator inEnd, std::string::const_iterator &outBegin, std::string::const_iterator &outEnd, ui::TextEditor::PaletteIndex &paletteIndex) -> bool {
                std::string_view inView(inBegin, inEnd);
                if (inView.starts_with("D: "))
                    paletteIndex = ui::TextEditor::PaletteIndex::DefaultText;
                else if (inView.starts_with("I: "))
                    paletteIndex = ui::TextEditor::PaletteIndex::DebugText;
                else if (inView.starts_with("W: "))
                    paletteIndex = ui::TextEditor::PaletteIndex::WarningText;
                else if (inView.starts_with("E: "))
                    paletteIndex = ui::TextEditor::PaletteIndex::ErrorText;
                else
                    paletteIndex = ui::TextEditor::PaletteIndex::Max;

                outBegin = inBegin;
                outEnd = inEnd;

                return true;
            };

            langDef.m_name = "Console Log";
            langDef.m_caseSensitive   = false;
            langDef.m_autoIndentation = false;
            langDef.m_commentStart = "";
            langDef.m_commentEnd = "";
            langDef.m_singleLineComment = "";
            langDef.m_docComment = "";
            langDef.m_globalDocComment = "";

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

    void ViewPatternEditor::setupFindReplace(ui::TextEditor *editor) {
        if (editor == nullptr)
            return;
        if (m_openFindReplacePopUp) {
            m_openFindReplacePopUp = false;
            auto style = ImGui::GetStyle();

            auto labelSize = ImGui::CalcTextSize(ICON_VS_WHOLE_WORD);

            // Six icon buttons
            auto popupSizeX =  (labelSize.x + style.FramePadding.x * 2.0F) * 6.0F;

            // 2 * 9 spacings in between elements
            popupSizeX += style.FramePadding.x * 18.0F;

            // Text input fields are set to 12 characters wide
            popupSizeX += ImGui::GetFontSize() * 12.0F;

            // IndexOfCount text
            popupSizeX +=  ImGui::CalcTextSize("2000 of 2000").x + 2.0F;
            popupSizeX += style.FramePadding.x * 2.0F;

            // Position of popup relative to parent window
            ImVec2 windowPosForPopup;
            windowPosForPopup.x = m_textEditorHoverBox.Max.x - style.ScrollbarSize - popupSizeX;

            if (m_focusedSubWindowName.contains(consoleView))
                windowPosForPopup.y = m_consoleHoverBox.Min.y;
            else if (m_focusedSubWindowName.contains(textEditorView))
                windowPosForPopup.y = m_textEditorHoverBox.Min.y;
            else
                return;
            ImGui::SetNextWindowPos(windowPosForPopup);
            ImGui::OpenPopup("##text_editor_view_find_replace_popup");
        }
        drawTextEditorFindReplacePopup(editor);

    }

    void ViewPatternEditor::setupGotoLine(ui::TextEditor *editor) {

        // Context menu entries that open the goto line popup
        if (m_openGotoLinePopUp) {
            m_openGotoLinePopUp = false;
            // Place the popup at the top right of the window
            auto style = ImGui::GetStyle();


            auto labelSize = ImGui::CalcTextSize("hex.builtin.view.pattern_editor.goto_line"_lang);

            auto popupSizeX =  (labelSize.x + style.FramePadding.x * 2.0F);


            // Text input fields are set to 8 characters wide
            popupSizeX += ImGui::CalcTextSize("00000000").x;

            popupSizeX += style.WindowPadding.x * 2.0F;
            // Position of popup relative to parent window
            ImVec2 windowPosForPopup;


            // Move to the right so as not to overlap the scrollbar
            windowPosForPopup.x = m_textEditorHoverBox.Max.x - style.ScrollbarSize - popupSizeX;

            if (m_focusedSubWindowName.contains(consoleView)) {
                windowPosForPopup.y = m_consoleHoverBox.Min.y;
            } else if (m_focusedSubWindowName.contains(textEditorView)) {
                windowPosForPopup.y = m_textEditorHoverBox.Min.y;
            } else {
                return;
            }

            ImGui::SetNextWindowPos(windowPosForPopup);
            ImGui::OpenPopup("##text_editor_view_goto_line_popup");

        }
        drawTextEditorGotoLinePopup(editor);
    }

    void ViewPatternEditor::drawContent() {
        auto provider = ImHexApi::Provider::get();

        if (ImHexApi::Provider::isValid() && provider->isAvailable()) {
            static float height = 0;
            const ImGuiContext& g = *ImGui::GetCurrentContext();
            if (g.CurrentWindow->Appearing)
                return;
            const auto availableSize = ImVec2(g.CurrentWindow->Size.x, g.CurrentWindow->Size.y - 2 * ImGui::GetFrameHeightWithSpacing());
            const auto windowPosition = ImGui::GetCursorScreenPos();
            static ImVec2 oldTextEditorSize = ImVec2(0, 0);
            ImVec2 textEditorSize = ImVec2(availableSize.x, oldTextEditorSize.x == 0 ? availableSize.y * 3.0F / 5.0F : oldTextEditorSize.y);
            static ImVec2 oldAvailableSize = ImVec2(0, 0);
            if (availableSize.y != oldAvailableSize.y && oldAvailableSize.y != 0) {
                float factor = availableSize.y / oldAvailableSize.y;
                height *= factor;
            }

            static float oldHeight = 0;
            auto heightIncrement = height - oldHeight;
            if (heightIncrement != 0) {
                float smallestTexEditorHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ScrollbarSize;
                float largestTextEditorHeight = availableSize.y - ImGui::GetTextLineHeightWithSpacing() - 2 * ImGui::GetFrameHeightWithSpacing() - ImGui::GetStyle().ScrollbarSize;
                textEditorSize.y = std::clamp(textEditorSize.y + heightIncrement, smallestTexEditorHeight, largestTextEditorHeight);
            }

            oldAvailableSize = availableSize;
            oldTextEditorSize = textEditorSize;
            oldHeight = height;
            if (g.NavWindow != nullptr) {
                std::string name =  g.NavWindow->Name;
                if (name.contains(textEditorView) || name.contains(consoleView) || name.contains(variablesView) || name.contains(settingsView) || name.contains(virtualFilesView) || name.contains(debuggerView))
                    m_focusedSubWindowName = name;
            }

            fonts::CodeEditor().push();
            m_textEditor.get(provider).render("##pattern_editor", textEditorSize, false);
            fonts::CodeEditor().pop();

            m_textEditorHoverBox = ImRect(windowPosition,windowPosition+textEditorSize);
            m_consoleHoverBox = ImRect(ImVec2(windowPosition.x,windowPosition.y+textEditorSize.y),windowPosition+availableSize);

            if (m_textEditor.get(provider).raiseContextMenu())  {
                RequestOpenPopup::post("hex.builtin.menu.edit");
                m_textEditor.get(provider).clearRaiseContextMenu();

                if (!m_textEditor.get(provider).hasSelection())
                    m_textEditor.get(provider).selectWordUnderCursor();
            }

            if (m_cursorNeedsUpdate.get(provider)) {
                m_textEditor.get(provider).setFocusAtCoords(m_cursorPosition.get(provider));
                m_cursorNeedsUpdate.get(provider) = false;
            }

            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr) {
                setupFindReplace(editor);
                setupGotoLine(editor);
            }

            static bool dragging = false;
            ImGui::Button("##settings_drag_bar", ImVec2(ImGui::GetContentRegionAvail().x, 4_scaled));
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
                const auto startY = ImGui::GetCursorPosY();

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
                if (ImGui::BeginTabItem("hex.builtin.view.pattern_editor.virtual_files"_lang)) {
                    this->drawVirtualFiles(settingsSize, *m_virtualFiles);
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("hex.builtin.view.pattern_editor.debugger"_lang)) {
                    this->drawDebugger(settingsSize);
                    ImGui::EndTabItem();
                }

                ImGui::SetCursorPosY(startY + settingsSize.y + ImGui::GetStyle().ItemSpacing.y * 2);

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
                        this->evaluatePattern(m_textEditor.get(provider).getText(), provider);
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
                    ImGui::SameLine(0, 10_scaled);
                    if (ImGuiExt::DimmedIconToggle(ICON_VS_EDIT_SPARKLE, &m_runAutomatically)) {
                        if (m_runAutomatically)
                            m_hasUnevaluatedChanges.get(provider) = true;
                    }
                    ImGui::SetItemTooltip("%s", "hex.builtin.view.pattern_editor.auto"_lang.get());

                    ImGui::SameLine();

                    bool synced = m_sourceCode.isSynced();
                    if (ImGuiExt::DimmedIconToggle(ICON_VS_REPO_PINNED, &synced)) {
                        ContentRegistry::Settings::write<bool>("hex.builtin.setting.general", "hex.builtin.setting.general.sync_pattern_source", synced);
                    }
                    ImGui::SetItemTooltip("%s", "hex.builtin.setting.general.sync_pattern_source"_lang.get());

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

            if (m_textEditor.get(provider).isBreakpointsChanged()) {
                m_breakpoints = m_textEditor.get(provider).getBreakpoints();
                m_textEditor.get(provider).clearBreakpointsChanged();
                const auto &runtime = ContentRegistry::PatternLanguage::getRuntime();
                auto &evaluator = runtime.getInternals().evaluator;
                if (evaluator) {
                    evaluator->setBreakpoints(m_breakpoints);
                }
            }

            if (m_textEditor.get(provider).isTextChanged()) {
                m_textEditor.get(provider).setTextChanged(false);
                if (!m_hasUnevaluatedChanges.get(provider) ) {
                    m_hasUnevaluatedChanges.get(provider) = true;
                    m_changesWereParsed = false;
                }
                m_lastEditorChangeTime = std::chrono::steady_clock::now();
                ImHexApi::Provider::markDirty();
                markPatternFileDirty(provider);
            }

            if (m_hasUnevaluatedChanges.get(provider) && m_runningEvaluators == 0 && m_runningParsers == 0 &&
                (std::chrono::steady_clock::now() - m_lastEditorChangeTime) > std::chrono::seconds(1ll)) {

                    auto code = m_textEditor.get(provider).getText();
                    EventPatternEditorChanged::post(code);

                    TaskManager::createBackgroundTask("hex.builtin.task.parsing_pattern", [this, code = std::move(code), provider](auto &){
                        this->parsePattern(code, provider);

                        if (m_runAutomatically)
                            m_triggerAutoEvaluate = true;
                    });
                    m_hasUnevaluatedChanges.get(provider) = false;
            }

            if (m_triggerAutoEvaluate.exchange(false)) {
                this->evaluatePattern(m_textEditor.get(provider).getText(), provider);
            }
            if (m_textHighlighter.m_needsToUpdateColors && m_changesWereParsed && (m_runningParsers + m_runningEvaluators == 0)) {
                TaskHolder taskHolder;
                if (m_textHighlighter.getRunningColorizers() == 0) {
                    m_textHighlighter.m_needsToUpdateColors = false;
                    m_changesWereParsed = false;
                    taskHolder = TaskManager::createBackgroundTask("HighlightSourceCode", [this](auto &) { m_textHighlighter.highlightSourceCode(); });
                } else {
                    taskHolder.interrupt();
                }
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

    void ViewPatternEditor::drawTextEditorFindReplacePopup(ui::TextEditor *textEditor) {
        auto provider = ImHexApi::Provider::get();
        ImGuiWindowFlags popupFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;
        if (ImGui::BeginPopup("##text_editor_view_find_replace_popup", popupFlags)) {
            static std::string findWord;
            static bool requestFocus = false;
            static u64 position = 0;
            static u64 count = 0;
            static bool updateCount = false;
            static bool canReplace = true;
            ui::TextEditor::FindReplaceHandler *findReplaceHandler = textEditor->getFindReplaceHandler();
            if (ImGui::IsWindowAppearing()) {
                findReplaceHandler->resetMatches();

                // Use selection as find word if there is one, otherwise use the word under the cursor
                if (!textEditor->hasSelection())
                    textEditor->selectWordUnderCursor();

                findWord = textEditor->getSelectedText();

                // Find all matches to findWord
                findReplaceHandler->findAllMatches(textEditor, findWord);
                position = findReplaceHandler->findPosition(textEditor, textEditor->getCursorPosition(), true);
                count = findReplaceHandler->getMatches().size();
                findReplaceHandler->setFindWord(textEditor, findWord);
                requestFocus = true;
                updateCount = true;
                if (m_focusedSubWindowName.contains(consoleView))
                    canReplace = false;
                else if (m_focusedSubWindowName.contains(textEditorView))
                    canReplace = true;
            }
            bool enter     = ImGui::IsKeyPressed(ImGuiKey_Enter, false)         || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
            bool upArrow   = ImGui::IsKeyPressed(ImGuiKey_UpArrow, false)       || ImGui::IsKeyPressed(ImGuiKey_Keypad8, false);
            bool downArrow = ImGui::IsKeyPressed(ImGuiKey_DownArrow, false)     || ImGui::IsKeyPressed(ImGuiKey_Keypad2, false);
            bool shift     = ImGui::IsKeyDown(ImGuiKey_LeftShift)               || ImGui::IsKeyDown(ImGuiKey_RightShift);
            bool alt       = ImGui::IsKeyDown(ImGuiKey_LeftAlt)                 || ImGui::IsKeyDown(ImGuiKey_RightAlt);
            std::string childName;
            if (m_focusedSubWindowName.contains(consoleView))
                childName = "##console_find_replace";
            else if (m_focusedSubWindowName.contains(textEditorView))
                childName = "##text_editor_find_replace";
            else
                return;
            ImGui::BeginChild(childName.c_str(), ImVec2(), ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY);

            if (ImGui::BeginTable("##text_editor_find_replace_table", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerH)) {
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

                static int findFlags = ImGuiInputTextFlags_EnterReturnsTrue;

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
                    findReplaceHandler->setFindWord(textEditor, findWord);
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
                    findReplaceHandler->setFindWord(textEditor, findWord);
                    position = findReplaceHandler->findPosition(textEditor, textEditor->getCursorPosition(), true);
                    if (ImGuiInputTextState* input_state = ImGui::GetInputTextState(ImGui::GetID("###findInputTextWidget")))
                        input_state->ReloadUserBufAndMoveToEnd();
                    count = findReplaceHandler->getMatches().size();
                    updateCount = true;
                    requestFocusFind = true;
                }
                ImGui::PopItemWidth();

                ImGui::SameLine();

                bool matchCase = findReplaceHandler->getMatchCase();

                // Allow Alt-C to toggle case sensitivity
                bool altCPressed = ImGui::IsKeyPressed(ImGuiKey_C, false) && alt;
                if (altCPressed || ImGuiExt::DimmedIconToggle(ICON_VS_CASE_SENSITIVE, &matchCase)) {
                    if (altCPressed)
                        matchCase = !matchCase;
                    findReplaceHandler->setMatchCase(textEditor, matchCase);
                    position = findReplaceHandler->findPosition(textEditor, textEditor->getCursorPosition(), true);
                    count = findReplaceHandler->getMatches().size();
                    updateCount = true;
                    requestFocusFind = true;
                }

                ImGui::SameLine();

                bool matchWholeWord = findReplaceHandler->getWholeWord();

                // Allow Alt-W to toggle whole word
                bool altWPressed = ImGui::IsKeyPressed(ImGuiKey_W, false) && alt;
                if (altWPressed || ImGuiExt::DimmedIconToggle(ICON_VS_WHOLE_WORD, &matchWholeWord)) {
                    if (altWPressed)
                        matchWholeWord = !matchWholeWord;
                    findReplaceHandler->setWholeWord(textEditor, matchWholeWord);
                    position = findReplaceHandler->findPosition(textEditor, textEditor->getCursorPosition(), true);
                    count = findReplaceHandler->getMatches().size();
                    updateCount = true;
                    requestFocusFind = true;
                }

                ImGui::SameLine();

                bool useRegex = findReplaceHandler->getFindRegEx();

                // Allow Alt-R to toggle regex
                bool altRPressed = ImGui::IsKeyPressed(ImGuiKey_R, false) && alt;
                if (altRPressed || ImGuiExt::DimmedIconToggle(ICON_VS_REGEX, &useRegex)) {
                    if (altRPressed)
                        useRegex = !useRegex;
                    findReplaceHandler->setFindRegEx(textEditor, useRegex);
                    position = findReplaceHandler->findPosition(textEditor, textEditor->getCursorPosition(), true);
                    count = findReplaceHandler->getMatches().size();
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
                            counterString = "? ";
                        else
                            counterString = fmt::format("{} ", position);
                        counterString += "hex.builtin.view.pattern_editor.of"_lang.operator const char *();
                        if (count > 1999)
                            counterString += " 1999+";
                        else
                            counterString += fmt::format(" {}", count);
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

                    static int replaceFlags = ImGuiInputTextFlags_EnterReturnsTrue;

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
                        findReplaceHandler->setReplaceWord(replaceWord);
                        historyInsert(m_replaceHistory, m_replaceHistorySize, m_replaceHistoryIndex, replaceWord);

                        bool textReplaced = findReplaceHandler->replace(textEditor, !shift && !upArrowReplace);
                        if (textReplaced) {
                            if (count > 0) {
                                if (position == count)
                                    position -= 1;
                                count -= 1;
                            }
                            updateCount = true;
                        }

                        downArrowReplace = false;
                        upArrowReplace = false;

                        if (enterPressedFind) {
                            enterPressedFind = false;
                            requestFocusFind = false;
                        }
                        requestFocusReplace = true;
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
                        findReplaceHandler->setReplaceWord(replaceWord);
                        if (ImGuiInputTextState* input_state = ImGui::GetInputTextState(ImGui::GetID("###replaceInputTextWidget")))
                            input_state->ReloadUserBufAndMoveToEnd();
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
                        findReplaceHandler->setReplaceWord(replaceWord);
                        historyInsert(m_replaceHistory,m_replaceHistorySize, m_replaceHistoryIndex, replaceWord);
                        findReplaceHandler->replaceAll(textEditor);
                        count = 0;
                        position = 0;
                        requestFocusFind = true;
                        updateCount = true;
                    }
                }
                ImGui::EndTable();

                if ((ImGui::IsKeyPressed(ImGuiKey_F3, false)) || downArrowFind || upArrowFind || enterPressedFind) {
                    historyInsert(m_findHistory, m_findHistorySize, m_findHistoryIndex, findWord);
                    position = findReplaceHandler->findMatch(textEditor, !shift && !upArrowFind);
                    count = findReplaceHandler->getMatches().size();
                    updateCount = true;
                    downArrowFind = false;
                    upArrowFind = false;
                    requestFocusFind = true;
                    enterPressedFind = false;
                }
            }
            // Escape key to close the popup
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
                m_popupWindowHeight = 0;
                m_textEditor.get(provider).setTopMarginChanged(0);
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndChild();
            if (m_focusedSubWindowName.contains(textEditorView)) {
                if (auto window = ImGui::GetCurrentWindow(); window != nullptr) {
                    auto height = window->Size.y;
                    auto heightChange = height - m_popupWindowHeight;
                    auto heightChangeChange = heightChange - m_popupWindowHeightChange;
                    if (std::fabs(heightChange) < 0.5 && std::fabs(heightChangeChange) > 1.0) {
                        m_textEditor.get(provider).setTopMarginChanged(height);
                    }
                    m_popupWindowHeightChange = heightChange;
                    m_popupWindowHeight = height;
                }
            }
            ImGui::EndPopup();
            m_frPopupIsClosed = false;
        } else if (!m_frPopupIsClosed) {
            m_frPopupIsClosed = true;
            m_popupWindowHeight = 0;
            m_textEditor.get(provider).setTopMarginChanged(0);
        }
    }

    void ViewPatternEditor::drawTextEditorGotoLinePopup(ui::TextEditor *textEditor) {
        auto provider = ImHexApi::Provider::get();
        ImGuiWindowFlags popupFlags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;
        if (ImGui::BeginPopup("##text_editor_view_goto_line_popup", popupFlags)) {
            std::string childName;
            if (m_focusedSubWindowName.contains(consoleView))
                childName = "##console_goto_line";
            else if (m_focusedSubWindowName.contains(textEditorView))
                childName = "##text_editor_goto_line";
            else
                return;
            ImGui::BeginChild(childName.c_str(), ImVec2(), ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY);
            bool enter = ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
            static i32 line = 1;

            std::string hint = "hex.builtin.view.pattern_editor.goto_line"_lang.operator std::string();
            ImGui::TextUnformatted(hint.c_str());
            ImGui::SameLine();
            ImGui::PushItemWidth(ImGui::CalcTextSize("00000000").x);

            if (ImGui::InputInt("###text_editor_goto_line", &line, 0, 0, ImGuiInputTextFlags_CharsDecimal)) {
                enter = false;
            }
            ImGui::SetKeyboardFocusHere(-1);
            ImGui::PopItemWidth();
            if (enter) {
                ImGui::CloseCurrentPopup();
                if (line < 0)
                    line = textEditor->getTotalLines() + line + 1;
                line = std::clamp(line, 1, textEditor->getTotalLines());
                textEditor->jumpToLine(line - 1);
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
                m_popupWindowHeight = 0;
                m_textEditor.get(provider).setTopMarginChanged(0);
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndChild();
            if (m_focusedSubWindowName.contains(textEditorView)) {
                if (auto window = ImGui::GetCurrentWindow(); window != nullptr) {
                    auto height = window->Size.y;
                    auto heightChange = height - m_popupWindowHeight;
                    auto heightChangeChange = heightChange - m_popupWindowHeightChange;
                    if (std::fabs(heightChange) < 0.5 && std::fabs(heightChangeChange) > 1.0) {
                        m_textEditor.get(provider).setTopMarginChanged(height);
                    }
                    m_popupWindowHeightChange = heightChange;
                    m_popupWindowHeight = height;
                }
            }
            ImGui::EndPopup();
            m_gotoPopupIsClosed = false;
        } else if (!m_gotoPopupIsClosed) {
            m_gotoPopupIsClosed = true;
            m_popupWindowHeight = 0;
            m_textEditor.get(provider).setTopMarginChanged(0);
        }
    }

    void ViewPatternEditor::drawConsole(ImVec2 size) {
        auto provider = ImHexApi::Provider::get();

        if (m_consoleEditor.get(provider).raiseContextMenu()) {
            RequestOpenPopup::post("hex.builtin.menu.edit");
            m_consoleEditor.get(provider).clearRaiseContextMenu();
        }

        if (m_consoleCursorNeedsUpdate.get(provider)) {
            m_consoleEditor.get(provider).setFocusAtCoords(m_consoleCursorPosition.get(provider));
            m_consoleCursorNeedsUpdate.get(provider) = false;
        }

        if (m_consoleNeedsUpdate) {
            std::scoped_lock lock(m_logMutex);
            auto lineCount = m_consoleEditor.get(provider).getTextLines().size();
            if (m_console->size() < lineCount || (lineCount == 1 && m_consoleEditor.get(provider).getLineText(0).empty())) {
                m_consoleEditor.get(provider).setText("");
                lineCount = 0;
            }

            const auto linesToAdd = m_console->size() - lineCount;

            for (size_t i = 0; i < linesToAdd; i += 1) {
                m_consoleEditor.get(provider).appendLine(m_console->at(lineCount + i));
            }
            m_consoleNeedsUpdate = false;
        }

        fonts::CodeEditor().push();
        m_consoleEditor.get(provider).render("##console", size, true);
        fonts::CodeEditor().pop();

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y + 1_scaled);
    }

    void ViewPatternEditor::drawEnvVars(ImVec2 size, std::list<EnvVar> &envVars) {
        static u32 envVarCounter = 1;

        if (ImGui::BeginChild("##env_vars", size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
            if (envVars.size() <= 1) {
                ImGuiExt::TextOverlay("hex.builtin.view.pattern_editor.no_env_vars"_lang, ImGui::GetWindowPos() + ImGui::GetWindowSize() / 2, ImGui::GetWindowWidth() * 0.7);
            }

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
                                i64 displayValue = i64(hex::get_or<i128>(value, 0));
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
        auto provider = ImHexApi::Provider::get();
        if (ImGui::BeginChild("##settings", size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
            if (patternVariables.empty()) {
                ImGuiExt::TextOverlay("hex.builtin.view.pattern_editor.no_in_out_vars"_lang, ImGui::GetWindowPos() + ImGui::GetWindowSize() / 2, ImGui::GetWindowWidth() * 0.7);
            }

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
                            i64 value = i64(hex::get_or<i128>(variable.value, 0ll));
                            if (ImGui::InputScalar(label.c_str(), ImGuiDataType_S64, &value))
                                m_hasUnevaluatedChanges.get(provider) = true;
                            variable.value = i128(value);
                        } else if (pl::core::Token::isUnsigned(variable.type)) {
                            u64 value = u64(hex::get_or<u128>(variable.value, 0));
                            if (ImGui::InputScalar(label.c_str(), ImGuiDataType_U64, &value))
                                m_hasUnevaluatedChanges.get(provider) = true;
                            variable.value = u128(value);
                        } else if (pl::core::Token::isFloatingPoint(variable.type)) {
                            auto value = hex::get_or<double>(variable.value, 0.0);
                            if (ImGui::InputScalar(label.c_str(), ImGuiDataType_Double, &value))
                                m_hasUnevaluatedChanges.get(provider) = true;
                            variable.value = value;
                        } else if (variable.type == pl::core::Token::ValueType::Boolean) {
                            auto value = hex::get_or<bool>(variable.value, false);
                            if (ImGui::Checkbox(label.c_str(), &value))
                                m_hasUnevaluatedChanges.get(provider) = true;
                            variable.value = value;
                        } else if (variable.type == pl::core::Token::ValueType::Character) {
                            std::array<char, 2> buffer = { hex::get_or<char>(variable.value, '\x00') };
                            if (ImGui::InputText(label.c_str(), buffer.data(), buffer.size()))
                                m_hasUnevaluatedChanges.get(provider) = true;
                            variable.value = buffer[0];
                        } else if (variable.type == pl::core::Token::ValueType::String) {
                            std::string buffer = hex::get_or<std::string>(variable.value, "");
                            if (ImGui::InputText(label.c_str(), buffer))
                                m_hasUnevaluatedChanges.get(provider) = true;
                            variable.value = buffer;
                        }
                    }
                    ImGui::PopItemWidth();
                }

                ImGui::EndTable();
            }
        }
        ImGui::EndChild();
    }

    void ViewPatternEditor::drawVirtualFiles(ImVec2 size, const std::vector<VirtualFile> &virtualFiles) const {
        std::vector<const VirtualFile*> virtualFilePointers;

        for (const auto &file : virtualFiles)
            virtualFilePointers.emplace_back(&file);

        if (ImGui::BeginTable("Virtual File Tree", 1, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, size)) {
            if (virtualFiles.empty()) {
                ImGuiExt::TextOverlay("hex.builtin.view.pattern_editor.no_virtual_files"_lang, ImGui::GetWindowPos() + ImGui::GetWindowSize() / 2, ImGui::GetWindowWidth() * 0.7);
            }
            
            ImGui::TableSetupColumn("##path", ImGuiTableColumnFlags_WidthStretch);
            levelId = 1;
            drawVirtualFileTree(virtualFilePointers);

            ImGui::EndTable();
        }
    }


    void ViewPatternEditor::drawDebugger(ImVec2 size) {
        auto provider = ImHexApi::Provider::get();
        const auto &runtime = ContentRegistry::PatternLanguage::getRuntime();

        if (ImGui::BeginChild("##debugger", size, true)) {
            auto &evaluator = runtime.getInternals().evaluator;
            m_breakpoints = m_textEditor.get(provider).getBreakpoints();
            evaluator->setBreakpoints(m_breakpoints);
            const auto line = m_textEditor.get(provider).getCursorPosition().m_line + 1;

            if (!m_breakpoints->contains(line)) {
                if (ImGuiExt::IconButton(ICON_VS_CIRCLE, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed))) {
                    evaluator->addBreakpoint(line);
                }
                ImGuiExt::InfoTooltip("hex.builtin.view.pattern_editor.debugger.add_tooltip"_lang);
            } else {
                if (ImGuiExt::IconButton(ICON_VS_CIRCLE_FILLED, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed))) {
                    evaluator->removeBreakpoint(line);
                }
                ImGuiExt::InfoTooltip("hex.builtin.view.pattern_editor.debugger.remove_tooltip"_lang);
            }
            m_breakpoints = evaluator->getBreakpoints();
            m_textEditor.get(provider).setBreakpoints(m_breakpoints);
            ImGui::SameLine();

            if (*m_breakpointHit) {
                auto displayValue = [&](const auto &parent, size_t index) {
                    return fmt::format("{0} {1} [{2}]",
                                        "hex.builtin.view.pattern_editor.debugger.scope"_lang,
                                        evaluator->getScopeCount() - index - 1,
                                        parent == nullptr ?
                                        "hex.builtin.view.pattern_editor.debugger.scope.global"_lang :
                                        fmt::format("0x{0:08X}", parent->getOffset())
                    );
                };

                if (evaluator->getScopeCount() > 0) {
                    ImGui::SetNextItemWidth(-1);
                    const auto &currScope = evaluator->getScope(-m_debuggerScopeIndex);
                    if (ImGui::BeginCombo("##scope", displayValue(currScope.parent, m_debuggerScopeIndex).c_str())) {
                        for (size_t i = 0; i < evaluator->getScopeCount(); i++) {
                            auto &scope = evaluator->getScope(-i32(i));

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

                    if (pauseLine.has_value())
                        m_textEditor.get(provider).jumpToLine(pauseLine.value() - 1);
                }

                const auto &currScope = evaluator->getScope(-m_debuggerScopeIndex);
                (*m_debuggerDrawer)->draw(*currScope.scope, &runtime, size.y - ImGui::GetTextLineHeightWithSpacing() * 4);
            }
        }
        ImGui::EndChild();
    }

    void ViewPatternEditor::drawAlwaysVisibleContent() {
        auto provider = ImHexApi::Provider::get();
        if (provider == nullptr)
            return;

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

                ui::TextEditor::ErrorMarkers errorMarkers;
                if (*m_callStack != nullptr && !(*m_callStack)->empty()) {
                    for (const auto &frame : **m_callStack | std::views::reverse) {
                        auto location = frame.node->getLocation();
                        if (location.source != nullptr && location.source->mainSource) {
                            std::string message = "";
                            if (m_lastEvaluationError->has_value())
                                message = processMessage((*m_lastEvaluationError)->message);
                            auto key = ui::TextEditor::Coordinates(location.line, location.column);
                            errorMarkers[key] = std::make_pair(u32(location.length), message);
                        }
                    }
                } else {
                    if (m_lastEvaluationError->has_value()) {
                        auto key = ui::TextEditor::Coordinates((*m_lastEvaluationError)->line, 0);
                        errorMarkers[key] = std::make_pair(0,processMessage((*m_lastEvaluationError)->message));
                    }
                }

                if (!m_lastCompileError->empty()) {
                    for (const auto &error : *m_lastCompileError) {
                       auto source = error.getLocation().source;
                        if (source != nullptr && source->mainSource) {
                            auto key = ui::TextEditor::Coordinates(error.getLocation().line, error.getLocation().column);
                            if (!errorMarkers.contains(key) ||errorMarkers[key].first < error.getLocation().length)
                                    errorMarkers[key] = std::make_pair(u32(error.getLocation().length), processMessage(error.getMessage()));
                        }
                    }
                }

                m_textEditor.get(provider).setErrorMarkers(errorMarkers);
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

            m_analysisTask = TaskManager::createBackgroundTask("hex.builtin.task.analyzing_data", [this, provider](const Task &task) {
                if (!m_autoLoadPatterns)
                    return;

                pl::PatternLanguage runtime;
                ContentRegistry::PatternLanguage::configureRuntime(runtime, provider);

                bool foundCorrectType = false;

                auto mimeType = magic::getMIMEType(provider, 0, 4_KiB, true);
                runtime.addPragma("MIME", [&mimeType, &foundCorrectType](const pl::PatternLanguage &runtime, const std::string &value) {
                    std::ignore = runtime;

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
                    const auto pattern = [value = value]() mutable -> std::optional<BinaryPattern> {
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

                        value = wolv::util::trim(value);

                        return BinaryPattern(value);
                    }();

                    const auto address = [value = value, provider]() mutable -> std::optional<u64> {
                        value = wolv::util::trim(value);

                        if (value.empty())
                            return std::nullopt;

                        const auto start = value.find('@');
                        if (start == std::string::npos)
                            return std::nullopt;

                        value = value.substr(start + 1);
                        value = wolv::util::trim(value);

                        size_t end = 0;
                        auto result = std::stoll(value, &end, 0);
                        if (end != value.length())
                            return std::nullopt;

                        if (result < 0) {
                            const auto size = provider->getActualSize();
                            if (u64(-result) > size) {
                                return std::nullopt;
                            }

                            return size + result;
                        } else {
                            return result;
                        }
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
            ImGui::ColorButton(pattern->getVariableName().c_str(), ImColor(pattern->getColor()), ImGuiColorEditFlags_AlphaOpaque);
            ImGui::SameLine(0, 10);
            ImGuiExt::TextFormattedColored(ui::TextEditor::getPalette()[u32(ui::TextEditor::PaletteIndex::BuiltInType)], "{} ", pattern->getFormattedName());
            ImGui::SameLine(0, 5);
            ImGuiExt::TextFormatted("{}", pattern->getDisplayName());
            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();

            if (const auto &inlineVisualizeArgs = pattern->getAttributeArguments("hex::inline_visualize"); !inlineVisualizeArgs.empty()) {
                auto x = ImGui::GetCursorPosX();
                ImGui::Dummy(ImVec2(125_scaled, ImGui::GetTextLineHeight()));
                ImGui::SameLine();
                ImGui::SetCursorPos(ImVec2(x, ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y));
                m_visualizerDrawer->drawVisualizer(ContentRegistry::PatternLanguage::impl::getInlineVisualizers(), inlineVisualizeArgs, *pattern, true);
            } else {
                ImGuiExt::TextFormatted("{: <{}} ", hex::limitStringLength(pattern->getFormattedValue(), 64), shiftHeld ? 40 : 0);
            }

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

                if (const auto &visualizeArgs = pattern->getAttributeArguments("hex::visualize"); !visualizeArgs.empty()) {
                    m_visualizerDrawer->drawVisualizer(ContentRegistry::PatternLanguage::impl::getVisualizers(), visualizeArgs, *pattern, m_tooltipJustOpened);
                }

                ImGui::Unindent();

                m_tooltipJustOpened = false;
            } else {
                m_tooltipJustOpened = true;
            }
        }

        ImGui::PopID();
    }


    void ViewPatternEditor::loadPatternFile(const std::fs::path &path, prv::Provider *provider, bool trackFile) {
        wolv::io::File file(path, wolv::io::File::Mode::Read);
        if (file.isValid()) {
            auto code = wolv::util::preprocessText(file.readString());

            this->evaluatePattern(code, provider);
            m_textEditor.get(provider).setText(code, true);
            m_sourceCode.get(provider) = code;
            if (trackFile) {
                m_changeTracker.get(provider) = wolv::io::ChangeTracker(file);
                m_changeTracker.get(provider).startTracking([this, provider]{ this->handleFileChange(provider); });
            }
            m_textHighlighter.m_needsToUpdateColors = false;
            TaskManager::createBackgroundTask("hex.builtin.task.parsing_pattern", [this, code, provider](auto&) { this->parsePattern(code, provider); });
        }
    }

    void ViewPatternEditor::parsePattern(const std::string &code, prv::Provider *provider) {
        m_runningParsers += 1;

        ContentRegistry::PatternLanguage::configureRuntime(*m_editorRuntime, nullptr);
        const auto &ast = m_editorRuntime->parseString(code, pl::api::Source::DefaultSource);
        m_textEditor.get(provider).setLongestLineLength(m_editorRuntime->getInternals().preprocessor.get()->getLongestLineLength());

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
        m_changesWereParsed = true;
        m_runningParsers -= 1;
    }

    void ViewPatternEditor::evaluatePattern(const std::string &code, prv::Provider *provider) {
        EventPatternEvaluating::post();

        auto lock = std::scoped_lock(ContentRegistry::PatternLanguage::getRuntimeLock());

        m_runningEvaluators += 1;
        m_executionDone.get(provider) = false;

        m_textEditor.get(provider).clearActionables();

        m_consoleEditor.get(provider).clearActionables();
        m_console.get(provider).clear();
        m_consoleLongestLineLength.get(provider) = 0;
        m_consoleNeedsUpdate = true;

        m_consoleEditor.get(provider).setText("");
        m_virtualFiles->clear();

        m_accessHistory = {};
        m_accessHistoryIndex = 0;
        m_patternEvaluating = true;

        EventHighlightingChanged::post();

        TaskManager::createTask("hex.builtin.view.pattern_editor.evaluating", TaskManager::NoProgress, [this, code, provider](auto &task) {
            auto runtimeLock = std::scoped_lock(ContentRegistry::PatternLanguage::getRuntimeLock());

            auto &runtime = ContentRegistry::PatternLanguage::getRuntime();
            ContentRegistry::PatternLanguage::configureRuntime(runtime, provider);
            runtime.getInternals().evaluator->setBreakpointHitCallback([this, &runtime, provider] {
                m_debuggerScopeIndex = 0;
                *m_breakpointHit = true;
                m_resetDebuggerVariables = true;
                auto optPauseLine = runtime.getInternals().evaluator->getPauseLine();
                if (optPauseLine.has_value())
                    m_textEditor.get(provider).jumpToLine(optPauseLine.value() - 1);
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

            runtime.setLogCallback([this, provider](auto level, auto message) {
                std::scoped_lock lock(m_logMutex);

                auto lines = wolv::util::splitString(message, "\n");
                for (auto &line : lines) {
                    if (!wolv::util::trim(line).empty()) {
                        switch (level) {
                            using enum pl::core::LogConsole::Level;

                            case Debug:     line = fmt::format("D: {}", line); break;
                            case Info:      line = fmt::format("I: {}", line); break;
                            case Warning:   line = fmt::format("W: {}", line); break;
                            case Error:     line = fmt::format("E: {}", line); break;
                            default: break;
                        }
                    }
                    if (m_consoleLongestLineLength.get(provider) < line.size()) {
                       m_consoleLongestLineLength.get(provider) = line.size();
                        m_consoleEditor.get(provider).setLongestLineLength(line.size());
                    }
                    m_console.get(provider).emplace_back(line);
                    m_consoleNeedsUpdate = true;
                }
            });

            ON_SCOPE_EXIT {
                runtime.getInternals().evaluator->setDebugMode(false);
                *m_lastEvaluationOutVars = runtime.getOutVariables();

                m_runningEvaluators -= 1;

                m_lastEvaluationProcessed = false;

                std::scoped_lock lock(m_logMutex);
                m_console.get(provider).emplace_back(
                   fmt::format("I: Evaluation took {}", std::chrono::duration<double>(runtime.getLastRunningTime()))
                );
                m_consoleNeedsUpdate = true;
            };


            m_lastEvaluationResult = runtime.executeString(code, pl::api::Source::DefaultSource, envVars, inVariables);
            if (!m_lastEvaluationResult) {
                *m_lastEvaluationError = runtime.getEvalError();
                *m_lastCompileError    = runtime.getCompileErrors();
                *m_callStack           = &runtime.getInternals().evaluator->getCallStack();
            }

            TaskManager::doLater([code] {
                EventPatternExecuted::post(code);
            });
        });
    }

    void ViewPatternEditor::registerEvents() {
        RequestPatternEditorSelectionChange::subscribe(this, [this](u32 line, u32 column) {
            auto provider = ImHexApi::Provider::get();
            if (line == 0)
                return;

            const ui::TextEditor::Coordinates coords = { int(line) - 1, int(column) };
            m_textEditor.get(provider).setCursorPosition(coords);
        });

        RequestLoadPatternLanguageFile::subscribe(this, [this](const std::fs::path &path, bool trackFile) {
            this->loadPatternFile(path, ImHexApi::Provider::get(), trackFile);
        });

        RequestRunPatternCode::subscribe(this, [this] {
            m_triggerAutoEvaluate = true;
        });

        RequestSavePatternLanguageFile::subscribe(this, [this](const std::fs::path &path) {
            auto provider = ImHexApi::Provider::get();
            wolv::io::File file(path, wolv::io::File::Mode::Create);
            file.writeString(wolv::util::trim(m_textEditor.get(provider).getText()));
        });

        RequestSetPatternLanguageCode::subscribe(this, [this](const std::string &code) {
            auto provider = ImHexApi::Provider::get();
            m_textEditor.get(provider).setText(wolv::util::preprocessText(code));
            m_sourceCode.get(provider) = code;
            m_hasUnevaluatedChanges.get(provider) = true;
            m_textHighlighter.m_needsToUpdateColors = false;
        });

        ContentRegistry::Settings::onChange("hex.builtin.setting.general", "hex.builtin.setting.general.sync_pattern_source", [this](const ContentRegistry::Settings::SettingsValue &value) {
            m_sourceCode.enableSync(value.get<bool>(false));
        });

        ContentRegistry::Settings::onChange("hex.builtin.setting.general", "hex.builtin.setting.general.auto_load_patterns", [this](const ContentRegistry::Settings::SettingsValue &value) {
            m_autoLoadPatterns = value.get<bool>(true);
        });

        EventProviderOpened::subscribe(this, [this](prv::Provider *provider) {
            m_textEditor.get(provider).setLanguageDefinition(PatternLanguage());
            m_textEditor.get(provider).setShowWhitespaces(false);

            m_consoleEditor.get(provider).setLanguageDefinition(ConsoleLog());
            m_consoleEditor.get(provider).setShowWhitespaces(false);
            m_consoleEditor.get(provider).setReadOnly(true);
            m_consoleEditor.get(provider).setShowCursor(false);
            m_consoleEditor.get(provider).setShowLineNumbers(false);
            m_consoleEditor.get(provider).setSourceCodeEditor(&m_textEditor.get(provider));
            std::string sourcecode = pl::api::Source::DefaultSource;
            std::string error = "E: ";
            std::string end = ":";
            std::string arrow = "  -->   in ";
            m_consoleEditor.get(provider).addClickableText(error + sourcecode + end);
            m_consoleEditor.get(provider).addClickableText(error + arrow + sourcecode + end);
            m_shouldAnalyze.get(provider) = true;
            m_envVarEntries.get(provider).emplace_back(0, "", i128(0), EnvVarType::Integer);

            m_debuggerDrawer.get(provider) = std::make_unique<ui::PatternDrawer>();
            m_cursorPosition.get(provider) =  ui::TextEditor::Coordinates(0, 0);
        });

        EventProviderChanged::subscribe(this, [this](prv::Provider *oldProvider, prv::Provider *newProvider) {
            if (oldProvider != nullptr) {
                m_sourceCode.get(oldProvider) = m_textEditor.get(oldProvider).getText();
                m_cursorPosition.get(oldProvider) = m_textEditor.get(oldProvider).getCursorPosition();
                m_selection.get(oldProvider) = m_textEditor.get(oldProvider).getSelection();
                m_consoleCursorPosition.get(oldProvider) = m_consoleEditor.get(oldProvider).getCursorPosition();
                m_consoleSelection.get(oldProvider) = m_consoleEditor.get(oldProvider).getSelection();
                m_consoleLongestLineLength.get(oldProvider) = m_consoleEditor.get(oldProvider).getLongestLineLength();
                m_breakpoints.get(oldProvider) = m_textEditor.get(oldProvider).getBreakpoints();
                m_cursorNeedsUpdate.get(oldProvider) = false;
                m_consoleCursorNeedsUpdate.get(oldProvider) = false;
            }

            if (newProvider != nullptr) {
                m_textEditor.get(newProvider).setText(wolv::util::preprocessText(m_sourceCode.get(newProvider)));
                m_textEditor.get(newProvider).setCursorPosition(m_cursorPosition.get(newProvider));
                ui::TextEditor::Selection selection = m_selection.get(newProvider);
                m_textEditor.get(newProvider).setSelection(selection);
                m_textEditor.get(newProvider).setBreakpoints(m_breakpoints.get(newProvider));
                m_consoleEditor.get(newProvider).setText(wolv::util::combineStrings(m_console.get(newProvider), "\n"));
                m_consoleEditor.get(newProvider).setCursorPosition(m_consoleCursorPosition.get(newProvider));
                m_consoleEditor.get(newProvider).setLongestLineLength(m_consoleLongestLineLength.get(newProvider));
                selection = m_consoleSelection.get(newProvider);
                m_consoleEditor.get(newProvider).setSelection(selection);
                m_cursorNeedsUpdate.get(newProvider) = true;
                m_consoleCursorNeedsUpdate.get(newProvider) = true;
                m_textEditor.get(newProvider).setTextChanged(false);
                m_hasUnevaluatedChanges.get(newProvider) = true;
            }
            m_textHighlighter.m_needsToUpdateColors = false;

        });

        RequestAddVirtualFile::subscribe(this, [this](const std::fs::path &path, const std::vector<u8> &data, Region region) {
            m_virtualFiles->emplace_back(path, data, region);
        });

    }

    static void createNestedMenu(const std::vector<std::string> &menus, const std::function<void()> &function) {
        if (menus.empty())
            return;

        if (menus.size() == 1) {
            if (menu::menuItem(menus.front().c_str()))
                function();
        } else {
            if (menu::beginMenu(menus.front().c_str())) {
                createNestedMenu({ menus.begin() + 1, menus.end() }, function);
                menu::endMenu();
            }
        }
    }

    void ViewPatternEditor::appendEditorText(const std::string &text) {
        auto provider = ImHexApi::Provider::get();
        m_textEditor.get(provider).appendLine(text);
        m_triggerEvaluation = true;
    }

    void ViewPatternEditor::appendVariable(const std::string &type) {
        const auto &selection = ImHexApi::HexEditor::getSelection();

        appendEditorText(fmt::format("{0} {0}_at_0x{1:02X} @ 0x{1:02X};", type, selection->getStartAddress()));
        AchievementManager::unlockAchievement("hex.builtin.achievement.patterns", "hex.builtin.achievement.patterns.place_menu.name");
    }

    void ViewPatternEditor::appendArray(const std::string &type, size_t size) {
        const auto &selection = ImHexApi::HexEditor::getSelection();

        appendEditorText(fmt::format("{0} {0}_array_at_0x{1:02X}[0x{2:02X}] @ 0x{1:02X};", type, selection->getStartAddress(), (selection->getSize() + (size - 1)) / size));
    }

    ui::TextEditor *ViewPatternEditor::getEditorFromFocusedWindow() {
        auto provider = ImHexApi::Provider::get();
        if (provider != nullptr) {
            if (m_focusedSubWindowName.contains(consoleView)) {
                return &m_consoleEditor.get(provider);
            }
            if (m_focusedSubWindowName.contains(textEditorView)) {
                return &m_textEditor.get(provider);
            }
        }

        return nullptr;
    }

    void ViewPatternEditor::registerMenuItems() {


        /* Open File */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.view.pattern_editor.menu.file.open_pattern" }, ICON_VS_FOLDER_OPENED, 1100, AllowWhileTyping + CTRLCMD + Keys::O, [this] {
            m_openPatternFile(true);
        }, [] { return ImHexApi::Provider::isValid(); },
        this);

        /* Save */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.view.pattern_editor.menu.file.save_pattern" }, ICON_VS_SAVE, 1350, AllowWhileTyping + CTRLCMD + Keys::S, [this] {
            m_savePatternFile(true);
        },[this] {
            auto provider      = ImHexApi::Provider::get();
            bool providerValid = ImHexApi::Provider::isValid();

            return providerValid && isPatternDirty(provider);
        },
        this);

        /* Save As */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.view.pattern_editor.menu.file.save_pattern_as" }, ICON_VS_SAVE_AS, 1375, AllowWhileTyping + CTRLCMD + SHIFT + Keys::S, [this] {
            m_savePatternAsFile(true);
        },[] {
            return ImHexApi::Provider::isValid();
        },
        this);

        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.file" }, 1500, this);

        /* Find */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.view.pattern_editor.menu.find" }, ICON_VS_SEARCH, 1510, AllowWhileTyping + CTRLCMD + Keys::F, [this] {
            m_replaceMode = false;
            m_openFindReplacePopUp = true;
        }, [] { return true; },
        this);

        /* Find Next */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.view.pattern_editor.menu.find_next" }, 1520, AllowWhileTyping + Keys::F3, [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr) {
                ui::TextEditor::FindReplaceHandler *findReplaceHandler = editor->getFindReplaceHandler();
                findReplaceHandler->findMatch(editor, true);
            } else {
                m_textEditor->getFindReplaceHandler()->findMatch(&*m_textEditor, true);
            }
        }, [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr) {
                return ImHexApi::Provider::isValid() && !editor->getFindReplaceHandler()->getFindWord().empty();
            } else {
                return false;
            }
         },
        []{ return false; },
        this);

        /* Find Previous */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.view.pattern_editor.menu.find_previous" }, 1530, AllowWhileTyping + SHIFT + Keys::F3, [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr) {
                ui::TextEditor::FindReplaceHandler *findReplaceHandler = editor->getFindReplaceHandler();
                findReplaceHandler->findMatch(editor, false);
            } else {
                m_textEditor->getFindReplaceHandler()->findMatch(&*m_textEditor, false);
            }
        }, [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr) {
                return ImHexApi::Provider::isValid() && !m_textEditor->getFindReplaceHandler()->getFindWord().empty();
            } else {
                return false;
            }
        },
        []{ return false; },
        this);

        /* Replace */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.view.pattern_editor.menu.replace" }, ICON_VS_REPLACE, 1540, AllowWhileTyping + CTRLCMD + Keys::H, [this] {
            m_replaceMode = true;
            m_openFindReplacePopUp = true;
        }, [this] { return m_focusedSubWindowName.contains(textEditorView); },
        this);

        /* Replace Next */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.view.pattern_editor.menu.replace_next" }, 1550, Shortcut::None, [this] {
            m_textEditor->getFindReplaceHandler()->replace(&*m_textEditor, true);
        }, [this] { return ImHexApi::Provider::isValid() && !m_textEditor->getFindReplaceHandler()->getReplaceWord().empty() && m_focusedSubWindowName.contains(textEditorView); },
        []{ return false; },
        this);

        /* Replace Previous */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.view.pattern_editor.menu.replace_previous" }, 1560, Shortcut::None, [this] {
            m_textEditor->getFindReplaceHandler()->replace(&*m_textEditor, false);
        }, [this] { return ImHexApi::Provider::isValid() && !m_textEditor->getFindReplaceHandler()->getReplaceWord().empty() && m_focusedSubWindowName.contains(textEditorView); },
        []{ return false; },
        this);

        /* Replace All */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.view.pattern_editor.menu.replace_all" }, ICON_VS_REPLACE_ALL, 1570, Shortcut::None, [this] {
            m_textEditor->getFindReplaceHandler()->replaceAll(&*m_textEditor);
        }, [this] { return ImHexApi::Provider::isValid() && !m_textEditor->getFindReplaceHandler()->getReplaceWord().empty() && m_focusedSubWindowName.contains(textEditorView); },
        this);


        /* Goto Line */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.view.pattern_editor.menu.goto_line" }, ICON_VS_DEBUG_STEP_INTO, 1600, AllowWhileTyping + CTRLCMD + Keys::G, [this] {
            m_openGotoLinePopUp = true;
        }, [] { return true; },
        this);

        /* Import Pattern */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.import", "hex.builtin.menu.file.import.pattern" }, ICON_VS_FILE_CODE, 5600, Shortcut::None, [this] {
            m_openPatternFile(false);
        }, ImHexApi::Provider::isValid);

        /* Export Pattern */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.export", "hex.builtin.menu.file.export.pattern" }, ICON_VS_FILE_CODE, 7050, Shortcut::None, [this] {
            m_savePatternFile(false);
        }, [this] {
            return ImHexApi::Provider::isValid() && !wolv::util::trim(m_textEditor->getText()).empty();
        });

        /* Undo */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.pattern_editor.menu.edit.undo" }, ICON_VS_DISCARD, 1250, AllowWhileTyping + CTRLCMD + Keys::Z, [this] {
            m_textEditor->undo();
        }, [this] { return ImHexApi::Provider::isValid() && m_textEditor->canUndo() && m_focusedSubWindowName.contains(textEditorView); },
        this);

        /* Redo */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.pattern_editor.menu.edit.redo" }, ICON_VS_REDO, 1275, AllowWhileTyping + CTRLCMD + Keys::Y, [this] {
            m_textEditor->redo();
        }, [this] { return ImHexApi::Provider::isValid() && m_textEditor->canRedo() && m_focusedSubWindowName.contains(textEditorView); },
        this);

        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.edit" }, 1280, this);


        /* Cut */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.pattern_editor.menu.edit.cut" }, ICON_VS_COMBINE, 1300, AllowWhileTyping + CTRLCMD + Keys::X, [this] {
            m_textEditor->cut();
        }, [this] { return ImHexApi::Provider::isValid() && m_textEditor->hasSelection() && m_focusedSubWindowName.contains(textEditorView); },
        this);

        /* Copy */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.pattern_editor.menu.edit.copy" }, ICON_VS_COPY, 1400, AllowWhileTyping + CTRLCMD + Keys::C, [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr) {
                editor->copy();
            } else {
                m_textEditor->copy();
            }
        }, [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                return ImHexApi::Provider::isValid() && editor->hasSelection();
            else
                return false;
        },
        this);

        /* Paste */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.pattern_editor.menu.edit.paste" }, ICON_VS_OUTPUT, 1500, AllowWhileTyping + CTRLCMD + Keys::V, [this] {
            m_textEditor->paste();
        }, [this] { return m_focusedSubWindowName.contains(textEditorView); },
        this);


        /* Select All */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.pattern_editor.menu.edit.select_all" }, ICON_VS_LIST_FLAT, 1650, AllowWhileTyping + CTRLCMD + Keys::A, [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->selectAll();
        }, [] { return ImHexApi::Provider::isValid(); },
        this);

        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.edit" }, 1700, this);

        /* Add Breakpoint */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.pattern_editor.menu.edit.add_breakpoint"}, ICON_VS_DEBUG_BREAKPOINT_DATA, 1750, Keys::F8 + AllowWhileTyping, [this] {
            const auto line = m_textEditor.get(ImHexApi::Provider::get()).getCursorPosition().m_line + 1;
            const auto &runtime = ContentRegistry::PatternLanguage::getRuntime();

            auto &evaluator = runtime.getInternals().evaluator;
            m_breakpoints = m_textEditor.get(ImHexApi::Provider::get()).getBreakpoints();
            evaluator->setBreakpoints(m_breakpoints);

            if (m_breakpoints->contains(line))
                evaluator->removeBreakpoint(line);
             else
                evaluator->addBreakpoint(line);

            m_breakpoints = evaluator->getBreakpoints();
            m_textEditor.get(ImHexApi::Provider::get()).setBreakpoints(m_breakpoints);
        },  [] { return ImHexApi::Provider::isValid(); },
        this);

        /* Trigger Evaluation */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit","hex.builtin.view.pattern_editor.menu.edit.run_pattern" }, ICON_VS_PLAY, 1800, Keys::F5 + AllowWhileTyping, [this] {
            auto &runtime = ContentRegistry::PatternLanguage::getRuntime();
            if (runtime.isRunning()) {
                m_breakpointHit = false;
                runtime.abort();
            }
            m_triggerAutoEvaluate = true;
        },  [] { return ImHexApi::Provider::isValid(); },
         this);

        /* Continue debugger */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit","hex.builtin.view.pattern_editor.menu.edit.continue_debugger"}, ICON_VS_DEBUG_CONTINUE, 1850, SHIFT + Keys::F9 + AllowWhileTyping,  [this] {
            const auto &runtime = ContentRegistry::PatternLanguage::getRuntime();
            if (runtime.isRunning())
                m_breakpointHit = false;
        },  [] { return ImHexApi::Provider::isValid(); },
         this);

        /* Step debugger */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit","hex.builtin.view.pattern_editor.menu.edit.step_debugger" },ICON_VS_DEBUG_STEP_INTO, 1900, SHIFT + Keys::F7 + AllowWhileTyping,  [this] {
            const auto &runtime = ContentRegistry::PatternLanguage::getRuntime();
            if (runtime.isRunning()) {
                runtime.getInternals().evaluator->pauseNextLine();
                m_breakpointHit = false;
            }
        },  [] { return ImHexApi::Provider::isValid(); },
        this);

        constexpr static std::array<std::pair<const char *, size_t>, 21>  Types = {{
           { "u8", 1 }, { "u16", 2 }, { "u24", 3 }, { "u32", 4 }, { "u48", 6 }, { "u64", 8 }, { "u96", 12 }, { "u128", 16 },
           { "s8", 1 }, { "s16", 2 }, { "s24", 3 }, { "s32", 4 }, { "s48", 6 }, { "s64", 8 }, { "s96", 12 }, { "s128", 16 },
           { "float", 4 }, { "double", 8 },
           { "bool", 1 }, { "char", 1 }, { "char16", 2 }
        }};

        /* Place pattern... */
        ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.edit", "hex.builtin.view.pattern_editor.menu.edit.place_pattern" }, ICON_VS_LIBRARY, 3000,
            [&, this] {
                if (menu::beginMenu("hex.builtin.view.pattern_editor.menu.edit.place_pattern.builtin"_lang)) {
                    if (menu::beginMenu("hex.builtin.view.pattern_editor.menu.edit.place_pattern.builtin.single"_lang)) {
                        for (const auto &[type, size] : Types) {
                            if (menu::menuItem(type))
                                appendVariable(type);
                        }
                        menu::endMenu();
                    }

                    if (menu::beginMenu("hex.builtin.view.pattern_editor.menu.edit.place_pattern.builtin.array"_lang)) {
                        for (const auto &[type, size] : Types) {
                            if (menu::menuItem(type))
                                appendArray(type, size);
                        }
                        menu::endMenu();
                    }

                    menu::endMenu();
                }

                const auto &types = m_editorRuntime->getInternals().parser->getTypes();
                const bool hasPlaceableTypes = std::ranges::any_of(types, [](const auto &type) { return !type.second->isTemplateType(); });

                if (menu::beginMenu("hex.builtin.view.pattern_editor.menu.edit.place_pattern.custom"_lang, hasPlaceableTypes)) {
                    const auto &selection = ImHexApi::HexEditor::getSelection();

                    for (const auto &[typeName, type] : types) {
                        if (type->isTemplateType())
                            continue;

                        createNestedMenu(wolv::util::splitString(typeName, "::"), [&, this] {
                            std::string variableName;
                            for (const char c : wolv::util::replaceStrings(typeName, "::", "_"))
                                variableName += static_cast<char>(std::tolower(c));
                            variableName += fmt::format("_at_0x{:02X}", selection->getStartAddress());

                            appendEditorText(fmt::format("{0} {1} @ 0x{2:02X};", typeName, variableName, selection->getStartAddress()));
                        });
                    }

                    menu::endMenu();
                }
            }, [this] {
                return ImHexApi::Provider::isValid() && ImHexApi::HexEditor::isSelectionValid() && m_runningParsers == 0;
            }, ContentRegistry::Views::getViewByName("hex.builtin.view.hex_editor.name"));
    }

    void ViewPatternEditor::registerHandlers() {
        ContentRegistry::FileHandler::add({ ".hexpat", ".pat" }, [](const std::fs::path &path) -> bool {
            wolv::io::File file(path, wolv::io::File::Mode::Read);

            if (file.isValid()) {
                RequestSetPatternLanguageCode::post(wolv::util::preprocessText(file.readString()));
                return true;
            } else {
                return false;
            }
        });

        ContentRegistry::Settings::onChange("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.pattern_parent_highlighting", [this](const ContentRegistry::Settings::SettingsValue &value) {
            m_parentHighlightingEnabled = bool(value.get<int>(false));
        });

        ImHexApi::HexEditor::addBackgroundHighlightingProvider([this](u64 address, const u8 *data, size_t size, bool) -> std::optional<color_t> {
            std::ignore = data;
            std::ignore = size;

            if (m_runningEvaluators != 0)
                return std::nullopt;

            const auto &runtime = ContentRegistry::PatternLanguage::getRuntime();

            std::optional<ImColor> color;

            if (TRY_LOCK(ContentRegistry::PatternLanguage::getRuntimeLock())) {
                for (const auto &patternColor : runtime.getColorsAtAddress(address)) {
                    color = blendColors(color, patternColor);
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
            std::ignore = data;
            std::ignore = size;

            if (TRY_LOCK(ContentRegistry::PatternLanguage::getRuntimeLock())) {
                const auto &runtime = ContentRegistry::PatternLanguage::getRuntime();

                std::set<pl::ptrn::Pattern*> drawnPatterns;
                for (u64 offset = 0; offset < size; offset += 1) {
                    auto patterns = runtime.getPatternsAtAddress(address + offset);
                    if (!patterns.empty() && !std::ranges::all_of(patterns, [](const auto &pattern) { return pattern->getVisibility() == pl::ptrn::Visibility::Hidden || pattern->getVisibility() == pl::ptrn::Visibility::HighlightHidden; })) {
                        ImGui::BeginTooltip();

                        for (const auto &pattern : patterns) {
                            // Avoid drawing the same pattern multiple times
                            if (!drawnPatterns.insert(pattern).second)
                                continue;

                            auto visibility = pattern->getVisibility();
                            if (visibility == pl::ptrn::Visibility::Hidden || visibility == pl::ptrn::Visibility::HighlightHidden)
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
            }
        });

        ProjectFile::registerPerProviderHandler({
            .basePath = "pattern_source_code.hexpat",
            .required = false,
            .load = [this](prv::Provider *provider, const std::fs::path &basePath, const Tar &tar) {
                const auto sourceCode = wolv::util::preprocessText(tar.readString(basePath));

                m_sourceCode.get(provider) = sourceCode;

                if (provider == ImHexApi::Provider::get())
                    m_textEditor.get(provider).setText(sourceCode);

                m_hasUnevaluatedChanges.get(provider) = true;
                m_textHighlighter.m_needsToUpdateColors = false;
                return true;
            },
            .store = [this](prv::Provider *provider, const std::fs::path &basePath, const Tar &tar) {
                if (provider == ImHexApi::Provider::get())
                    m_sourceCode.get(provider) = m_textEditor.get(provider).getText();

                const auto &sourceCode = m_sourceCode.get(provider);

                tar.writeString(basePath, wolv::util::trim(sourceCode));
                return true;
            }
        });

        ShortcutManager::addShortcut(this, CTRL + SHIFT + Keys::C + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.match_case_toggle", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr) {
                ui::TextEditor::FindReplaceHandler *findReplaceHandler = editor->getFindReplaceHandler();
                findReplaceHandler->setMatchCase(editor, !findReplaceHandler->getMatchCase());
            }
        });

        ShortcutManager::addShortcut(this, CTRL + SHIFT + Keys::R + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.regex_toggle", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr) {
                ui::TextEditor::FindReplaceHandler *findReplaceHandler = editor->getFindReplaceHandler();
                findReplaceHandler->setFindRegEx(editor, !findReplaceHandler->getFindRegEx());
            }
        });

        ShortcutManager::addShortcut(this, CTRL + SHIFT + Keys::W + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.whole_word_toggle", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr) {
                ui::TextEditor::FindReplaceHandler *findReplaceHandler = editor->getFindReplaceHandler();
                findReplaceHandler->setWholeWord(editor, !findReplaceHandler->getWholeWord());
            }
        });

        ShortcutManager::addShortcut(this, Keys::Delete + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.delete", [this] {
            if (m_focusedSubWindowName.contains(textEditorView))
                m_textEditor.get(ImHexApi::Provider::get()).deleteChar();
        });

        ShortcutManager::addShortcut(this, SHIFT + Keys::Right + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_right", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveRight(1, true, false);
        });

        ShortcutManager::addShortcut(this, CTRLCMD + SHIFT + Keys::Right + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_word_right", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveRight(1, true, true);
        });

        ShortcutManager::addShortcut(this, SHIFT + Keys::Left + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_left", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveLeft(1, true, false);
        });

        ShortcutManager::addShortcut(this, CTRLCMD + SHIFT + Keys::Left + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_word_left", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveLeft(1, true, true);
        });

        ShortcutManager::addShortcut(this, SHIFT + Keys::Up + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_up", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveUp(1, true);
        });

        ShortcutManager::addShortcut(this, SHIFT +Keys::PageUp + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_page_up", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveUp(editor->getPageSize() - 4, true);
        });

        ShortcutManager::addShortcut(this, SHIFT + Keys::Down + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_down", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveDown(1, true);
        });

        ShortcutManager::addShortcut(this, SHIFT +Keys::PageDown + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_page_down", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveDown(editor->getPageSize() - 4, true);
        });

        ShortcutManager::addShortcut(this, CTRLCMD + SHIFT + Keys::Home + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_top", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveTop(true);
        });

        ShortcutManager::addShortcut(this, CTRLCMD + SHIFT + Keys::End + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_bottom", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveBottom(true);
        });

        ShortcutManager::addShortcut(this, SHIFT + Keys::Home + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_home", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveHome(true);
        });

        ShortcutManager::addShortcut(this, SHIFT + Keys::End + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.select_end", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveEnd(true);
        });

        ShortcutManager::addShortcut(this, CTRLCMD + Keys::Delete + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.delete_word_right", [this] {
            if (m_focusedSubWindowName.contains(textEditorView))
                m_textEditor.get(ImHexApi::Provider::get()).deleteWordRight();
        });

        ShortcutManager::addShortcut(this, CTRLCMD + Keys::Backspace + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.delete_word_left", [this] {
            if (m_focusedSubWindowName.contains(textEditorView))
                m_textEditor.get(ImHexApi::Provider::get()).deleteWordLeft();
        });

        ShortcutManager::addShortcut(this, Keys::Backspace + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.backspace", [this] {
            if (m_focusedSubWindowName.contains(textEditorView))
                m_textEditor.get(ImHexApi::Provider::get()).backspace();
        });

        ShortcutManager::addShortcut(this, SHIFT + Keys::Backspace + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.backspace_shifted", [this] {
            if (m_focusedSubWindowName.contains(textEditorView))
                m_textEditor.get(ImHexApi::Provider::get()).backspace();
        });

        ShortcutManager::addShortcut(this, Keys::Insert + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.toggle_insert", [this] {
            if (m_focusedSubWindowName.contains(textEditorView))
                m_textEditor.get(ImHexApi::Provider::get()).setOverwrite(!m_textEditor.get(ImHexApi::Provider::get()).isOverwrite());
        });

        ShortcutManager::addShortcut(this, CTRLCMD + Keys::Right + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_word_right", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveRight(1, false, true);
        });

        ShortcutManager::addShortcut(this, Keys::Right + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_right", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveRight(1, false, false);
        });

        ShortcutManager::addShortcut(this, CTRLCMD + Keys::Left + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_word_left", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveLeft(1, false, true);
        });

        ShortcutManager::addShortcut(this, Keys::Left + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_left", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveLeft(1, false, false);
        });

        ShortcutManager::addShortcut(this, Keys::Up + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_up", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveUp(1, false);
        });

        ShortcutManager::addShortcut(this, ALT + Keys::Up + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_pixel_up", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveUp(-1, false);
        });

        ShortcutManager::addShortcut(this, Keys::PageUp + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_page_up", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveUp(editor->getPageSize() - 4, false);
        });

        ShortcutManager::addShortcut(this, Keys::Down + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_down", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveDown(1, false);
        });

        ShortcutManager::addShortcut(this, ALT+ Keys::Down + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_pixel_down", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveDown(-1, false);
        });

        ShortcutManager::addShortcut(this, Keys::PageDown + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_page_down", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveDown(editor->getPageSize() - 4, false);
        });

        ShortcutManager::addShortcut(this, CTRLCMD + Keys::Home + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_top", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveTop(false);
        });

        ShortcutManager::addShortcut(this, CTRLCMD + Keys::End + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_bottom", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveBottom(false);
        });

        ShortcutManager::addShortcut(this, Keys::Home + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_home", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveHome(false);
        });

        ShortcutManager::addShortcut(this, Keys::End + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_end", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveEnd(false);
        });

        ShortcutManager::addShortcut(this, CTRLCMD + SHIFT + Keys::M + AllowWhileTyping, "hex.builtin.view.pattern_editor.shortcut.move_matched_bracket", [this] {
            if (auto editor = getEditorFromFocusedWindow(); editor != nullptr)
                editor->moveToMatchedBracket(false);
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

    void ViewPatternEditor::handleFileChange(prv::Provider *provider) {
        if (m_ignoreNextChangeEvent.get(provider)) {
            m_ignoreNextChangeEvent.get(provider) = false;
            return;
        }

        if (m_changeEventAcknowledgementPending.get(provider)) {
            return;
        }

        m_changeEventAcknowledgementPending.get(provider) = true;
        hex::ui::BannerButton::open(ICON_VS_INFO, "hex.builtin.provider.file.reload_changes", ImColor(66, 104, 135), "hex.builtin.provider.file.reload_changes.reload", [this, provider] {
            m_changeEventAcknowledgementPending.get(provider) = false;
        });
    }
}
