#pragma once

#include "parser/ast_node.hpp"

#include "views/view.hpp"
#include "views/pattern_data.hpp"

#include "providers/provider.hpp"

#include <concepts>
#include <cstring>


#include "imfilebrowser.h"

namespace hex {

    class ViewPattern : public View {
    public:
        explicit ViewPattern(prv::Provider* &dataProvider, std::vector<hex::PatternData*> &patternData);
        ~ViewPattern() override;

        void createMenu() override;
        void createView() override;

    private:
        char *m_buffer = nullptr;

        std::vector<hex::PatternData*> &m_patternData;
        prv::Provider* &m_dataProvider;
        bool m_windowOpen = true;

        ImGui::FileBrowser m_fileBrowser;


        void addPatternData(PatternData *patternData);
        void clearPatternData();
        void parsePattern(char *buffer);

        s32 highlightUsingDecls(std::vector<lang::ASTNode*> &ast, lang::ASTNodeTypeDecl* currTypeDeclNode, lang::ASTNodeVariableDecl* currVarDec, u64 offset, std::string name);
        s32 highlightStruct(std::vector<lang::ASTNode*> &ast, lang::ASTNodeStruct* currStructNode, u64 offset, std::string name);
        s32 highlightEnum(std::vector<lang::ASTNode*> &ast, lang::ASTNodeEnum* currEnumNode, u64 offset, std::string name);
    };

}