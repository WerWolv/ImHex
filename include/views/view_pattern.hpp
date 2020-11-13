#pragma once

#include "parser/ast_node.hpp"
#include "parser/parser.hpp"
#include "parser/lexer.hpp"

#include "views/view.hpp"
#include "views/highlight.hpp"

#include "providers/provider.hpp"

#include <concepts>
#include <cstring>


#include "imfilebrowser.h"

namespace hex {

    class ViewPattern : public View {
    public:
        explicit ViewPattern(prv::Provider* &dataProvider, std::vector<Highlight> &highlights);
        ~ViewPattern() override;

        void createMenu() override;
        void createView() override;

    private:
        char *m_buffer = nullptr;

        std::vector<Highlight> &m_highlights;
        prv::Provider* &m_dataProvider;
        bool m_windowOpen = true;

        ImGui::FileBrowser m_fileBrowser;


        void setHighlight(u64 offset, std::string name, lang::Token::TypeToken::Type type, size_t size = 0, u32 color = 0);
        void parsePattern(char *buffer);

        s32 highlightUsingDecls(std::vector<lang::ASTNode*> &ast, lang::ASTNodeTypeDecl* currTypeDeclNode, lang::ASTNodeVariableDecl* currVarDec, u64 offset, std::string name);
        s32 highlightStruct(std::vector<lang::ASTNode*> &ast, lang::ASTNodeStruct* currStructNode, u64 offset, std::string name);
    };

}