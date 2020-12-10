#include "lang/parser.hpp"

#include <optional>
#include <variant>

#define MATCHES(x) begin() && x

namespace hex::lang {

    Parser::Parser() {

    }

    ASTNode* Parser::parseType(s32 startIndex) {
        std::optional<std::endian> endian;

        if (matchesOptional(KEYWORD_LE, 0))
            endian = std::endian::little;
        else if (matchesOptional(KEYWORD_BE, 0))
            endian = std::endian::big;

        if (getType(startIndex) == Token::Type::Identifier) { // Custom type
            if (!this->m_types.contains(getValue<std::string>(startIndex)))
                return nullptr;

            return new ASTNodeTypeDecl("[Temporary Type]", this->m_types[getValue<std::string>(startIndex)], endian.value_or(std::endian::native));
        }
        else { // Builtin type
            return new ASTNodeTypeDecl("[Temporary Type]", new ASTNodeBuiltinType(getValue<Token::ValueType>(startIndex)), endian.value_or(std::endian::native));
        }
    }

    ASTNode* Parser::parseUsingDeclaration() {
        ASTNodeTypeDecl* temporaryType = dynamic_cast<ASTNodeTypeDecl *>(parseType(-2));
        if (temporaryType == nullptr) return nullptr;
        SCOPE_EXIT( delete temporaryType; );

        if (matchesOptional(KEYWORD_BE) || matchesOptional(KEYWORD_LE))
            return new ASTNodeTypeDecl(getValue<std::string>(-5), temporaryType->getType(), temporaryType->getEndian());
        else
            return new ASTNodeTypeDecl(getValue<std::string>(-4), temporaryType->getType(), temporaryType->getEndian());
    }

    ASTNode* Parser::parseVariablePlacement() {
        ASTNodeTypeDecl* type = dynamic_cast<ASTNodeTypeDecl *>(parseType(-5));
        if (type == nullptr) return nullptr;
        SCOPE_EXIT( delete type; );

        return new ASTNodeVariableDecl(getValue<std::string>(-4), new ASTNodeTypeDecl(std::move(*type)));
    }

    ASTNode* Parser::parseStatement() {
        if (MATCHES(sequence(KEYWORD_USING, IDENTIFIER, OPERATOR_ASSIGNMENT) && (optional(KEYWORD_BE), optional(KEYWORD_LE)) && variant(IDENTIFIER, VALUETYPE_ANY) && sequence(SEPARATOR_ENDOFEXPRESSION)))
            return parseUsingDeclaration();
        else if (MATCHES((optional(KEYWORD_BE), optional(KEYWORD_LE)) && variant(IDENTIFIER, VALUETYPE_ANY) && sequence(IDENTIFIER, OPERATOR_AT, INTEGER, SEPARATOR_ENDOFEXPRESSION)))
            return parseVariablePlacement();
        else throwParseError("Invalid sequence", 0);

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