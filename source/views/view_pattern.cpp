#include "views/view_pattern.hpp"

#include "lang/preprocessor.hpp"
#include "lang/parser.hpp"
#include "lang/lexer.hpp"
#include "lang/validator.hpp"
#include "lang/evaluator.hpp"

#include "helpers/project_file_handler.hpp"
#include "helpers/utils.hpp"

#include <magic.h>

namespace hex {

    static const TextEditor::LanguageDefinition& PatternLanguage() {
        static bool initialized = false;
        static TextEditor::LanguageDefinition langDef;
        if (!initialized) {
            static const char* const keywords[] = {
                "using", "struct", "union", "enum", "bitfield", "be", "le"
            };
            for (auto& k : keywords)
                langDef.mKeywords.insert(k);

            static std::pair<const char* const, size_t> builtInTypes[] = {
                    { "u8", 1 }, { "u16", 2 }, { "u32", 4 }, { "u64", 8 }, { "u128", 16 },
                    { "s8", 1 }, { "s16", 2 }, { "s32", 4 }, { "s64", 8 }, { "s128", 16 },
                    { "float", 4 }, { "double", 8 }, { "padding", 1 }
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
                else if (TokenizeCStyleIdentifier(inBegin, inEnd, outBegin, outEnd))
                    paletteIndex = TextEditor::PaletteIndex::Identifier;
                else if (TokenizeCStyleNumber(inBegin, inEnd, outBegin, outEnd))
                    paletteIndex = TextEditor::PaletteIndex::Number;
                else if (TokenizeCStyleCharacterLiteral(inBegin, inEnd, outBegin, outEnd))
                    paletteIndex = TextEditor::PaletteIndex::CharLiteral;

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


    ViewPattern::ViewPattern(prv::Provider* &dataProvider, std::vector<lang::PatternData*> &patternData)
        : View("Pattern"), m_dataProvider(dataProvider), m_patternData(patternData) {

        this->m_textEditor.SetLanguageDefinition(PatternLanguage());
        this->m_textEditor.SetShowWhitespaces(false);

        View::subscribeEvent(Events::ProjectFileStore, [this](const void*) {
            ProjectFile::setPattern(this->m_textEditor.GetText());
        });

        View::subscribeEvent(Events::ProjectFileLoad, [this](const void*) {
            this->m_textEditor.SetText(ProjectFile::getPattern());
            this->parsePattern(this->m_textEditor.GetText().data());
        });

        View::subscribeEvent(Events::AppendPatternLanguageCode, [this](const void *userData) {
             const char *code = static_cast<const char*>(userData);

             this->m_textEditor.InsertText("\n");
             this->m_textEditor.InsertText(code);
        });

        View::subscribeEvent(Events::FileLoaded, [this](const void* userData) {
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

            std::vector<u8> buffer(std::min(this->m_dataProvider->getSize(), size_t(0xFFFF)), 0x00);
            this->m_dataProvider->read(0, buffer.data(), buffer.size());

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

                if (foundCorrectType) {
                    this->m_possiblePatternFile = entry.path();
                    View::doLater([] { ImGui::OpenPopup("Accept Pattern"); });
                    ImGui::SetNextWindowSize(ImVec2(200, 100));
                    break;
                }
            }
        });
    }

    ViewPattern::~ViewPattern() {
        View::unsubscribeEvent(Events::ProjectFileStore);
        View::unsubscribeEvent(Events::ProjectFileLoad);
    }

    void ViewPattern::createMenu() {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Load pattern...")) {
                View::doLater([]{ ImGui::OpenPopup("Open Hex Pattern"); });
            }
            ImGui::EndMenu();
        }
    }

    void ViewPattern::createView() {
        if (ImGui::Begin("Pattern", &this->getWindowOpenState(), ImGuiWindowFlags_None | ImGuiWindowFlags_NoCollapse)) {
            if (this->m_dataProvider != nullptr && this->m_dataProvider->isAvailable()) {
                this->m_textEditor.Render("Pattern");

                if (this->m_textEditor.IsTextChanged()) {
                    this->parsePattern(this->m_textEditor.GetText().data());
                }
            }
        }
        ImGui::End();

        if (this->m_fileBrowser.showFileDialog("Open Hex Pattern", imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, ImVec2(0, 0), ".hexpat")) {
            this->loadPatternFile(this->m_fileBrowser.selected_path);
        }

        if (ImGui::BeginPopupModal("Accept Pattern", nullptr, ImGuiWindowFlags_NoResize)) {
            ImGui::TextUnformatted("A pattern compatible with this data type has been found:");
            ImGui::Text("%ls", this->m_possiblePatternFile.filename().c_str());
            ImGui::NewLine();
            ImGui::Text("Do you want to load it?");
            ImGui::NewLine();

            if (ImGui::Button("Yes", ImVec2(40, 20))) {
                this->loadPatternFile(this->m_possiblePatternFile.string());
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(40, 20))) {
                ImGui::CloseCurrentPopup();
            }

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

    template<std::derived_from<lang::ASTNode> T>
    static std::vector<T*> findNodes(const lang::ASTNode::Type type, const std::vector<lang::ASTNode*> &nodes) {
        std::vector<T*> result;

        for (const auto & node : nodes)
            if (node->getType() == type)
                result.push_back(static_cast<T*>(node));

        return result;
    }

    void ViewPattern::parsePattern(char *buffer) {
        this->clearPatternData();
        this->m_textEditor.SetErrorMarkers({ });
        this->postEvent(Events::PatternChanged);

        hex::lang::Preprocessor preprocessor;
        std::endian defaultDataEndianess = std::endian::native;

        preprocessor.addPragmaHandler("endian", [&defaultDataEndianess](std::string value) {
           if (value == "big") {
               defaultDataEndianess = std::endian::big;
               return true;
           } else if (value == "little") {
               defaultDataEndianess = std::endian::little;
               return true;
           } else if (value == "native") {
               defaultDataEndianess = std::endian::native;
               return true;
           } else
               return false;
        });
        preprocessor.addDefaultPragmaHandlers();

        auto [preprocessingResult, preprocesedCode] = preprocessor.preprocess(buffer);
        if (preprocessingResult.failed()) {
            this->m_textEditor.SetErrorMarkers({ preprocessor.getError() });
            return;
        }

        hex::lang::Lexer lexer;
        auto [lexResult, tokens] = lexer.lex(preprocesedCode);
        if (lexResult.failed()) {
            this->m_textEditor.SetErrorMarkers({ lexer.getError() });
            return;
        }

        hex::lang::Parser parser;
        auto [parseResult, ast] = parser.parse(tokens);
        if (parseResult.failed()) {
            this->m_textEditor.SetErrorMarkers({ parser.getError() });
            printf("%d %s\n", parser.getError().first, parser.getError().second.c_str());
            return;
        }

        hex::ScopeExit deleteAst([&ast]{ for(auto &node : ast) delete node; });

        hex::lang::Validator validator;
        auto validatorResult = validator.validate(ast);
        if (!validatorResult) {
            this->m_textEditor.SetErrorMarkers({ validator.getError() });
            return;
        }

        hex::lang::Evaluator evaluator(this->m_dataProvider, defaultDataEndianess);
        auto [evaluateResult, patternData] = evaluator.evaluate(ast);
        if (evaluateResult.failed()) {
            this->m_textEditor.SetErrorMarkers({ evaluator.getError() });
            return;
        }
        this->m_patternData = patternData;

        this->postEvent(Events::PatternChanged);
    }

}