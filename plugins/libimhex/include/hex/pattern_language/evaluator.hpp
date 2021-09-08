#pragma once

#include <hex.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/pattern_language/ast_node.hpp>
#include <hex/pattern_language/log_console.hpp>

#include <bit>
#include <string>
#include <map>
#include <vector>

namespace hex::prv { class Provider; }

namespace hex::pl {

    class PatternData;

    class Evaluator {
    public:
        Evaluator() = default;

        std::optional<std::vector<PatternData*>> evaluate(const std::vector<ASTNode*>& ast);

        LogConsole& getConsole() { return this->m_console; }

        void setDefaultEndian(std::endian endian) { this->m_defaultDataEndian = endian; }
        void setRecursionLimit(u32 limit) { this->m_recursionLimit = limit; }
        void setProvider(prv::Provider *provider) { this->m_provider = provider; }
        [[nodiscard]] std::endian getCurrentEndian() const { return this->m_endianStack.back(); }

        PatternData* patternFromName(const ASTNodeRValue::Path &name);

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
        std::vector<std::vector<PatternData*>*> m_localVariables;
        std::vector<PatternData*> m_currMemberScope;
        std::vector<u8> m_localStack;
        std::map<std::string, ContentRegistry::PatternLanguageFunctions::Function> m_definedFunctions;
        LogConsole m_console;

        u32 m_recursionLimit;
        u32 m_currRecursionDepth;

        void createLocalVariable(const std::string &varName, PatternData *pattern);
        void setLocalVariableValue(const std::string &varName, const void *value, size_t size);

        ASTNodeIntegerLiteral* evaluateScopeResolution(ASTNodeScopeResolution *node);
        ASTNodeIntegerLiteral* evaluateRValue(ASTNodeRValue *node);
        ASTNode* evaluateFunctionCall(ASTNodeFunctionCall *node);
        ASTNodeIntegerLiteral* evaluateTypeOperator(ASTNodeTypeOperator *typeOperatorNode);
        ASTNodeIntegerLiteral* evaluateOperator(ASTNodeIntegerLiteral *left, ASTNodeIntegerLiteral *right, Token::Operator op);
        ASTNodeIntegerLiteral* evaluateOperand(ASTNode *node);
        ASTNodeIntegerLiteral* evaluateTernaryExpression(ASTNodeTernaryExpression *node);
        ASTNodeIntegerLiteral* evaluateMathematicalExpression(ASTNodeNumericExpression *node);
        void evaluateFunctionDefinition(ASTNodeFunctionDefinition *node);
        std::optional<ASTNode*> evaluateFunctionBody(const std::vector<ASTNode*> &body);

        PatternData* findPattern(std::vector<PatternData*> currMembers, const ASTNodeRValue::Path &path);
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
        PatternData* evaluateStaticArray(ASTNodeArrayVariableDecl *node);
        PatternData* evaluateDynamicArray(ASTNodeArrayVariableDecl *node);
        PatternData* evaluatePointer(ASTNodePointerVariableDecl *node);
    };

}