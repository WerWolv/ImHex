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


        struct Function {
            constexpr static u32 UnlimitedParameters   = 0xFFFF'FFFF;
            constexpr static u32 MoreParametersThan    = 0x8000'0000;
            constexpr static u32 LessParametersThan    = 0x4000'0000;
            constexpr static u32 NoParameters          = 0x0000'0000;

            u32 parameterCount;
            std::function<ASTNodeIntegerLiteral*(std::vector<ASTNodeIntegerLiteral*>)> func;
        };

    private:
        std::map<std::string, ASTNode*> m_types;
        prv::Provider* &m_provider;
        std::endian m_defaultDataEndian;
        u64 m_currOffset = 0;
        std::vector<std::endian> m_endianStack;
        std::vector<PatternData*> m_globalMembers;
        std::vector<std::vector<PatternData*>*> m_currMembers;
        std::map<std::string, Function> m_functions;

        std::pair<u32, std::string> m_error;

        using EvaluateError = std::pair<u32, std::string>;

        [[noreturn]] static void throwEvaluateError(std::string_view error, u32 lineNumber) {
            throw EvaluateError(lineNumber, "Evaluator: " + std::string(error));
        }

        [[nodiscard]] std::endian getCurrentEndian() const {
            return this->m_endianStack.back();
        }

        void addFunction(std::string_view name, u32 parameterCount, std::function<ASTNodeIntegerLiteral*(std::vector<ASTNodeIntegerLiteral*>)> func) {
            if (this->m_functions.contains(name.data()))
                throwEvaluateError(hex::format("redefinition of function '%s'", name.data()), 1);

            this->m_functions[name.data()] = { parameterCount, func };
        }

        ASTNodeIntegerLiteral* evaluateScopeResolution(ASTNodeScopeResolution *node);
        ASTNodeIntegerLiteral* evaluateRValue(ASTNodeRValue *node);
        ASTNodeIntegerLiteral* evaluateFunctionCall(ASTNodeFunctionCall *node);
        ASTNodeIntegerLiteral* evaluateOperator(ASTNodeIntegerLiteral *left, ASTNodeIntegerLiteral *right, Token::Operator op);
        ASTNodeIntegerLiteral* evaluateOperand(ASTNode *node);
        ASTNodeIntegerLiteral* evaluateTernaryExpression(ASTNodeTernaryExpression *node);
        ASTNodeIntegerLiteral* evaluateMathematicalExpression(ASTNodeNumericExpression *node);

        PatternData* evaluateBuiltinType(ASTNodeBuiltinType *node);
        std::vector<PatternData*> evaluateMember(ASTNode *node);
        PatternData* evaluateStruct(ASTNodeStruct *node);
        PatternData* evaluateUnion(ASTNodeUnion *node);
        PatternData* evaluateEnum(ASTNodeEnum *node);
        PatternData* evaluateBitfield(ASTNodeBitfield *node);
        PatternData* evaluateType(ASTNodeTypeDecl *node);
        PatternData* evaluateVariable(ASTNodeVariableDecl *node);
        PatternData* evaluateArray(ASTNodeArrayVariableDecl *node);
        PatternData* evaluatePointer(ASTNodePointerVariableDecl *node);


        #define BUILTIN_FUNCTION(name) ASTNodeIntegerLiteral* name(std::vector<ASTNodeIntegerLiteral*> params)

        BUILTIN_FUNCTION(findSequence);
        BUILTIN_FUNCTION(readUnsigned);
        BUILTIN_FUNCTION(readSigned);

        #undef BUILTIN_FUNCTION
    };

}