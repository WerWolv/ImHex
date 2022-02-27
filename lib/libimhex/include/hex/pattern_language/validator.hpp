#pragma once

#include <hex.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <hex/pattern_language/error.hpp>

namespace hex::pl {

    class ASTNode;

    class Validator {
    public:
        Validator() = default;

        bool validate(const std::vector<std::shared_ptr<ASTNode>> &ast);

        const std::optional<PatternLanguageError> &getError() { return this->m_error; }

    private:
        std::optional<PatternLanguageError> m_error;

        [[noreturn]] static void throwValidatorError(const std::string &error, u32 lineNumber) {
            throw PatternLanguageError(lineNumber, "Validator: " + error);
        }
    };

}