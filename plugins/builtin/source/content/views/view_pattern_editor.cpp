#include "content/views/view_pattern_editor.hpp"

#include <pl/preprocessor.hpp>
#include <pl/patterns/pattern.hpp>
#include <pl/ast/ast_node_variable_decl.hpp>
#include <pl/ast/ast_node_type_decl.hpp>
#include <pl/ast/ast_node_builtin_type.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/file.hpp>
#include <hex/helpers/project_file_handler.hpp>
#include <hex/helpers/magic.hpp>

namespace hex::plugin::builtin {

    using namespace hex::literals;

    static const TextEditor::LanguageDefinition &PatternLanguage() {
        static bool initialized = false;
        static TextEditor::LanguageDefinition langDef;
        if (!initialized) {
            static const char *const keywords[] = {
                "using", "struct", "union", "enum", "bitfield", "be", "le", "if", "else", "false", "true", "this", "parent", "addressof", "sizeof", "$", "while", "for", "fn", "return", "break", "continue", "namespace", "in", "out"
            };
            for (auto &k : keywords)
                langDef.mKeywords.insert(k);

            static const char *const builtInTypes[] = {
                "u8", "u16", "u32", "u64", "u128", "s8", "s16", "s32", "s64", "s128", "float", "double", "char", "char16", "bool", "padding", "str", "auto"
            };

            for (const auto name : builtInTypes) {
                TextEditor::Identifier id;
                id.mDeclaration = "";
                langDef.mIdentifiers.insert(std::make_pair(std::string(name), id));
            }

            langDef.mTokenize = [](const char *inBegin, const char *inEnd, const char *&outBegin, const char *&outEnd, TextEditor::PaletteIndex &paletteIndex) -> bool {
                paletteIndex = TextEditor::PaletteIndex::Max;

                while (inBegin < inEnd && isascii(*inBegin) && isblank(*inBegin))
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
        this->m_parserRuntime = new pl::PatternLanguage();

        this->m_textEditor.SetLanguageDefinition(PatternLanguage());
        this->m_textEditor.SetShowWhitespaces(false);

        this->m_envVarEntries.push_back({ 0, "", 0, EnvVarType::Integer });
        this->m_envVarIdCounter = 1;

        EventManager::subscribe<EventProjectFileStore>(this, [this]() {
            ProjectFile::setPattern(this->m_textEditor.GetText());
        });

        EventManager::subscribe<EventProjectFileLoad>(this, [this]() {
            this->m_textEditor.SetText(ProjectFile::getPattern());
            this->evaluatePattern(this->m_textEditor.GetText());
        });

        EventManager::subscribe<RequestSetPatternLanguageCode>(this, [this](const std::string &code) {
            this->m_textEditor.SelectAll();
            this->m_textEditor.Delete();
            this->m_textEditor.InsertText(code);
        });

        EventManager::subscribe<EventFileLoaded>(this, [this](const std::fs::path &path) {
            hex::unused(path);

            if (!ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.auto_load_patterns", 1))
                return;

            if (!ImHexApi::Provider::isValid())
                return;

            auto provider = ImHexApi::Provider::get();
            auto &runtime = provider->getPatternLanguageRuntime();

            auto mimeType = magic::getMIMEType(ImHexApi::Provider::get());

            bool foundCorrectType = false;
            runtime.addPragma("MIME", [&mimeType, &foundCorrectType](pl::PatternLanguage &runtime, const std::string &value) {
                hex::unused(runtime);

                if (value == mimeType) {
                    foundCorrectType = true;
                    return true;
                }
                return !std::all_of(value.begin(), value.end(), isspace) && !value.ends_with('\n') && !value.ends_with('\r');
            });

            this->m_possiblePatternFiles.clear();

            std::error_code errorCode;
            for (const auto &dir : fs::getDefaultPaths(fs::ImHexPath::Patterns)) {
                for (auto &entry : std::fs::directory_iterator(dir, errorCode)) {
                    foundCorrectType = false;
                    if (!entry.is_regular_file())
                        continue;

                    fs::File file(entry.path().string(), fs::File::Mode::Read);
                    if (!file.isValid())
                        continue;

                    runtime.getInternals().preprocessor->preprocess(runtime, file.readString());

                    if (foundCorrectType)
                        this->m_possiblePatternFiles.push_back(entry.path());
                }
            }

            runtime.addPragma("MIME", [](pl::PatternLanguage&, const std::string &value) { return !value.empty(); });

            if (!this->m_possiblePatternFiles.empty()) {
                this->m_selectedPatternFile = 0;
                EventManager::post<RequestOpenPopup>("hex.builtin.view.pattern_editor.accept_pattern"_lang);
                this->m_acceptPatternWindowOpen = true;
            }
        });

        EventManager::subscribe<EventFileUnloaded>(this, [] {
            ImHexApi::Provider::get()->getPatternLanguageRuntime().abort();
        });

        EventManager::subscribe<EventProviderChanged>(this, [this](prv::Provider *oldProvider, prv::Provider *newProvider) {
            if (oldProvider != nullptr) oldProvider->getPatternLanguageSourceCode() = this->m_textEditor.GetText();
            if (newProvider != nullptr) this->m_textEditor.SetText(newProvider->getPatternLanguageSourceCode());

            auto lines = this->m_textEditor.GetTextLines();
            lines.pop_back();
            this->m_textEditor.SetTextLines(lines);
        });

        /* Settings */
        {

            EventManager::subscribe<RequestChangeTheme>(this, [this](u32 theme) {
                switch (theme) {
                    default:
                    case 1: /* Dark theme */
                        this->m_textEditor.SetPalette(TextEditor::GetDarkPalette());
                        break;
                    case 2: /* Light theme */
                        this->m_textEditor.SetPalette(TextEditor::GetLightPalette());
                        break;
                    case 3: /* Classic theme */
                        this->m_textEditor.SetPalette(TextEditor::GetRetroBluePalette());
                        break;
                }
            });
        }

        ContentRegistry::FileHandler::add({ ".hexpat", ".pat" }, [](const std::fs::path &path) -> bool {
            fs::File file(path.string(), fs::File::Mode::Read);

            if (file.isValid()) {
                EventManager::post<RequestSetPatternLanguageCode>(file.readString());
                return true;
            } else {
                return false;
            }
        });

        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.file", 2000, [&, this] {
            bool providerValid = ImHexApi::Provider::isValid();

            if (ImGui::MenuItem("hex.builtin.view.pattern_editor.menu.file.load_pattern"_lang, nullptr, false, providerValid)) {

                std::vector<std::fs::path> paths;

                for (const auto &imhexPath : fs::getDefaultPaths(fs::ImHexPath::Patterns)) {
                    if (!fs::exists(imhexPath)) continue;

                    std::error_code error;
                    for (auto &entry : std::fs::recursive_directory_iterator(imhexPath, error)) {
                        if (entry.is_regular_file() && entry.path().extension() == ".hexpat") {
                            paths.push_back(entry.path());
                        }
                    }
                }

                View::showFileChooserPopup(paths, { {"Pattern File", "hexpat"} },
                    [this](const std::fs::path &path) {
                        this->loadPatternFile(path);
                    });
            }

            if (ImGui::MenuItem("hex.builtin.view.pattern_editor.menu.file.save_pattern"_lang, nullptr, false, providerValid)) {
                fs::openFileBrowser(fs::DialogMode::Save, { {"Pattern", "hexpat"} },
                    [this](const auto &path) {
                        fs::File file(path, fs::File::Mode::Create);

                        file.write(this->m_textEditor.GetText());
                    });
            }
        });


        ImHexApi::HexEditor::addBackgroundHighlightingProvider([](u64 address, const u8* data, size_t size) -> std::optional<color_t> {
            hex::unused(data, size);

            const auto &patterns = ImHexApi::Provider::get()->getPatternLanguageRuntime().getPatterns();
            for (const auto &pattern : patterns) {
                auto child = pattern->getPattern(address);
                if (child != nullptr) {
                    return child->getColor();
                }
            }

            return std::nullopt;
        });

        ImHexApi::HexEditor::addTooltipProvider([](u64 address, const u8 *data, size_t size) {
            hex::unused(data, size);

            const auto &patterns = ImHexApi::Provider::get()->getPatternLanguageRuntime().getPatterns();
            for (const auto &pattern : patterns) {
                auto child = pattern->getPattern(address);
                if (child != nullptr) {
                    ImGui::BeginTooltip();
                    ImGui::ColorButton(pattern->getVariableName().c_str(), ImColor(pattern->getColor()));
                    ImGui::SameLine(0, 10);
                    ImGui::TextUnformatted(pattern->getVariableName().c_str());
                    ImGui::EndTooltip();
                }
            }
        });
    }

    ViewPatternEditor::~ViewPatternEditor() {
        delete this->m_parserRuntime;

        EventManager::unsubscribe<EventProjectFileStore>(this);
        EventManager::unsubscribe<EventProjectFileLoad>(this);
        EventManager::unsubscribe<RequestSetPatternLanguageCode>(this);
        EventManager::unsubscribe<EventFileLoaded>(this);
        EventManager::unsubscribe<RequestChangeTheme>(this);
        EventManager::unsubscribe<EventFileUnloaded>(this);
    }

    void ViewPatternEditor::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.pattern_editor.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_None | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            auto provider = ImHexApi::Provider::get();

            if (ImHexApi::Provider::isValid() && provider->isAvailable()) {
                auto textEditorSize = ImGui::GetContentRegionAvail();
                textEditorSize.y *= 3.75 / 5.0;
                textEditorSize.y -= ImGui::GetTextLineHeightWithSpacing();
                this->m_textEditor.Render("hex.builtin.view.pattern_editor.name"_lang, textEditorSize, true);

                auto settingsSize = ImGui::GetContentRegionAvail();
                settingsSize.y -= ImGui::GetTextLineHeightWithSpacing() * 2.5;

                if (ImGui::BeginTabBar("##settings")) {
                    if (ImGui::BeginTabItem("hex.builtin.view.pattern_editor.console"_lang)) {
                        this->drawConsole(settingsSize);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("hex.builtin.view.pattern_editor.env_vars"_lang)) {
                        this->drawEnvVars(settingsSize);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("hex.builtin.view.pattern_editor.settings"_lang)) {
                        this->drawVariableSettings(settingsSize);
                        ImGui::EndTabItem();
                    }

                    ImGui::EndTabBar();
                }

                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);

                auto &runtime = provider->getPatternLanguageRuntime();
                if (runtime.isRunning()) {
                    if (ImGui::IconButton(ICON_VS_DEBUG_STOP, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed)))
                        runtime.abort();
                } else {
                    if (ImGui::IconButton(ICON_VS_DEBUG_START, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarGreen)))
                        this->evaluatePattern(this->m_textEditor.GetText());
                }


                ImGui::PopStyleVar();

                ImGui::SameLine();
                if (this->m_runningEvaluators > 0)
                    ImGui::TextSpinner("hex.builtin.view.pattern_editor.evaluating"_lang);
                else {
                    if (ImGui::Checkbox("hex.builtin.view.pattern_editor.auto"_lang, &this->m_runAutomatically)) {
                        if (this->m_runAutomatically)
                            this->m_hasUnevaluatedChanges = true;
                    }

                    ImGui::SameLine();
                    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                    ImGui::SameLine();

                    ImGui::TextFormatted("{} / {}",
                        provider->getPatternLanguageRuntime().getCreatedPatternCount(),
                        provider->getPatternLanguageRuntime().getMaximumPatternCount());
                }

                if (this->m_textEditor.IsTextChanged()) {
                    ProjectFile::markDirty();
                    this->m_hasUnevaluatedChanges = true;
                }

                if (this->m_hasUnevaluatedChanges && this->m_runningEvaluators == 0 && this->m_runningParsers == 0) {
                    this->m_hasUnevaluatedChanges = false;

                    if (this->m_runAutomatically)
                        this->evaluatePattern(this->m_textEditor.GetText());
                    else
                        this->parsePattern(this->m_textEditor.GetText());
                }
            }

            if (this->m_dangerousFunctionCalled && !ImGui::IsPopupOpen(View::toWindowName("hex.builtin.view.pattern_editor.dangerous_function.name").c_str())) {
                ImGui::OpenPopup(View::toWindowName("hex.builtin.view.pattern_editor.dangerous_function.name").c_str());
            }

            if (ImGui::BeginPopupModal(View::toWindowName("hex.builtin.view.pattern_editor.dangerous_function.name").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::NewLine();
                ImGui::TextUnformatted("hex.builtin.view.pattern_editor.dangerous_function.desc"_lang);
                ImGui::NewLine();

                View::confirmButtons(
                    "hex.builtin.common.yes"_lang, "hex.builtin.common.no"_lang,
                    [this] {
                        this->m_dangerousFunctionsAllowed = DangerousFunctionPerms::Allow;
                        ImGui::CloseCurrentPopup();
                    }, [this] {
                        this->m_dangerousFunctionsAllowed = DangerousFunctionPerms::Deny;
                        ImGui::CloseCurrentPopup();
                    });

                ImGui::EndPopup();
            }

            View::discardNavigationRequests();
        }
        ImGui::End();

        if (!this->m_lastEvaluationProcessed) {
            this->m_console = this->m_lastEvaluationLog;

            if (!this->m_lastEvaluationResult) {
                if (this->m_lastEvaluationError) {
                    TextEditor::ErrorMarkers errorMarkers = {
                        {this->m_lastEvaluationError->getLineNumber(), this->m_lastEvaluationError->what()}
                    };
                    this->m_textEditor.SetErrorMarkers(errorMarkers);
                }
            } else {
                for (auto &[name, variable] : this->m_patternVariables) {
                    if (variable.outVariable && this->m_lastEvaluationOutVars.contains(name))
                        variable.value = this->m_lastEvaluationOutVars.at(name);
                }

                EventManager::post<EventHighlightingChanged>();
            }

            this->m_lastEvaluationProcessed = true;
        }
    }

    void ViewPatternEditor::drawConsole(ImVec2 size) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, this->m_textEditor.GetPalette()[u32(TextEditor::PaletteIndex::Background)]);
        if (ImGui::BeginChild("##console", size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_HorizontalScrollbar)) {
            ImGuiListClipper clipper(this->m_console.size());
            while (clipper.Step())
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                    const auto &[level, message] = this->m_console[i];

                    switch (level) {
                        case pl::LogConsole::Level::Debug:
                            ImGui::PushStyleColor(ImGuiCol_Text, this->m_textEditor.GetPalette()[u32(TextEditor::PaletteIndex::Comment)]);
                            break;
                        case pl::LogConsole::Level::Info:
                            ImGui::PushStyleColor(ImGuiCol_Text, this->m_textEditor.GetPalette()[u32(TextEditor::PaletteIndex::Default)]);
                            break;
                        case pl::LogConsole::Level::Warning:
                            ImGui::PushStyleColor(ImGuiCol_Text, this->m_textEditor.GetPalette()[u32(TextEditor::PaletteIndex::Preprocessor)]);
                            break;
                        case pl::LogConsole::Level::Error:
                            ImGui::PushStyleColor(ImGuiCol_Text, this->m_textEditor.GetPalette()[u32(TextEditor::PaletteIndex::ErrorMarker)]);
                            break;
                        default:
                            continue;
                    }

                    if (ImGui::Selectable(message.c_str()))
                        ImGui::SetClipboardText(message.c_str());

                    ImGui::PopStyleColor();
                }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(1);
    }

    void ViewPatternEditor::drawEnvVars(ImVec2 size) {
        if (ImGui::BeginChild("##env_vars", size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
            int index = 0;
            if (ImGui::BeginTable("##env_vars_table", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerH)) {
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.1F);
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.4F);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.38F);
                ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthStretch, 0.12F);

                for (auto iter = this->m_envVarEntries.begin(); iter != this->m_envVarEntries.end(); iter++) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    auto &[id, name, value, type] = *iter;

                    ImGui::PushID(index++);
                    ON_SCOPE_EXIT { ImGui::PopID(); };

                    ImGui::PushItemWidth(ImGui::GetContentRegionAvailWidth());
                    constexpr const char *Types[] = { "I", "F", "S", "B" };
                    if (ImGui::BeginCombo("", Types[static_cast<int>(type)])) {
                        for (auto i = 0; i < IM_ARRAYSIZE(Types); i++) {
                            if (ImGui::Selectable(Types[i]))
                                type = static_cast<EnvVarType>(i);
                        }

                        ImGui::EndCombo();
                    }
                    ImGui::PopItemWidth();

                    ImGui::TableNextColumn();

                    ImGui::PushItemWidth(ImGui::GetContentRegionAvailWidth());
                    ImGui::InputText("###name", name);
                    ImGui::PopItemWidth();

                    ImGui::TableNextColumn();

                    ImGui::PushItemWidth(ImGui::GetContentRegionAvailWidth());
                    switch (type) {
                        case EnvVarType::Integer:
                            {
                                i64 displayValue = hex::get_or<i128>(value, 0);
                                ImGui::InputScalar("###value", ImGuiDataType_S64, &displayValue);
                                value = i128(displayValue);
                                break;
                            }
                        case EnvVarType::Float:
                            {
                                auto displayValue = hex::get_or<double>(value, 0.0);
                                ImGui::InputDouble("###value", &displayValue);
                                value = displayValue;
                                break;
                            }
                        case EnvVarType::Bool:
                            {
                                auto displayValue = hex::get_or<bool>(value, false);
                                ImGui::Checkbox("###value", &displayValue);
                                value = displayValue;
                                break;
                            }
                        case EnvVarType::String:
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
                        iter++;
                        this->m_envVarEntries.insert(iter, { this->m_envVarIdCounter++, "", i128(0), EnvVarType::Integer });
                        iter--;
                    }

                    ImGui::SameLine();

                    ImGui::BeginDisabled(this->m_envVarEntries.size() <= 1);
                    {
                        if (ImGui::IconButton(ICON_VS_REMOVE, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                            bool isFirst = iter == this->m_envVarEntries.begin();
                            this->m_envVarEntries.erase(iter);

                            if (isFirst)
                                iter = this->m_envVarEntries.begin();
                        }
                    }
                    ImGui::EndDisabled();
                }

                ImGui::EndTable();
            }
        }
        ImGui::EndChild();
    }

    void ViewPatternEditor::drawVariableSettings(ImVec2 size) {
        if (ImGui::BeginChild("##settings", size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
            if (this->m_patternVariables.empty()) {
                ImGui::TextFormattedCentered("hex.builtin.view.pattern_editor.no_in_out_vars"_lang);
            } else {
                if (ImGui::BeginTable("##in_out_vars_table", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerH)) {
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.4F);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.6F);

                    for (auto &[name, variable] : this->m_patternVariables) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        ImGui::TextUnformatted(name.c_str());

                        ImGui::TableNextColumn();

                        if (variable.outVariable) {
                            ImGui::TextUnformatted(pl::Token::literalToString(variable.value, true).c_str());
                        } else if (variable.inVariable) {
                            if (pl::Token::isSigned(variable.type)) {
                                i64 value = hex::get_or<i128>(variable.value, 0);
                                ImGui::InputScalar("", ImGuiDataType_S64, &value);
                                variable.value = i128(value);
                            } else if (pl::Token::isUnsigned(variable.type)) {
                                u64 value = hex::get_or<u128>(variable.value, 0);
                                ImGui::InputScalar("", ImGuiDataType_U64, &value);
                                variable.value = u128(value);
                            } else if (pl::Token::isFloatingPoint(variable.type)) {
                                double value = hex::get_or<double>(variable.value, 0.0);
                                ImGui::InputScalar("", ImGuiDataType_Double, &value);
                                variable.value = value;
                            } else if (variable.type == pl::Token::ValueType::Boolean) {
                                bool value = hex::get_or<bool>(variable.value, false);
                                ImGui::Checkbox("", &value);
                                variable.value = value;
                            } else if (variable.type == pl::Token::ValueType::Character) {
                                char buffer[2];
                                ImGui::InputText("", buffer, 2);
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

    void ViewPatternEditor::drawAlwaysVisible() {

        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
        if (ImGui::BeginPopupModal("hex.builtin.view.pattern_editor.accept_pattern"_lang, &this->m_acceptPatternWindowOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextFormattedWrapped("{}", static_cast<const char *>("hex.builtin.view.pattern_editor.accept_pattern.desc"_lang));

            std::vector<std::string> entries;
            entries.resize(this->m_possiblePatternFiles.size());

            for (u32 i = 0; i < entries.size(); i++) {
                entries[i] = std::fs::path(this->m_possiblePatternFiles[i]).filename().string();
            }

            if (ImGui::BeginListBox("##patterns_accept", ImVec2(-FLT_MIN, 0))) {

                u32 index = 0;
                for (auto &path : this->m_possiblePatternFiles) {
                    if (ImGui::Selectable(path.filename().string().c_str(), index == this->m_selectedPatternFile))
                        this->m_selectedPatternFile = index;
                    index++;
                }

                ImGui::EndListBox();
            }

            ImGui::NewLine();
            ImGui::TextUnformatted("hex.builtin.view.pattern_editor.accept_pattern.question"_lang);

            confirmButtons(
                "hex.builtin.common.yes"_lang, "hex.builtin.common.no"_lang, [this] {
                this->loadPatternFile(this->m_possiblePatternFiles[this->m_selectedPatternFile].string());
                ImGui::CloseCurrentPopup(); }, [] { ImGui::CloseCurrentPopup(); });

            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
    }


    void ViewPatternEditor::loadPatternFile(const std::fs::path &path) {
        fs::File file(path, fs::File::Mode::Read);
        if (file.isValid()) {
            auto code = file.readString();

            this->evaluatePattern(code);
            this->m_textEditor.SetText(code);
        }
    }

    void ViewPatternEditor::clearPatterns() {
        if (!ImHexApi::Provider::isValid()) return;

        ImHexApi::Provider::get()->getPatternLanguageRuntime().reset();
    }


    void ViewPatternEditor::parsePattern(const std::string &code) {
        this->m_runningParsers++;
        std::thread([this, code] {
            auto ast = this->m_parserRuntime->parseString(code);

            this->m_patternVariables.clear();
            this->m_patternTypes.clear();

            if (ast) {
                for (auto &node : *ast) {
                    if (auto variableDecl = dynamic_cast<pl::ASTNodeVariableDecl *>(node.get())) {
                        auto type = dynamic_cast<pl::ASTNodeTypeDecl *>(variableDecl->getType().get());
                        if (type == nullptr) continue;

                        auto builtinType = dynamic_cast<pl::ASTNodeBuiltinType *>(type->getType().get());
                        if (builtinType == nullptr) continue;

                        PatternVariable variable = {
                            .inVariable  = variableDecl->isInVariable(),
                            .outVariable = variableDecl->isOutVariable(),
                            .type        = builtinType->getType(),
                            .value       = { }
                        };

                        if (variable.inVariable || variable.outVariable) {
                            if (!this->m_patternVariables.contains(variableDecl->getName()))
                                this->m_patternVariables[variableDecl->getName()] = variable;
                        }
                    }
                }
            }

            this->m_runningParsers--;
        }).detach();
    }

    void ViewPatternEditor::evaluatePattern(const std::string &code) {
        this->m_runningEvaluators++;

        this->m_textEditor.SetErrorMarkers({});
        this->m_console.clear();
        this->clearPatterns();

        EventManager::post<EventHighlightingChanged>();

        std::thread([this, code] {
            std::map<std::string, pl::Token::Literal> envVars;
            for (const auto &[id, name, value, type] : this->m_envVarEntries)
                envVars.insert({ name, value });

            std::map<std::string, pl::Token::Literal> inVariables;
            for (auto &[name, variable] : this->m_patternVariables) {
                if (variable.inVariable)
                    inVariables[name] = variable.value;
            }

            auto provider = ImHexApi::Provider::get();
            auto &runtime = provider->getPatternLanguageRuntime();

            runtime.setDangerousFunctionCallHandler([this]{
                this->m_dangerousFunctionCalled = true;

                while (this->m_dangerousFunctionsAllowed == DangerousFunctionPerms::Ask) {
                    std::this_thread::yield();
                }

                return this->m_dangerousFunctionsAllowed == DangerousFunctionPerms::Allow;
            });

            this->m_lastEvaluationResult = runtime.executeString(code, envVars, inVariables);
            if (!this->m_lastEvaluationResult) {
                this->m_lastEvaluationError = runtime.getError();
            }

            this->m_lastEvaluationLog     = runtime.getConsoleLog();
            this->m_lastEvaluationOutVars = runtime.getOutVariables();
            this->m_runningEvaluators--;

            this->m_lastEvaluationProcessed = false;
        }).detach();
    }

}
