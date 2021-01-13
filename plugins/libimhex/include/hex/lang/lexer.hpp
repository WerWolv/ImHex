#pragma once

#include <hex.hpp>

#include "token.hpp"

#include <optional>
#include <string>
#include <vector>

namespace hex::lang {

    class Lexer {
    public:
        using LexerError = std::pair<u32, std::string>;

        Lexer();

        std::optional<std::vector<Token>> lex(const std::string& code);
        const LexerError& getError() { return this->m_error; }

    private:
        LexerError m_error;

        [[noreturn]] void throwLexerError(std::string_view error, u32 lineNumber) const {
            throw LexerError(lineNumber, "Lexer: " + std::string(error));
        }
    };

}