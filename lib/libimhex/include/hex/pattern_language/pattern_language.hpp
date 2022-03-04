#pragma once

#include <hex.hpp>

#include <bit>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <hex/pattern_language/log_console.hpp>
#include <hex/pattern_language/token.hpp>
#include <hex/pattern_language/patterns/pattern.hpp>

namespace hex::prv {
    class Provider;
}

namespace hex::pl {

    class Preprocessor;
    class Lexer;
    class Parser;
    class Validator;
    class Evaluator;
    class Pattern;

    class ASTNode;

    class PatternLanguage {
    public:
        PatternLanguage();
        ~PatternLanguage();

        [[nodiscard]] std::optional<std::vector<std::shared_ptr<ASTNode>>> parseString(const std::string &code);
        [[nodiscard]] bool executeString(prv::Provider *provider, const std::string &string, const std::map<std::string, Token::Literal> &envVars = {}, const std::map<std::string, Token::Literal> &inVariables = {}, bool checkResult = true);
        [[nodiscard]] bool executeFile(prv::Provider *provider, const std::fs::path &path, const std::map<std::string, Token::Literal> &envVars = {}, const std::map<std::string, Token::Literal> &inVariables = {});
        [[nodiscard]] std::pair<bool, std::optional<Token::Literal>> executeFunction(prv::Provider *provider, const std::string &code);
        [[nodiscard]] const std::vector<std::shared_ptr<ASTNode>> &getCurrentAST() const;

        void abort();

        [[nodiscard]] const std::vector<std::pair<LogConsole::Level, std::string>> &getConsoleLog();
        [[nodiscard]] const std::optional<PatternLanguageError> &getError();
        [[nodiscard]] std::map<std::string, Token::Literal> getOutVariables() const;

        [[nodiscard]] u32 getCreatedPatternCount();
        [[nodiscard]] u32 getMaximumPatternCount();

        [[nodiscard]] bool hasDangerousFunctionBeenCalled() const;
        void allowDangerousFunctions(bool allow);

        [[nodiscard]] const std::vector<std::shared_ptr<Pattern>> &getPatterns() {
            const static std::vector<std::shared_ptr<Pattern>> empty;

            if (isRunning()) return empty;
            else return this->m_patterns;
        }

        void reset();
        [[nodiscard]] bool isRunning() const { return this->m_running; }

    private:
        Preprocessor *m_preprocessor;
        Lexer *m_lexer;
        Parser *m_parser;
        Validator *m_validator;
        Evaluator *m_evaluator;

        std::vector<std::shared_ptr<ASTNode>> m_currAST;

        std::optional<PatternLanguageError> m_currError;

        std::vector<std::shared_ptr<Pattern>> m_patterns;

        bool m_running = false;
    };

}