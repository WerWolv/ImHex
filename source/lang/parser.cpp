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

    /* Mathematical expressions */

    // <Integer|((parseMathematicalExpression))>
    ASTNode* Parser::parseFactor() {
        if (MATCHES(sequence(INTEGER)))
            return new ASTNodeNumericExpression(new ASTNodeIntegerLiteral(getValue<s128>(-1), Token::ValueType::Signed128Bit), new ASTNodeIntegerLiteral(0, Token::ValueType::Signed128Bit), Token::Operator::Plus);
        else if (MATCHES(sequence(SEPARATOR_ROUNDBRACKETOPEN))) {
            auto node = this->parseMathematicalExpression();
            if (!MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE)))
                throwParseError("Expected closing parenthesis");
            return node;
        }
        else
            throwParseError("Expected integer or parenthesis");

        return nullptr;
    }

    // (parseFactor) <*|/> (parseFactor)
    ASTNode* Parser::parseMultiplicativeExpression() {
        auto node = this->parseFactor();

        while (MATCHES(variant(OPERATOR_STAR, OPERATOR_SLASH))) {
            if (matches(OPERATOR_STAR, -1))
                node = new ASTNodeNumericExpression(node, this->parseFactor(), Token::Operator::Star);
            else
                node = new ASTNodeNumericExpression(node, this->parseFactor(), Token::Operator::Slash);
        }

        return node;
    }

    // (parseMultiplicativeExpression) <+|-> (parseMultiplicativeExpression)
    ASTNode* Parser::parseAdditiveExpression() {
        auto node = this->parseMultiplicativeExpression();

        while (MATCHES(variant(OPERATOR_PLUS, OPERATOR_MINUS))) {
            if (matches(OPERATOR_PLUS, -1))
                node = new ASTNodeNumericExpression(node, this->parseMultiplicativeExpression(), Token::Operator::Plus);
            else
                node = new ASTNodeNumericExpression(node, this->parseMultiplicativeExpression(), Token::Operator::Minus);
        }

        return node;
    }

    // (parseAdditiveExpression) <>>|<<> (parseAdditiveExpression)
    ASTNode* Parser::parseShiftExpression() {
        auto node = this->parseAdditiveExpression();

        while (MATCHES(variant(OPERATOR_SHIFTLEFT, OPERATOR_SHIFTRIGHT))) {
            if (matches(OPERATOR_SHIFTLEFT, -1))
                node = new ASTNodeNumericExpression(node, this->parseAdditiveExpression(), Token::Operator::ShiftLeft);
            else
                node = new ASTNodeNumericExpression(node, this->parseAdditiveExpression(), Token::Operator::ShiftRight);
        }

        return node;
    }

    // (parseShiftExpression) & (parseShiftExpression)
    ASTNode* Parser::parseBinaryAndExpression() {
        auto node = this->parseShiftExpression();

        while (MATCHES(sequence(OPERATOR_BITAND))) {
            node = new ASTNodeNumericExpression(node, this->parseShiftExpression(), Token::Operator::BitAnd);
        }

        return node;
    }

    // (parseBinaryAndExpression) ^ (parseBinaryAndExpression)
    ASTNode* Parser::parseBinaryXorExpression() {
        auto node = this->parseBinaryAndExpression();

        while (MATCHES(sequence(OPERATOR_BITXOR))) {
            node = new ASTNodeNumericExpression(node, this->parseBinaryAndExpression(), Token::Operator::BitXor);
        }

        return node;
    }

    // (parseBinaryXorExpression) | (parseBinaryXorExpression)
    ASTNode* Parser::parseBinaryOrExpression() {
        auto node = this->parseBinaryXorExpression();

        while (MATCHES(sequence(OPERATOR_BITOR))) {
            node = new ASTNodeNumericExpression(node, this->parseBinaryXorExpression(), Token::Operator::BitOr);
        }

        return node;
    }

    // (parseBinaryOrExpression)
    ASTNode* Parser::parseMathematicalExpression() {
        return this->parseBinaryOrExpression();
    }

    /* Type declarations */

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

            return new ASTNodeTypeDecl({ }, this->m_types[getValue<std::string>(startIndex)]->clone(), endian);
        }
        else { // Builtin type
            return new ASTNodeTypeDecl({ }, new ASTNodeBuiltinType(getValue<Token::ValueType>(startIndex)), endian);
        }
    }

    // using Identifier = (parseType);
    ASTNode* Parser::parseUsingDeclaration() {
        auto *temporaryType = dynamic_cast<ASTNodeTypeDecl *>(parseType(-1));
        if (temporaryType == nullptr) throwParseError("Invalid type used in variable declaration", -1);
        SCOPE_EXIT( delete temporaryType; );

        if (matchesOptional(KEYWORD_BE) || matchesOptional(KEYWORD_LE))
            return new ASTNodeTypeDecl(getValue<std::string>(-4), temporaryType->getType()->clone(), temporaryType->getEndian());
        else
            return new ASTNodeTypeDecl(getValue<std::string>(-3), temporaryType->getType()->clone(), temporaryType->getEndian());
    }

    // (parseType) Identifier;
    ASTNode* Parser::parseMemberVariable() {
        auto temporaryType = dynamic_cast<ASTNodeTypeDecl *>(parseType(-2));
        if (temporaryType == nullptr) throwParseError("Invalid type used in variable declaration", -1);
        SCOPE_EXIT( delete temporaryType; );

        return new ASTNodeVariableDecl(getValue<std::string>(-1), temporaryType->getType()->clone());
    }

    // (parseType) Identifier[(parseMathematicalExpression)];
    ASTNode* Parser::parseMemberArrayVariable() {
        auto temporaryType = dynamic_cast<ASTNodeTypeDecl *>(parseType(-3));
        if (temporaryType == nullptr) throwParseError("Invalid type used in variable declaration", -1);
        SCOPE_EXIT( delete temporaryType; );

        auto name = getValue<std::string>(-2);
        auto size = parseMathematicalExpression();

        if (!MATCHES(sequence(SEPARATOR_SQUAREBRACKETCLOSE)))
            throwParseError("Expected closing ']' at end of array declaration", -1);

        return new ASTNodeArrayVariableDecl(name, temporaryType->getType()->clone(), size);
    }

    // struct Identifier { <(parseMember)...> };
    ASTNode* Parser::parseStruct() {
        const auto structNode = new ASTNodeStruct();
        const auto &typeName = getValue<std::string>(-2);
        ScopeExit structGuard([&]{ delete structNode; });

        while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
            if (MATCHES((optional(KEYWORD_BE), optional(KEYWORD_LE)) && variant(IDENTIFIER, VALUETYPE_ANY) && sequence(IDENTIFIER, SEPARATOR_SQUAREBRACKETOPEN)))
                structNode->addMember(parseMemberArrayVariable());
            else if (MATCHES((optional(KEYWORD_BE), optional(KEYWORD_LE)) && variant(IDENTIFIER, VALUETYPE_ANY) && sequence(IDENTIFIER)))
                structNode->addMember(parseMemberVariable());
            else if (MATCHES(sequence(SEPARATOR_ENDOFPROGRAM)))
                throwParseError("Unexpected end of program", -2);
            else
                throwParseError("Invalid struct member", 0);

            if (!MATCHES(sequence(SEPARATOR_ENDOFEXPRESSION)))
                throwParseError("Missing ';' at end of expression", -1);
        }

        structGuard.release();

        return new ASTNodeTypeDecl(typeName, structNode);
    }

    // union Identifier { <(parseMember)...> };
    ASTNode* Parser::parseUnion() {
        const auto unionNode = new ASTNodeUnion();
        const auto &typeName = getValue<std::string>(-2);
        ScopeExit unionGuard([&]{ delete unionNode; });

        while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
            if (MATCHES((optional(KEYWORD_BE), optional(KEYWORD_LE)) && variant(IDENTIFIER, VALUETYPE_ANY) && sequence(IDENTIFIER, SEPARATOR_SQUAREBRACKETOPEN)))
                unionNode->addMember(parseMemberArrayVariable());
            else if (MATCHES((optional(KEYWORD_BE), optional(KEYWORD_LE)) && variant(IDENTIFIER, VALUETYPE_ANY) && sequence(IDENTIFIER)))
                unionNode->addMember(parseMemberVariable());
            else if (MATCHES(sequence(SEPARATOR_ENDOFPROGRAM)))
                throwParseError("Unexpected end of program", -2);
            else
                throwParseError("Invalid union member", 0);

            if (!MATCHES(sequence(SEPARATOR_ENDOFEXPRESSION)))
                throwParseError("Missing ';' at end of expression", -1);
        }

        unionGuard.release();

        return new ASTNodeTypeDecl(typeName, unionNode);
    }

    // enum Identifier : (parseType) { <(parseEnumEntry)...> };
    ASTNode* Parser::parseEnum() {
        std::string typeName;
        if (matchesOptional(KEYWORD_BE) || matchesOptional(KEYWORD_LE))
            typeName = getValue<std::string>(-5);
        else
            typeName = getValue<std::string>(-4);

        auto temporaryTypeDecl = dynamic_cast<ASTNodeTypeDecl*>(parseType(-2));
        if (temporaryTypeDecl == nullptr) throwParseError("Failed to parse type", -2);
        auto underlyingType = dynamic_cast<ASTNodeBuiltinType*>(temporaryTypeDecl->getType());
        if (underlyingType == nullptr) throwParseError("Underlying type is not a built-in type", -2);

        const auto enumNode = new ASTNodeEnum(underlyingType);
        ScopeExit enumGuard([&]{ delete enumNode; });

        while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
            if (MATCHES(sequence(IDENTIFIER, OPERATOR_ASSIGNMENT))) {
                auto name = getValue<std::string>(-2);
                enumNode->addEntry(name, parseMathematicalExpression());
            }
            else if (MATCHES(sequence(IDENTIFIER))) {
                ASTNode *valueExpr;
                auto name = getValue<std::string>(-1);
                if (enumNode->getEntries().empty())
                    valueExpr = new ASTNodeIntegerLiteral(0, underlyingType->getType());
                else
                    valueExpr = new ASTNodeNumericExpression(enumNode->getEntries().back().second, new ASTNodeIntegerLiteral(1, Token::ValueType::Signed128Bit), Token::Operator::Plus);

                enumNode->addEntry(name, valueExpr);
            }
            else if (MATCHES(sequence(SEPARATOR_ENDOFPROGRAM)))
                throwParseError("Unexpected end of program", -2);
            else
                throwParseError("Invalid union member", 0);

            if (!MATCHES(sequence(SEPARATOR_COMMA))) {
                if (MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE)))
                    break;
                else
                    throwParseError("Missing ',' between enum entries", 0);
            }
        }

        enumGuard.release();

        return new ASTNodeTypeDecl(typeName, enumNode);
    }

    // (parseType) Identifier @ Integer;
    ASTNode* Parser::parseVariablePlacement() {
        auto temporaryType = dynamic_cast<ASTNodeTypeDecl *>(parseType(-4));
        if (temporaryType == nullptr) throwParseError("Invalid type used in variable declaration", -1);
        SCOPE_EXIT( delete temporaryType; );

        return new ASTNodeVariableDecl(getValue<std::string>(-3), temporaryType->getType()->clone(), getValue<s128>(-1));
    }

    // (parseType) Identifier[(parseMathematicalExpression)] @ Integer;
    ASTNode* Parser::parseArrayVariablePlacement() {
        auto temporaryType = dynamic_cast<ASTNodeTypeDecl *>(parseType(-3));
        if (temporaryType == nullptr) throwParseError("Invalid type used in variable declaration", -1);
        SCOPE_EXIT( delete temporaryType; );

        auto name = getValue<std::string>(-2);
        auto size = parseMathematicalExpression();

        if (!MATCHES(sequence(SEPARATOR_SQUAREBRACKETCLOSE)))
            throwParseError("Expected closing ']' at end of array declaration", -1);

        if (!MATCHES(sequence(OPERATOR_AT, INTEGER)))
            throwParseError("Expected placement instruction", -1);

        return new ASTNodeArrayVariableDecl(name, temporaryType->getType()->clone(), size, getValue<s128>(-1));
    }



    /* Program */

    // <(parseUsingDeclaration)|(parseVariablePlacement)|(parseStruct)>
    ASTNode* Parser::parseStatement() {
        ASTNode *statement;

        if (MATCHES(sequence(KEYWORD_USING, IDENTIFIER, OPERATOR_ASSIGNMENT) && (optional(KEYWORD_BE), optional(KEYWORD_LE)) && variant(IDENTIFIER, VALUETYPE_ANY)))
            statement = dynamic_cast<ASTNodeTypeDecl*>(parseUsingDeclaration());
        else if (MATCHES((optional(KEYWORD_BE), optional(KEYWORD_LE)) && variant(IDENTIFIER, VALUETYPE_ANY) && sequence(IDENTIFIER, SEPARATOR_SQUAREBRACKETOPEN)))
            statement = parseArrayVariablePlacement();
        else if (MATCHES((optional(KEYWORD_BE), optional(KEYWORD_LE)) && variant(IDENTIFIER, VALUETYPE_ANY) && sequence(IDENTIFIER, OPERATOR_AT, INTEGER)))
            statement = parseVariablePlacement();
        else if (MATCHES(sequence(KEYWORD_STRUCT, IDENTIFIER, SEPARATOR_CURLYBRACKETOPEN)))
            statement = parseStruct();
        else if (MATCHES(sequence(KEYWORD_UNION, IDENTIFIER, SEPARATOR_CURLYBRACKETOPEN)))
            statement = parseUnion();
        else if (MATCHES(sequence(KEYWORD_ENUM, IDENTIFIER, OPERATOR_INHERIT) && (optional(KEYWORD_BE), optional(KEYWORD_LE)) && sequence(VALUETYPE_UNSIGNED, SEPARATOR_CURLYBRACKETOPEN)))
            statement = parseEnum();
        else throwParseError("Invalid sequence", 0);

        if (!MATCHES(sequence(SEPARATOR_ENDOFEXPRESSION)))
            throwParseError("Missing ';' at end of expression", -1);

        if (auto typeDecl = dynamic_cast<ASTNodeTypeDecl*>(statement); typeDecl != nullptr)
            this->m_types.insert({ typeDecl->getName().data(), typeDecl });

        return statement;
    }

    // <(parseStatement)...> EndOfProgram
    std::pair<Result, std::vector<ASTNode*>> Parser::parse(const std::vector<Token> &tokens) {
        this->m_curr = tokens.begin();

        this->m_types.clear();

        try {
            auto program = parseTillToken(SEPARATOR_ENDOFPROGRAM);

            if (program.empty() || this->m_curr != tokens.end())
                throwParseError("Program is empty!", -1);

            return { ResultSuccess, program };
        } catch (ParseError &e) {
            this->m_error = e;
        }

        return { ResultParseError, { } };
    }

}