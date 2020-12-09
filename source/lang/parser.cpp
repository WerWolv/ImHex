#include "lang/parser.hpp"

#include "helpers/utils.hpp"
#include "lang/token.hpp"

#include <optional>

namespace hex::lang {

    Parser::Parser() {

    }

    std::vector<ASTNode*> Parser::parseStatement() {
        std::vector<ASTNode*> program;
        SCOPE_EXIT( for (auto &node : program) delete node; );

        if (tryConsume(KEYWORD_STRUCT, IDENTIFIER, SEPARATOR_CURLYBRACKETOPEN)) {
            printf("Struct\n");
        } else if (tryConsume(KEYWORD_UNION, IDENTIFIER, SEPARATOR_CURLYBRACKETOPEN)) {
            printf("Union\n");
        } else if (tryConsume(KEYWORD_ENUM, IDENTIFIER, SEPARATOR_CURLYBRACKETOPEN)) {
            printf("Enum\n");
        } else throwParseError("Invalid sequence");

        return program;
    }

    std::pair<Result, std::vector<ASTNode*>> Parser::parse(const std::vector<Token> &tokens) {
        this->m_curr = tokens.begin();

        try {
            auto program = parseTillToken(SEPARATOR_ENDOFPROGRAM);

            if (program.empty() || this->m_curr != tokens.end())
                throwParseError("Program is empty!");

            return { ResultSuccess, program };
        } catch (ParseError &e) {
            this->m_error = e;
        }

        return { ResultParseError, { } };
    }

}