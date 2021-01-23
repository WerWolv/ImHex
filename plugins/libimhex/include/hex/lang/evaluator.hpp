#pragma once

#include <hex.hpp>

#include <hex/providers/provider.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/lang/pattern_data.hpp>
#include <hex/lang/ast_node.hpp>
#include <hex/lang/log_console.hpp>

#include <bit>
#include <string>
#include <unordered_map>
#include <vector>

namespace hex::lang {

    class Evaluator {
    public:
        Evaluator() = default;

        std::optional<std::vector<PatternData*>> evaluate(const std::vector<ASTNode*>& ast);

        LogConsole& getConsole() { return this->m_console; }

        void setDefaultEndian(std::endian endian) { this->m_defaultDataEndian = endian; }
        void setProvider(prv::Provider *provider) { this->m_provider = provider; }
        [[nodiscard]] std::endian getCurrentEndian() const { return this->m_endianStack.back(); }

        PatternData* patternFromName(const std::vector<std::string> &name);

        template<typename T>
        T* asType(ASTNode *param) {
            if (auto evaluatedParam = dynamic_cast<T*>(param); evaluatedParam != nullptr)
                return evaluatedParam;
            else
                this->getConsole().abortEvaluation("function got wrong type of parameter");
        }

    private:
        std::map<std::string, ASTNode*> m_types;
        prv::Provider* m_provider = nullptr;
        std::endian m_defaultDataEndian = std::endian::native;
        u64 m_currOffset = 0;
        std::vector<std::endian> m_endianStack;
        std::vector<PatternData*> m_globalMembers;
        std::vector<std::vector<PatternData*>*> m_currMembers;
        LogConsole m_console;



        ASTNodeIntegerLiteral* evaluateScopeResolution(ASTNodeScopeResolution *node);
        ASTNodeIntegerLiteral* evaluateRValue(ASTNodeRValue *node);
        ASTNode* evaluateFunctionCall(ASTNodeFunctionCall *node);
        ASTNodeIntegerLiteral* evaluateOperator(ASTNodeIntegerLiteral *left, ASTNodeIntegerLiteral *right, Token::Operator op);
        ASTNodeIntegerLiteral* evaluateOperand(ASTNode *node);
        ASTNodeIntegerLiteral* evaluateTernaryExpression(ASTNodeTernaryExpression *node);
        ASTNodeIntegerLiteral* evaluateMathematicalExpression(ASTNodeNumericExpression *node);

        PatternData* evaluateAttributes(ASTNode *currNode, PatternData *currPattern);
        PatternData* evaluateBuiltinType(ASTNodeBuiltinType *node);
        void evaluateMember(ASTNode *node, std::vector<PatternData*> &currMembers, bool increaseOffset);
        PatternData* evaluateStruct(ASTNodeStruct *node);
        PatternData* evaluateUnion(ASTNodeUnion *node);
        PatternData* evaluateEnum(ASTNodeEnum *node);
        PatternData* evaluateBitfield(ASTNodeBitfield *node);
        PatternData* evaluateType(ASTNodeTypeDecl *node);
        PatternData* evaluateVariable(ASTNodeVariableDecl *node);
        PatternData* evaluateArray(ASTNodeArrayVariableDecl *node);
        PatternData* evaluatePointer(ASTNodePointerVariableDecl *node);
    };

}