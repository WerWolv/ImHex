#pragma once

#include <hex.hpp>

#include "token.hpp"

#include <string>
#include <vector>

namespace hex::lang {

    class Lexer {
    public:
        Lexer();

        std::pair<Result, std::vector<Token>> lex(const std::string& code);
    };

}