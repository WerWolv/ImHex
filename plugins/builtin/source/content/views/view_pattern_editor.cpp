#include "content/views/view_pattern_editor.hpp"

#include <hex/api/content_registry.hpp>

#include <pl/patterns/pattern.hpp>
#include <pl/core/preprocessor.hpp>
#include <pl/core/parser.hpp>
#include <pl/core/ast/ast_node_variable_decl.hpp>
#include <pl/core/ast/ast_node_type_decl.hpp>
#include <pl/core/ast/ast_node_builtin_type.hpp>

#include <hex/helpers/fs.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/helpers/magic.hpp>
#include <hex/helpers/binary_pattern.hpp>

#include <content/popups/popup_file_chooser.hpp>
#include <content/popups/popup_question.hpp>

#include <nlohmann/json.hpp>
#include <chrono>

#include <wolv/io/file.hpp>
#include <wolv/io/fs.hpp>
#include <wolv/utils/guards.hpp>
#include <wolv/utils/lock.hpp>

namespace hex::plugin::builtin {

    using namespace hex::literals;

    static const TextEditor::LanguageDefinition &PatternLanguage() {
        static bool initialized = false;
        static TextEditor::LanguageDefinition langDef;
        if (!initialized) {
            constexpr static std::array keywords = {
                "using", "struct", "union", "enum", "bitfield", "be", "le", "if", "else", "match", "false", "true", "this", "parent", "addressof", "sizeof", "$", "while", "for", "fn", "return", "break", "continue", "namespace", "in", "out", "ref", "null", "const", "unsigned", "signed", "try", "catch"
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
                } else if (TokenizeCStyleNumber(inBegin, inEnd, outBegin, outEnd))
                    paletteIndex = TextEditor::PaletteIndex::Number;
                else if (TokenizeCStyleCharacterLiteral(inBegin, inEnd, outBegin, outEnd))
                    paletteIndex = TextEditor::PaletteIndex::CharLiteral;
                else if (TokenizeCStyleString(inBegin, inEnd, outBegin, outEnd))
                    paletteIndex = TextEditor::PaletteIndex::String;

                return paletteIndex != TextEditor::PaletteIndex::Max;
            };

            langDef.mCommentStart      = "/*";
            langDef.mCommentEnd        = "*/";
            langDef.mSingleLineComment = "//";

            langDef.mCaseSensitive   = true;
            langDef.mAutoIndentation = true;
            langDef.mPreprocChar     = '#';

            langDef.mName = "Pattern Language";

            initialized = true;
        }
        return langDef;
    }

    ViewPatternEditor::ViewPatternEditor() : View("hex.builtin.view.pattern_editor.name") {
        this->m_parserRuntime = std::make_unique<pl::PatternLanguage>();
        ContentRegistry::PatternLanguage::configureRuntime(*this->m_parserRuntime, nullptr);

        this->m_textEditor.SetLanguageDefinition(PatternLanguage());
        this->m_textEditor.SetShowWhitespaces(false);

        this->registerEvents();
        this->registerMenuItems();
        this->registerHandlers();
    }

    ViewPatternEditor::~ViewPatternEditor() {
        EventManager::unsubscribe<RequestSetPatternLanguageCode>(this);
        EventManager::unsubscribe<EventFileLoaded>(this);
        EventManager::unsubscribe<RequestChangeTheme>(this);
        EventManager::unsubscribe<EventProviderChanged>(this);
        EventManager::unsubscribe<EventProviderClosed>(this);
    }

    void ViewPatternEditor::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.pattern_editor.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_None | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            auto provider = ImHexApi::Provider::get();

            if (ImHexApi::Provider::isValid() && provider->isAvailable()) {
                static float height = 0;
                static bool dragging = false;

                auto availableSize = ImGui::GetContentRegionAvail();
                auto textEditorSize = availableSize;
                textEditorSize.y *= 3.5 / 5.0;
                textEditorSize.y -= ImGui::GetTextLineHeightWithSpacing();
                textEditorSize.y += height;

                if (availableSize.y > 1)
                    textEditorSize.y = std::clamp(textEditorSize.y, 1.0F, availableSize.y - ImGui::GetTextLineHeightWithSpacing() * 3);

                this->m_textEditor.Render("hex.builtin.view.pattern_editor.name"_lang, textEditorSize, true);

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
                        this->drawConsole(settingsSize, *this->m_console);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("hex.builtin.view.pattern_editor.env_vars"_lang)) {
                        this->drawEnvVars(settingsSize, *this->m_envVarEntries);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("hex.builtin.view.pattern_editor.settings"_lang)) {
                        this->drawVariableSettings(settingsSize, *this->m_patternVariables);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("hex.builtin.view.pattern_editor.sections"_lang)) {
                        this->drawSectionSelector(settingsSize, *this->m_sections);
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
                        if (this->m_breakpointHit) {
                            if (ImGui::IconButton(ICON_VS_DEBUG_CONTINUE, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarYellow)))
                                this->m_breakpointHit = false;
                        } else {
                            if (ImGui::IconButton(ICON_VS_DEBUG_STOP, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed)))
                                runtime.abort();
                        }
                    } else {
                        if (ImGui::IconButton(ICON_VS_DEBUG_START, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarGreen)) || this->m_triggerEvaluation) {
                            this->m_triggerEvaluation = false;
                            this->evaluatePattern(this->m_textEditor.GetText(), provider);
                        }
                    }


                    ImGui::PopStyleVar();

                    ImGui::SameLine();
                    if (this->m_runningEvaluators > 0) {
                        if (this->m_breakpointHit) {
                            ImGui::TextFormatted("hex.builtin.view.pattern_editor.breakpoint_hit"_lang, runtime.getInternals().evaluator->getPauseLine().value_or(0));
                        } else {
                            ImGui::TextSpinner("hex.builtin.view.pattern_editor.evaluating"_lang);
                        }
                    } else {
                        if (ImGui::Checkbox("hex.builtin.view.pattern_editor.auto"_lang, &this->m_runAutomatically)) {
                            if (this->m_runAutomatically)
                                this->m_hasUnevaluatedChanges = true;
                        }

                        ImGui::SameLine();
                        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                        ImGui::SameLine();

                        if (auto max = runtime.getMaximumPatternCount(); max >= std::numeric_limits<u32>::max()) {
                            ImGui::TextFormatted("{} / {}",
                                                 runtime.getCreatedPatternCount(),
                                                 ICON_FA_INFINITY);
                        } else {
                            ImGui::TextFormatted("{} / {}",
                                                 runtime.getCreatedPatternCount(),
                                                 runtime.getMaximumPatternCount());
                        }
                    }
                }

                if (this->m_textEditor.IsTextChanged()) {
                    this->m_hasUnevaluatedChanges = true;
                    ImHexApi::Provider::markDirty();
                }

                if (this->m_hasUnevaluatedChanges && this->m_runningEvaluators == 0 && this->m_runningParsers == 0) {
                    this->m_hasUnevaluatedChanges = false;

                    auto code = this->m_textEditor.GetText();
                    EventManager::post<EventPatternEditorChanged>(code);

                    TaskManager::createBackgroundTask("Pattern Parsing", [this, code, provider](auto &){
                        this->parsePattern(code, provider);

                        if (this->m_runAutomatically)
                            this->evaluatePattern(code, provider);
                    });
                }
            }

            if (this->m_dangerousFunctionCalled && !ImGui::IsPopupOpen(ImGuiID(0), ImGuiPopupFlags_AnyPopup)) {
                PopupQuestion::open("hex.builtin.view.pattern_editor.dangerous_function.desc"_lang,
                    [this] {
                        this->m_dangerousFunctionsAllowed = DangerousFunctionPerms::Allow;
                    }, [this] {
                        this->m_dangerousFunctionsAllowed = DangerousFunctionPerms::Deny;
                    }
                );

                this->m_dangerousFunctionCalled = false;
            }

            View::discardNavigationRequests();
        }
        ImGui::End();
    }

    void ViewPatternEditor::drawConsole(ImVec2 size, const std::vector<std::pair<pl::core::LogConsole::Level, std::string>> &console) {
        const auto &palette = TextEditor::GetPalette();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, palette[u32(TextEditor::PaletteIndex::Background)]);
        if (ImGui::BeginChild("##console", size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_HorizontalScrollbar)) {
            ImGuiListClipper clipper;

            std::scoped_lock lock(this->m_logMutex);
            clipper.Begin(console.size());

            while (clipper.Step())
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                    auto [level, message] = console[i];
                    std::replace_if(message.begin(), message.end(), [](char c) { return c == 0x00; }, ' ');

                    switch (level) {
                        using enum pl::core::LogConsole::Level;

                        case Debug:
                            ImGui::PushStyleColor(ImGuiCol_Text, palette[u32(TextEditor::PaletteIndex::Comment)]);
                            break;
                        case Info:
                            ImGui::PushStyleColor(ImGuiCol_Text, palette[u32(TextEditor::PaletteIndex::Default)]);
                            break;
                        case Warning:
                            ImGui::PushStyleColor(ImGuiCol_Text, palette[u32(TextEditor::PaletteIndex::Preprocessor)]);
                            break;
                        case Error:
                            ImGui::PushStyleColor(ImGuiCol_Text, palette[u32(TextEditor::PaletteIndex::ErrorMarker)]);
                            break;
                        default:
                            continue;
                    }

                    if (ImGui::Selectable(hex::format("{}##ConsoleLine", message).c_str()))
                        ImGui::SetClipboardText(message.c_str());

                    ImGui::PopStyleColor();
                }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(1);
    }

    void ViewPatternEditor::drawEnvVars(ImVec2 size, std::list<EnvVar> &envVars) {
        static u32 envVarCounter = 1;

        if (ImGui::BeginChild("##env_vars", size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
            int index = 0;
            if (ImGui::BeginTable("##env_vars_table", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerH)) {
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.1F);
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.4F);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.38F);
                ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthStretch, 0.12F);

                for (auto iter = envVars.begin(); iter != envVars.end(); iter++) {
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

                    if (ImGui::IconButton(ICON_VS_ADD, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                        envVars.insert(std::next(iter), { envVarCounter++, "", i128(0), EnvVarType::Integer });
                    }

                    ImGui::SameLine();

                    ImGui::BeginDisabled(envVars.size() <= 1);
                    {
                        if (ImGui::IconButton(ICON_VS_REMOVE, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                            bool isFirst = iter == envVars.begin();
                            bool isLast  = std::next(iter) == envVars.end();
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
                ImGui::TextFormattedCentered("hex.builtin.view.pattern_editor.no_in_out_vars"_lang);
            } else {
                if (ImGui::BeginTable("##in_out_vars_table", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerH)) {
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.4F);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.6F);

                    for (auto &[name, variable] : patternVariables) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        ImGui::TextUnformatted(name.c_str());

                        ImGui::TableNextColumn();

                        if (variable.outVariable) {
                            ImGui::TextUnformatted(variable.value.toString(true).c_str());
                        } else if (variable.inVariable) {
                            const std::string label { "##" + name };

                            if (pl::core::Token::isSigned(variable.type)) {
                                i64 value = hex::get_or<i128>(variable.value, 0);
                                ImGui::InputScalar(label.c_str(), ImGuiDataType_S64, &value);
                                variable.value = i128(value);
                            } else if (pl::core::Token::isUnsigned(variable.type)) {
                                u64 value = hex::get_or<u128>(variable.value, 0);
                                ImGui::InputScalar(label.c_str(), ImGuiDataType_U64, &value);
                                variable.value = u128(value);
                            } else if (pl::core::Token::isFloatingPoint(variable.type)) {
                                auto value = hex::get_or<double>(variable.value, 0.0);
                                ImGui::InputScalar(label.c_str(), ImGuiDataType_Double, &value);
                                variable.value = value;
                            } else if (variable.type == pl::core::Token::ValueType::Boolean) {
                                auto value = hex::get_or<bool>(variable.value, false);
                                ImGui::Checkbox(label.c_str(), &value);
                                variable.value = value;
                            } else if (variable.type == pl::core::Token::ValueType::Character) {
                                std::array<char, 2> buffer = { };
                                ImGui::InputText(label.c_str(), buffer.data(), buffer.size());
                                variable.value = buffer[0];
                            }
                        }
                    }

                    ImGui::EndTable();
                }
            }
        }
        ImGui::EndChild();
    }

    void ViewPatternEditor::drawSectionSelector(ImVec2 size, std::map<u64, pl::api::Section> &sections) {
        auto &runtime = ContentRegistry::PatternLanguage::getRuntime();

        if (ImGui::BeginTable("##sections_table", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, size)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("hex.builtin.common.name"_lang, ImGuiTableColumnFlags_WidthStretch, 0.5F);
            ImGui::TableSetupColumn("hex.builtin.common.size"_lang, ImGuiTableColumnFlags_WidthStretch, 0.5F);
            ImGui::TableSetupColumn("##button", ImGuiTableColumnFlags_WidthFixed, 20_scaled);

            ImGui::TableHeadersRow();

            if (TRY_LOCK(ContentRegistry::PatternLanguage::getRuntimeLock())) {
                for (auto &[id, section] : sections) {
                    ImGui::PushID(id);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    ImGui::TextUnformatted(section.name.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextFormatted("{} | 0x{:02X}", hex::toByteString(section.data.size()), section.data.size());
                    ImGui::TableNextColumn();
                    if (ImGui::IconButton(ICON_VS_OPEN_PREVIEW, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                        auto dataProvider = std::make_unique<MemoryFileProvider>();
                        dataProvider->resize(section.data.size());
                        dataProvider->writeRaw(0x00, section.data.data(), section.data.size());
                        dataProvider->setReadOnly(true);

                        auto hexEditor = auto(this->m_sectionHexEditor);

                        hexEditor.setBackgroundHighlightCallback([this, id, &runtime](u64 address, const u8 *, size_t) -> std::optional<color_t> {
                            if (this->m_runningEvaluators != 0)
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


                        this->m_sectionWindowDrawer[patternProvider] = [this, id, patternProvider, dataProvider = std::move(dataProvider), hexEditor, patternDrawer = ui::PatternDrawer(), &runtime] mutable {
                            hexEditor.setProvider(dataProvider.get());
                            hexEditor.draw(480_scaled);
                            patternDrawer.setSelectionCallback([&](const auto &region) {
                                hexEditor.setSelection(region);
                            });

                            const auto &patterns = [&, this] -> const auto& {
                                if (patternProvider->isReadable() && *this->m_executionDone)
                                    return runtime.getPatterns(id);
                                else {
                                    static const std::vector<std::shared_ptr<pl::ptrn::Pattern>> empty;
                                    return empty;
                                }
                            }();

                            if (*this->m_executionDone)
                                patternDrawer.draw(patterns, &runtime, 150_scaled);
                        };
                    }

                    ImGui::PopID();
                }
            }

            ImGui::EndTable();
        }
    }

    void ViewPatternEditor::drawDebugger(ImVec2 size) {
        auto &runtime = ContentRegistry::PatternLanguage::getRuntime();
        auto &evaluator = runtime.getInternals().evaluator;

        if (ImGui::BeginChild("##debugger", size, true)) {
            const auto &breakpoints = evaluator->getBreakpoints();
            auto line = this->m_textEditor.GetCursorPosition().mLine + 1;

            if (!breakpoints.contains(line)) {
                if (ImGui::IconButton(ICON_VS_DEBUG_BREAKPOINT, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed))) {
                    evaluator->addBreakpoint(line);
                    this->m_textEditor.SetBreakpoints(breakpoints);
                }
                ImGui::InfoTooltip("hex.builtin.view.pattern_editor.debugger.add_tooltip"_lang);
            } else {
                if (ImGui::IconButton(ICON_VS_DEBUG_BREAKPOINT_UNVERIFIED, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed))) {
                    evaluator->removeBreakpoint(line);
                    this->m_textEditor.SetBreakpoints(breakpoints);
                }
                ImGui::InfoTooltip("hex.builtin.view.pattern_editor.debugger.remove_tooltip"_lang);
            }

            if (*this->m_breakpointHit) {
                auto &variables = *evaluator->getScope(0).scope;

                if (this->m_resetDebuggerVariables) {
                    this->m_debuggerDrawer->reset();
                    this->m_resetDebuggerVariables = false;

                    auto pauseLine = evaluator->getPauseLine();
                    if (pauseLine.has_value())
                        this->m_textEditor.SetCursorPosition({ int(pauseLine.value() - 1), 0 });
                }

                this->m_debuggerDrawer->draw(variables, &runtime, size.y - ImGui::GetTextLineHeightWithSpacing() * 4);
            }
        }
        ImGui::EndChild();
    }

    void ViewPatternEditor::drawAlwaysVisible() {
        auto provider = ImHexApi::Provider::get();

        auto open = this->m_sectionWindowDrawer.contains(provider);
        if (open) {
            ImGui::SetNextWindowSize(scaled(ImVec2(600, 700)), ImGuiCond_Appearing);
            if (ImGui::Begin("hex.builtin.view.pattern_editor.section_popup"_lang, &open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                this->m_sectionWindowDrawer[provider]();
            }
            ImGui::End();
        }

        if (!open && this->m_sectionWindowDrawer.contains(provider)) {
            ImHexApi::HexEditor::setSelection(Region::Invalid());
            this->m_sectionWindowDrawer.erase(provider);
        }

        if (!this->m_lastEvaluationProcessed) {
            if (!this->m_lastEvaluationResult) {
                if (this->m_lastEvaluationError->has_value()) {
                    TextEditor::ErrorMarkers errorMarkers = {
                            { (*this->m_lastEvaluationError)->line, (*this->m_lastEvaluationError)->message }
                    };
                    this->m_textEditor.SetErrorMarkers(errorMarkers);
                }
            } else {
                for (auto &[name, variable] : *this->m_patternVariables) {
                    if (variable.outVariable && this->m_lastEvaluationOutVars->contains(name))
                        variable.value = this->m_lastEvaluationOutVars->at(name);
                }

                EventManager::post<EventHighlightingChanged>();
            }

            this->m_lastEvaluationProcessed = true;
            *this->m_executionDone = true;
        }

        if (this->m_shouldAnalyze) {
            this->m_shouldAnalyze = false;

            TaskManager::createBackgroundTask("Analyzing file content", [this, provider](auto &) {
                if (!this->m_autoLoadPatterns)
                    return;

                // Copy over current pattern source code to the new provider
                if (!this->m_syncPatternSourceCode) {
                    *this->m_sourceCode = this->m_textEditor.GetText();
                }

                pl::PatternLanguage runtime;
                ContentRegistry::PatternLanguage::configureRuntime(runtime, provider);

                auto mimeType = magic::getMIMEType(provider);

                bool foundCorrectType = false;
                runtime.addPragma("MIME", [&mimeType, &foundCorrectType](pl::PatternLanguage &runtime, const std::string &value) {
                    hex::unused(runtime);

                    if (!magic::isValidMIMEType(value))
                        return false;

                    if (value == mimeType) {
                        foundCorrectType = true;
                        return true;
                    }
                    return !std::all_of(value.begin(), value.end(), isspace) && !value.ends_with('\n') && !value.ends_with('\r');
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

                        auto end = value.find(']');
                        if (end == std::string::npos)
                            return std::nullopt;

                        value = value.substr(0, end - 1);
                        value = wolv::util::trim(value);

                        return BinaryPattern(value);
                }();

                    const auto address = [value = value] mutable -> std::optional<u64> {
                        value = wolv::util::trim(value);

                        if (value.empty())
                            return std::nullopt;

                        auto start = value.find('@');
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
                    provider->read(*address, bytes.data(), bytes.size());

                    if (pattern->matches(bytes))
                        foundCorrectType = true;

                    return true;
                });

                this->m_possiblePatternFiles.get(provider).clear();

                std::error_code errorCode;
                for (const auto &dir : fs::getDefaultPaths(fs::ImHexPath::Patterns)) {
                    for (auto &entry : std::fs::recursive_directory_iterator(dir, errorCode)) {
                        foundCorrectType = false;
                        if (!entry.is_regular_file())
                            continue;

                        wolv::io::File file(entry.path(), wolv::io::File::Mode::Read);
                        if (!file.isValid())
                            continue;

                        runtime.getInternals().preprocessor->preprocess(runtime, file.readString());

                        if (foundCorrectType)
                            this->m_possiblePatternFiles.get(provider).push_back(entry.path());

                        runtime.reset();
                    }
                }

                if (!this->m_possiblePatternFiles.get(provider).empty()) {
                    PopupAcceptPattern::open(this);
                }
            });
        }
    }


    void ViewPatternEditor::drawPatternTooltip(pl::ptrn::Pattern *pattern) {
        ImGui::PushID(pattern);
        {
            ImGui::ColorButton(pattern->getVariableName().c_str(), ImColor(pattern->getColor()));
            ImGui::SameLine(0, 10);
            ImGui::TextFormattedColored(ImColor(0xFF9BC64D), "{} ", pattern->getFormattedName());
            ImGui::SameLine(0, 5);
            ImGui::TextFormatted("{}", pattern->getDisplayName());
            ImGui::SameLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();
            ImGui::TextFormatted("{} ", hex::limitStringLength(pattern->getFormattedValue(), 64));

            if (ImGui::GetIO().KeyShift) {
                ImGui::Indent();
                if (ImGui::BeginTable("##extra_info", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_NoClip)) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextFormatted("{} ", "hex.builtin.common.type"_lang);
                    ImGui::TableNextColumn();
                    ImGui::TextFormatted(" {}", pattern->getTypeName());

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextFormatted("{} ", "hex.builtin.common.address"_lang);
                    ImGui::TableNextColumn();
                    ImGui::TextFormatted(" 0x{:08X}", pattern->getOffset());

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextFormatted("{} ", "hex.builtin.common.size"_lang);
                    ImGui::TableNextColumn();
                    ImGui::TextFormatted(" {}", hex::toByteString(pattern->getSize()));

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextFormatted("{} ", "hex.builtin.common.endian"_lang);
                    ImGui::TableNextColumn();
                    ImGui::TextFormatted(" {}", pattern->getEndian() == std::endian::little ? "hex.builtin.common.little"_lang : "hex.builtin.common.big"_lang);

                    if (const auto &comment = pattern->getComment(); !comment.empty()) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextFormatted("{} ", "hex.builtin.common.comment"_lang);
                        ImGui::TableNextColumn();
                        ImGui::TextWrapped(" \"%s\"", comment.c_str());
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
            this->m_textEditor.SetText(code);

            TaskManager::createBackgroundTask("Parse pattern", [this, code, provider](auto&) { this->parsePattern(code, provider); });
        }
    }

    void ViewPatternEditor::parsePattern(const std::string &code, prv::Provider *provider) {
        this->m_runningParsers++;

        ContentRegistry::PatternLanguage::configureRuntime(*this->m_parserRuntime, nullptr);
        auto ast = this->m_parserRuntime->parseString(code);

        auto &patternVariables = this->m_patternVariables.get(provider);

        patternVariables.clear();

        if (ast) {
            for (auto &node : *ast) {
                if (auto variableDecl = dynamic_cast<pl::core::ast::ASTNodeVariableDecl *>(node.get())) {
                    auto type = dynamic_cast<pl::core::ast::ASTNodeTypeDecl *>(variableDecl->getType().get());
                    if (type == nullptr) continue;

                    auto builtinType = dynamic_cast<pl::core::ast::ASTNodeBuiltinType *>(type->getType().get());
                    if (builtinType == nullptr)
                        continue;

                    PatternVariable variable = {
                        .inVariable  = variableDecl->isInVariable(),
                        .outVariable = variableDecl->isOutVariable(),
                        .type        = builtinType->getType(),
                        .value       = { }
                    };

                    if (variable.inVariable || variable.outVariable) {
                        if (!patternVariables.contains(variableDecl->getName()))
                            patternVariables[variableDecl->getName()] = variable;
                    }
                }
            }
        }

        this->m_runningParsers--;
    }

    void ViewPatternEditor::evaluatePattern(const std::string &code, prv::Provider *provider) {
        auto lock = std::scoped_lock(ContentRegistry::PatternLanguage::getRuntimeLock());

        this->m_runningEvaluators++;
        *this->m_executionDone = false;


        this->m_textEditor.SetErrorMarkers({});
        this->m_console->clear();

        this->m_sectionWindowDrawer.clear();

        EventManager::post<EventHighlightingChanged>();

        TaskManager::createTask("hex.builtin.view.pattern_editor.evaluating", TaskManager::NoProgress, [this, code, provider](auto &task) {
            auto lock = std::scoped_lock(ContentRegistry::PatternLanguage::getRuntimeLock());

            auto &runtime = ContentRegistry::PatternLanguage::getRuntime();
            ContentRegistry::PatternLanguage::configureRuntime(runtime, provider);
            runtime.getInternals().evaluator->setBreakpointHitCallback([this]{
                *this->m_breakpointHit = true;
                this->m_resetDebuggerVariables = true;
                while (*this->m_breakpointHit) {
                    std::this_thread::yield();
                }
            });

            task.setInterruptCallback([&runtime] { runtime.abort(); });

            std::map<std::string, pl::core::Token::Literal> envVars;
            for (const auto &[id, name, value, type] : *this->m_envVarEntries)
                envVars.insert({ name, value });

            std::map<std::string, pl::core::Token::Literal> inVariables;
            for (auto &[name, variable] : *this->m_patternVariables) {
                if (variable.inVariable)
                    inVariables[name] = variable.value;
            }

            runtime.setDangerousFunctionCallHandler([this]{
                this->m_dangerousFunctionCalled = true;

                while (this->m_dangerousFunctionsAllowed == DangerousFunctionPerms::Ask) {
                    std::this_thread::yield();
                }

                return this->m_dangerousFunctionsAllowed == DangerousFunctionPerms::Allow;
            });

            runtime.setLogCallback([this](auto level, auto message) {
                std::scoped_lock lock(this->m_logMutex);
                this->m_console->emplace_back(level, message);
            });

            ON_SCOPE_EXIT {
                *this->m_lastEvaluationOutVars = runtime.getOutVariables();
                *this->m_sections              = runtime.getSections();

                this->m_runningEvaluators--;

                this->m_lastEvaluationProcessed = false;

                std::scoped_lock lock(this->m_logMutex);
                this->m_console->emplace_back(
                   pl::core::LogConsole::Level::Info,
                   hex::format("Evaluation took {}", runtime.getLastRunningTime())
                );
            };


            this->m_lastEvaluationResult = runtime.executeString(code, envVars, inVariables);
            if (!this->m_lastEvaluationResult) {
                *this->m_lastEvaluationError = runtime.getError();
            }

            TaskManager::doLater([code] {
                EventManager::post<EventPatternExecuted>(code);
            });
        });
    }

    void ViewPatternEditor::registerEvents() {
        EventManager::subscribe<RequestLoadPatternLanguageFile>(this, [this](const std::fs::path &path) {
            this->loadPatternFile(path, ImHexApi::Provider::get());
        });

        EventManager::subscribe<RequestSavePatternLanguageFile>(this, [this](const std::fs::path &path) {
            wolv::io::File file(path, wolv::io::File::Mode::Create);
            file.writeString(wolv::util::trim(this->m_textEditor.GetText()));
        });

        EventManager::subscribe<RequestSetPatternLanguageCode>(this, [this](const std::string &code) {
            this->m_textEditor.SetText(code);
            this->m_hasUnevaluatedChanges = true;
        });

        EventManager::subscribe<EventSettingsChanged>(this, [this] {
            this->m_syncPatternSourceCode = ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.sync_pattern_source", 0) == 1;
            this->m_autoLoadPatterns      = ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.auto_load_patterns", 1) == 1;
        });

        EventManager::subscribe<EventProviderOpened>(this, [this](prv::Provider *provider) {
            this->m_shouldAnalyze.get(provider) = true;
            this->m_envVarEntries->push_back({ 0, "", 0, EnvVarType::Integer });
        });

        EventManager::subscribe<EventProviderChanged>(this, [this](prv::Provider *oldProvider, prv::Provider *newProvider) {
            if (!this->m_syncPatternSourceCode) {
                if (oldProvider != nullptr)
                    this->m_sourceCode.get(oldProvider) = this->m_textEditor.GetText();

                if (newProvider != nullptr)
                    this->m_textEditor.SetText(wolv::util::trim(this->m_sourceCode.get(newProvider)));
                else
                    this->m_textEditor.SetText("");
            } else {
                this->m_hasUnevaluatedChanges = true;
            }
        });

        EventManager::subscribe<EventProviderClosed>(this, [this](prv::Provider *) {
            if (this->m_syncPatternSourceCode && ImHexApi::Provider::getProviders().empty()) {
                this->m_textEditor.SetText("");
            }
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
        this->m_textEditor.SetCursorPosition(TextEditor::Coordinates { this->m_textEditor.GetTotalLines(), 0 });
        this->m_textEditor.InsertText(hex::format("\n{0}", text));
        this->m_triggerEvaluation = true;
    }

    void ViewPatternEditor::appendVariable(const std::string &type) {
        auto selection = ImHexApi::HexEditor::getSelection();

        appendEditorText(hex::format("{0} {0}_at_0x{1:02X} @ 0x{1:02X};", type, selection->getStartAddress()));
    }

    void ViewPatternEditor::appendArray(const std::string &type, size_t size) {
        auto selection = ImHexApi::HexEditor::getSelection();

        appendEditorText(hex::format("{0} {0}_array_at_0x{1:02X}[0x{2:02X}] @ 0x{1:02X};", type, selection->getStartAddress(), (selection->getSize() + (size - 1)) / size));
    }

    void ViewPatternEditor::registerMenuItems() {
        /* Import Pattern */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.import", "hex.builtin.menu.file.import.pattern" }, 4050, Shortcut::None,
                                                [this] {
                                                    auto provider = ImHexApi::Provider::get();
                                                    std::vector<std::fs::path> paths;

                                                    for (const auto &imhexPath : fs::getDefaultPaths(fs::ImHexPath::Patterns)) {
                                                        if (!wolv::io::fs::exists(imhexPath)) continue;

                                                        std::error_code error;
                                                        for (auto &entry : std::fs::recursive_directory_iterator(imhexPath, error)) {
                                                            if (entry.is_regular_file() && entry.path().extension() == ".hexpat") {
                                                                paths.push_back(entry.path());
                                                            }
                                                        }
                                                    }

                                                    PopupFileChooser::open(paths, std::vector<nfdfilteritem_t>{ { "Pattern File", "hexpat" } }, false,
                                                                               [this, provider](const std::fs::path &path) {
                                                                                   this->loadPatternFile(path, provider);
                                                                               });
                                                }, ImHexApi::Provider::isValid);

        /* Export Pattern */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.export", "hex.builtin.menu.file.export.pattern" }, 7050, Shortcut::None,
                                                [this] {
                                                    fs::openFileBrowser(fs::DialogMode::Save, { {"Pattern", "hexpat"} },
                                                                        [this](const auto &path) {
                                                                            wolv::io::File file(path, wolv::io::File::Mode::Create);

                                                                            file.writeString(wolv::util::trim(this->m_textEditor.GetText()));
                                                                        });
                                                }, [this] {
                                                    return !wolv::util::trim(this->m_textEditor.GetText()).empty() && ImHexApi::Provider::isValid();
                                                }
        );

        constexpr static std::array<std::pair<const char *, size_t>, 21>  Types = {{
           { "u8", 1 }, { "u16", 2 }, { "u24", 3 }, { "u32", 4 }, { "u48", 6 }, { "u64", 8 }, { "u96", 12 }, { "u128", 16 },
           { "s8", 1 }, { "s16", 2 }, { "s24", 3 }, { "s32", 4 }, { "s48", 6 }, { "s64", 8 }, { "s96", 12 }, { "s128", 16 },
           { "float", 4 }, { "double", 8 },
           { "bool", 1 }, { "char", 1 }, { "char16", 2 }
        }};

        /* Place pattern... */
        ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.edit", "hex.builtin.view.pattern_editor.menu.edit.place_pattern", "hex.builtin.view.pattern_editor.menu.edit.place_pattern.builtin" }, 3000,
                                                       [&, this] {
                                                           if (ImGui::BeginMenu("hex.builtin.view.pattern_editor.menu.edit.place_pattern.builtin.single"_lang)) {
                                                               for (const auto &[type, size] : Types)
                                                                   if (ImGui::MenuItem(type))
                                                                       appendVariable(type);
                                                               ImGui::EndMenu();
                                                           }

                                                           if (ImGui::BeginMenu("hex.builtin.view.pattern_editor.menu.edit.place_pattern.builtin.array"_lang)) {
                                                               for (const auto &[type, size] : Types)
                                                                   if (ImGui::MenuItem(type))
                                                                       appendArray(type, size);
                                                               ImGui::EndMenu();
                                                           }
                                                       }, [this] {
                                                           return ImHexApi::Provider::isValid() && ImHexApi::HexEditor::isSelectionValid() && this->m_runningParsers == 0;
                                                       });

        ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.edit", "hex.builtin.view.pattern_editor.menu.edit.place_pattern", "hex.builtin.view.pattern_editor.menu.edit.place_pattern.custom" }, 3050,
                                                       [&, this] {
                                                           const auto &types = this->m_parserRuntime->getInternals().parser->getTypes();
                                                           auto selection = ImHexApi::HexEditor::getSelection();

                                                           for (const auto &[typeName, type] : types) {
                                                               if (type->isTemplateType())
                                                                   continue;

                                                               createNestedMenu(hex::splitString(typeName, "::"), [&, this] {
                                                                   std::string variableName;
                                                                   for (char &c : hex::replaceStrings(typeName, "::", "_"))
                                                                       variableName += static_cast<char>(std::tolower(c));
                                                                   variableName += hex::format("_at_0x{:02X}", selection->getStartAddress());

                                                                   appendEditorText(hex::format("{0} {1} @ 0x{2:02X};", typeName, variableName, selection->getStartAddress()));
                                                               });
                                                           }
                                                       }, [this] {
                                                           const auto &types = this->m_parserRuntime->getInternals().parser->getTypes();
                                                           bool hasPlaceableTypes = std::any_of(types.begin(), types.end(), [](const auto &type) { return !type.second->isTemplateType(); });

                                                           return ImHexApi::Provider::isValid() && ImHexApi::HexEditor::isSelectionValid() && this->m_runningParsers == 0 && hasPlaceableTypes;
                                                       });
    }

    void ViewPatternEditor::registerHandlers() {
        ContentRegistry::FileHandler::add({ ".hexpat", ".pat" }, [](const std::fs::path &path) -> bool {
            wolv::io::File file(path, wolv::io::File::Mode::Read);

            if (file.isValid()) {
                EventManager::post<RequestSetPatternLanguageCode>(file.readString());
                return true;
            } else {
                return false;
            }
        });

        ImHexApi::HexEditor::addBackgroundHighlightingProvider([this](u64 address, const u8 *data, size_t size, bool) -> std::optional<color_t> {
            hex::unused(data, size);

            if (this->m_runningEvaluators != 0)
                return std::nullopt;

            auto &runtime = ContentRegistry::PatternLanguage::getRuntime();

            std::optional<ImColor> color;

            if (TRY_LOCK(ContentRegistry::PatternLanguage::getRuntimeLock())) {
                for (const auto &pattern : runtime.getPatternsAtAddress(address)) {
                    if (pattern->getVisibility() != pl::ptrn::Visibility::Visible)
                        continue;

                    if (color.has_value())
                        color = ImAlphaBlendColors(*color, pattern->getColor());
                    else
                        color = pattern->getColor();
                }
            }

            return color;
        });

        ImHexApi::HexEditor::addTooltipProvider([this](u64 address, const u8 *data, size_t size) {
            hex::unused(data, size);

            if (TRY_LOCK(ContentRegistry::PatternLanguage::getRuntimeLock())) {
                auto &runtime = ContentRegistry::PatternLanguage::getRuntime();

                auto patterns = runtime.getPatternsAtAddress(address);
                if (!patterns.empty() && !std::all_of(patterns.begin(), patterns.end(), [](const auto &pattern) { return pattern->getVisibility() == pl::ptrn::Visibility::Hidden; })) {
                    ImGui::BeginTooltip();

                    for (const auto &pattern : patterns) {
                        if (pattern->getVisibility() != pl::ptrn::Visibility::Visible)
                            continue;

                        auto tooltipColor = (pattern->getColor() & 0x00FF'FFFF) | 0x7000'0000;
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
            .load = [this](prv::Provider *provider, const std::fs::path &basePath, Tar &tar) {
                std::string sourceCode = tar.readString(basePath);

                if (!this->m_syncPatternSourceCode)
                    this->m_sourceCode.get(provider) = sourceCode;

                if (provider == ImHexApi::Provider::get())
                    this->m_textEditor.SetText(sourceCode);

                return true;
            },
            .store = [this](prv::Provider *provider, const std::fs::path &basePath, Tar &tar) {
                std::string sourceCode;

                if (provider == ImHexApi::Provider::get())
                    this->m_sourceCode.get(provider) = this->m_textEditor.GetText();

                if (this->m_syncPatternSourceCode)
                    sourceCode = this->m_textEditor.GetText();
                else
                    sourceCode = this->m_sourceCode.get(provider);

                tar.writeString(basePath, wolv::util::trim(sourceCode));
                return true;
            }
        });

        ShortcutManager::addShortcut(this, Keys::F8 + AllowWhileTyping, [this] {
            auto line = this->m_textEditor.GetCursorPosition().mLine + 1;
            auto &runtime = ContentRegistry::PatternLanguage::getRuntime();

            auto &evaluator = runtime.getInternals().evaluator;
            auto &breakpoints = evaluator->getBreakpoints();

            if (breakpoints.contains(line)) {
                evaluator->removeBreakpoint(line);
            } else {
                evaluator->addBreakpoint(line);
            }

            this->m_textEditor.SetBreakpoints(breakpoints);
        });
    }

}