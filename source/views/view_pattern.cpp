#include "views/view_pattern.hpp"

#include "helpers/project_file_handler.hpp"
#include <hex/helpers/utils.hpp>
#include <hex/lang/preprocessor.hpp>

#include <magic.h>

namespace hex {

    static const TextEditor::LanguageDefinition& PatternLanguage() {
        static bool initialized = false;
        static TextEditor::LanguageDefinition langDef;
        if (!initialized) {
            static const char* const keywords[] = {
                "using", "struct", "union", "enum", "bitfield", "be", "le", "if", "else", "false", "true"
            };
            for (auto& k : keywords)
                langDef.mKeywords.insert(k);

            static std::pair<const char* const, size_t> builtInTypes[] = {
                    { "u8", 1 }, { "u16", 2 }, { "u32", 4 }, { "u64", 8 }, { "u128", 16 },
                    { "s8", 1 }, { "s16", 2 }, { "s32", 4 }, { "s64", 8 }, { "s128", 16 },
                    { "float", 4 }, { "double", 8 }, { "char", 1 }, { "bool", 1 }, { "padding", 1 }
            };

            for (const auto &[name, size] : builtInTypes) {
                TextEditor::Identifier id;
                id.mDeclaration = std::to_string(size);
                id.mDeclaration += size == 1 ? " byte" : " bytes";
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
                    if (SharedData::patternLanguageFunctions.contains(std::string(outBegin, outEnd - outBegin)))
                        paletteIndex = TextEditor::PaletteIndex::LineNumber;
                    else
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


    ViewPattern::ViewPattern(std::vector<lang::PatternData*> &patternData) : View("Pattern"), m_patternData(patternData) {
        this->m_patternLanguageRuntime = new lang::PatternLanguage();

        this->m_textEditor.SetLanguageDefinition(PatternLanguage());
        this->m_textEditor.SetShowWhitespaces(false);

        View::subscribeEvent(Events::ProjectFileStore, [this](auto) {
            ProjectFile::setPattern(this->m_textEditor.GetText());
        });

        View::subscribeEvent(Events::ProjectFileLoad, [this](auto) {
            this->m_textEditor.SetText(ProjectFile::getPattern());
            this->parsePattern(this->m_textEditor.GetText().data());
        });

        View::subscribeEvent(Events::AppendPatternLanguageCode, [this](auto userData) {
             auto code = std::any_cast<const char*>(userData);

             this->m_textEditor.InsertText("\n");
             this->m_textEditor.InsertText(code);
        });

        View::subscribeEvent(Events::FileLoaded, [this](auto) {
            if (this->m_textEditor.GetText().find_first_not_of(" \f\n\r\t\v") != std::string::npos)
                return;

            lang::Preprocessor preprocessor;
            std::string magicFiles;

            std::error_code error;
            for (const auto &entry : std::filesystem::directory_iterator("magic", error)) {
                if (entry.is_regular_file() && entry.path().extension() == ".mgc")
                    magicFiles += entry.path().string() + MAGIC_PATH_SEPARATOR;
            }

            if (error)
                return;

            auto provider = SharedData::currentProvider;

            if (provider == nullptr)
                return;

            std::vector<u8> buffer(std::min(provider->getSize(), size_t(0xFFFF)), 0x00);
            provider->read(0, buffer.data(), buffer.size());

            std::string mimeType;

            magic_t cookie = magic_open(MAGIC_MIME_TYPE);
            if (magic_load(cookie, magicFiles.c_str()) != -1)
                mimeType = magic_buffer(cookie, buffer.data(), buffer.size());

            magic_close(cookie);

            bool foundCorrectType = false;
            preprocessor.addPragmaHandler("MIME", [&mimeType, &foundCorrectType](std::string value) {
                if (value == mimeType) {
                    foundCorrectType = true;
                    return true;
                }
                return !std::all_of(value.begin(), value.end(), isspace) && !value.ends_with('\n') && !value.ends_with('\r');
            });
            preprocessor.addDefaultPragmaHandlers();


            std::error_code errorCode;
            for (auto &entry : std::filesystem::directory_iterator("patterns", errorCode)) {
                if (!entry.is_regular_file())
                    continue;

                FILE *file = fopen(entry.path().string().c_str(), "r");

                if (file == nullptr)
                    continue;

                fseek(file, 0, SEEK_END);
                size_t size = ftell(file);
                rewind(file);

                std::vector<char> buffer( size + 1, 0x00);
                fread(buffer.data(), 1, size, file);
                fclose(file);

                preprocessor.preprocess(buffer.data());

                if (foundCorrectType)
                    this->m_possiblePatternFiles.push_back(entry.path().filename().string());
            }

            if (!this->m_possiblePatternFiles.empty()) {
                this->m_selectedPatternFile = 0;
                View::doLater([] { ImGui::OpenPopup("Accept Pattern"); });
            }
        });

        /* Settings */
        {

            constexpr auto SettingsCategoryInterface    = "Interface";

            constexpr auto SettingColorTheme            = "Color theme";

            ContentRegistry::Settings::add(SettingsCategoryInterface, SettingColorTheme, 0, [](nlohmann::json &setting) {
                static int selection = setting;
                if (ImGui::Combo("##nolabel", &selection, "Dark\0Light\0Classic\0")) {
                    setting = selection;
                    return true;
                }

                return false;
            });

            View::subscribeEvent(Events::SettingsChanged, [this](auto) {
                int theme = ContentRegistry::Settings::getSettingsData()[SettingsCategoryInterface][SettingColorTheme];

                switch (theme) {
                    default:
                    case 0: /* Dark theme */
                        ImGui::StyleColorsDark();
                        this->m_textEditor.SetPalette(TextEditor::GetDarkPalette());
                        break;
                    case 1: /* Light theme */
                        ImGui::StyleColorsLight();
                        this->m_textEditor.SetPalette(TextEditor::GetLightPalette());
                        break;
                    case 2: /* Classic theme */
                        ImGui::StyleColorsClassic();
                        this->m_textEditor.SetPalette(TextEditor::GetRetroBluePalette());
                        break;
                }
            });

        }
    }

    ViewPattern::~ViewPattern() {
        View::unsubscribeEvent(Events::ProjectFileStore);
        View::unsubscribeEvent(Events::ProjectFileLoad);
    }

    void ViewPattern::drawMenu() {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Load pattern...")) {
                View::openFileBrowser("Open Hex Pattern", imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, ".hexpat", [this](auto path) {
                    this->loadPatternFile(path);
                });
            }
            ImGui::EndMenu();
        }
    }

    void ViewPattern::drawContent() {
        if (ImGui::Begin("Pattern", &this->getWindowOpenState(), ImGuiWindowFlags_None | ImGuiWindowFlags_NoCollapse)) {
            auto provider = SharedData::currentProvider;

            if (provider != nullptr && provider->isAvailable()) {
                auto textEditorSize = ImGui::GetContentRegionAvail();
                textEditorSize.y *= 4.0/5.0;
                this->m_textEditor.Render("Pattern", textEditorSize, true);

                auto consoleSize = ImGui::GetContentRegionAvail();
                ImGui::PushStyleColor(ImGuiCol_ChildBg, this->m_textEditor.GetPalette()[u32(TextEditor::PaletteIndex::Background)]);

                if (ImGui::BeginChild("##console", consoleSize, true, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
                    for (auto &[level, message] : this->m_console) {
                        switch (level) {
                            case lang::LogConsole::Level::Debug:
                                ImGui::PushStyleColor(ImGuiCol_Text, this->m_textEditor.GetPalette()[u32(TextEditor::PaletteIndex::Comment)]);
                                break;
                            case lang::LogConsole::Level::Info:
                                ImGui::PushStyleColor(ImGuiCol_Text, this->m_textEditor.GetPalette()[u32(TextEditor::PaletteIndex::Default)]);
                                break;
                            case lang::LogConsole::Level::Warning:
                                ImGui::PushStyleColor(ImGuiCol_Text, this->m_textEditor.GetPalette()[u32(TextEditor::PaletteIndex::Preprocessor)]);
                                break;
                            case lang::LogConsole::Level::Error:
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

                if (this->m_textEditor.IsTextChanged()) {
                    this->parsePattern(this->m_textEditor.GetText().data());
                }
            }

            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
                ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
        }
        ImGui::End();



        if (ImGui::BeginPopupModal("Accept Pattern", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("One or more patterns compatible with this data type has been found");

            char *entries[this->m_possiblePatternFiles.size()];

            for (u32 i = 0; i < this->m_possiblePatternFiles.size(); i++) {
                entries[i] = this->m_possiblePatternFiles[i].data();
            }

            ImGui::ListBox("Patterns", &this->m_selectedPatternFile, entries, IM_ARRAYSIZE(entries), 4);

            ImGui::NewLine();
            ImGui::Text("Do you want to load it?");

            confirmButtons("Yes", "No", [this]{
                this->loadPatternFile("patterns/" + this->m_possiblePatternFiles[this->m_selectedPatternFile]);
                ImGui::CloseCurrentPopup();
            }, []{
                ImGui::CloseCurrentPopup();
            });

            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
    }


    void ViewPattern::loadPatternFile(std::string path) {
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

    void ViewPattern::clearPatternData() {
        for (auto &data : this->m_patternData)
            delete data;

        this->m_patternData.clear();
        lang::PatternData::resetPalette();
    }

    void ViewPattern::parsePattern(char *buffer) {
        this->clearPatternData();
        this->m_textEditor.SetErrorMarkers({ });
        this->m_console.clear();
        this->postEvent(Events::PatternChanged);

        auto result = this->m_patternLanguageRuntime->executeString(SharedData::currentProvider, buffer);

        auto error = this->m_patternLanguageRuntime->getError();
        if (error.has_value()) {
            this->m_textEditor.SetErrorMarkers({ error.value() });
        }

        this->m_console = this->m_patternLanguageRuntime->getConsoleLog();

        if (result.has_value()) {
            this->m_patternData = std::move(result.value());
            View::postEvent(Events::PatternChanged);
        }
    }

}