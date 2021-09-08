#pragma once

#include <hex.hpp>

#include <hex/pattern_language/token.hpp>

#include <optional>
#include <string>
#include <vector>

namespace hex::pl {

    class Lexer {
    public:
        using LexerError = std::pair<u32, std::string>;

        Lexer() = default;

        std::optional<std::vector<Token>> lex(const std::string& code);
        const LexerError& getError() { return this->m_error; }

    private:
        LexerError m_error;

        [[noreturn]] void throwLexerError(const std::string &error, u32 lineNumber) const {
            throw LexerError(lineNumber, "Lexer: " + error);
        }
    };

}