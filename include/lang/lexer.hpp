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
    };

}