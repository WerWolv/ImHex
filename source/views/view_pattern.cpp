#include "views/view_pattern.hpp"

#include "lang/preprocessor.hpp"
#include "lang/parser.hpp"
#include "lang/lexer.hpp"
#include "lang/validator.hpp"
#include "lang/evaluator.hpp"
#include "utils.hpp"

namespace hex {

    ViewPattern::ViewPattern(prv::Provider* &dataProvider, std::vector<lang::PatternData*> &patternData)
        : View(), m_dataProvider(dataProvider), m_patternData(patternData) {

        this->m_buffer = new char[0xFF'FFFF];
        std::memset(this->m_buffer, 0x00, 0xFF'FFFF);
    }
    ViewPattern::~ViewPattern() {
        if (this->m_buffer != nullptr)
            delete[] this->m_buffer;
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
            if (this->m_buffer != nullptr && this->m_dataProvider != nullptr && this->m_dataProvider->isReadable()) {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

                auto size = ImGui::GetWindowSize();
                size.y -= 50;
                ImGui::InputTextMultiline("Pattern", this->m_buffer, 0xFFFF, size,
                                          ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackEdit,
                                          [](ImGuiInputTextCallbackData *data) -> int {
                                              auto _this = static_cast<ViewPattern *>(data->UserData);

                                              _this->parsePattern(data->Buf);

                                              return 0;
                                          }, this
                );

                ImGui::PopStyleVar(2);
            }
        }
        ImGui::End();

        if (this->m_fileBrowser.showFileDialog("Open Hex Pattern", imgui_addons::ImGuiFileBrowser::DialogMode::OPEN, ImVec2(0, 0), ".hexpat")) {

            FILE *file = fopen(this->m_fileBrowser.selected_path.c_str(), "rb");

            if (file != nullptr) {
                fseek(file, 0, SEEK_END);
                size_t size = ftell(file);
                rewind(file);

                if (size >= 0xFF'FFFF) {
                    fclose(file);
                    return;
                }

                fread(this->m_buffer, size, 1, file);

                fclose(file);

                this->parsePattern(this->m_buffer);
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