#pragma once

#include <hex.hpp>

#include "token.hpp"
#include "ast_node.hpp"

#include <string>
#include <vector>

namespace hex::lang {

    class Validator {
    public:
        Validator();

        bool validate(const std::vector<ASTNode*>& ast);
    };

}