#pragma once

#include "parser/ast_node.hpp"
#include "parser/parser.hpp"
#include "parser/lexer.hpp"
#include "views/view.hpp"

#include <concepts>
#include <cstring>

#include "views/highlight.hpp"

#include "imfilebrowser.h"

namespace hex {

    class ViewPattern : public View {
    public:
        explicit ViewPattern(std::vector<Highlight> &highlights);
        ~ViewPattern() override;

        void createMenu() override;
        void createView() override;

    private:
        char *m_buffer;

        std::vector<Highlight> &m_highlights;
        bool m_windowOpen = true;

        ImGui::FileBrowser m_fileBrowser;


        void setHighlight(u64 offset, size_t size, std::string name, u32 color = 0);
        void parsePattern(char *buffer);

        s32 highlightUsingDecls(std::vector<lang::ASTNode*> &ast, lang::ASTNodeTypeDecl* currTypeDeclNode, lang::ASTNodeVariableDecl* currVarDec, u64 offset, std::string name);
        s32 highlightStruct(std::vector<lang::ASTNode*> &ast, lang::ASTNodeStruct* currStructNode, u64 offset, std::string name);
    };

}