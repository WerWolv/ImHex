#pragma once

#include <hex.hpp>

#include "providers/provider.hpp"
#include "lang/pattern_data.hpp"
#include "ast_node.hpp"

#include <bit>
#include <string>
#include <unordered_map>
#include <vector>

namespace hex::lang {

    class Evaluator {
    public:
        Evaluator(prv::Provider* &provider, std::endian defaultDataEndianess);

        std::pair<Result, std::vector<PatternData*>> evaluate(const std::vector<ASTNode*>& ast);

        const std::pair<u32, std::string>& getError() { return this->m_error; }

    private:
        std::unordered_map<std::string, ASTNode*> m_types;
        prv::Provider* &m_provider;
        std::endian m_defaultDataEndianess;

        std::pair<u32, std::string> m_error;

        std::pair<PatternData*, size_t> createStructPattern(ASTNodeVariableDecl *varDeclNode, u64 offset);
        std::pair<PatternData*, size_t> createUnionPattern(ASTNodeVariableDecl *varDeclNode, u64 offset);
        std::pair<PatternData*, size_t> createEnumPattern(ASTNodeVariableDecl *varDeclNode, u64 offset);
        std::pair<PatternData*, size_t> createBitfieldPattern(ASTNodeVariableDecl *varDeclNode, u64 offset);
        std::pair<PatternData*, size_t> createArrayPattern(ASTNodeVariableDecl *varDeclNode, u64 offset);
        std::pair<PatternData*, size_t> createStringPattern(ASTNodeVariableDecl *varDeclNode, u64 offset);
        std::pair<PatternData*, size_t> createCustomTypePattern(ASTNodeVariableDecl *varDeclNode, u64 offset);
        std::pair<PatternData*, size_t> createBuiltInTypePattern(ASTNodeVariableDecl *varDeclNode, u64 offset);

    };

}