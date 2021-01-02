#pragma once

#include <hex.hpp>

#include "token.hpp"

#include <optional>
#include <string>
#include <vector>

namespace hex::lang {

    class Lexer {
    public:
        Lexer();

        std::optional<std::vector<Token>> lex(const std::string& code);

        const std::pair<u32, std::string>& getError() { return this->m_error; }

    private:
        std::pair<u32, std::string> m_error;

        using LexerError = std::pair<u32, std::string>;

        [[noreturn]] void throwLexerError(std::string_view error, u32 lineNumber) const {
            throw LexerError(lineNumber, "Lexer: " + std::string(error));
        }
    };

}