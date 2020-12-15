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
        Evaluator(prv::Provider* &provider, std::endian defaultDataEndian);

        std::optional<std::vector<PatternData*>> evaluate(const std::vector<ASTNode*>& ast);

        const std::pair<u32, std::string>& getError() { return this->m_error; }

    private:
        std::unordered_map<std::string, ASTNode*> m_types;
        prv::Provider* &m_provider;
        std::endian m_defaultDataEndian;
        u64 m_currOffset = 0;
        std::optional<std::endian> m_currEndian;

        std::pair<u32, std::string> m_error;

        using EvaluateError = std::pair<u32, std::string>;

        [[noreturn]] static void throwEvaluateError(std::string_view error, u32 lineNumber) {
            throw EvaluateError(lineNumber, "Evaluator: " + std::string(error));
        }

        [[nodiscard]] std::endian getCurrentEndian() const {
            return this->m_currEndian.value_or(this->m_defaultDataEndian);
        }

        PatternData* evaluateBuiltinType(ASTNodeBuiltinType *node);
        PatternData* evaluateStruct(ASTNodeStruct *node);
        PatternData* evaluateUnion(ASTNodeUnion *node);
        PatternData* evaluateEnum(ASTNodeEnum *node);
        PatternData* evaluateBitfield(ASTNodeBitfield *node);
        PatternData* evaluateType(ASTNodeTypeDecl *node);
        PatternData* evaluateVariable(ASTNodeVariableDecl *node);
        PatternData* evaluateArray(ASTNodeArrayVariableDecl *node);

    };

}