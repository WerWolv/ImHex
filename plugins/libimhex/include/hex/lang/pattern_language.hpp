#pragma once

#include <hex.hpp>

#include <bit>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <hex/lang/pattern_data.hpp>
#include <hex/lang/log_console.hpp>

namespace hex::prv { class Provider; }

namespace hex::lang {

    class Preprocessor;
    class Lexer;
    class Parser;
    class Validator;
    class Evaluator;

    class PatternLanguage {
    public:
        PatternLanguage();
        ~PatternLanguage();

        std::optional<std::vector<PatternData*>> executeString(prv::Provider *provider, std::string_view string);
        std::optional<std::vector<PatternData*>> executeFile(prv::Provider *provider, std::string_view path);

        const std::vector<std::pair<LogConsole::Level, std::string>>& getConsoleLog();
        const std::optional<std::pair<u32, std::string>>& getError();

    private:
        Preprocessor *m_preprocessor;
        Lexer *m_lexer;
        Parser *m_parser;
        Validator *m_validator;
        Evaluator *m_evaluator;

        prv::Provider *m_provider = nullptr;
        std::endian m_defaultEndian = std::endian::native;
        u32 m_recursionLimit = 32;

        std::optional<std::pair<u32, std::string>> m_currError;
    };

}