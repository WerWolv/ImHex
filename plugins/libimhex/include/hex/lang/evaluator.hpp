#pragma once

#include <hex.hpp>

#include <hex/providers/provider.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/lang/pattern_data.hpp>
#include <hex/lang/ast_node.hpp>

#include <bit>
#include <string>
#include <unordered_map>
#include <vector>

namespace hex::lang {

    class LogConsole {
    public:
        enum Level {
            Debug,
            Info,
            Warning,
            Error
        };

        const auto& getLog() { return this->m_consoleLog; }

        using EvaluateError = std::string;

        void log(Level level, std::string_view message) {
            switch (level) {
                default:
                case Level::Debug:   this->m_consoleLog.emplace_back(level, "[-] " + std::string(message)); break;
                case Level::Info:    this->m_consoleLog.emplace_back(level, "[i] " + std::string(message)); break;
                case Level::Warning: this->m_consoleLog.emplace_back(level, "[*] " + std::string(message)); break;
                case Level::Error:   this->m_consoleLog.emplace_back(level, "[!] " + std::string(message)); break;
            }
        }

        [[noreturn]] static void abortEvaluation(std::string_view message) {
            throw EvaluateError(message);
        }

    private:
        std::vector<std::pair<Level, std::string>> m_consoleLog;
    };

    class Evaluator {
    public:
        Evaluator(prv::Provider* &provider, std::endian defaultDataEndian);

        std::optional<std::vector<PatternData*>> evaluate(const std::vector<ASTNode*>& ast);

        auto& getConsole() { return this->m_console; }

    private:
        std::map<std::string, ASTNode*> m_types;
        prv::Provider* &m_provider;
        std::endian m_defaultDataEndian;
        u64 m_currOffset = 0;
        std::vector<std::endian> m_endianStack;
        std::vector<PatternData*> m_globalMembers;
        std::vector<std::vector<PatternData*>*> m_currMembers;
        LogConsole m_console;

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

        PatternData* patternFromName(const std::vector<std::string> &name);
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
                this->m_console.abortEvaluation("function got wrong type of parameter");
        }

        void registerBuiltinFunctions();

        #define BUILTIN_FUNCTION(name) ASTNodeIntegerLiteral* TOKEN_CONCAT(builtin_, name)(LogConsole &console, std::vector<ASTNode*> params)

        BUILTIN_FUNCTION(findSequence);
        BUILTIN_FUNCTION(readUnsigned);
        BUILTIN_FUNCTION(readSigned);

        BUILTIN_FUNCTION(assert);
        BUILTIN_FUNCTION(warnAssert);
        BUILTIN_FUNCTION(print);

        #undef BUILTIN_FUNCTION
    };

}