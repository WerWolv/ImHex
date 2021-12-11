#include "content/views/view_pattern_editor.hpp"

#include <hex/helpers/project_file_handler.hpp>
#include <hex/pattern_language/preprocessor.hpp>
#include <hex/pattern_language/pattern_data.hpp>
#include <hex/helpers/paths.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/file.hpp>

#include <hex/helpers/magic.hpp>
#include <hex/helpers/literals.hpp>

#include <imgui_imhex_extensions.h>

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    using namespace hex::literals;
    namespace fs = std::filesystem;

    static const TextEditor::LanguageDefinition& PatternLanguage() {
        static bool initialized = false;
        static TextEditor::LanguageDefinition langDef;
        if (!initialized) {
            static const char* const keywords[] = {
                "using", "struct", "union", "enum", "bitfield", "be", "le", "if", "else", "false", "true", "this", "parent", "addressof", "sizeof", "$", "while", "for", "fn", "return", "namespace"
            };
            for (auto& k : keywords)
                langDef.mKeywords.insert(k);

            static const char* const builtInTypes[] = {
                    "u8", "u16", "u32", "u64", "u128",
                    "s8", "s16", "s32", "s64", "s128",
                    "float", "double", "char", "char16",
                    "bool", "padding", "str", "auto"
            };

            for (const auto name : builtInTypes) {
                TextEditor::Identifier id;
                id.mDeclaration = "Built-in type";
                langDef.mIdentifiers.insert(std::make_pair(std::string(name), id));
            }

            langDef.mTokenize = [](const char * inBegin, const char * inEnd, const char *& outBegin, const char *& outEnd, TextEditor::PaletteIndex & paletteIndex) -> bool {
                paletteIndex = TextEditor::PaletteIndex::Max;

                while (inBegin < inEnd && isascii(*inBegin) && isblank(*inBegin))
                    inBegin++;

                if (inBegin == inEnd) {
                    outBegin = inEnd;
                    outEnd = inEnd;
                    paletteIndex = TextEditor::PaletteIndex::Default;
                }
                else if (TokenizeCStyleIdentifier(inBegin, inEnd, outBegin, outEnd)) {
                    paletteIndex = TextEditor::PaletteIndex::Identifier;
                }
                else if (TokenizeCStyleNumber(inBegin, inEnd, outBegin, outEnd))
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
        this->m_patternLanguageRuntime = new pl::PatternLanguage();

        this->m_textEditor.SetLanguageDefinition(PatternLanguage());
        this->m_textEditor.SetShowWhitespaces(false);

        this->m_envVarEntries = { { "", s128(0), EnvVarType::Integer } };

        EventManager::subscribe<EventProjectFileStore>(this, [this]() {
            ProjectFile::setPattern(this->m_textEditor.GetText());
        });

        EventManager::subscribe<EventProjectFileLoad>(this, [this]() {
            this->m_textEditor.SetText(ProjectFile::getPattern());
            this->parsePattern(this->m_textEditor.GetText().data());
        });

        EventManager::subscribe<RequestSetPatternLanguageCode>(this, [this](std::string code) {
             this->m_textEditor.SelectAll();
             this->m_textEditor.Delete();
             this->m_textEditor.InsertText(code);
        });

        EventManager::subscribe<EventFileLoaded>(this, [this](const std::string &path) {
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
                for (auto &entry : std::filesystem::directory_iterator(dir, errorCode)) {
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

        EventManager::subscribe<EventFileUnloaded>(this, [this]{
            this->m_textEditor.SetText("");
            this->m_patternLanguageRuntime->abort();
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
    }

    ViewPatternEditor::~ViewPatternEditor() {
        delete this->m_patternLanguageRuntime;

        EventManager::unsubscribe<EventProjectFileStore>(this);
        EventManager::unsubscribe<EventProjectFileLoad>(this);
        EventManager::unsubscribe<RequestSetPatternLanguageCode>(this);
        EventManager::unsubscribe<EventFileLoaded>(this);
        EventManager::unsubscribe<RequestChangeTheme>(this);
        EventManager::unsubscribe<EventFileUnloaded>(this);
    }

    void ViewPatternEditor::drawMenu() {
        if (ImGui::BeginMenu("hex.menu.file"_lang)) {

            ImGui::Separator();

            if (ImGui::MenuItem("hex.builtin.view.pattern_editor.menu.file.load_pattern"_lang)) {

                this->m_selectedPatternFile = 0;
                this->m_possiblePatternFiles.clear();

                for (auto &imhexPath : hex::getPath(ImHexPath::Patterns)) {
                    if (!fs::exists(imhexPath)) continue;

                    for (auto &entry: fs::recursive_directory_iterator(imhexPath)) {
                        if (entry.is_regular_file() && entry.path().extension() == ".hexpat") {
                            this->m_possiblePatternFiles.push_back(entry.path());
                        }
                    }
                }

                View::doLater([]{ ImGui::OpenPopup("hex.builtin.view.pattern_editor.menu.file.load_pattern"_lang); });
            }

            if (ImGui::MenuItem("hex.builtin.view.pattern_editor.menu.file.save_pattern"_lang)) {
                hex::openFileBrowser("hex.builtin.view.pattern_editor.menu.file.save_pattern"_lang, DialogMode::Save, { { "Pattern", "hexpat" }}, [this](const std::string &path) {
                    File file(path, File::Mode::Create);

                    file.write(this->m_textEditor.GetText());
                });
            }

            ImGui::EndMenu();
        }
    }

    void ViewPatternEditor::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.pattern_editor.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_None | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            auto provider = ImHexApi::Provider::get();

            if (ImHexApi::Provider::isValid() && provider->isAvailable()) {
                auto textEditorSize = ImGui::GetContentRegionAvail();
                textEditorSize.y *= 4.0/5.0;
                textEditorSize.y -= ImGui::GetTextLineHeightWithSpacing();
                this->m_textEditor.Render("hex.builtin.view.pattern_editor.name"_lang, textEditorSize, true);

                auto consoleSize = ImGui::GetContentRegionAvail();
                consoleSize.y -= ImGui::GetTextLineHeightWithSpacing();

                ImGui::PushStyleColor(ImGuiCol_ChildBg, this->m_textEditor.GetPalette()[u32(TextEditor::PaletteIndex::Background)]);
                if (ImGui::BeginChild("##console", consoleSize, true, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
                    for (auto &[level, message] : this->m_console) {
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
                            default: continue;
                        }

                        ImGui::TextUnformatted(message.c_str());

                        ImGui::PopStyleColor();
                    }

                }
                ImGui::EndChild();
                ImGui::PopStyleColor(1);

                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);

                if (this->m_evaluatorRunning) {
                    if (ImGui::IconButton(ICON_VS_DEBUG_STOP, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed)))
                        this->m_patternLanguageRuntime->abort();
                } else {
                    if (ImGui::IconButton(ICON_VS_DEBUG_START, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarGreen)))
                        this->parsePattern(this->m_textEditor.GetText().data());
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

                    ImGui::TextUnformatted(hex::format("{} / {}",
                                                       this->m_patternLanguageRuntime->getCreatedPatternCount(),
                                                       this->m_patternLanguageRuntime->getMaximumPatternCount()
                                                       ).c_str()
                                           );

                    ImGui::SameLine();

                    if (ImGui::IconButton(ICON_VS_GLOBE, ImGui::GetStyleColorVec4(ImGuiCol_Text)))
                        ImGui::OpenPopup("env_vars");

                    if (ImGui::BeginPopup("env_vars")) {
                        ImGui::TextUnformatted("hex.builtin.view.pattern_editor.env_vars"_lang);
                        ImGui::SameLine();
                        if (ImGui::IconButton(ICON_VS_ADD, ImGui::GetStyleColorVec4(ImGuiCol_Text))) this->m_envVarEntries.push_back({ "", s128(0), EnvVarType::Integer });
                        ImGui::Separator();

                        int index = 0;
                        for (auto &[name, value, type] : this->m_envVarEntries) {
                            ImGui::PushID(index++);

                            ImGui::PushItemWidth(ImGui::GetTextLineHeightWithSpacing() * 2);
                            constexpr const char* Types[] = { "I", "F", "S", "B" };
                            if (ImGui::BeginCombo("", Types[static_cast<int>(type)])) {
                                for (auto i = 0; i < IM_ARRAYSIZE(Types); i++) {
                                    if (ImGui::Selectable(Types[i]))
                                        type = static_cast<EnvVarType>(i);
                                }

                                ImGui::EndCombo();
                            }
                            ImGui::PopItemWidth();

                            ImGui::SameLine();

                            ImGui::InputText("###name",  name.data(), name.capacity(), ImGuiInputTextFlags_CallbackResize, ImGui::UpdateStringSizeCallback, &name);
                            ImGui::SameLine();

                            switch (type) {
                                case EnvVarType::Integer: {
                                    s64 displayValue = hex::get_or<s128>(value, 0);
                                    ImGui::InputScalar("###value",  ImGuiDataType_S64, &displayValue);
                                    value = s128(displayValue);
                                    break;
                                }
                                case EnvVarType::Float: {
                                    auto displayValue = hex::get_or<double>(value, 0.0);
                                    ImGui::InputDouble("###value", &displayValue);
                                    value = displayValue;
                                    break;
                                }
                                case EnvVarType::Bool: {
                                    auto displayValue = hex::get_or<bool>(value, false);
                                    ImGui::Checkbox("###value", &displayValue);
                                    value = displayValue;
                                    break;
                                }
                                case EnvVarType::String: {
                                    auto displayValue = hex::get_or<std::string>(value, "");
                                    ImGui::InputText("###value",  displayValue.data(), displayValue.capacity(), ImGuiInputTextFlags_CallbackResize, ImGui::UpdateStringSizeCallback, &displayValue);
                                    value = displayValue;
                                    break;
                                }
                            }

                            ImGui::PopID();
                        }

                        ImGui::EndPopup();
                    }
                }

                if (this->m_textEditor.IsTextChanged()) {
                    ProjectFile::markDirty();

                    if (this->m_runAutomatically)
                        this->m_hasUnevaluatedChanges = true;
                }

                if (this->m_hasUnevaluatedChanges && !this->m_evaluatorRunning) {
                    this->m_hasUnevaluatedChanges = false;
                    this->parsePattern(this->m_textEditor.GetText().data());
                }
            }

            View::discardNavigationRequests();
        }
        ImGui::End();
    }

    void ViewPatternEditor::drawAlwaysVisible() {
        if (ImGui::BeginPopupModal("hex.builtin.view.pattern_editor.accept_pattern"_lang, &this->m_acceptPatternWindowOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("%s", static_cast<const char *>("hex.builtin.view.pattern_editor.accept_pattern.desc"_lang));

            std::vector<std::string> entries;
            entries.resize(this->m_possiblePatternFiles.size());

            for (u32 i = 0; i < entries.size(); i++) {
                entries[i] = std::filesystem::path(this->m_possiblePatternFiles[i]).filename().string();
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
            ImGui::Text("%s", static_cast<const char *>("hex.builtin.view.pattern_editor.accept_pattern.question"_lang));

            confirmButtons("hex.common.yes"_lang, "hex.common.no"_lang, [this]{
                this->loadPatternFile(this->m_possiblePatternFiles[this->m_selectedPatternFile].string());
                ImGui::CloseCurrentPopup();
            }, []{
                ImGui::CloseCurrentPopup();
            });

            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        bool opened = true;
        if (ImGui::BeginPopupModal("hex.builtin.view.pattern_editor.menu.file.load_pattern"_lang, &opened, ImGuiWindowFlags_AlwaysAutoResize)) {

            if (ImGui::BeginListBox("##patterns", ImVec2(-FLT_MIN, 0))) {

                u32 index = 0;
                for (auto &path : this->m_possiblePatternFiles) {
                    if (ImGui::Selectable(path.filename().string().c_str(), index == this->m_selectedPatternFile))
                        this->m_selectedPatternFile = index;
                    index++;
                }

                ImGui::EndListBox();
            }

            if (ImGui::Button("hex.common.open"_lang)) {
                this->loadPatternFile(this->m_possiblePatternFiles[this->m_selectedPatternFile].string());
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("hex.common.browse"_lang)) {
                hex::openFileBrowser("hex.builtin.view.pattern_editor.open_pattern"_lang, DialogMode::Open, { { "Pattern File", "hexpat" } }, [this](auto path) {
                    this->loadPatternFile(path);
                    ImGui::CloseCurrentPopup();
                });
            }

            ImGui::EndPopup();
        }
    }


    void ViewPatternEditor::loadPatternFile(const std::string &path) {
        FILE *file = fopen(path.c_str(), "rb");

        if (file != nullptr) {
            char *buffer;
            fseek(file, 0, SEEK_END);
            size_t size = ftell(file);
            rewind(file);

            buffer = new char[size + 1];

            fread(buffer, size, 1, file);
            buffer[size] = 0x00;


            fclose(file);

            this->parsePattern(buffer);
            this->m_textEditor.SetText(buffer);

            delete[] buffer;
        }
    }

    void ViewPatternEditor::clearPatternData() {
        for (auto &data : SharedData::patternData)
            delete data;

        SharedData::patternData.clear();
        pl::PatternData::resetPalette();
    }

    void ViewPatternEditor::parsePattern(char *buffer) {
        this->m_evaluatorRunning = true;

        this->m_textEditor.SetErrorMarkers({ });
        this->m_console.clear();
        this->clearPatternData();
        EventManager::post<EventPatternChanged>();

        std::thread([this, buffer = std::string(buffer)] {
            std::map<std::string, pl::Token::Literal> envVars;
            for (const auto &[name, value, type] : this->m_envVarEntries)
                envVars.insert({ name, value });

            auto result = this->m_patternLanguageRuntime->executeString(ImHexApi::Provider::get(), buffer, envVars);

            auto error = this->m_patternLanguageRuntime->getError();
            if (error.has_value()) {
                this->m_textEditor.SetErrorMarkers({ error.value() });
            }

            this->m_console = this->m_patternLanguageRuntime->getConsoleLog();

            if (result.has_value()) {
                SharedData::patternData = std::move(result.value());
                //View::doLater([]{
                    EventManager::post<EventPatternChanged>();
                //});
            }

            this->m_evaluatorRunning = false;
        }).detach();

    }

}
