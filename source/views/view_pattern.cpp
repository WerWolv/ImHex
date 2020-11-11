#include <random>
#include "views/view_pattern.hpp"

#include "utils.hpp"

namespace hex {

    ViewPattern::ViewPattern(std::vector<Highlight> &highlights) : View(), m_highlights(highlights) {
        this->m_buffer = new char[0xFFFFFF];
        std::memset(this->m_buffer, 0x00, 0xFFFFFF);
    }
    ViewPattern::~ViewPattern() {
        delete[] this->m_buffer;
    }

    void ViewPattern::createMenu() {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Load pattern...")) {
                auto filePath = openFileDialog();
                if (filePath.has_value()) {
                    FILE *file = fopen(filePath->c_str(), "r+b");

                    fseek(file, 0, SEEK_END);
                    size_t size = ftell(file);
                    rewind(file);

                    if (size > 0xFF'FFFF)
                        return;

                    fread(this->m_buffer, size, 1, file);

                    fclose(file);

                    this->parsePattern(this->m_buffer);
                }

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
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

            auto size = ImGui::GetWindowSize();
            size.y -= 50;
            ImGui::InputTextMultiline("Pattern", this->m_buffer, 0xFFFF, size, ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackEdit,
                [](ImGuiInputTextCallbackData* data) -> int {
                    auto _this = static_cast<ViewPattern*>(data->UserData);

                    _this->parsePattern(data->Buf);

                    return 0;
                }, this
            );

            ImGui::PopStyleVar(2);
        }
        ImGui::End();
    }


    void ViewPattern::setHighlight(u64 offset, size_t size, std::string name, u32 color) {
        if (color == 0)
            color = std::mt19937(std::random_device()())();

        color &= ~0xFF00'0000;
        color |= 0x5000'0000;

        this->m_highlights.emplace_back(offset, size, color, name);
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
        static hex::lang::Lexer lexer;
        static hex::lang::Parser parser;

        this->m_highlights.clear();

        auto [lexResult, tokens] = lexer.lex(buffer);

        if (lexResult.failed()) {
            return;
        }

        auto [parseResult, ast] = parser.parse(tokens);
        if (parseResult.failed()) {
            for(auto &node : ast) delete node;
            return;
        }

        for (auto &varNode : findNodes<lang::ASTNodeVariableDecl>(lang::ASTNode::Type::VariableDecl, ast)) {
            if (!varNode->getOffset().has_value())
                continue;

            u64 offset = varNode->getOffset().value();
            if (varNode->getVariableType() != lang::Token::TypeToken::Type::CustomType) {
                this->setHighlight(offset, static_cast<u32>(varNode->getVariableType()) >> 4, varNode->getVariableName());
            } else {
                for (auto &structNode : findNodes<lang::ASTNodeStruct>(lang::ASTNode::Type::Struct, ast))
                    if (varNode->getCustomVariableTypeName() == structNode->getName())
                        if (this->highlightStruct(ast, structNode, offset) == -1)
                            this->m_highlights.clear();

                for (auto &usingNode : findNodes<lang::ASTNodeTypeDecl>(lang::ASTNode::Type::TypeDecl, ast))
                    if (varNode->getCustomVariableTypeName() == usingNode->getTypeName())
                        if (this->highlightUsingDecls(ast, usingNode, varNode, offset) == -1)
                            this->m_highlights.clear();
            }

        }

        for(auto &node : ast) delete node;
    }

    s32 ViewPattern::highlightUsingDecls(std::vector<lang::ASTNode*> &ast, lang::ASTNodeTypeDecl* currTypeDeclNode, lang::ASTNodeVariableDecl* currVarDecl, u64 offset) {
        if (currTypeDeclNode->getAssignedType() != lang::Token::TypeToken::Type::CustomType) {
            size_t size = static_cast<u32>(currTypeDeclNode->getAssignedType()) >> 4;

            this->setHighlight(offset, size, currVarDecl->getVariableName());
            offset += size;
        } else {
            bool foundType = false;
            for (auto &structNode : findNodes<lang::ASTNodeStruct>(lang::ASTNode::Type::Struct, ast))
                if (structNode->getName() == currTypeDeclNode->getAssignedCustomTypeName()) {
                    offset = this->highlightStruct(ast, structNode, offset);
                    foundType = true;
                    break;
                }

            for (auto &typeDeclNode : findNodes<lang::ASTNodeTypeDecl>(lang::ASTNode::Type::TypeDecl, ast))
                if (typeDeclNode->getTypeName() == currTypeDeclNode->getAssignedCustomTypeName()) {
                    offset = this->highlightUsingDecls(ast, typeDeclNode, currVarDecl, offset);
                    foundType = true;
                    break;
                }

            if (!foundType)
                return -1;
        }

        return offset;
    }

    s32 ViewPattern::highlightStruct(std::vector<lang::ASTNode*> &ast, lang::ASTNodeStruct* currStructNode, u64 offset) {
        u64 startOffset = offset;

        for (auto &node : currStructNode->getNodes()) {
            auto var = static_cast<lang::ASTNodeVariableDecl*>(node);

            if (var->getVariableType() != lang::Token::TypeToken::Type::CustomType) {
                size_t size = static_cast<u32>(var->getVariableType()) >> 4;

                this->setHighlight(offset, size, var->getVariableName());
                offset += size;
            } else {
                bool foundType = false;
                for (auto &structNode : findNodes<lang::ASTNodeStruct>(lang::ASTNode::Type::Struct, ast))
                    if (structNode->getName() == var->getCustomVariableTypeName()) {
                        auto size = this->highlightStruct(ast, structNode, offset);

                        if (size == -1)
                            return -1;

                        offset += size;
                        foundType = true;
                        break;
                    }

                for (auto &typeDeclNode : findNodes<lang::ASTNodeTypeDecl>(lang::ASTNode::Type::TypeDecl, ast))
                    if (typeDeclNode->getTypeName() == var->getCustomVariableTypeName()) {
                        auto size = this->highlightUsingDecls(ast, typeDeclNode, var, offset);

                        if (size == -1)
                            return -1;

                        offset = size;
                        foundType = true;
                        break;
                    }

                if (!foundType)
                    return -1;
            }

        }

        return offset - startOffset;
    }

}