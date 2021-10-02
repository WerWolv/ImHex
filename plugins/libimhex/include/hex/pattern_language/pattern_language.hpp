#pragma once

#include <hex.hpp>

#include <bit>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <hex/pattern_language/log_console.hpp>

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

        std::optional<std::vector<PatternData*>> executeString(prv::Provider *provider, const std::string &string);
        std::optional<std::vector<PatternData*>> executeFile(prv::Provider *provider, const std::string &path);

        const std::vector<std::pair<LogConsole::Level, std::string>>& getConsoleLog();
        const std::optional<std::pair<u32, std::string>>& getError();

        u32 getCreatedPatternCount();
        u32 getMaximumPatternCount();

    private:
        Preprocessor *m_preprocessor;
        Lexer *m_lexer;
        Parser *m_parser;
        Validator *m_validator;
        Evaluator *m_evaluator;

        std::vector<ASTNode*> m_currAST;

        prv::Provider *m_provider = nullptr;
        std::endian m_defaultEndian = std::endian::native;

        std::optional<std::pair<u32, std::string>> m_currError;
    };

}