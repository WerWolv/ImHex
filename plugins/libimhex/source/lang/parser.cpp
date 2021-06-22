#include <hex/lang/parser.hpp>

#include <optional>
#include <variant>

#define MATCHES(x) (begin() && x)

#define TO_NUMERIC_EXPRESSION(node) new ASTNodeNumericExpression((node), new ASTNodeIntegerLiteral(s32(0)), Token::Operator::Plus)

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
        auto paramCleanup = SCOPE_GUARD {
            for (auto &param : params)
                delete param;
        };

        while (!MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE))) {
            if (MATCHES(sequence(STRING)))
                params.push_back(parseStringLiteral());
            else
                params.push_back(parseMathematicalExpression());

            if (MATCHES(sequence(SEPARATOR_COMMA, SEPARATOR_ROUNDBRACKETCLOSE)))
                throwParseError("unexpected ',' at end of function parameter list", -1);
            else if (MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE)))
                break;
            else if (!MATCHES(sequence(SEPARATOR_COMMA)))
                throwParseError("missing ',' between parameters", -1);

        }

        paramCleanup.release();

        return new ASTNodeFunctionCall(functionName, params);
    }

    ASTNode* Parser::parseStringLiteral() {
        return new ASTNodeStringLiteral(getValue<std::string>(-1));
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
    ASTNode* Parser::parseRValue(ASTNodeRValue::Path &path) {
        if (peek(IDENTIFIER, -1))
            path.push_back(getValue<std::string>(-1));
        else if (peek(KEYWORD_PARENT, -1))
            path.emplace_back("parent");

        if (MATCHES(sequence(SEPARATOR_SQUAREBRACKETOPEN))) {
            path.push_back(parseMathematicalExpression());
            if (!MATCHES(sequence(SEPARATOR_SQUAREBRACKETCLOSE)))
                throwParseError("expected closing ']' at end of array indexing");
        }

        if (MATCHES(sequence(SEPARATOR_DOT))) {
            if (MATCHES(oneOf(IDENTIFIER, KEYWORD_PARENT)))
                return this->parseRValue(path);
            else
                throwParseError("expected member name or 'parent' keyword", -1);
        } else
            return TO_NUMERIC_EXPRESSION(new ASTNodeRValue(path));
    }

    // <Integer|((parseMathematicalExpression))>
    ASTNode* Parser::parseFactor() {
        if (MATCHES(sequence(INTEGER)))
            return TO_NUMERIC_EXPRESSION(new ASTNodeIntegerLiteral(getValue<Token::IntegerLiteral>(-1)));
        else if (MATCHES(sequence(SEPARATOR_ROUNDBRACKETOPEN))) {
            auto node = this->parseMathematicalExpression();
            if (!MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE))) {
                delete node;
                throwParseError("expected closing parenthesis");
            }
            return node;
        } else if (MATCHES(sequence(IDENTIFIER, SEPARATOR_SCOPE_RESOLUTION))) {
            std::vector<std::string> path;
            this->m_curr--;
            return this->parseScopeResolution(path);
        } else if (MATCHES(sequence(IDENTIFIER, SEPARATOR_ROUNDBRACKETOPEN))) {
            return TO_NUMERIC_EXPRESSION(this->parseFunctionCall());
        } else if (MATCHES(oneOf(IDENTIFIER, KEYWORD_PARENT))) {
            ASTNodeRValue::Path path;
            return TO_NUMERIC_EXPRESSION(this->parseRValue(path));
        } else if (MATCHES(sequence(OPERATOR_DOLLAR))) {
            return TO_NUMERIC_EXPRESSION(new ASTNodeRValue({ "$" }));
        } else if (MATCHES(oneOf(OPERATOR_ADDRESSOF, OPERATOR_SIZEOF) && sequence(SEPARATOR_ROUNDBRACKETOPEN))) {
            auto op = getValue<Token::Operator>(-2);

            if (!MATCHES(oneOf(IDENTIFIER, KEYWORD_PARENT))) {
                throwParseError("expected rvalue identifier");
            }

            ASTNodeRValue::Path path;
            auto node = new ASTNodeTypeOperator(op, this->parseRValue(path));
            if (!MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE))) {
                delete node;
                throwParseError("expected closing parenthesis");
            }
            return TO_NUMERIC_EXPRESSION(node);
        } else
            throwParseError("expected integer or parenthesis");
    }

    // <+|-|!|~> (parseFactor)
    ASTNode* Parser::parseUnaryExpression() {
        if (MATCHES(oneOf(OPERATOR_PLUS, OPERATOR_MINUS, OPERATOR_BOOLNOT, OPERATOR_BITNOT))) {
            auto op = getValue<Token::Operator>(-1);

            return new ASTNodeNumericExpression(new ASTNodeIntegerLiteral(0), this->parseFactor(), op);
        }

        return this->parseFactor();
    }

    // (parseUnaryExpression) <*|/|%> (parseUnaryExpression)
    ASTNode* Parser::parseMultiplicativeExpression() {
        auto node = this->parseUnaryExpression();

        auto nodeCleanup = SCOPE_GUARD { delete node; };

        while (MATCHES(oneOf(OPERATOR_STAR, OPERATOR_SLASH, OPERATOR_PERCENT))) {
            auto op = getValue<Token::Operator>(-1);
            node = new ASTNodeNumericExpression(node, this->parseUnaryExpression(), op);
        }

        nodeCleanup.release();

        return node;
    }

    // (parseMultiplicativeExpression) <+|-> (parseMultiplicativeExpression)
    ASTNode* Parser::parseAdditiveExpression() {
        auto node = this->parseMultiplicativeExpression();

        auto nodeCleanup = SCOPE_GUARD { delete node; };

        while (MATCHES(variant(OPERATOR_PLUS, OPERATOR_MINUS))) {
            auto op = getValue<Token::Operator>(-1);
            node = new ASTNodeNumericExpression(node, this->parseMultiplicativeExpression(), op);
        }

        nodeCleanup.release();

        return node;
    }

    // (parseAdditiveExpression) < >>|<< > (parseAdditiveExpression)
    ASTNode* Parser::parseShiftExpression() {
        auto node = this->parseAdditiveExpression();

        auto nodeCleanup = SCOPE_GUARD { delete node; };

        while (MATCHES(variant(OPERATOR_SHIFTLEFT, OPERATOR_SHIFTRIGHT))) {
            auto op = getValue<Token::Operator>(-1);
            node = new ASTNodeNumericExpression(node, this->parseAdditiveExpression(), op);
        }

        nodeCleanup.release();

        return node;
    }

    // (parseAdditiveExpression) < >=|<=|>|< > (parseAdditiveExpression)
    ASTNode* Parser::parseRelationExpression() {
        auto node = this->parseShiftExpression();

        auto nodeCleanup = SCOPE_GUARD{ delete node; };

        while (MATCHES(sequence(OPERATOR_BOOLGREATERTHAN) || sequence(OPERATOR_BOOLLESSTHAN) || sequence(OPERATOR_BOOLGREATERTHANOREQUALS) || sequence(OPERATOR_BOOLLESSTHANOREQUALS))) {
            auto op = getValue<Token::Operator>(-1);
            node = new ASTNodeNumericExpression(node, this->parseShiftExpression(), op);
        }

        nodeCleanup.release();

        return node;
    }

    // (parseRelationExpression) <==|!=> (parseRelationExpression)
    ASTNode* Parser::parseEqualityExpression() {
        auto node = this->parseRelationExpression();

        auto nodeCleanup = SCOPE_GUARD { delete node; };

        while (MATCHES(sequence(OPERATOR_BOOLEQUALS) || sequence(OPERATOR_BOOLNOTEQUALS))) {
            auto op = getValue<Token::Operator>(-1);
            node = new ASTNodeNumericExpression(node, this->parseRelationExpression(), op);
        }

        nodeCleanup.release();

        return node;
    }

    // (parseEqualityExpression) & (parseEqualityExpression)
    ASTNode* Parser::parseBinaryAndExpression() {
        auto node = this->parseEqualityExpression();

        auto nodeCleanup = SCOPE_GUARD { delete node; };

        while (MATCHES(sequence(OPERATOR_BITAND))) {
            node = new ASTNodeNumericExpression(node, this->parseEqualityExpression(), Token::Operator::BitAnd);
        }

        nodeCleanup.release();

        return node;
    }

    // (parseBinaryAndExpression) ^ (parseBinaryAndExpression)
    ASTNode* Parser::parseBinaryXorExpression() {
        auto node = this->parseBinaryAndExpression();

        auto nodeCleanup = SCOPE_GUARD { delete node; };

        while (MATCHES(sequence(OPERATOR_BITXOR))) {
            node = new ASTNodeNumericExpression(node, this->parseBinaryAndExpression(), Token::Operator::BitXor);
        }

        nodeCleanup.release();

        return node;
    }

    // (parseBinaryXorExpression) | (parseBinaryXorExpression)
    ASTNode* Parser::parseBinaryOrExpression() {
        auto node = this->parseBinaryXorExpression();

        auto nodeCleanup = SCOPE_GUARD { delete node; };

        while (MATCHES(sequence(OPERATOR_BITOR))) {
            node = new ASTNodeNumericExpression(node, this->parseBinaryXorExpression(), Token::Operator::BitOr);
        }

        nodeCleanup.release();

        return node;
    }

    // (parseBinaryOrExpression) && (parseBinaryOrExpression)
    ASTNode* Parser::parseBooleanAnd() {
        auto node = this->parseBinaryOrExpression();

        auto nodeCleanup = SCOPE_GUARD { delete node; };

        while (MATCHES(sequence(OPERATOR_BOOLAND))) {
            node = new ASTNodeNumericExpression(node, this->parseBinaryOrExpression(), Token::Operator::BitOr);
        }

        nodeCleanup.release();

        return node;
    }

    // (parseBooleanAnd) ^^ (parseBooleanAnd)
    ASTNode* Parser::parseBooleanXor() {
        auto node = this->parseBooleanAnd();

        auto nodeCleanup = SCOPE_GUARD { delete node; };

        while (MATCHES(sequence(OPERATOR_BOOLXOR))) {
            node = new ASTNodeNumericExpression(node, this->parseBooleanAnd(), Token::Operator::BitOr);
        }

        nodeCleanup.release();

        return node;
    }

    // (parseBooleanXor) || (parseBooleanXor)
    ASTNode* Parser::parseBooleanOr() {
        auto node = this->parseBooleanXor();

        auto nodeCleanup = SCOPE_GUARD { delete node; };

        while (MATCHES(sequence(OPERATOR_BOOLOR))) {
            node = new ASTNodeNumericExpression(node, this->parseBooleanXor(), Token::Operator::BitOr);
        }

        nodeCleanup.release();

        return node;
    }

    // (parseBooleanOr) ? (parseBooleanOr) : (parseBooleanOr)
    ASTNode* Parser::parseTernaryConditional() {
        auto node = this->parseBooleanOr();

        auto nodeCleanup = SCOPE_GUARD { delete node; };

        while (MATCHES(sequence(OPERATOR_TERNARYCONDITIONAL))) {
            auto second = this->parseBooleanOr();

            if (!MATCHES(sequence(OPERATOR_INHERIT)))
                throwParseError("expected ':' in ternary expression");

            auto third = this->parseBooleanOr();
            node = TO_NUMERIC_EXPRESSION(new ASTNodeTernaryExpression(node, second, third, Token::Operator::TernaryConditional));
        }

        nodeCleanup.release();

        return node;
    }

    // (parseTernaryConditional)
    ASTNode* Parser::parseMathematicalExpression() {
        return this->parseTernaryConditional();
    }

    // [[ <Identifier[( (parseStringLiteral) )], ...> ]]
    void Parser::parseAttribute(Attributable *currNode) {
        if (currNode == nullptr)
            throwParseError("tried to apply attribute to invalid statement");

        do {
            if (!MATCHES(sequence(IDENTIFIER)))
                throwParseError("expected attribute expression");

            auto attribute = this->getValue<std::string>(-1);

            if (MATCHES(sequence(SEPARATOR_ROUNDBRACKETOPEN, STRING, SEPARATOR_ROUNDBRACKETCLOSE))) {
                auto value = this->getValue<std::string>(-2);
                currNode->addAttribute(new ASTNodeAttribute(attribute, value));
            }
            else
                currNode->addAttribute(new ASTNodeAttribute(attribute));

        } while (MATCHES(sequence(SEPARATOR_COMMA)));

        if (!MATCHES(sequence(SEPARATOR_SQUAREBRACKETCLOSE, SEPARATOR_SQUAREBRACKETCLOSE)))
            throwParseError("unfinished attribute. Expected ']]'");
    }

    /* Functions */

    ASTNode* Parser::parseFunctionDefintion() {
        const auto &functionName = getValue<std::string>(-2);
        std::vector<std::string> params;

        // Parse parameter list
        bool hasParams = MATCHES(sequence(IDENTIFIER));
        while (hasParams) {
            params.push_back(getValue<std::string>(-1));

            if (!MATCHES(sequence(SEPARATOR_COMMA))) {
                if (MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE)))
                    break;
                else
                    throwParseError("expected closing ')' after parameter list");
            }
        }
        if (!hasParams) {
            if (!MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE)))
                throwParseError("expected closing ')' after parameter list");
        }

        if (!MATCHES(sequence(SEPARATOR_CURLYBRACKETOPEN)))
            throwParseError("expected opening '{' after function definition");


        // Parse function body
        std::vector<ASTNode*> body;
        auto bodyCleanup = SCOPE_GUARD {
            for (auto &node : body)
                delete node;
        };

        while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
            body.push_back(this->parseFunctionStatement());
        }

        bodyCleanup.release();
        return new ASTNodeFunctionDefinition(functionName, params, body);
    }

    ASTNode* Parser::parseFunctionStatement() {
        bool needsSemicolon = true;
        ASTNode *statement;

        if (MATCHES(sequence(IDENTIFIER, SEPARATOR_ROUNDBRACKETOPEN)))
            statement = parseFunctionCall();
        else if (MATCHES((optional(KEYWORD_BE), optional(KEYWORD_LE)) && variant(IDENTIFIER, VALUETYPE_ANY) && sequence(IDENTIFIER)))
            statement = parseMemberVariable();
        else if (MATCHES(sequence(IDENTIFIER, OPERATOR_ASSIGNMENT)))
            statement = parseFunctionVariableAssignment();
        else if (MATCHES(sequence(KEYWORD_RETURN)))
            statement = parseFunctionReturnStatement();
        else if (MATCHES(sequence(KEYWORD_IF, SEPARATOR_ROUNDBRACKETOPEN))) {
            statement = parseFunctionConditional();
            needsSemicolon = false;
        } else if (MATCHES(sequence(KEYWORD_WHILE, SEPARATOR_ROUNDBRACKETOPEN))) {
            statement = parseFunctionWhileLoop();
            needsSemicolon = false;
        } else
            throwParseError("invalid sequence", 0);

        if (needsSemicolon && !MATCHES(sequence(SEPARATOR_ENDOFEXPRESSION))) {
            delete statement;
            throwParseError("missing ';' at end of expression", -1);
        }

        return statement;
    }

    ASTNode* Parser::parseFunctionVariableAssignment() {
        const auto &lvalue = getValue<std::string>(-2);

        auto rvalue = this->parseMathematicalExpression();

        return new ASTNodeAssignment(lvalue, rvalue);
    }

    ASTNode* Parser::parseFunctionReturnStatement() {
        if (peek(SEPARATOR_ENDOFEXPRESSION))
            return new ASTNodeReturnStatement(nullptr);
        else
            return new ASTNodeReturnStatement(this->parseMathematicalExpression());
    }

    ASTNode* Parser::parseFunctionConditional() {
        auto condition = parseMathematicalExpression();
        std::vector<ASTNode*> trueBody, falseBody;

        auto cleanup = SCOPE_GUARD {
            delete condition;
            for (auto &statement : trueBody)
                delete statement;
            for (auto &statement : falseBody)
                delete statement;
        };

        if (MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE, SEPARATOR_CURLYBRACKETOPEN))) {
            while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
                trueBody.push_back(parseFunctionStatement());
            }
        } else if (MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE))) {
            trueBody.push_back(parseFunctionStatement());
        } else
            throwParseError("expected body of conditional statement");

        if (MATCHES(sequence(KEYWORD_ELSE, SEPARATOR_CURLYBRACKETOPEN))) {
            while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
                falseBody.push_back(parseFunctionStatement());
            }
        } else if (MATCHES(sequence(KEYWORD_ELSE))) {
            falseBody.push_back(parseFunctionStatement());
        }

        cleanup.release();

        return new ASTNodeConditionalStatement(condition, trueBody, falseBody);
    }

    ASTNode* Parser::parseFunctionWhileLoop() {
        auto condition = parseMathematicalExpression();
        std::vector<ASTNode*> body;

        auto cleanup = SCOPE_GUARD {
            delete condition;
            for (auto &statement : body)
                delete statement;
        };

        if (MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE, SEPARATOR_CURLYBRACKETOPEN))) {
            while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
                body.push_back(parseFunctionStatement());
            }
        } else if (MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE))) {
            body.push_back(parseFunctionStatement());
        } else
            throwParseError("expected body of conditional statement");

        cleanup.release();

        return new ASTNodeWhileStatement(condition, body);
    }

    /* Control flow */

    // if ((parseMathematicalExpression)) { (parseMember) }
    ASTNode* Parser::parseConditional() {
        auto condition = parseMathematicalExpression();
        std::vector<ASTNode*> trueBody, falseBody;

        auto cleanup = SCOPE_GUARD {
            delete condition;
            for (auto &statement : trueBody)
                delete statement;
            for (auto &statement : falseBody)
                delete statement;
        };

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

    // while ((parseMathematicalExpression))
    ASTNode* Parser::parseWhileStatement() {
        auto condition = parseMathematicalExpression();

        auto cleanup = SCOPE_GUARD {
            delete condition;
        };

        if (!MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE)))
            throwParseError("expected closing ')' after while head");

        cleanup.release();

        return new ASTNodeWhileStatement(condition, { });
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
        auto *type = dynamic_cast<ASTNodeTypeDecl *>(parseType(-1));
        if (type == nullptr) throwParseError("invalid type used in variable declaration", -1);

        if (peekOptional(KEYWORD_BE) || peekOptional(KEYWORD_LE))
            return new ASTNodeTypeDecl(getValue<std::string>(-4), type, type->getEndian());
        else
            return new ASTNodeTypeDecl(getValue<std::string>(-3), type, type->getEndian());
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
        auto type = dynamic_cast<ASTNodeTypeDecl *>(parseType(-2));
        if (type == nullptr) throwParseError("invalid type used in variable declaration", -1);

        return new ASTNodeVariableDecl(getValue<std::string>(-1), type);
    }

    // (parseType) Identifier[(parseMathematicalExpression)]
    ASTNode* Parser::parseMemberArrayVariable() {
        auto type = dynamic_cast<ASTNodeTypeDecl *>(parseType(-3));
        if (type == nullptr) throwParseError("invalid type used in variable declaration", -1);

        auto name = getValue<std::string>(-2);

        ASTNode *size = nullptr;
        auto sizeCleanup = SCOPE_GUARD { delete size; };

        if (!MATCHES(sequence(SEPARATOR_SQUAREBRACKETCLOSE))) {
            if (MATCHES(sequence(KEYWORD_WHILE, SEPARATOR_ROUNDBRACKETOPEN)))
                size = parseWhileStatement();
            else
                size = parseMathematicalExpression();

            if (!MATCHES(sequence(SEPARATOR_SQUAREBRACKETCLOSE)))
                throwParseError("expected closing ']' at end of array declaration", -1);
        }

        sizeCleanup.release();

        return new ASTNodeArrayVariableDecl(name, type, size);
    }

    // (parseType) *Identifier : (parseType)
    ASTNode* Parser::parseMemberPointerVariable() {
        auto name = getValue<std::string>(-2);

        auto pointerType = dynamic_cast<ASTNodeTypeDecl *>(parseType(-4));
        if (pointerType == nullptr) throwParseError("invalid type used in variable declaration", -1);

        if (!MATCHES((optional(KEYWORD_BE), optional(KEYWORD_LE)) && sequence(VALUETYPE_UNSIGNED)))
            throwParseError("expected unsigned builtin type as size", -1);

        auto sizeType = dynamic_cast<ASTNodeTypeDecl *>(parseType(-1));
        if (sizeType == nullptr) throwParseError("invalid type used for pointer size", -1);

        return new ASTNodePointerVariableDecl(name, pointerType, sizeType);
    }

    // [(parsePadding)|(parseMemberVariable)|(parseMemberArrayVariable)|(parseMemberPointerVariable)]
    ASTNode* Parser::parseMember() {
        ASTNode *member;

        if (MATCHES(sequence(VALUETYPE_PADDING, SEPARATOR_SQUAREBRACKETOPEN)))
            member = parsePadding();
        else if (MATCHES((optional(KEYWORD_BE), optional(KEYWORD_LE)) && variant(IDENTIFIER, VALUETYPE_ANY) && sequence(IDENTIFIER, SEPARATOR_SQUAREBRACKETOPEN) && sequence<Not>(SEPARATOR_SQUAREBRACKETOPEN)))
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

        if (MATCHES(sequence(SEPARATOR_SQUAREBRACKETOPEN, SEPARATOR_SQUAREBRACKETOPEN)))
            parseAttribute(dynamic_cast<Attributable *>(member));

        if (!MATCHES(sequence(SEPARATOR_ENDOFEXPRESSION)))
            throwParseError("missing ';' at end of expression", -1);

        return member;
    }

    // struct Identifier { <(parseMember)...> }
    ASTNode* Parser::parseStruct() {
        const auto structNode = new ASTNodeStruct();
        const auto &typeName = getValue<std::string>(-2);
        auto structGuard = SCOPE_GUARD { delete structNode; };

        if (this->m_types.contains(typeName))
            throwParseError(hex::format("redefinition of type '{}'", typeName));

        this->m_types.insert({ typeName, new ASTNodeTypeDecl(typeName, nullptr) });

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
        auto unionGuard = SCOPE_GUARD { delete unionNode; };

        if (this->m_types.contains(typeName))
            throwParseError(hex::format("redefinition of type '{}'", typeName));

        this->m_types.insert({ typeName, new ASTNodeTypeDecl(typeName, nullptr) });

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

        auto underlyingType = dynamic_cast<ASTNodeTypeDecl*>(parseType(-2));
        if (underlyingType == nullptr) throwParseError("failed to parse type", -2);
        if (underlyingType->getEndian().has_value()) throwParseError("underlying type may not have an endian specification", -2);

        const auto enumNode = new ASTNodeEnum(underlyingType);
        auto enumGuard = SCOPE_GUARD { delete enumNode; };

        if (this->m_types.contains(typeName))
            throwParseError(hex::format("redefinition of type '{}'", typeName));

        this->m_types.insert({ typeName, new ASTNodeTypeDecl(typeName, nullptr) });

        ASTNode *lastEntry = nullptr;
        while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
            if (MATCHES(sequence(IDENTIFIER, OPERATOR_ASSIGNMENT))) {
                auto name = getValue<std::string>(-2);
                auto value = parseMathematicalExpression();

                enumNode->addEntry(name, value);
                lastEntry = value;
            }
            else if (MATCHES(sequence(IDENTIFIER))) {
                ASTNode *valueExpr;
                auto name = getValue<std::string>(-1);
                if (enumNode->getEntries().empty())
                    valueExpr = lastEntry = TO_NUMERIC_EXPRESSION(new ASTNodeIntegerLiteral(u8(0)));
                else
                    valueExpr = new ASTNodeNumericExpression(lastEntry->clone(), new ASTNodeIntegerLiteral(s32(1)), Token::Operator::Plus);

                enumNode->addEntry(name, valueExpr);
            }
            else if (MATCHES(sequence(SEPARATOR_ENDOFPROGRAM)))
                throwParseError("unexpected end of program", -2);
            else
                throwParseError("invalid enum entry", -1);

            if (!MATCHES(sequence(SEPARATOR_COMMA))) {
                if (MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE)))
                    break;
                else
                    throwParseError("missing ',' between enum entries", -1);
            }
        }

        enumGuard.release();

        return new ASTNodeTypeDecl(typeName, enumNode);
    }

    // bitfield Identifier { <Identifier : (parseMathematicalExpression)[;]...> }
    ASTNode* Parser::parseBitfield() {
        std::string typeName = getValue<std::string>(-2);

        const auto bitfieldNode = new ASTNodeBitfield();
        auto enumGuard = SCOPE_GUARD { delete bitfieldNode; };

        if (this->m_types.contains(typeName))
            throwParseError(hex::format("redefinition of type '{}'", typeName));

        this->m_types.insert({ typeName, new ASTNodeTypeDecl(typeName, nullptr) });

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
        auto type = dynamic_cast<ASTNodeTypeDecl *>(parseType(-3));
        if (type == nullptr) throwParseError("invalid type used in variable declaration", -1);

        return new ASTNodeVariableDecl(getValue<std::string>(-2), type, parseMathematicalExpression());
    }

    // (parseType) Identifier[[(parseMathematicalExpression)]] @ Integer
    ASTNode* Parser::parseArrayVariablePlacement() {
        auto type = dynamic_cast<ASTNodeTypeDecl *>(parseType(-3));
        if (type == nullptr) throwParseError("invalid type used in variable declaration", -1);

        auto name = getValue<std::string>(-2);

        ASTNode *size = nullptr;
        auto sizeCleanup = SCOPE_GUARD { delete size; };

        if (!MATCHES(sequence(SEPARATOR_SQUAREBRACKETCLOSE))) {
            if (MATCHES(sequence(KEYWORD_WHILE, SEPARATOR_ROUNDBRACKETOPEN)))
                size = parseWhileStatement();
            else
                size = parseMathematicalExpression();

            if (!MATCHES(sequence(SEPARATOR_SQUAREBRACKETCLOSE)))
                throwParseError("expected closing ']' at end of array declaration", -1);
        }

        if (!MATCHES(sequence(OPERATOR_AT)))
            throwParseError("expected placement instruction", -1);

        sizeCleanup.release();

        return new ASTNodeArrayVariableDecl(name, type, size, parseMathematicalExpression());
    }

    // (parseType) *Identifier : (parseType) @ Integer
    ASTNode* Parser::parsePointerVariablePlacement() {
        auto name = getValue<std::string>(-2);

        auto temporaryPointerType = dynamic_cast<ASTNodeTypeDecl *>(parseType(-4));
        if (temporaryPointerType == nullptr) throwParseError("invalid type used in variable declaration", -1);

        if (!MATCHES((optional(KEYWORD_BE), optional(KEYWORD_LE)) && sequence(VALUETYPE_UNSIGNED)))
            throwParseError("expected unsigned builtin type as size", -1);

        auto temporaryPointerSizeType = dynamic_cast<ASTNodeTypeDecl *>(parseType(-1));
        if (temporaryPointerSizeType == nullptr) throwParseError("invalid size type used in pointer declaration", -1);

        if (!MATCHES(sequence(OPERATOR_AT)))
            throwParseError("expected placement instruction", -1);

        return new ASTNodePointerVariableDecl(name, temporaryPointerType, temporaryPointerSizeType, parseMathematicalExpression());
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
        else if (MATCHES(sequence(IDENTIFIER, SEPARATOR_ROUNDBRACKETOPEN)))
            statement = parseFunctionCall();
        else if (MATCHES(sequence(KEYWORD_FUNCTION, IDENTIFIER, SEPARATOR_ROUNDBRACKETOPEN)))
            statement = parseFunctionDefintion();
        else throwParseError("invalid sequence", 0);

        if (MATCHES(sequence(SEPARATOR_SQUAREBRACKETOPEN, SEPARATOR_SQUAREBRACKETOPEN)))
            parseAttribute(dynamic_cast<Attributable *>(statement));

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