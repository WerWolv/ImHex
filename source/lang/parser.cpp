#include "lang/parser.hpp"

#include <optional>
#include <variant>

#define MATCHES(x) (begin() && x)

// Definition syntax:
// [A]          : Either A or no token
// [A|B]        : Either A, B or no token
// <A|B>        : Either A or B
// <A...>       : One or more of A
// A B C        : Sequence of tokens A then B then C
// (parseXXXX)  : Parsing handled by other function
namespace hex::lang {

    Parser::Parser() {

    }

    // [be|le] <Identifier|u8|u16|u32|u64|u128|s8|s16|s32|s64|s128|float|double>
    ASTNode* Parser::parseType(s32 startIndex) {
        std::optional<std::endian> endian;

        if (matchesOptional(KEYWORD_LE, 0))
            endian = std::endian::little;
        else if (matchesOptional(KEYWORD_BE, 0))
            endian = std::endian::big;

        if (getType(startIndex) == Token::Type::Identifier) { // Custom type
            if (!this->m_types.contains(getValue<std::string>(startIndex)))
                throwParseError("Failed to parse type");

            auto type = dynamic_cast<ASTNodeTypeDecl*>(this->m_types[getValue<std::string>(startIndex + 1)]);

            return new ASTNodeTypeDecl("[Temporary Type]", this->m_types[getValue<std::string>(startIndex)], endian);
        }
        else { // Builtin type
            return new ASTNodeTypeDecl("[Temporary Type]", new ASTNodeBuiltinType(getValue<Token::ValueType>(startIndex)), endian);
        }
    }

    // using Identifier = (parseType);
    ASTNode* Parser::parseUsingDeclaration() {
        ASTNodeTypeDecl *temporaryType = static_cast<ASTNodeTypeDecl *>(parseType(-2));
        SCOPE_EXIT( delete temporaryType; );

        if (auto *typeDecl = dynamic_cast<ASTNodeTypeDecl*>(temporaryType->getType()); typeDecl != nullptr) {
            if (matchesOptional(KEYWORD_BE) || matchesOptional(KEYWORD_LE))
                return new ASTNodeTypeDecl(getValue<std::string>(-5), new ASTNodeTypeDecl(*typeDecl), temporaryType->getEndian());
            else
                return new ASTNodeTypeDecl(getValue<std::string>(-4), new ASTNodeTypeDecl(*typeDecl), temporaryType->getEndian());
        } else if (auto *builtInType = dynamic_cast<ASTNodeBuiltinType*>(temporaryType->getType()); builtInType != nullptr) {
            if (matchesOptional(KEYWORD_BE) || matchesOptional(KEYWORD_LE))
                return new ASTNodeTypeDecl(getValue<std::string>(-5), new ASTNodeBuiltinType(*builtInType), temporaryType->getEndian());
            else
                return new ASTNodeTypeDecl(getValue<std::string>(-4), new ASTNodeBuiltinType(*builtInType), temporaryType->getEndian());
        }

        throwParseError("Invalid type used in using declaration", -1);
        return nullptr;
    }

    // (parseType) Identifier;
    ASTNode* Parser::parseMemberVariable() {
        auto type = parseType(-3);
        if (type == nullptr) return nullptr;

        return new ASTNodeVariableDecl(getValue<std::string>(-2), type);
    }

    // struct Identifier { <(parseMember)...> };
    ASTNode* Parser::parseStruct() {
        const auto structNode = new ASTNodeStruct();
        const auto &typeName = getValue<std::string>(-2);
        ScopeExit structGuard([&]{ delete structNode; });

        while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE, SEPARATOR_ENDOFEXPRESSION))) {
            if (MATCHES((optional(KEYWORD_BE), optional(KEYWORD_LE)) && variant(IDENTIFIER, VALUETYPE_ANY) && sequence(IDENTIFIER, SEPARATOR_ENDOFEXPRESSION)))
                structNode->addMember(parseMemberVariable());
            else if (MATCHES(sequence(SEPARATOR_ENDOFPROGRAM)))
                throwParseError("Unexpected end of program", -2);
            else
                throwParseError("Invalid struct member", 0);
        }

        structGuard.release();

        return new ASTNodeTypeDecl(typeName, structNode);
    }

    // union Identifier { <(parseMember)...> };
    ASTNode* Parser::parseUnion() {
        const auto unionNode = new ASTNodeUnion();
        const auto &typeName = getValue<std::string>(-2);
        ScopeExit unionGuard([&]{ delete unionNode; });

        while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE, SEPARATOR_ENDOFEXPRESSION))) {
            if (MATCHES((optional(KEYWORD_BE), optional(KEYWORD_LE)) && variant(IDENTIFIER, VALUETYPE_ANY) && sequence(IDENTIFIER, SEPARATOR_ENDOFEXPRESSION)))
                unionNode->addMember(parseMemberVariable());
            else if (MATCHES(sequence(SEPARATOR_ENDOFPROGRAM)))
                throwParseError("Unexpected end of program", -2);
            else
                throwParseError("Invalid union member", 0);
        }

        unionGuard.release();

        return new ASTNodeTypeDecl(typeName, unionNode);
    }

    // (parseType) Identifier @ Integer;
    ASTNode* Parser::parseVariablePlacement() {
        ASTNodeTypeDecl* temporaryType = dynamic_cast<ASTNodeTypeDecl *>(parseType(-5));
        if (temporaryType == nullptr) return nullptr;
        SCOPE_EXIT( delete temporaryType; );

        if (auto *typeDecl = dynamic_cast<ASTNodeTypeDecl*>(temporaryType->getType()); typeDecl != nullptr) {
                return new ASTNodeVariableDecl(getValue<std::string>(-4), new ASTNodeTypeDecl(*typeDecl));
        } else if (auto *builtInType = dynamic_cast<ASTNodeBuiltinType*>(temporaryType->getType()); builtInType != nullptr) {
                return new ASTNodeVariableDecl(getValue<std::string>(-4), new ASTNodeBuiltinType(*builtInType));
        }

        throwParseError("Invalid type used in variable placement", -1);
        return nullptr;
    }

    // <(parseUsingDeclaration)|(parseVariablePlacement)|(parseStruct)>
    ASTNode* Parser::parseStatement() {
        if (MATCHES(sequence(KEYWORD_USING, IDENTIFIER, OPERATOR_ASSIGNMENT) && (optional(KEYWORD_BE), optional(KEYWORD_LE)) && variant(IDENTIFIER, VALUETYPE_ANY) && sequence(SEPARATOR_ENDOFEXPRESSION))) {
            auto node = dynamic_cast<ASTNodeTypeDecl*>(parseUsingDeclaration());
            this->m_types.insert({ node->getName().data(), node });
            return node;
        }
        else if (MATCHES((optional(KEYWORD_BE), optional(KEYWORD_LE)) && variant(IDENTIFIER, VALUETYPE_ANY) && sequence(IDENTIFIER, OPERATOR_AT, INTEGER, SEPARATOR_ENDOFEXPRESSION)))
            return parseVariablePlacement();
        else if (MATCHES(sequence(KEYWORD_STRUCT, IDENTIFIER, SEPARATOR_CURLYBRACKETOPEN)))
            return parseStruct();
        else throwParseError("Invalid sequence", 0);

        return nullptr;
    }

    // <(parseStatement)...> EndOfProgram
    std::pair<Result, std::vector<ASTNode*>> Parser::parse(const std::vector<Token> &tokens) {
        this->m_curr = tokens.begin();

        try {
            auto program = parseTillToken(SEPARATOR_ENDOFPROGRAM);

            if (program.empty() || this->m_curr != tokens.end())
                throwParseError("Program is empty!", -1);

            return { ResultSuccess, program };
        } catch (ParseError &e) {
            this->m_error = e;
            printf("Hit error\n");
        }

        return { ResultParseError, { } };
    }

}