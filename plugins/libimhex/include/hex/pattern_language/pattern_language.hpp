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

namespace hex::prv { class Provider; }

namespace hex::pl {

    class Preprocessor;
    class Lexer;
    class Parser;
    class Validator;
    class Evaluator;
    class PatternData;

    class ASTNode;

    class PatternLanguage {
    public:
        PatternLanguage();
        ~PatternLanguage();

        [[nodiscard]]
        std::optional<std::vector<ASTNode*>> parseString(const std::string &code);
        [[nodiscard]]
        std::optional<std::vector<PatternData*>> executeString(prv::Provider *provider, const std::string &string, const std::map<std::string, Token::Literal> &envVars = { }, const std::map<std::string, Token::Literal> &inVariables = { });
        [[nodiscard]]
        std::optional<std::vector<PatternData*>> executeFile(prv::Provider *provider, const fs::path &path, const std::map<std::string, Token::Literal> &envVars = { }, const std::map<std::string, Token::Literal> &inVariables = { });
        [[nodiscard]]
        const std::vector<ASTNode*>& getCurrentAST() const;

        void abort();

        [[nodiscard]]
        const std::vector<std::pair<LogConsole::Level, std::string>>& getConsoleLog();
        [[nodiscard]]
        const std::optional<std::pair<u32, std::string>>& getError();
        [[nodiscard]]
        std::map<std::string, Token::Literal> getOutVariables() const;

        [[nodiscard]]
        u32 getCreatedPatternCount();
        [[nodiscard]]
        u32 getMaximumPatternCount();

        [[nodiscard]]
        bool hasDangerousFunctionBeenCalled() const;
        void allowDangerousFunctions(bool allow);

    private:
        Preprocessor *m_preprocessor;
        Lexer *m_lexer;
        Parser *m_parser;
        Validator *m_validator;
        Evaluator *m_evaluator;

        std::vector<ASTNode*> m_currAST;

        std::optional<std::pair<u32, std::string>> m_currError;
    };

}