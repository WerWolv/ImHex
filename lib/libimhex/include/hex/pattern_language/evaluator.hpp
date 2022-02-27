#pragma once

#include <atomic>
#include <bit>
#include <map>
#include <optional>
#include <vector>

#include <hex/pattern_language/log_console.hpp>

#include <hex/api/content_registry.hpp>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/concepts.hpp>

namespace hex::prv {
    class Provider;
}

namespace hex::pl {

    enum class DangerousFunctionPermission
    {
        Ask,
        Deny,
        Allow
    };

    enum class ControlFlowStatement
    {
        None,
        Continue,
        Break,
        Return
    };

    class Pattern;
    class PatternCreationLimiter;
    class ASTNode;

    class Evaluator {
    public:
        Evaluator() = default;

        std::optional<std::vector<std::shared_ptr<Pattern>>> evaluate(const std::vector<std::shared_ptr<ASTNode>> &ast);

        [[nodiscard]] LogConsole &getConsole() {
            return this->m_console;
        }

        struct ParameterPack {
            std::string name;
            std::vector<Token::Literal> values;
        };

        struct Scope {
            Pattern *parent;
            std::vector<std::shared_ptr<Pattern>> *scope;
            std::optional<ParameterPack> parameterPack;
        };
        void pushScope(Pattern *parent, std::vector<std::shared_ptr<Pattern>> &scope) {
            if (this->m_scopes.size() > this->getEvaluationDepth())
                LogConsole::abortEvaluation(hex::format("evaluation depth exceeded set limit of {}", this->getEvaluationDepth()));

            this->handleAbort();

            this->m_scopes.push_back({ parent, &scope });
        }

        void popScope() {
            this->m_scopes.pop_back();
        }

        Scope &getScope(i32 index) {
            return this->m_scopes[this->m_scopes.size() - 1 + index];
        }

        const Scope &getScope(i32 index) const {
            return this->m_scopes[this->m_scopes.size() - 1 + index];
        }

        Scope &getGlobalScope() {
            return this->m_scopes.front();
        }

        const Scope &getGlobalScope() const {
            return this->m_scopes.front();
        }

        size_t getScopeCount() {
            return this->m_scopes.size();
        }

        bool isGlobalScope() {
            return this->m_scopes.size() == 1;
        }

        void setProvider(prv::Provider *provider) {
            this->m_provider = provider;
        }

        void setInVariables(const std::map<std::string, Token::Literal> &inVariables) {
            this->m_inVariables = inVariables;
        }

        [[nodiscard]] std::map<std::string, Token::Literal> getOutVariables() const {
            std::map<std::string, Token::Literal> result;

            for (const auto &[name, offset] : this->m_outVariables) {
                result.insert({ name, this->getStack()[offset] });
            }

            return result;
        }

        [[nodiscard]] prv::Provider *getProvider() const {
            return this->m_provider;
        }

        void setDefaultEndian(std::endian endian) {
            this->m_defaultEndian = endian;
        }

        [[nodiscard]] std::endian getDefaultEndian() const {
            return this->m_defaultEndian;
        }

        void setEvaluationDepth(u64 evalDepth) {
            this->m_evalDepth = evalDepth;
        }

        [[nodiscard]] u64 getEvaluationDepth() const {
            return this->m_evalDepth;
        }

        void setArrayLimit(u64 arrayLimit) {
            this->m_arrayLimit = arrayLimit;
        }

        [[nodiscard]] u64 getArrayLimit() const {
            return this->m_arrayLimit;
        }

        void setPatternLimit(u64 limit) {
            this->m_patternLimit = limit;
        }

        [[nodiscard]] u64 getPatternLimit() {
            return this->m_patternLimit;
        }

        [[nodiscard]] u64 getPatternCount() {
            return this->m_currPatternCount;
        }

        void setLoopLimit(u64 limit) {
            this->m_loopLimit = limit;
        }

        [[nodiscard]] u64 getLoopLimit() {
            return this->m_loopLimit;
        }

        u64 &dataOffset() { return this->m_currOffset; }

        bool addCustomFunction(const std::string &name, u32 numParams, const ContentRegistry::PatternLanguage::Callback &function) {
            const auto [iter, inserted] = this->m_customFunctions.insert({
                name, {numParams, function}
            });

            return inserted;
        }

        [[nodiscard]] const std::map<std::string, ContentRegistry::PatternLanguage::Function> &getCustomFunctions() const {
            return this->m_customFunctions;
        }

        [[nodiscard]] std::vector<Token::Literal> &getStack() {
            return this->m_stack;
        }

        [[nodiscard]] const std::vector<Token::Literal> &getStack() const {
            return this->m_stack;
        }

        void createParameterPack(const std::string &name, const std::vector<Token::Literal> &values);
        void createVariable(const std::string &name, ASTNode *type, const std::optional<Token::Literal> &value = std::nullopt, bool outVariable = false);
        void setVariable(const std::string &name, const Token::Literal &value);

        void abort() {
            this->m_aborted = true;
        }

        void handleAbort() {
            if (this->m_aborted)
                LogConsole::abortEvaluation("evaluation aborted by user");
        }

        [[nodiscard]] std::optional<Token::Literal> getEnvVariable(const std::string &name) const {
            if (this->m_envVariables.contains(name)) {
                auto value = this->m_envVariables.at(name);
                return this->m_envVariables.at(name);
            } else
                return std::nullopt;
        }

        void setEnvVariable(const std::string &name, const Token::Literal &value) {
            this->m_envVariables[name] = value;
        }

        [[nodiscard]] bool hasDangerousFunctionBeenCalled() const {
            return this->m_dangerousFunctionCalled;
        }

        void dangerousFunctionCalled() {
            this->m_dangerousFunctionCalled = true;
        }

        void allowDangerousFunctions(bool allow) {
            this->m_allowDangerousFunctions = allow ? DangerousFunctionPermission::Allow : DangerousFunctionPermission::Deny;
            this->m_dangerousFunctionCalled = false;
        }

        [[nodiscard]] DangerousFunctionPermission getDangerousFunctionPermission() const {
            return this->m_allowDangerousFunctions;
        }

        void setCurrentControlFlowStatement(ControlFlowStatement statement) {
            this->m_currControlFlowStatement = statement;
        }

        [[nodiscard]] ControlFlowStatement getCurrentControlFlowStatement() const {
            return this->m_currControlFlowStatement;
        }

        [[nodiscard]] const std::optional<Token::Literal> &getMainResult() {
            return this->m_mainResult;
        }

    private:
        void patternCreated();
        void patternDestroyed();

    private:
        u64 m_currOffset;
        prv::Provider *m_provider = nullptr;
        LogConsole m_console;

        std::endian m_defaultEndian = std::endian::native;
        u64 m_evalDepth;
        u64 m_arrayLimit;
        u64 m_patternLimit;
        u64 m_loopLimit;

        u64 m_currPatternCount;

        std::atomic<bool> m_aborted;

        std::vector<Scope> m_scopes;
        std::map<std::string, ContentRegistry::PatternLanguage::Function> m_customFunctions;
        std::vector<std::unique_ptr<ASTNode>> m_customFunctionDefinitions;
        std::vector<Token::Literal> m_stack;

        std::optional<Token::Literal> m_mainResult;

        std::map<std::string, Token::Literal> m_envVariables;
        std::map<std::string, Token::Literal> m_inVariables;
        std::map<std::string, size_t> m_outVariables;

        std::atomic<bool> m_dangerousFunctionCalled                        = false;
        std::atomic<DangerousFunctionPermission> m_allowDangerousFunctions = DangerousFunctionPermission::Ask;
        ControlFlowStatement m_currControlFlowStatement;

        friend class PatternCreationLimiter;
    };

}