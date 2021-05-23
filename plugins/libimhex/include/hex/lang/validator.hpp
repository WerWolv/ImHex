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

        const std::pair<u32, std::string>& getError() { return this->m_error; }

    private:
        std::pair<u32, std::string> m_error;

        using ValidatorError = std::pair<u32, std::string>;

        [[noreturn]] void throwValidateError(std::string_view error, u32 lineNumber) const {
            throw ValidatorError(lineNumber, error);
        }

    };

}