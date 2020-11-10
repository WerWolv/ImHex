#pragma once

#include <hex.hpp>
#include "token.hpp"
#include "ast_node.hpp"

#include <vector>

namespace hex::lang {

    class Parser {
    public:
        Parser();

        std::pair<Result, std::vector<ASTNode*>> parse(const std::vector<Token> &tokens);

    };

}