#include "content/views/view_pattern_editor.hpp"

#include <hex/helpers/project_file_handler.hpp>
#include <hex/pattern_language/preprocessor.hpp>
#include <hex/pattern_language/pattern_data.hpp>
#include <hex/pattern_language/ast_node.hpp>
#include <hex/helpers/paths.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/file.hpp>

#include <hex/helpers/magic.hpp>
#include <hex/helpers/literals.hpp>

#include <nlohmann/json.hpp>

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
                id.mDeclaration = "Built-in type";
                langDef.mIdentifiers.insert(std::make_pair(std::string(name), id));
            }

            langDef.mTokenize = [](const char *inBegin, const char *inEnd, const char *&outBegin, const char *&outEnd, TextEditor::PaletteIndex &paletteIndex) -> bool {
                paletteIndex = TextEditor::PaletteIndex::Max;

                while (inBegin < inEnd && isascii(*inBegin) && isblank(*inBegin))
                    inBegin++;

                if (inBegin == inEnd) {
                    outBegin = inEnd;
                    outEnd = inEnd;
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

            langDef.mCommentStart = "/*";
            langDef.mCommentEnd = "*/";
            langDef.mSingleLineComment = "//";

            langDef.mCaseSensitive = true;
            langDef.mAutoIndentation = true;
            langDef.mPreprocChar = '#';

            langDef.mName = "Pattern Language";

            initialized = true;
        }
        return langDef;
    }


    ViewPatternEditor::ViewPatternEditor() : View("hex.builtin.view.pattern_editor.name") {
        this->m_evaluatorRuntime = new pl::PatternLanguage();
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

        EventManager::subscribe<RequestSetPatternLanguageCode>(this, [this](std::string code) {
            this->m_textEditor.SelectAll();
            this->m_textEditor.Delete();
            this->m_textEditor.InsertText(code);
        });

        EventManager::subscribe<EventFileLoaded>(this, [this](const fs::path &path) {
            if (!ContentRegistry::Settings::read("hex.builtin.setting.general", "hex.builtin.setting.general.auto_load_patterns", 1))
                return;

            pl::Preprocessor preprocessor;

            if (!ImHexApi::Provider::isValid())
                return;

            std::string mimeType = magic::getMIMEType(ImHexApi::Provider::get());

            bool foundCorrectType = false;
            preprocessor.addPragmaHandler("MIME", [&mimeType, &foundCorrectType](std::string value) {
                if (value == mimeType) {
                    foundCorrectType = true;
                    return true;
                }
                return !std::all_of(value.begin(), value.end(), isspace) && !value.ends_with('\n') && !value.ends_with('\r');
            });
            preprocessor.addDefaultPragmaHandlers();

            this->m_possiblePatternFiles.clear();

            std::error_code errorCode;
            for (const auto &dir : hex::getPath(ImHexPath::Patterns)) {
                for (auto &entry : fs::directory_iterator(dir, errorCode)) {
                    foundCorrectType = false;
                    if (!entry.is_regular_file())
                        continue;

                    File file(entry.path().string(), File::Mode::Read);
                    if (!file.isValid())
                        continue;

                    preprocessor.preprocess(file.readString());

                    if (foundCorrectType)
                        this->m_possiblePatternFiles.push_back(entry.path());
                }
            }


            if (!this->m_possiblePatternFiles.empty()) {
                this->m_selectedPatternFile = 0;
                EventManager::post<RequestOpenPopup>("hex.builtin.view.pattern_editor.accept_pattern"_lang);
                this->m_acceptPatternWindowOpen = true;
            }
        });

        EventManager::subscribe<EventFileUnloaded>(this, [this] {
            this->m_textEditor.SetText("");
            this->m_evaluatorRuntime->abort();
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

        ContentRegistry::FileHandler::add({ ".hexpat", ".pat" }, [](const fs::path &path) -> bool {
            File file(path.string(), File::Mode::Read);

            if (file.isValid()) {
                EventManager::post<RequestSetPatternLanguageCode>(file.readString());
                return true;
            } else {
                return false;
            }
        });

        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.file", 2000, [&, this] {
            if (ImGui::MenuItem("hex.builtin.view.pattern_editor.menu.file.load_pattern"_lang)) {

                std::vector<fs::path> paths;

                for (auto &imhexPath : hex::getPath(ImHexPath::Patterns)) {
                    if (!fs::exists(imhexPath)) continue;

                    for (auto &entry : fs::recursive_directory_iterator(imhexPath)) {
                        if (entry.is_regular_file() && entry.path().extension() == ".hexpat") {
                            paths.push_back(entry.path());
                        }
                    }
                }

                View::showFileChooserPopup(paths, {
                                                      {"Pattern File", "hexpat"}
                },
                                           [this](const fs::path &path) {
                                               this->loadPatternFile(path);
                                           });
            }

            if (ImGui::MenuItem("hex.builtin.view.pattern_editor.menu.file.save_pattern"_lang)) {
                hex::openFileBrowser("hex.builtin.view.pattern_editor.menu.file.save_pattern"_lang, DialogMode::Save, {
                                                                                                                          {"Pattern", "hexpat"}
                },
                                     [this](const auto &path) {
                                         File file(path, File::Mode::Create);

                                         file.write(this->m_textEditor.GetText());
                                     });
            }
        });
    }

    ViewPatternEditor::~ViewPatternEditor() {
        delete this->m_evaluatorRuntime;
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

                auto size = ImGui::GetContentRegionAvail();
                size.y -= ImGui::GetTextLineHeightWithSpacing() * 2.5;

                if (ImGui::BeginTabBar("##settings")) {
                    if (ImGui::BeginTabItem("hex.builtin.view.pattern_editor.console"_lang)) {
                        this->drawConsole(size);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("hex.builtin.view.pattern_editor.env_vars"_lang)) {
                        this->drawEnvVars(size);
                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("hex.builtin.view.pattern_editor.settings"_lang)) {
                        this->drawVariableSettings(size);
                        ImGui::EndTabItem();
                    }

                    ImGui::EndTabBar();
                }

                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);

                if (this->m_evaluatorRunning) {
                    if (ImGui::IconButton(ICON_VS_DEBUG_STOP, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed)))
                        this->m_evaluatorRuntime->abort();
                } else {
                    if (ImGui::IconButton(ICON_VS_DEBUG_START, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarGreen)))
                        this->evaluatePattern(this->m_textEditor.GetText());
                }


                ImGui::PopStyleVar();

                ImGui::SameLine();
                if (this->m_evaluatorRunning)
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
                                         this->m_evaluatorRuntime->getCreatedPatternCount(),
                                         this->m_evaluatorRuntime->getMaximumPatternCount());
                }

                if (this->m_textEditor.IsTextChanged()) {
                    ProjectFile::markDirty();
                    this->m_hasUnevaluatedChanges = true;
                }

                if (this->m_hasUnevaluatedChanges && !this->m_evaluatorRunning && !this->m_parserRunning) {
                    this->m_hasUnevaluatedChanges = false;

                    if (this->m_runAutomatically)
                        this->evaluatePattern(this->m_textEditor.GetText());
                    else
                        this->parsePattern(this->m_textEditor.GetText());
                }
            }

            if (this->m_evaluatorRuntime->hasDangerousFunctionBeenCalled() && !ImGui::IsPopupOpen(View::toWindowName("hex.builtin.view.pattern_editor.dangerous_function.name").c_str())) {
                ImGui::OpenPopup(View::toWindowName("hex.builtin.view.pattern_editor.dangerous_function.name").c_str());
            }

            if (ImGui::BeginPopupModal(View::toWindowName("hex.builtin.view.pattern_editor.dangerous_function.name").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::NewLine();
                ImGui::TextUnformatted("hex.builtin.view.pattern_editor.dangerous_function.desc"_lang);
                ImGui::NewLine();

                View::confirmButtons(
                    "hex.common.yes"_lang, "hex.common.no"_lang, [this] {
                    this->m_evaluatorRuntime->allowDangerousFunctions(true);
                    ImGui::CloseCurrentPopup(); }, [this] {
                    this->m_evaluatorRuntime->allowDangerousFunctions(false);
                    ImGui::CloseCurrentPopup(); });

                ImGui::EndPopup();
            }

            View::discardNavigationRequests();
        }
        ImGui::End();
    }

    void ViewPatternEditor::drawConsole(ImVec2 size) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, this->m_textEditor.GetPalette()[u32(TextEditor::PaletteIndex::Background)]);
        if (ImGui::BeginChild("##console", size, true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_HorizontalScrollbar)) {
            ImGuiListClipper clipper(this->m_console.size());
            while (clipper.Step())
                for (u64 i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
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
                    ImGui::InputText("###name", name.data(), name.capacity(), ImGuiInputTextFlags_CallbackResize, ImGui::UpdateStringSizeCallback, &name);
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
                            ImGui::InputText("###value", displayValue.data(), displayValue.capacity(), ImGuiInputTextFlags_CallbackResize, ImGui::UpdateStringSizeCallback, &displayValue);
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
                entries[i] = fs::path(this->m_possiblePatternFiles[i]).filename().string();
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
                "hex.common.yes"_lang, "hex.common.no"_lang, [this] {
                this->loadPatternFile(this->m_possiblePatternFiles[this->m_selectedPatternFile].string());
                ImGui::CloseCurrentPopup(); }, [] { ImGui::CloseCurrentPopup(); });

            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
    }


    void ViewPatternEditor::loadPatternFile(const fs::path &path) {
        File file(path, File::Mode::Read);
        if (file.isValid()) {
            auto code = file.readString();

            this->evaluatePattern(code);
            this->m_textEditor.SetText(code);
        }
    }

    void ViewPatternEditor::clearPatternData() {
        for (auto &data : SharedData::patternData)
            delete data;

        SharedData::patternData.clear();
        pl::PatternData::resetPalette();
    }


    void ViewPatternEditor::parsePattern(const std::string &code) {
        this->m_parserRunning = true;
        std::thread([this, code] {
            auto ast = this->m_parserRuntime->parseString(code);

            this->m_patternVariables.clear();
            this->m_patternTypes.clear();

            if (ast) {
                for (auto node : *ast) {
                    if (auto variableDecl = dynamic_cast<pl::ASTNodeVariableDecl *>(node)) {
                        auto type = dynamic_cast<pl::ASTNodeTypeDecl *>(variableDecl->getType());
                        if (type == nullptr) continue;

                        auto builtinType = dynamic_cast<pl::ASTNodeBuiltinType *>(type->getType());
                        if (builtinType == nullptr) continue;

                        PatternVariable variable = {
                            .inVariable = variableDecl->isInVariable(),
                            .outVariable = variableDecl->isOutVariable(),
                            .type = builtinType->getType()
                        };

                        if (variable.inVariable || variable.outVariable) {
                            if (!this->m_patternVariables.contains(variableDecl->getName()))
                                this->m_patternVariables[variableDecl->getName()] = variable;
                        }
                    }
                }
            }

            this->m_parserRunning = false;
        }).detach();
    }

    void ViewPatternEditor::evaluatePattern(const std::string &code) {
        this->m_evaluatorRunning = true;

        this->m_textEditor.SetErrorMarkers({});
        this->m_console.clear();
        this->clearPatternData();

        {
            std::vector<pl::PatternData *> patterns;
            EventManager::post<EventPatternChanged>(patterns);
        }

        std::thread([this, code] {
            std::map<std::string, pl::Token::Literal> envVars;
            for (const auto &[id, name, value, type] : this->m_envVarEntries)
                envVars.insert({ name, value });

            std::map<std::string, pl::Token::Literal> inVariables;
            for (auto &[name, variable] : this->m_patternVariables) {
                if (variable.inVariable)
                    inVariables[name] = variable.value;
            }

            auto result = this->m_evaluatorRuntime->executeString(ImHexApi::Provider::get(), code, envVars, inVariables);

            auto error = this->m_evaluatorRuntime->getError();
            if (error.has_value()) {
                this->m_textEditor.SetErrorMarkers({ error.value() });
            }

            this->m_console = this->m_evaluatorRuntime->getConsoleLog();

            auto outVariables = this->m_evaluatorRuntime->getOutVariables();
            for (auto &[name, variable] : this->m_patternVariables) {
                if (variable.outVariable && outVariables.contains(name))
                    variable.value = outVariables.at(name);
            }

            if (result.has_value()) {
                SharedData::patternData = std::move(result.value());
                EventManager::post<EventPatternChanged>(SharedData::patternData);
            }

            this->m_evaluatorRunning = false;
        }).detach();
    }

}
