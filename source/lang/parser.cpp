#include "lang/parser.hpp"

#include <optional>
#include <variant>

#define MATCHES(x) (begin() && x)

#define TO_NUMERIC_EXPRESSION(node) new ASTNodeNumericExpression((node), new ASTNodeIntegerLiteral({ Token::ValueType::Signed32Bit, s32(0) }), Token::Operator::Plus)

// Definition syntax:
// [A]          : Either A or no token
// [A|B]        : Either A, B or no token
// <A|B>        : Either A or B
// <A...>       : One or more of A
// A B C        : Sequence of tokens A then B then C
// (parseXXXX)  : Parsing handled by other function
namespace hex::lang {

    /* Mathematical expressions */

    // Identifier([(parseMathematicalExpression)|<(parseMathematicalExpression),...>(parseMathematicalExpression)]
    ASTNode* Parser::parseFunctionCall() {
        auto functionName = getValue<std::string>(-2);
        std::vector<ASTNode*> params;
        ScopeExit paramCleanup([&]{
            for (auto &param : params)
                delete param;
        });

        while (!MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE))) {
            params.push_back(parseMathematicalExpression());

            if (MATCHES(sequence(SEPARATOR_COMMA, SEPARATOR_ROUNDBRACKETCLOSE)))
                throwParseError("unexpected ',' at end of function parameter list", -1);
            else if (MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE)))
                break;
            else if (!MATCHES(sequence(SEPARATOR_COMMA)))
                throwParseError("missing ',' between parameters", -1);

        }

        paramCleanup.release();

        return TO_NUMERIC_EXPRESSION(new ASTNodeFunctionCall(functionName, params));
    }

    // Identifier::<Identifier[::]...>
    ASTNode* Parser::parseScopeResolution(std::vector<std::string> &path) {
        if (peek(IDENTIFIER, -1))
            path.push_back(getValue<std::string>(-1));

        if (MATCHES(sequence(SEPARATOR_SCOPE_RESOLUTION))) {
            if (MATCHES(sequence(IDENTIFIER)))
                return this->parseScopeResolution(path);
            else
                throwParseError("expected member name", -1);
        } else
            return TO_NUMERIC_EXPRESSION(new ASTNodeScopeResolution(path));
    }

    // <Identifier[.]...>
    ASTNode* Parser::parseRValue(std::vector<std::string> &path) {
        if (peek(IDENTIFIER, -1))
            path.push_back(getValue<std::string>(-1));

        if (MATCHES(sequence(SEPARATOR_DOT))) {
            if (MATCHES(sequence(IDENTIFIER)))
                return this->parseRValue(path);
            else
                throwParseError("expected member name", -1);
        } else
            return TO_NUMERIC_EXPRESSION(new ASTNodeRValue(path));
    }

    // <Integer|((parseMathematicalExpression))>
    ASTNode* Parser::parseFactor() {
        if (MATCHES(sequence(INTEGER)))
            return TO_NUMERIC_EXPRESSION(new ASTNodeIntegerLiteral(getValue<Token::IntegerLiteral>(-1)));
        else if (MATCHES(sequence(SEPARATOR_ROUNDBRACKETOPEN))) {
            auto node = this->parseMathematicalExpression();
            if (!MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE)))
                throwParseError("expected closing parenthesis");
            return node;
        } else if (MATCHES(sequence(IDENTIFIER, SEPARATOR_SCOPE_RESOLUTION))) {
            std::vector<std::string> path;
            this->m_curr--;
            return this->parseScopeResolution(path);
        } else if (MATCHES(sequence(IDENTIFIER, SEPARATOR_ROUNDBRACKETOPEN))) {
            return this->parseFunctionCall();
        } else if (MATCHES(sequence(IDENTIFIER))) {
            std::vector<std::string> path;
            return this->parseRValue(path);
        } else
            throwParseError("expected integer or parenthesis");
    }

    // <+|-|!|~> (parseFactor)
    ASTNode* Parser::parseUnaryExpression() {
        if (MATCHES(sequence(OPERATOR_PLUS) || sequence(OPERATOR_MINUS) || sequence(OPERATOR_BOOLNOT) || sequence(OPERATOR_BITNOT))) {
            auto op = getValue<Token::Operator>(-1);

            return new ASTNodeNumericExpression(new ASTNodeIntegerLiteral({ Token::ValueType::Signed32Bit, 0}), this->parseFactor(), op);
        }

        return this->parseFactor();
    }

    // (parseUnaryExpression) <*|/> (parseUnaryExpression)
    ASTNode* Parser::parseMultiplicativeExpression() {
        auto node = this->parseUnaryExpression();

        while (MATCHES(variant(OPERATOR_STAR, OPERATOR_SLASH))) {
            auto op = getValue<Token::Operator>(-1);
            node = new ASTNodeNumericExpression(node, this->parseUnaryExpression(), op);
        }

        return node;
    }

    // (parseMultiplicativeExpression) <+|-> (parseMultiplicativeExpression)
    ASTNode* Parser::parseAdditiveExpression() {
        auto node = this->parseMultiplicativeExpression();

        while (MATCHES(variant(OPERATOR_PLUS, OPERATOR_MINUS))) {
            auto op = getValue<Token::Operator>(-1);
            node = new ASTNodeNumericExpression(node, this->parseMultiplicativeExpression(), op);
        }

        return node;
    }

    // (parseAdditiveExpression) < >>|<< > (parseAdditiveExpression)
    ASTNode* Parser::parseShiftExpression() {
        auto node = this->parseAdditiveExpression();

        while (MATCHES(variant(OPERATOR_SHIFTLEFT, OPERATOR_SHIFTRIGHT))) {
            auto op = getValue<Token::Operator>(-1);
            node = new ASTNodeNumericExpression(node, this->parseAdditiveExpression(), op);
        }

        return node;
    }

    // (parseAdditiveExpression) < >=|<=|>|< > (parseAdditiveExpression)
    ASTNode* Parser::parseRelationExpression() {
        auto node = this->parseShiftExpression();

        while (MATCHES(sequence(OPERATOR_BOOLGREATERTHAN) || sequence(OPERATOR_BOOLLESSTHAN) || sequence(OPERATOR_BOOLGREATERTHANOREQUALS) || sequence(OPERATOR_BOOLLESSTHANOREQUALS))) {
            auto op = getValue<Token::Operator>(-1);
            node = new ASTNodeNumericExpression(node, this->parseShiftExpression(), op);
        }

        return node;
    }

    // (parseRelationExpression) <==|!=> (parseRelationExpression)
    ASTNode* Parser::parseEqualityExpression() {
        auto node = this->parseRelationExpression();

        while (MATCHES(sequence(OPERATOR_BOOLEQUALS) || sequence(OPERATOR_BOOLNOTEQUALS))) {
            auto op = getValue<Token::Operator>(-1);
            node = new ASTNodeNumericExpression(node, this->parseRelationExpression(), op);
        }

        return node;
    }

    // (parseEqualityExpression) & (parseEqualityExpression)
    ASTNode* Parser::parseBinaryAndExpression() {
        auto node = this->parseEqualityExpression();

        while (MATCHES(sequence(OPERATOR_BITAND))) {
            node = new ASTNodeNumericExpression(node, this->parseEqualityExpression(), Token::Operator::BitAnd);
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

    // (parseBinaryOrExpression) && (parseBinaryOrExpression)
    ASTNode* Parser::parseBooleanAnd() {
        auto node = this->parseBinaryOrExpression();

        while (MATCHES(sequence(OPERATOR_BOOLAND))) {
            node = new ASTNodeNumericExpression(node, this->parseBinaryOrExpression(), Token::Operator::BitOr);
        }

        return node;
    }

    // (parseBooleanAnd) ^^ (parseBooleanAnd)
    ASTNode* Parser::parseBooleanXor() {
        auto node = this->parseBooleanAnd();

        while (MATCHES(sequence(OPERATOR_BOOLXOR))) {
            node = new ASTNodeNumericExpression(node, this->parseBooleanAnd(), Token::Operator::BitOr);
        }

        return node;
    }

    // (parseBooleanXor) || (parseBooleanXor)
    ASTNode* Parser::parseBooleanOr() {
        auto node = this->parseBooleanXor();

        while (MATCHES(sequence(OPERATOR_BOOLOR))) {
            node = new ASTNodeNumericExpression(node, this->parseBooleanXor(), Token::Operator::BitOr);
        }

        return node;
    }

    // (parseBooleanOr) ? (parseBooleanOr) : (parseBooleanOr)
    ASTNode* Parser::parseTernaryConditional() {
        auto node = this->parseBooleanXor();

        while (MATCHES(sequence(OPERATOR_TERNARYCONDITIONAL))) {
            auto second = this->parseBooleanXor();

            if (!MATCHES(sequence(OPERATOR_INHERIT)))
                throwParseError("expected ':' in ternary expression");

            auto third = this->parseBooleanXor();
            node = new ASTNodeTernaryExpression(node, second, third, Token::Operator::TernaryConditional);
        }

        return node;
    }

    // (parseTernaryConditional)
    ASTNode* Parser::parseMathematicalExpression() {
        return this->parseTernaryConditional();
    }


    /* Control flow */

    // if ((parseMathematicalExpression)) { (parseMember) }
    ASTNode* Parser::parseConditional() {
        auto condition = parseMathematicalExpression();
        std::vector<ASTNode*> trueBody, falseBody;

        ScopeExit cleanup([&]{
            delete condition;
            for (auto &statement : trueBody)
                delete statement;
            for (auto &statement : falseBody)
                delete statement;
        });

        if (MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE, SEPARATOR_CURLYBRACKETOPEN))) {
            while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
                trueBody.push_back(parseMember());
            }
        } else if (MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE))) {
            trueBody.push_back(parseMember());
        } else
            throwParseError("expected body of conditional statement");

        if (MATCHES(sequence(KEYWORD_ELSE, SEPARATOR_CURLYBRACKETOPEN))) {
            while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
                falseBody.push_back(parseMember());
            }
        } else if (MATCHES(sequence(KEYWORD_ELSE))) {
            falseBody.push_back(parseMember());
        }

        cleanup.release();

        return new ASTNodeConditionalStatement(condition, trueBody, falseBody);
    }

    /* Type declarations */

    // [be|le] <Identifier|u8|u16|u32|u64|u128|s8|s16|s32|s64|s128|float|double>
    ASTNode* Parser::parseType(s32 startIndex) {
        std::optional<std::endian> endian;

        if (peekOptional(KEYWORD_LE, 0))
            endian = std::endian::little;
        else if (peekOptional(KEYWORD_BE, 0))
            endian = std::endian::big;

        if (getType(startIndex) == Token::Type::Identifier) { // Custom type
            if (!this->m_types.contains(getValue<std::string>(startIndex)))
                throwParseError("failed to parse type");

            return new ASTNodeTypeDecl({ }, this->m_types[getValue<std::string>(startIndex)]->clone(), endian);
        }
        else { // Builtin type
            return new ASTNodeTypeDecl({ }, new ASTNodeBuiltinType(getValue<Token::ValueType>(startIndex)), endian);
        }
    }

    // using Identifier = (parseType)
    ASTNode* Parser::parseUsingDeclaration() {
        auto *temporaryType = dynamic_cast<ASTNodeTypeDecl *>(parseType(-1));
        if (temporaryType == nullptr) throwParseError("invalid type used in variable declaration", -1);
        SCOPE_EXIT( delete temporaryType; );

        if (peekOptional(KEYWORD_BE) || peekOptional(KEYWORD_LE))
            return new ASTNodeTypeDecl(getValue<std::string>(-4), temporaryType->getType()->clone(), temporaryType->getEndian());
        else
            return new ASTNodeTypeDecl(getValue<std::string>(-3), temporaryType->getType()->clone(), temporaryType->getEndian());
    }

    // padding[(parseMathematicalExpression)]
    ASTNode* Parser::parsePadding() {
        auto size = parseMathematicalExpression();

        if (!MATCHES(sequence(SEPARATOR_SQUAREBRACKETCLOSE))) {
            delete size;
            throwParseError("expected closing ']' at end of array declaration", -1);
        }

        return new ASTNodeArrayVariableDecl({ }, new ASTNodeTypeDecl({ }, new ASTNodeBuiltinType(Token::ValueType::Padding)), size);;
    }

    // (parseType) Identifier
    ASTNode* Parser::parseMemberVariable() {
        auto temporaryType = dynamic_cast<ASTNodeTypeDecl *>(parseType(-2));
        if (temporaryType == nullptr) throwParseError("invalid type used in variable declaration", -1);
        SCOPE_EXIT( delete temporaryType; );

        return new ASTNodeVariableDecl(getValue<std::string>(-1), temporaryType->getType()->clone());
    }

    // (parseType) Identifier[(parseMathematicalExpression)]
    ASTNode* Parser::parseMemberArrayVariable() {
        auto temporaryType = dynamic_cast<ASTNodeTypeDecl *>(parseType(-3));
        if (temporaryType == nullptr) throwParseError("invalid type used in variable declaration", -1);
        SCOPE_EXIT( delete temporaryType; );

        auto name = getValue<std::string>(-2);
        auto size = parseMathematicalExpression();

        if (!MATCHES(sequence(SEPARATOR_SQUAREBRACKETCLOSE)))
            throwParseError("expected closing ']' at end of array declaration", -1);

        return new ASTNodeArrayVariableDecl(name, temporaryType->getType()->clone(), size);
    }

    // (parseType) *Identifier : (parseType)
    ASTNode* Parser::parseMemberPointerVariable() {
        auto name = getValue<std::string>(-2);

        auto temporaryPointerType = dynamic_cast<ASTNodeTypeDecl *>(parseType(-4));
        if (temporaryPointerType == nullptr) throwParseError("invalid type used in variable declaration", -1);
        SCOPE_EXIT( delete temporaryPointerType; );

        if (!MATCHES((optional(KEYWORD_BE), optional(KEYWORD_LE)) && sequence(VALUETYPE_UNSIGNED)))
            throwParseError("expected unsigned builtin type as size", -1);

        auto temporarySizeType = dynamic_cast<ASTNodeTypeDecl *>(parseType(-1));
        if (temporarySizeType == nullptr) throwParseError("invalid type used for pointer size", -1);
        SCOPE_EXIT( delete temporarySizeType; );

        return new ASTNodePointerVariableDecl(name, temporaryPointerType->getType()->clone(), temporarySizeType->getType()->clone());
    }

    // [(parsePadding)|(parseMemberVariable)|(parseMemberArrayVariable)|(parseMemberPointerVariable)]
    ASTNode* Parser::parseMember() {
        ASTNode *member;

        if (MATCHES(sequence(VALUETYPE_PADDING, SEPARATOR_SQUAREBRACKETOPEN)))
            member = parsePadding();
        else if (MATCHES((optional(KEYWORD_BE), optional(KEYWORD_LE)) && variant(IDENTIFIER, VALUETYPE_ANY) && sequence(IDENTIFIER, SEPARATOR_SQUAREBRACKETOPEN)))
            member = parseMemberArrayVariable();
        else if (MATCHES((optional(KEYWORD_BE), optional(KEYWORD_LE)) && variant(IDENTIFIER, VALUETYPE_ANY) && sequence(IDENTIFIER)))
            member = parseMemberVariable();
        else if (MATCHES((optional(KEYWORD_BE), optional(KEYWORD_LE)) && variant(IDENTIFIER, VALUETYPE_ANY) && sequence(OPERATOR_STAR, IDENTIFIER, OPERATOR_INHERIT)))
            member = parseMemberPointerVariable();
        else if (MATCHES(sequence(KEYWORD_IF, SEPARATOR_ROUNDBRACKETOPEN)))
            return parseConditional();
        else if (MATCHES(sequence(SEPARATOR_ENDOFPROGRAM)))
            throwParseError("unexpected end of program", -2);
        else
            throwParseError("invalid struct member", 0);

        if (!MATCHES(sequence(SEPARATOR_ENDOFEXPRESSION)))
            throwParseError("missing ';' at end of expression", -1);

        return member;
    }

    // struct Identifier { <(parseMember)...> }
    ASTNode* Parser::parseStruct() {
        const auto structNode = new ASTNodeStruct();
        const auto &typeName = getValue<std::string>(-2);
        ScopeExit structGuard([&]{ delete structNode; });

        while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
            structNode->addMember(parseMember());
        }

        structGuard.release();

        return new ASTNodeTypeDecl(typeName, structNode);
    }

    // union Identifier { <(parseMember)...> }
    ASTNode* Parser::parseUnion() {
        const auto unionNode = new ASTNodeUnion();
        const auto &typeName = getValue<std::string>(-2);
        ScopeExit unionGuard([&]{ delete unionNode; });

        while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
            unionNode->addMember(parseMember());
        }

        unionGuard.release();

        return new ASTNodeTypeDecl(typeName, unionNode);
    }

    // enum Identifier : (parseType) { <<Identifier|Identifier = (parseMathematicalExpression)[,]>...> }
    ASTNode* Parser::parseEnum() {
        std::string typeName;
        if (peekOptional(KEYWORD_BE) || peekOptional(KEYWORD_LE))
            typeName = getValue<std::string>(-5);
        else
            typeName = getValue<std::string>(-4);

        auto temporaryTypeDecl = dynamic_cast<ASTNodeTypeDecl*>(parseType(-2));
        if (temporaryTypeDecl == nullptr) throwParseError("failed to parse type", -2);
        auto underlyingType = dynamic_cast<ASTNodeBuiltinType*>(temporaryTypeDecl->getType());
        if (underlyingType == nullptr) throwParseError("underlying type is not a built-in type", -2);

        const auto enumNode = new ASTNodeEnum(underlyingType);
        ScopeExit enumGuard([&]{ delete enumNode; });

        ASTNode *lastEntry = nullptr;
        while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
            if (MATCHES(sequence(IDENTIFIER, OPERATOR_ASSIGNMENT))) {
                auto name = getValue<std::string>(-2);
                enumNode->addEntry(name, parseMathematicalExpression());
            }
            else if (MATCHES(sequence(IDENTIFIER))) {
                ASTNode *valueExpr;
                auto name = getValue<std::string>(-1);
                if (enumNode->getEntries().empty()) {
                    auto type = underlyingType->getType();

                    switch (type) {
                        case Token::ValueType::Signed8Bit:
                            valueExpr = new ASTNodeIntegerLiteral({ type, s8(0) });
                            break;
                        case Token::ValueType::Unsigned8Bit:
                            valueExpr = new ASTNodeIntegerLiteral({ type, u8(0) });
                            break;
                        case Token::ValueType::Signed16Bit:
                            valueExpr = new ASTNodeIntegerLiteral({ type, s16(0) });
                            break;
                        case Token::ValueType::Unsigned16Bit:
                            valueExpr = new ASTNodeIntegerLiteral({ type, u16(0) });
                            break;
                        case Token::ValueType::Signed32Bit:
                            valueExpr = new ASTNodeIntegerLiteral({ type, s32(0) });
                            break;
                        case Token::ValueType::Unsigned32Bit:
                            valueExpr = new ASTNodeIntegerLiteral({ type, u32(0) });
                            break;
                        case Token::ValueType::Signed64Bit:
                            valueExpr = new ASTNodeIntegerLiteral({ type, s64(0) });
                            break;
                        case Token::ValueType::Unsigned64Bit:
                            valueExpr = new ASTNodeIntegerLiteral({ type, u64(0) });
                            break;
                        case Token::ValueType::Signed128Bit:
                            valueExpr = new ASTNodeIntegerLiteral({ type, s128(0) });
                            break;
                        case Token::ValueType::Unsigned128Bit:
                            valueExpr = new ASTNodeIntegerLiteral({ type, u128(0) });
                            break;
                        default:
                            throwParseError("invalid enum underlying type", -1);
                    }

                    lastEntry = valueExpr;
                }
                else
                    valueExpr = new ASTNodeNumericExpression(lastEntry, new ASTNodeIntegerLiteral({ Token::ValueType::Signed32Bit, s32(1) }), Token::Operator::Plus);

                enumNode->addEntry(name, valueExpr);
            }
            else if (MATCHES(sequence(SEPARATOR_ENDOFPROGRAM)))
                throwParseError("unexpected end of program", -2);
            else
                throwParseError("invalid union member", 0);

            if (!MATCHES(sequence(SEPARATOR_COMMA))) {
                if (MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE)))
                    break;
                else
                    throwParseError("missing ',' between enum entries", 0);
            }
        }

        enumGuard.release();

        return new ASTNodeTypeDecl(typeName, enumNode);
    }

    // bitfield Identifier { <Identifier : (parseMathematicalExpression)[;]...> }
    ASTNode* Parser::parseBitfield() {
        std::string typeName = getValue<std::string>(-2);

        const auto bitfieldNode = new ASTNodeBitfield();
        ScopeExit enumGuard([&]{ delete bitfieldNode; });

        while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
            if (MATCHES(sequence(IDENTIFIER, OPERATOR_INHERIT))) {
                auto name = getValue<std::string>(-2);
                bitfieldNode->addEntry(name, parseMathematicalExpression());
            }
            else if (MATCHES(sequence(SEPARATOR_ENDOFPROGRAM)))
                throwParseError("unexpected end of program", -2);
            else
                throwParseError("invalid bitfield member", 0);

            if (!MATCHES(sequence(SEPARATOR_ENDOFEXPRESSION))) {
                throwParseError("missing ';' at end of expression", -1);
            }
        }

        enumGuard.release();

        return new ASTNodeTypeDecl(typeName, bitfieldNode);
    }

    // (parseType) Identifier @ Integer
    ASTNode* Parser::parseVariablePlacement() {
        auto temporaryType = dynamic_cast<ASTNodeTypeDecl *>(parseType(-3));
        if (temporaryType == nullptr) throwParseError("invalid type used in variable declaration", -1);
        SCOPE_EXIT( delete temporaryType; );

        return new ASTNodeVariableDecl(getValue<std::string>(-2), temporaryType->getType()->clone(), parseMathematicalExpression());
    }

    // (parseType) Identifier[(parseMathematicalExpression)] @ Integer
    ASTNode* Parser::parseArrayVariablePlacement() {
        auto temporaryType = dynamic_cast<ASTNodeTypeDecl *>(parseType(-3));
        if (temporaryType == nullptr) throwParseError("invalid type used in variable declaration", -1);
        SCOPE_EXIT( delete temporaryType; );

        auto name = getValue<std::string>(-2);
        auto size = parseMathematicalExpression();

        if (!MATCHES(sequence(SEPARATOR_SQUAREBRACKETCLOSE)))
            throwParseError("expected closing ']' at end of array declaration", -1);

        if (!MATCHES(sequence(OPERATOR_AT)))
            throwParseError("expected placement instruction", -1);

        return new ASTNodeArrayVariableDecl(name, temporaryType->getType()->clone(), size, parseMathematicalExpression());
    }

    // (parseType) *Identifier : (parseType) @ Integer
    ASTNode* Parser::parsePointerVariablePlacement() {
        auto name = getValue<std::string>(-2);

        auto temporaryPointerType = dynamic_cast<ASTNodeTypeDecl *>(parseType(-4));
        if (temporaryPointerType == nullptr) throwParseError("invalid type used in variable declaration", -1);
        SCOPE_EXIT( delete temporaryPointerType; );

        if (!MATCHES((optional(KEYWORD_BE), optional(KEYWORD_LE)) && sequence(VALUETYPE_UNSIGNED)))
            throwParseError("expected unsigned builtin type as size", -1);

        auto temporaryPointerSizeType = dynamic_cast<ASTNodeTypeDecl *>(parseType(-1));
        if (temporaryPointerSizeType == nullptr) throwParseError("invalid size type used in pointer declaration", -1);
        SCOPE_EXIT( delete temporaryPointerSizeType; );

        if (!MATCHES(sequence(OPERATOR_AT)))
            throwParseError("expected placement instruction", -1);

        return new ASTNodePointerVariableDecl(name, temporaryPointerType->getType()->clone(), temporaryPointerSizeType->getType()->clone(), parseMathematicalExpression());
    }



    /* Program */

    // <(parseUsingDeclaration)|(parseVariablePlacement)|(parseStruct)>
    ASTNode* Parser::parseStatement() {
        ASTNode *statement;

        if (MATCHES(sequence(KEYWORD_USING, IDENTIFIER, OPERATOR_ASSIGNMENT) && (optional(KEYWORD_BE), optional(KEYWORD_LE)) && variant(IDENTIFIER, VALUETYPE_ANY)))
            statement = dynamic_cast<ASTNodeTypeDecl*>(parseUsingDeclaration());
        else if (MATCHES((optional(KEYWORD_BE), optional(KEYWORD_LE)) && variant(IDENTIFIER, VALUETYPE_ANY) && sequence(IDENTIFIER, SEPARATOR_SQUAREBRACKETOPEN)))
            statement = parseArrayVariablePlacement();
        else if (MATCHES((optional(KEYWORD_BE), optional(KEYWORD_LE)) && variant(IDENTIFIER, VALUETYPE_ANY) && sequence(IDENTIFIER, OPERATOR_AT)))
            statement = parseVariablePlacement();
        else if (MATCHES((optional(KEYWORD_BE), optional(KEYWORD_LE)) && variant(IDENTIFIER, VALUETYPE_ANY) && sequence(OPERATOR_STAR, IDENTIFIER, OPERATOR_INHERIT)))
            statement = parsePointerVariablePlacement();
        else if (MATCHES(sequence(KEYWORD_STRUCT, IDENTIFIER, SEPARATOR_CURLYBRACKETOPEN)))
            statement = parseStruct();
        else if (MATCHES(sequence(KEYWORD_UNION, IDENTIFIER, SEPARATOR_CURLYBRACKETOPEN)))
            statement = parseUnion();
        else if (MATCHES(sequence(KEYWORD_ENUM, IDENTIFIER, OPERATOR_INHERIT) && (optional(KEYWORD_BE), optional(KEYWORD_LE)) && sequence(VALUETYPE_UNSIGNED, SEPARATOR_CURLYBRACKETOPEN)))
            statement = parseEnum();
        else if (MATCHES(sequence(KEYWORD_BITFIELD, IDENTIFIER, SEPARATOR_CURLYBRACKETOPEN)))
            statement = parseBitfield();
        else throwParseError("invalid sequence", 0);

        if (!MATCHES(sequence(SEPARATOR_ENDOFEXPRESSION)))
            throwParseError("missing ';' at end of expression", -1);

        if (auto typeDecl = dynamic_cast<ASTNodeTypeDecl*>(statement); typeDecl != nullptr)
            this->m_types.insert({ typeDecl->getName().data(), typeDecl });

        return statement;
    }

    // <(parseStatement)...> EndOfProgram
    std::optional<std::vector<ASTNode*>> Parser::parse(const std::vector<Token> &tokens) {
        this->m_curr = tokens.begin();

        this->m_types.clear();

        try {
            auto program = parseTillToken(SEPARATOR_ENDOFPROGRAM);

            if (program.empty() || this->m_curr != tokens.end())
                throwParseError("program is empty!", -1);

            return program;
        } catch (ParseError &e) {
            this->m_error = e;
        }

        return { };
    }

}