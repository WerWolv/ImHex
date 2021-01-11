#pragma once

#include <hex.hpp>

#include "providers/provider.hpp"
#include "helpers/utils.hpp"
#include "lang/pattern_data.hpp"
#include "ast_node.hpp"

#include <bit>
#include <string>
#include <unordered_map>
#include <vector>

namespace hex::lang {

    class Evaluator {
    public:
        enum ConsoleLogLevel {
            Debug,
            Info,
            Warning,
            Error
        };

        Evaluator(prv::Provider* &provider, std::endian defaultDataEndian);

        std::optional<std::vector<PatternData*>> evaluate(const std::vector<ASTNode*>& ast);
        const auto& getConsoleLog() { return this->m_consoleLog; }

    private:
        std::map<std::string, ASTNode*> m_types;
        prv::Provider* &m_provider;
        std::endian m_defaultDataEndian;
        u64 m_currOffset = 0;
        std::vector<std::endian> m_endianStack;
        std::vector<PatternData*> m_globalMembers;
        std::vector<std::vector<PatternData*>*> m_currMembers;

        std::vector<std::pair<ConsoleLogLevel, std::string>> m_consoleLog;

        using EvaluateError = std::string;

        void emmitDebugInfo(std::string_view message) {
            this->m_consoleLog.emplace_back(ConsoleLogLevel::Debug, "[-] " + std::string(message));
        }

        void emmitInfo(std::string_view message) {
            this->m_consoleLog.emplace_back(ConsoleLogLevel::Info, "[i] " + std::string(message));
        }

        void emmitWaring(std::string_view message) {
            this->m_consoleLog.emplace_back(ConsoleLogLevel::Warning, "[*] " + std::string(message));
        }

        [[noreturn]] static void throwEvaluateError(std::string_view message) {
            throw EvaluateError("[!] " + std::string(message));
        }

        [[nodiscard]] std::endian getCurrentEndian() const {
            return this->m_endianStack.back();
        }

        ASTNodeIntegerLiteral* evaluateScopeResolution(ASTNodeScopeResolution *node);
        ASTNodeIntegerLiteral* evaluateRValue(ASTNodeRValue *node);
        ASTNode* evaluateFunctionCall(ASTNodeFunctionCall *node);
        ASTNodeIntegerLiteral* evaluateOperator(ASTNodeIntegerLiteral *left, ASTNodeIntegerLiteral *right, Token::Operator op);
        ASTNodeIntegerLiteral* evaluateOperand(ASTNode *node);
        ASTNodeIntegerLiteral* evaluateTernaryExpression(ASTNodeTernaryExpression *node);
        ASTNodeIntegerLiteral* evaluateMathematicalExpression(ASTNodeNumericExpression *node);

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

        template<typename T>
        T* asType(ASTNode *param) {
            if (auto evaluatedParam = dynamic_cast<T*>(param); evaluatedParam != nullptr)
                return evaluatedParam;
            else
                throwEvaluateError("function got wrong type of parameter");
        }

        void registerBuiltinFunctions();

        #define BUILTIN_FUNCTION(name) ASTNodeIntegerLiteral* TOKEN_CONCAT(builtin_, name)(std::vector<ASTNode*> params)

        BUILTIN_FUNCTION(findSequence);
        BUILTIN_FUNCTION(readUnsigned);
        BUILTIN_FUNCTION(readSigned);

        BUILTIN_FUNCTION(assert);
        BUILTIN_FUNCTION(warnAssert);
        BUILTIN_FUNCTION(print);

        #undef BUILTIN_FUNCTION
    };

}