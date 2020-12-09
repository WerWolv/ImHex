#include "lang/parser.hpp"

#include "helpers/utils.hpp"
#include "lang/token.hpp"

#include <optional>
#include <variant>

#define MATCHES(x) begin() && x

namespace hex::lang {

    Parser::Parser() {

    }

    ASTNode* Parser::parseUsingDeclaration() {
        if (this->m_curr[-2].type == Token::Type::Identifier)
            return new ASTNodeTypeDecl(getLineNumber(-5), Token::ValueType::CustomType, getValue<std::string>(-4), getValue<std::string>(-2));
        else
            return new ASTNodeTypeDecl(getLineNumber(-5), getValue<Token::ValueType>(-2), getValue<std::string>(-4));
    }

    ASTNode* Parser::parseStatement() {
        if (MATCHES(sequence(KEYWORD_USING, IDENTIFIER, OPERATOR_ASSIGNMENT) && variant(IDENTIFIER, VALUETYPE_ANY) && sequence(SEPARATOR_ENDOFEXPRESSION))) {
            return parseUsingDeclaration();
        } else throwParseError("Invalid sequence", 0);

        return nullptr;
    }

    std::pair<Result, std::vector<ASTNode*>> Parser::parse(const std::vector<Token> &tokens) {
        this->m_curr = tokens.begin();

        try {
            auto program = parseTillToken(SEPARATOR_ENDOFPROGRAM);

            if (program.empty() || this->m_curr != tokens.end())
                throwParseError("Program is empty!", 0);

            return { ResultSuccess, program };
        } catch (ParseError &e) {
            this->m_error = e;
        }

        return { ResultParseError, { } };
    }

}