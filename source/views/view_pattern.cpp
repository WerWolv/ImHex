#include "views/view_pattern.hpp"

#include "lang/preprocessor.hpp"
#include "lang/parser.hpp"
#include "lang/lexer.hpp"
#include "lang/validator.hpp"
#include "lang/evaluator.hpp"
#include "utils.hpp"

namespace hex {

    static const TextEditor::LanguageDefinition& PatternLanguage() {
        static bool initialized = false;
        static TextEditor::LanguageDefinition langDef;
        if (!initialized) {
            static const char* const keywords[] = {
                "using", "struct", "union", "enum", "bitfield"
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
        : View(), m_dataProvider(dataProvider), m_patternData(patternData) {

        this->m_textEditor.SetLanguageDefinition(PatternLanguage());
        this->m_textEditor.SetShowWhitespaces(false);
    }

    ViewPattern::~ViewPattern() {

    }

    void ViewPattern::createMenu() {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Load pattern...")) {
                View::doLater([]{ ImGui::OpenPopup("Open Hex Pattern"); });
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Pattern View", "", &this->m_windowOpen);
            ImGui::EndMenu();
        }
    }

    void ViewPattern::createView() {
        if (!this->m_windowOpen)
            return;

        if (ImGui::Begin("Pattern", &this->m_windowOpen, ImGuiWindowFlags_None)) {
            this->m_textEditor.Render("Pattern");

            if (this->m_textEditor.IsTextChanged()) {
                this->parsePattern(this->m_textEditor.GetText().data());
            }
        }
        ImGui::End();

        if (this->m_fileBrowser.showFileDialog("Open Hex Pattern", imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, ImVec2(0, 0), ".hexpat")) {

            FILE *file = fopen(this->m_fileBrowser.selected_path.c_str(), "rb");

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
        hex::lang::Preprocessor preprocessor;
        hex::lang::Lexer lexer;
        hex::lang::Parser parser;
        hex::lang::Validator validator;
        hex::lang::Evaluator evaluator;

        this->clearPatternData();
        this->postEvent(Events::PatternChanged);

        auto [preprocessingResult, preprocesedCode] = preprocessor.preprocess(buffer);
        if (preprocessingResult.failed())
            return;

        auto [lexResult, tokens] = lexer.lex(preprocesedCode);
        if (lexResult.failed()) {
            return;
        }

        auto [parseResult, ast] = parser.parse(tokens);
        if (parseResult.failed()) {
            return;
        }

        hex::ScopeExit deleteAst([&ast]{ for(auto &node : ast) delete node; });

        auto validatorResult = validator.validate(ast);
        if (!validatorResult) {
            return;
        }

        auto [evaluateResult, patternData] = evaluator.evaluate(ast);
        if (evaluateResult.failed()) {
            return;
        }
        this->m_patternData = patternData;

        this->postEvent(Events::PatternChanged);
    }

}