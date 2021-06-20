#include "views/view_pattern.hpp"

#include "helpers/project_file_handler.hpp"
#include <hex/helpers/utils.hpp>
#include <hex/lang/preprocessor.hpp>

#include <magic.h>

#include <imgui_imhex_extensions.h>

namespace hex {

    static const TextEditor::LanguageDefinition& PatternLanguage() {
        static bool initialized = false;
        static TextEditor::LanguageDefinition langDef;
        if (!initialized) {
            static const char* const keywords[] = {
                "using", "struct", "union", "enum", "bitfield", "be", "le", "if", "else", "false", "true", "parent", "addressof", "sizeof", "$", "while", "fn", "return"
            };
            for (auto& k : keywords)
                langDef.mKeywords.insert(k);

            static std::pair<const char* const, size_t> builtInTypes[] = {
                    { "u8", 1 }, { "u16", 2 }, { "u32", 4 }, { "u64", 8 }, { "u128", 16 },
                    { "s8", 1 }, { "s16", 2 }, { "s32", 4 }, { "s64", 8 }, { "s128", 16 },
                    { "float", 4 }, { "double", 8 }, { "char", 1 }, { "char16", 2 }, { "bool", 1 }, { "padding", 1 }
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


    ViewPattern::ViewPattern() : View("hex.view.pattern.name") {
        this->m_patternLanguageRuntime = new lang::PatternLanguage();

        this->m_textEditor.SetLanguageDefinition(PatternLanguage());
        this->m_textEditor.SetShowWhitespaces(false);

        EventManager::subscribe<EventProjectFileStore>(this, [this]() {
            ProjectFile::setPattern(this->m_textEditor.GetText());
        });

        EventManager::subscribe<EventProjectFileLoad>(this, [this]() {
            this->m_textEditor.SetText(ProjectFile::getPattern());
            this->parsePattern(this->m_textEditor.GetText().data());
        });

        EventManager::subscribe<RequestAppendPatternLanguageCode>(this, [this](std::string code) {
             this->m_textEditor.InsertText("\n");
             this->m_textEditor.InsertText(code);
        });

        EventManager::subscribe<EventFileLoaded>(this, [this](const std::string &path) {
            if (this->m_textEditor.GetText().find_first_not_of(" \f\n\r\t\v") != std::string::npos)
                return;

            lang::Preprocessor preprocessor;
            std::string magicFiles;

            std::error_code error;
            for (const auto &dir : hex::getPath(ImHexPath::Magic)) {
                if (!std::filesystem::is_directory(dir))
                    continue;

                for (const auto &entry : std::filesystem::directory_iterator(dir, error)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".mgc")
                        magicFiles += entry.path().string() + MAGIC_PATH_SEPARATOR;
                }
            }

            if (error)
                return;

            auto provider = SharedData::currentProvider;

            if (provider == nullptr)
                return;

            std::vector<u8> buffer(std::min(provider->getSize(), size_t(0xFFFF)), 0x00);
            provider->readRelative(0, buffer.data(), buffer.size());

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

            this->m_possiblePatternFiles.clear();

            std::error_code errorCode;
            for (const auto &dir : hex::getPath(ImHexPath::Patterns)) {
                for (auto &entry : std::filesystem::directory_iterator(dir, errorCode)) {
                    if (!entry.is_regular_file())
                        continue;

                    FILE *file = fopen(entry.path().string().c_str(), "r");

                    if (file == nullptr)
                        continue;

                    fseek(file, 0, SEEK_END);
                    size_t size = ftell(file);
                    rewind(file);

                    std::vector<char> patternBuffer( size + 1, 0x00);
                    fread(patternBuffer.data(), 1, size, file);
                    fclose(file);

                    preprocessor.preprocess(patternBuffer.data());

                    if (foundCorrectType)
                        this->m_possiblePatternFiles.push_back(entry.path().string());
                }
            }


            if (!this->m_possiblePatternFiles.empty()) {
                this->m_selectedPatternFile = 0;
                View::doLater([] { ImGui::OpenPopup("hex.view.pattern.accept_pattern"_lang); });
            }
        });

        /* Settings */
        {

            EventManager::subscribe<EventSettingsChanged>(this, [this]() {
                auto theme = ContentRegistry::Settings::getSetting("hex.builtin.setting.interface", "hex.builtin.setting.interface.color");

                if (theme.is_number()) {
                    switch (static_cast<int>(theme)) {
                        default:
                        case 0: /* Dark theme */
                            this->m_textEditor.SetPalette(TextEditor::GetDarkPalette());
                            break;
                        case 1: /* Light theme */
                            this->m_textEditor.SetPalette(TextEditor::GetLightPalette());
                            break;
                        case 2: /* Classic theme */
                            this->m_textEditor.SetPalette(TextEditor::GetRetroBluePalette());
                            break;
                    }
                }
            });

        }
    }

    ViewPattern::~ViewPattern() {
        delete this->m_patternLanguageRuntime;

        EventManager::unsubscribe<EventProjectFileStore>(this);
        EventManager::unsubscribe<EventProjectFileLoad>(this);
        EventManager::unsubscribe<RequestAppendPatternLanguageCode>(this);
        EventManager::unsubscribe<EventFileLoaded>(this);
        EventManager::unsubscribe<EventSettingsChanged>(this);
    }

    void ViewPattern::drawMenu() {
        if (ImGui::BeginMenu("hex.menu.file"_lang)) {
            if (ImGui::MenuItem("hex.view.pattern.menu.file.load_pattern"_lang)) {
                View::openFileBrowser("hex.view.pattern.open_pattern"_lang, DialogMode::Open, { { "Pattern File", "hexpat" } }, [this](auto path) {
                    this->loadPatternFile(path);
                });
            }
            ImGui::EndMenu();
        }
    }

    void ViewPattern::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.view.pattern.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_None | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            auto provider = SharedData::currentProvider;

            if (provider != nullptr && provider->isAvailable()) {
                auto textEditorSize = ImGui::GetContentRegionAvail();
                textEditorSize.y *= 4.0/5.0;
                textEditorSize.y -= ImGui::GetTextLineHeightWithSpacing();
                this->m_textEditor.Render("hex.view.pattern.name"_lang, textEditorSize, true);

                auto consoleSize = ImGui::GetContentRegionAvail();
                consoleSize.y -= ImGui::GetTextLineHeightWithSpacing();

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

                ImGui::Disabled([this]{
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(0x20, 0x85, 0x20)));
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);

                    if (ImGui::ArrowButton("evaluate", ImGuiDir_Right))
                        this->parsePattern(this->m_textEditor.GetText().data());

                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor();
                }, this->m_evaluatorRunning);

                ImGui::SameLine();
                if (this->m_evaluatorRunning)
                     ImGui::TextSpinner("hex.view.pattern.evaluating"_lang);
                else
                    ImGui::Checkbox("hex.view.pattern.auto"_lang, &this->m_runAutomatically);

                if (this->m_textEditor.IsTextChanged() && this->m_runAutomatically) {
                    this->parsePattern(this->m_textEditor.GetText().data());
                }
            }

            View::discardNavigationRequests();
        }
        ImGui::End();
    }

    void ViewPattern::drawAlwaysVisible() {
        if (ImGui::BeginPopupModal("hex.view.pattern.accept_pattern"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("hex.view.pattern.accept_pattern.desc"_lang);

            std::vector<std::string> entries;
            entries.resize(this->m_possiblePatternFiles.size());

            for (u32 i = 0; i < entries.size(); i++) {
                entries[i] = std::filesystem::path(this->m_possiblePatternFiles[i]).filename().string();
            }

            ImGui::ListBox("hex.view.pattern.accept_pattern.patterns"_lang, &this->m_selectedPatternFile, [](void *data, int id, const char** outText) -> bool {
                auto &entries = *static_cast<std::vector<std::string>*>(data);

                *outText = entries[id].c_str();

                return true;
            }, &entries, entries.size(), 4);

            ImGui::NewLine();
            ImGui::Text("hex.view.pattern.accept_pattern.question"_lang);

            confirmButtons("hex.common.yes"_lang, "hex.common.no"_lang, [this]{
                this->loadPatternFile(this->m_possiblePatternFiles[this->m_selectedPatternFile]);
                ImGui::CloseCurrentPopup();
            }, []{
                ImGui::CloseCurrentPopup();
            });

            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
    }


    void ViewPattern::loadPatternFile(std::string_view path) {
        FILE *file = fopen(path.data(), "rb");

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
        for (auto &data : SharedData::patternData)
            delete data;

        SharedData::patternData.clear();
        lang::PatternData::resetPalette();
    }

    void ViewPattern::parsePattern(char *buffer) {
        this->m_evaluatorRunning = true;

        this->clearPatternData();
        this->m_textEditor.SetErrorMarkers({ });
        this->m_console.clear();
        EventManager::post<EventPatternChanged>();

        std::thread([this, buffer = std::string(buffer)] {
            auto result = this->m_patternLanguageRuntime->executeString(SharedData::currentProvider, buffer);

            auto error = this->m_patternLanguageRuntime->getError();
            if (error.has_value()) {
                this->m_textEditor.SetErrorMarkers({ error.value() });
            }

            this->m_console = this->m_patternLanguageRuntime->getConsoleLog();

            if (result.has_value()) {
                SharedData::patternData = std::move(result.value());
                View::doLater([]{
                    EventManager::post<EventPatternChanged>();
                });
            }

            this->m_evaluatorRunning = false;
        }).detach();

    }

}