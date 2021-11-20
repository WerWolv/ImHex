#include <hex/pattern_language/parser.hpp>

#include <optional>

#define MATCHES(x) (begin() && x)

// Definition syntax:
// [A]          : Either A or no token
// [A|B]        : Either A, B or no token
// <A|B>        : Either A or B
// <A...>       : One or more of A
// A B C        : Sequence of tokens A then B then C
// (parseXXXX)  : Parsing handled by other function
namespace hex::pl {

    /* Mathematical expressions */

    // Identifier([(parseMathematicalExpression)|<(parseMathematicalExpression),...>(parseMathematicalExpression)]
    ASTNode* Parser::parseFunctionCall() {
        std::string functionName = parseNamespaceResolution();

        if (!MATCHES(sequence(SEPARATOR_ROUNDBRACKETOPEN)))
            throwParseError("expected '(' after function name");

        std::vector<ASTNode*> params;
        auto paramCleanup = SCOPE_GUARD {
            for (auto &param : params)
                delete param;
        };

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

        return create(new ASTNodeFunctionCall(functionName, params));
    }

    ASTNode* Parser::parseStringLiteral() {
        return create(new ASTNodeLiteral(getValue<Token::Literal>(-1)));
    }

    std::string Parser::parseNamespaceResolution() {
        std::string name;

        while (true) {
            name += getValue<Token::Identifier>(-1).get();

            if (MATCHES(sequence(OPERATOR_SCOPERESOLUTION, IDENTIFIER))) {
                name += "::";
                continue;
            }
            else
                break;
        }

        return name;
    }

    ASTNode* Parser::parseScopeResolution() {
        std::string typeName;

        while (true) {
            typeName += getValue<Token::Identifier>(-1).get();

            if (MATCHES(sequence(OPERATOR_SCOPERESOLUTION, IDENTIFIER))) {
                if (peek(OPERATOR_SCOPERESOLUTION, 0) && peek(IDENTIFIER, 1)) {
                    typeName += "::";
                   continue;
                } else {
                    if (!this->m_types.contains(typeName))
                        throwParseError(hex::format("cannot access scope of invalid type '{}'", typeName), -1);

                    return create(new ASTNodeScopeResolution(this->m_types[typeName]->clone(), getValue<Token::Identifier>(-1).get()));
                }
            }
            else
                break;
        }

        throwParseError("failed to parse scope resolution. Expected 'TypeName::Identifier'");
    }

    // <Identifier[.]...>
    ASTNode* Parser::parseRValue(ASTNodeRValue::Path &path) {
        if (peek(IDENTIFIER, -1))
            path.push_back(getValue<Token::Identifier>(-1).get());
        else if (peek(KEYWORD_PARENT, -1))
            path.emplace_back("parent");
        else if (peek(KEYWORD_THIS, -1))
            path.emplace_back("this");

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
            return create(new ASTNodeRValue(path));
    }

    // <Integer|((parseMathematicalExpression))>
    ASTNode* Parser::parseFactor() {
        if (MATCHES(sequence(INTEGER)))
            return new ASTNodeLiteral(getValue<Token::Literal>(-1));
        else if (peek(OPERATOR_PLUS) || peek(OPERATOR_MINUS) || peek(OPERATOR_BITNOT) || peek(OPERATOR_BOOLNOT))
            return this->parseMathematicalExpression();
        else if (MATCHES(sequence(SEPARATOR_ROUNDBRACKETOPEN))) {
            auto node = this->parseMathematicalExpression();
            if (!MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE))) {
                delete node;
                throwParseError("expected closing parenthesis");
            }
            return node;
        } else if (MATCHES(sequence(IDENTIFIER))) {
            auto originalPos = this->m_curr;
            parseNamespaceResolution();
            bool isFunction = peek(SEPARATOR_ROUNDBRACKETOPEN);
            this->m_curr = originalPos;


            if (isFunction) {
                return this->parseFunctionCall();
            } else if (peek(OPERATOR_SCOPERESOLUTION, 0)) {
                return this->parseScopeResolution();
            } else {
                ASTNodeRValue::Path path;
                return this->parseRValue(path);
            }
        } else if (MATCHES(oneOf(KEYWORD_PARENT, KEYWORD_THIS))) {
            ASTNodeRValue::Path path;
            return this->parseRValue(path);
        } else if (MATCHES(sequence(OPERATOR_DOLLAR))) {
            return new ASTNodeRValue({ "$" });
        } else if (MATCHES(oneOf(OPERATOR_ADDRESSOF, OPERATOR_SIZEOF) && sequence(SEPARATOR_ROUNDBRACKETOPEN))) {
            auto op = getValue<Token::Operator>(-2);

            if (!MATCHES(oneOf(IDENTIFIER, KEYWORD_PARENT, KEYWORD_THIS))) {
                throwParseError("expected rvalue identifier");
            }

            ASTNodeRValue::Path path;
            auto node = create(new ASTNodeTypeOperator(op, this->parseRValue(path)));
            if (!MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE))) {
                delete node;
                throwParseError("expected closing parenthesis");
            }
            return node;
        } else
            throwParseError("expected value or parenthesis");
    }

    ASTNode* Parser::parseCastExpression() {
        if (peek(KEYWORD_BE) || peek(KEYWORD_LE) || peek(VALUETYPE_ANY)) {
            auto type = parseType(true);
            auto builtinType = dynamic_cast<ASTNodeBuiltinType*>(type->getType());

            if (builtinType == nullptr)
                throwParseError("invalid type used for pointer size", -1);

            if (!peek(SEPARATOR_ROUNDBRACKETOPEN))
                throwParseError("expected '(' before cast expression", -1);

            auto node = parseFactor();

            return new ASTNodeCast(node, type);
        } else return parseFactor();
    }

    // <+|-|!|~> (parseFactor)
    ASTNode* Parser::parseUnaryExpression() {
        if (MATCHES(oneOf(OPERATOR_PLUS, OPERATOR_MINUS, OPERATOR_BOOLNOT, OPERATOR_BITNOT))) {
            auto op = getValue<Token::Operator>(-1);

            return create(new ASTNodeMathematicalExpression(new ASTNodeLiteral(0), this->parseCastExpression(), op));
        } else if (MATCHES(sequence(STRING))) {
            return this->parseStringLiteral();
        }

        return this->parseCastExpression();
    }

    // (parseUnaryExpression) <*|/|%> (parseUnaryExpression)
    ASTNode* Parser::parseMultiplicativeExpression() {
        auto node = this->parseUnaryExpression();

        auto nodeCleanup = SCOPE_GUARD { delete node; };

        while (MATCHES(oneOf(OPERATOR_STAR, OPERATOR_SLASH, OPERATOR_PERCENT))) {
            auto op = getValue<Token::Operator>(-1);
            node = create(new ASTNodeMathematicalExpression(node, this->parseUnaryExpression(), op));
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
            node = create(new ASTNodeMathematicalExpression(node, this->parseMultiplicativeExpression(), op));
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
            node = create(new ASTNodeMathematicalExpression(node, this->parseAdditiveExpression(), op));
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
            node = create(new ASTNodeMathematicalExpression(node, this->parseShiftExpression(), op));
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
            node = create(new ASTNodeMathematicalExpression(node, this->parseRelationExpression(), op));
        }

        nodeCleanup.release();

        return node;
    }

    // (parseEqualityExpression) & (parseEqualityExpression)
    ASTNode* Parser::parseBinaryAndExpression() {
        auto node = this->parseEqualityExpression();

        auto nodeCleanup = SCOPE_GUARD { delete node; };

        while (MATCHES(sequence(OPERATOR_BITAND))) {
            node = create(new ASTNodeMathematicalExpression(node, this->parseEqualityExpression(), Token::Operator::BitAnd));
        }

        nodeCleanup.release();

        return node;
    }

    // (parseBinaryAndExpression) ^ (parseBinaryAndExpression)
    ASTNode* Parser::parseBinaryXorExpression() {
        auto node = this->parseBinaryAndExpression();

        auto nodeCleanup = SCOPE_GUARD { delete node; };

        while (MATCHES(sequence(OPERATOR_BITXOR))) {
            node = create(new ASTNodeMathematicalExpression(node, this->parseBinaryAndExpression(), Token::Operator::BitXor));
        }

        nodeCleanup.release();

        return node;
    }

    // (parseBinaryXorExpression) | (parseBinaryXorExpression)
    ASTNode* Parser::parseBinaryOrExpression() {
        auto node = this->parseBinaryXorExpression();

        auto nodeCleanup = SCOPE_GUARD { delete node; };

        while (MATCHES(sequence(OPERATOR_BITOR))) {
            node = create(new ASTNodeMathematicalExpression(node, this->parseBinaryXorExpression(), Token::Operator::BitOr));
        }

        nodeCleanup.release();

        return node;
    }

    // (parseBinaryOrExpression) && (parseBinaryOrExpression)
    ASTNode* Parser::parseBooleanAnd() {
        auto node = this->parseBinaryOrExpression();

        auto nodeCleanup = SCOPE_GUARD { delete node; };

        while (MATCHES(sequence(OPERATOR_BOOLAND))) {
            node = create(new ASTNodeMathematicalExpression(node, this->parseBinaryOrExpression(), Token::Operator::BoolAnd));
        }

        nodeCleanup.release();

        return node;
    }

    // (parseBooleanAnd) ^^ (parseBooleanAnd)
    ASTNode* Parser::parseBooleanXor() {
        auto node = this->parseBooleanAnd();

        auto nodeCleanup = SCOPE_GUARD { delete node; };

        while (MATCHES(sequence(OPERATOR_BOOLXOR))) {
            node = create(new ASTNodeMathematicalExpression(node, this->parseBooleanAnd(), Token::Operator::BoolXor));
        }

        nodeCleanup.release();

        return node;
    }

    // (parseBooleanXor) || (parseBooleanXor)
    ASTNode* Parser::parseBooleanOr() {
        auto node = this->parseBooleanXor();

        auto nodeCleanup = SCOPE_GUARD { delete node; };

        while (MATCHES(sequence(OPERATOR_BOOLOR))) {
            node = create(new ASTNodeMathematicalExpression(node, this->parseBooleanXor(), Token::Operator::BoolOr));
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
            node = create(new ASTNodeTernaryExpression(node, second, third, Token::Operator::TernaryConditional));
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

            auto attribute = getValue<Token::Identifier>(-1).get();

            if (MATCHES(sequence(SEPARATOR_ROUNDBRACKETOPEN, STRING, SEPARATOR_ROUNDBRACKETCLOSE))) {
                auto value = getValue<Token::Literal>(-2);
                auto string = std::get_if<std::string>(&value);

                if (string == nullptr)
                    throwParseError("expected string attribute argument");

                currNode->addAttribute(create(new ASTNodeAttribute(attribute, *string)));
            }
            else
                currNode->addAttribute(create(new ASTNodeAttribute(attribute)));

        } while (MATCHES(sequence(SEPARATOR_COMMA)));

        if (!MATCHES(sequence(SEPARATOR_SQUAREBRACKETCLOSE, SEPARATOR_SQUAREBRACKETCLOSE)))
            throwParseError("unfinished attribute. Expected ']]'");
    }

    /* Functions */

    ASTNode* Parser::parseFunctionDefinition() {
        const auto &functionName = getValue<Token::Identifier>(-2).get();
        std::vector<std::pair<std::string, ASTNode*>> params;

        // Parse parameter list
        bool hasParams = !peek(SEPARATOR_ROUNDBRACKETCLOSE);
        u32 unnamedParamCount = 0;
        while (hasParams) {
            auto type = parseType(true);

            if (MATCHES(sequence(IDENTIFIER)))
                params.emplace_back(getValue<Token::Identifier>(-1).get(), type);
            else {
                params.emplace_back(std::to_string(unnamedParamCount), type);
                unnamedParamCount++;
            }

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
        return create(new ASTNodeFunctionDefinition(getNamespacePrefixedName(functionName), params, body));
    }

    ASTNode* Parser::parseFunctionVariableDecl() {
        ASTNode *statement;
        auto type = parseType(true);

        if (MATCHES(sequence(IDENTIFIER))) {
            auto identifier = getValue<Token::Identifier>(-1).get();
            statement = parseMemberVariable(type);

            if (MATCHES(sequence(OPERATOR_ASSIGNMENT))) {
                auto expression = parseMathematicalExpression();

                statement = create(new ASTNodeCompoundStatement({ statement, create(new ASTNodeAssignment(identifier, expression)) }));
            }
        }
        else
            throwParseError("invalid variable declaration");

        return statement;
    }

    ASTNode* Parser::parseFunctionStatement() {
        bool needsSemicolon = true;
        ASTNode *statement;

        if (MATCHES(sequence(IDENTIFIER, OPERATOR_ASSIGNMENT)))
            statement = parseFunctionVariableAssignment();
        else if (MATCHES(sequence(KEYWORD_RETURN)))
            statement = parseFunctionReturnStatement();
        else if (MATCHES(sequence(KEYWORD_IF, SEPARATOR_ROUNDBRACKETOPEN))) {
            statement = parseFunctionConditional();
            needsSemicolon = false;
        } else if (MATCHES(sequence(KEYWORD_WHILE, SEPARATOR_ROUNDBRACKETOPEN))) {
            statement = parseFunctionWhileLoop();
            needsSemicolon = false;
        } else if (MATCHES(sequence(KEYWORD_FOR, SEPARATOR_ROUNDBRACKETOPEN))) {
            statement = parseFunctionForLoop();
            needsSemicolon = false;
        } else if (MATCHES(sequence(IDENTIFIER))) {
            auto originalPos = this->m_curr;
            parseNamespaceResolution();
            bool isFunction = peek(SEPARATOR_ROUNDBRACKETOPEN);

            if (isFunction) {
                this->m_curr = originalPos;
                statement = parseFunctionCall();
            }
            else {
                this->m_curr = originalPos - 1;
                statement = parseFunctionVariableDecl();
            }
        }
        else if (peek(KEYWORD_BE) || peek(KEYWORD_LE) || peek(VALUETYPE_ANY)) {
            statement = parseFunctionVariableDecl();
        }
        else
            throwParseError("invalid sequence", 0);

        if (needsSemicolon && !MATCHES(sequence(SEPARATOR_ENDOFEXPRESSION))) {
            delete statement;
            throwParseError("missing ';' at end of expression", -1);
        }

        // Consume superfluous semicolons
        while (needsSemicolon && MATCHES(sequence(SEPARATOR_ENDOFEXPRESSION)));

        return statement;
    }

    ASTNode* Parser::parseFunctionVariableAssignment() {
        const auto &lvalue = getValue<Token::Identifier>(-2).get();

        auto rvalue = this->parseMathematicalExpression();

        return create(new ASTNodeAssignment(lvalue, rvalue));
    }

    ASTNode* Parser::parseFunctionReturnStatement() {
        if (peek(SEPARATOR_ENDOFEXPRESSION))
            return create(new ASTNodeReturnStatement(nullptr));
        else
            return create(new ASTNodeReturnStatement(this->parseMathematicalExpression()));
    }

    std::vector<ASTNode*> Parser::parseStatementBody() {
        std::vector<ASTNode*> body;

        auto bodyCleanup = SCOPE_GUARD {
            for (auto &node : body)
                delete node;
        };

        if (MATCHES(sequence(SEPARATOR_CURLYBRACKETOPEN))) {
            while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
                body.push_back(parseFunctionStatement());
            }
        } else {
            body.push_back(parseFunctionStatement());
        }

        bodyCleanup.release();

        return body;
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

        if (!MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE)))
            throwParseError("expected closing ')' after statement head");

        trueBody = parseStatementBody();

        if (MATCHES(sequence(KEYWORD_ELSE)))
            falseBody = parseStatementBody();

        cleanup.release();

        return create(new ASTNodeConditionalStatement(condition, trueBody, falseBody));
    }

    ASTNode* Parser::parseFunctionWhileLoop() {
        auto condition = parseMathematicalExpression();
        std::vector<ASTNode*> body;

        auto cleanup = SCOPE_GUARD {
            delete condition;
            for (auto &statement : body)
                delete statement;
        };

        if (!MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE)))
            throwParseError("expected closing ')' after statement head");

        body = parseStatementBody();

        cleanup.release();

        return create(new ASTNodeWhileStatement(condition, body));
    }

    ASTNode* Parser::parseFunctionForLoop() {
        auto variable = parseFunctionVariableDecl();
        auto variableCleanup = SCOPE_GUARD { delete variable; };

        if (!MATCHES(sequence(SEPARATOR_COMMA)))
            throwParseError("expected ',' after for loop variable declaration");

        auto condition = parseMathematicalExpression();
        auto conditionCleanup = SCOPE_GUARD { delete condition; };

        if (!MATCHES(sequence(SEPARATOR_COMMA)))
            throwParseError("expected ',' after for loop condition");

        if (!MATCHES(sequence(IDENTIFIER, OPERATOR_ASSIGNMENT)))
            throwParseError("expected for loop variable assignment");

        auto postExpression = parseFunctionVariableAssignment();
        auto postExpressionCleanup = SCOPE_GUARD { delete postExpression; };

        std::vector<ASTNode*> body;

        auto bodyCleanup = SCOPE_GUARD {
            for (auto &statement : body)
                delete statement;
        };

        if (!MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE)))
            throwParseError("expected closing ')' after statement head");

        body = parseStatementBody();

        body.push_back(postExpression);

        variableCleanup.release();
        conditionCleanup.release();
        postExpressionCleanup.release();
        bodyCleanup.release();

        return create(new ASTNodeCompoundStatement({ variable, create(new ASTNodeWhileStatement(condition, body)) }, true));
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

        return create(new ASTNodeConditionalStatement(condition, trueBody, falseBody));
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

        return create(new ASTNodeWhileStatement(condition, { }));
    }

    /* Type declarations */

    // [be|le] <Identifier|u8|u16|u32|u64|u128|s8|s16|s32|s64|s128|float|double|str>
    ASTNodeTypeDecl* Parser::parseType(bool allowFunctionTypes) {
        std::optional<std::endian> endian;

        if (MATCHES(sequence(KEYWORD_LE)))
            endian = std::endian::little;
        else if (MATCHES(sequence(KEYWORD_BE)))
            endian = std::endian::big;

        if (MATCHES(sequence(IDENTIFIER))) { // Custom type
            std::string typeName = parseNamespaceResolution();

            if (this->m_types.contains(typeName))
                return create(new ASTNodeTypeDecl({ }, this->m_types[typeName]->clone(), endian));
            else if (this->m_types.contains(getNamespacePrefixedName(typeName)))
                return create(new ASTNodeTypeDecl({ }, this->m_types[getNamespacePrefixedName(typeName)]->clone(), endian));
            else
                throwParseError(hex::format("unknown type '{}'", typeName));
        }
        else if (MATCHES(sequence(VALUETYPE_ANY))) { // Builtin type
            auto type = getValue<Token::ValueType>(-1);
            if (!allowFunctionTypes) {
                if (type == Token::ValueType::String)
                    throwParseError("cannot use 'str' in this context. Use a character array instead");
                else if (type == Token::ValueType::Auto)
                    throwParseError("cannot use 'auto' in this context");
            }

            return create(new ASTNodeTypeDecl({ }, new ASTNodeBuiltinType(type), endian));
        } else throwParseError("failed to parse type. Expected identifier or builtin type");
    }

    // using Identifier = (parseType)
    ASTNode* Parser::parseUsingDeclaration() {
        auto name = parseNamespaceResolution();

        if (!MATCHES(sequence(OPERATOR_ASSIGNMENT)))
            throwParseError("expected '=' after type name of using declaration");

        auto *type = dynamic_cast<ASTNodeTypeDecl *>(parseType());
        if (type == nullptr) throwParseError("invalid type used in variable declaration", -1);

        return addType(name, type, type->getEndian());
    }

    // padding[(parseMathematicalExpression)]
    ASTNode* Parser::parsePadding() {
        auto size = parseMathematicalExpression();

        if (!MATCHES(sequence(SEPARATOR_SQUAREBRACKETCLOSE))) {
            delete size;
            throwParseError("expected closing ']' at end of array declaration", -1);
        }

        return create(new ASTNodeArrayVariableDecl({ }, new ASTNodeTypeDecl({ }, new ASTNodeBuiltinType(Token::ValueType::Padding)), size));
    }

    // (parseType) Identifier
    ASTNode* Parser::parseMemberVariable(ASTNodeTypeDecl *type) {
        if (peek(SEPARATOR_COMMA)) {

            std::vector<ASTNode*> variables;
            auto variableCleanup = SCOPE_GUARD { for (auto var : variables) delete var; };

            do {
                variables.push_back(create(new ASTNodeVariableDecl(getValue<Token::Identifier>(-1).get(), type->clone())));
            } while (MATCHES(sequence(SEPARATOR_COMMA, IDENTIFIER)));
            delete type;

            variableCleanup.release();

            return create(new ASTNodeMultiVariableDecl(variables));
        } else
            return create(new ASTNodeVariableDecl(getValue<Token::Identifier>(-1).get(), type));
    }

    // (parseType) Identifier[(parseMathematicalExpression)]
    ASTNode* Parser::parseMemberArrayVariable(ASTNodeTypeDecl *type) {
        auto name = getValue<Token::Identifier>(-2).get();

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

        return create(new ASTNodeArrayVariableDecl(name, type, size));
    }

    // (parseType) *Identifier : (parseType)
    ASTNode* Parser::parseMemberPointerVariable(ASTNodeTypeDecl *type) {
        auto name = getValue<Token::Identifier>(-2).get();

        auto sizeType = parseType();

        {
            auto builtinType = dynamic_cast<ASTNodeBuiltinType*>(sizeType->getType());

            if (builtinType == nullptr || !Token::isUnsigned(builtinType->getType()))
                throwParseError("invalid type used for pointer size", -1);
        }

        return create(new ASTNodePointerVariableDecl(name, type, sizeType));
    }

    // [(parsePadding)|(parseMemberVariable)|(parseMemberArrayVariable)|(parseMemberPointerVariable)]
    ASTNode* Parser::parseMember() {
        ASTNode *member;


        if (peek(KEYWORD_BE) || peek(KEYWORD_LE) || peek(VALUETYPE_ANY) || peek(IDENTIFIER)) {
            // Some kind of variable definition

            auto type = parseType();

            if (MATCHES(sequence(IDENTIFIER, SEPARATOR_SQUAREBRACKETOPEN)) && sequence<Not>(SEPARATOR_SQUAREBRACKETOPEN))
                member = parseMemberArrayVariable(type);
            else if (MATCHES(sequence(IDENTIFIER)))
                member = parseMemberVariable(type);
            else if (MATCHES(sequence(OPERATOR_STAR, IDENTIFIER, OPERATOR_INHERIT)))
                member = parseMemberPointerVariable(type);
            else
                throwParseError("invalid variable declaration");
        }
        else if (MATCHES(sequence(VALUETYPE_PADDING, SEPARATOR_SQUAREBRACKETOPEN)))
            member = parsePadding();
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

        // Consume superfluous semicolons
        while (MATCHES(sequence(SEPARATOR_ENDOFEXPRESSION)));

        return member;
    }

    // struct Identifier { <(parseMember)...> }
    ASTNode* Parser::parseStruct() {
        const auto &typeName = getValue<Token::Identifier>(-1).get();

        const auto structNode = create(new ASTNodeStruct());
        const auto typeDecl = addType(typeName, structNode);
        auto structGuard = SCOPE_GUARD { delete structNode; delete typeDecl; };

        if (MATCHES(sequence(OPERATOR_INHERIT, IDENTIFIER))) {
            // Inheritance

            do {
                auto inheritedTypeName = getValue<Token::Identifier>(-1).get();
                if (!this->m_types.contains(inheritedTypeName))
                    throwParseError(hex::format("cannot inherit from unknown type '{}'", inheritedTypeName), -1);

                structNode->addInheritance(this->m_types[inheritedTypeName]->clone());
            } while (MATCHES(sequence(SEPARATOR_COMMA, IDENTIFIER)));

        } else if (MATCHES(sequence(OPERATOR_INHERIT, VALUETYPE_ANY))) {
            throwParseError("cannot inherit from builtin type");
        }

        if (!MATCHES(sequence(SEPARATOR_CURLYBRACKETOPEN)))
            throwParseError("expected '{' after struct definition", -1);

        while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
            structNode->addMember(parseMember());
        }

        structGuard.release();

        return typeDecl;
    }

    // union Identifier { <(parseMember)...> }
    ASTNode* Parser::parseUnion() {
        const auto &typeName = getValue<Token::Identifier>(-2).get();

        const auto unionNode = create(new ASTNodeUnion());
        const auto typeDecl = addType(typeName, unionNode);
        auto unionGuard = SCOPE_GUARD { delete unionNode; delete typeDecl; };

        while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
            unionNode->addMember(parseMember());
        }

        unionGuard.release();

        return typeDecl;
    }

    // enum Identifier : (parseType) { <<Identifier|Identifier = (parseMathematicalExpression)[,]>...> }
    ASTNode* Parser::parseEnum() {
        auto typeName = getValue<Token::Identifier>(-2).get();

        auto underlyingType = parseType();
        if (underlyingType->getEndian().has_value()) throwParseError("underlying type may not have an endian specification", -2);

        const auto enumNode = create(new ASTNodeEnum(underlyingType));
        const auto typeDecl = addType(typeName, enumNode);
        auto enumGuard = SCOPE_GUARD { delete enumNode; delete typeDecl; };

        if (!MATCHES(sequence(SEPARATOR_CURLYBRACKETOPEN)))
            throwParseError("expected '{' after enum definition", -1);

        ASTNode *lastEntry = nullptr;
        while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
            if (MATCHES(sequence(IDENTIFIER, OPERATOR_ASSIGNMENT))) {
                auto name = getValue<Token::Identifier>(-2).get();
                auto value = parseMathematicalExpression();

                enumNode->addEntry(name, value);
                lastEntry = value;
            }
            else if (MATCHES(sequence(IDENTIFIER))) {
                ASTNode *valueExpr;
                auto name = getValue<Token::Identifier>(-1).get();
                if (enumNode->getEntries().empty())
                    valueExpr = lastEntry = create(new ASTNodeLiteral(u128(0)));
                else
                    valueExpr = lastEntry = create(new ASTNodeMathematicalExpression(lastEntry->clone(), new ASTNodeLiteral(u128(1)), Token::Operator::Plus));

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

        return typeDecl;
    }

    // bitfield Identifier { <Identifier : (parseMathematicalExpression)[;]...> }
    ASTNode* Parser::parseBitfield() {
        std::string typeName = getValue<Token::Identifier>(-2).get();

        const auto bitfieldNode = create(new ASTNodeBitfield());
        const auto typeDecl = addType(typeName, bitfieldNode);

        auto enumGuard = SCOPE_GUARD { delete bitfieldNode; delete typeDecl; };

        while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
            if (MATCHES(sequence(IDENTIFIER, OPERATOR_INHERIT))) {
                auto name = getValue<Token::Identifier>(-2).get();
                bitfieldNode->addEntry(name, parseMathematicalExpression());
            } else if (MATCHES(sequence(VALUETYPE_PADDING, OPERATOR_INHERIT))) {
              bitfieldNode->addEntry("padding", parseMathematicalExpression());
            } else if (MATCHES(sequence(SEPARATOR_ENDOFPROGRAM)))
                throwParseError("unexpected end of program", -2);
            else
                throwParseError("invalid bitfield member", 0);

            if (!MATCHES(sequence(SEPARATOR_ENDOFEXPRESSION)))
                throwParseError("missing ';' at end of expression", -1);

            // Consume superfluous semicolons
            while (MATCHES(sequence(SEPARATOR_ENDOFEXPRESSION)));
        }

        enumGuard.release();

        return typeDecl;
    }

    // (parseType) Identifier @ Integer
    ASTNode* Parser::parseVariablePlacement(ASTNodeTypeDecl *type) {
        auto name = getValue<Token::Identifier>(-1).get();

        ASTNode *placementOffset;
        if (MATCHES(sequence(OPERATOR_AT))) {
            placementOffset = parseMathematicalExpression();
        } else {
            placementOffset = nullptr;
        }

        return create(new ASTNodeVariableDecl(name, type, placementOffset));
    }

    // (parseType) Identifier[[(parseMathematicalExpression)]] @ Integer
    ASTNode* Parser::parseArrayVariablePlacement(ASTNodeTypeDecl *type) {
        auto name = getValue<Token::Identifier>(-2).get();

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

        auto placementOffset = parseMathematicalExpression();

        sizeCleanup.release();

        return create(new ASTNodeArrayVariableDecl(name, type, size, placementOffset));
    }

    // (parseType) *Identifier : (parseType) @ Integer
    ASTNode* Parser::parsePointerVariablePlacement(ASTNodeTypeDecl *type) {
        auto name = getValue<Token::Identifier>(-2).get();

        auto sizeType = parseType();
        auto sizeCleanup = SCOPE_GUARD { delete sizeType; };

        {
            auto builtinType = dynamic_cast<ASTNodeBuiltinType*>(sizeType->getType());

            if (builtinType == nullptr || !Token::isUnsigned(builtinType->getType()))
                throwParseError("invalid type used for pointer size", -1);
        }

        if (!MATCHES(sequence(OPERATOR_AT)))
            throwParseError("expected placement instruction", -1);

        auto placementOffset = parseMathematicalExpression();

        sizeCleanup.release();

        return create(new ASTNodePointerVariableDecl(name, type, sizeType, placementOffset));
    }

    std::vector<ASTNode*> Parser::parseNamespace() {
        std::vector<ASTNode*> statements;

        if (!MATCHES(sequence(IDENTIFIER)))
            throwParseError("expected namespace identifier");

        this->m_currNamespace.push_back(this->m_currNamespace.back());

        while (true) {
            this->m_currNamespace.back().push_back(getValue<Token::Identifier>(-1).get());

            if (MATCHES(sequence(OPERATOR_SCOPERESOLUTION, IDENTIFIER)))
                continue;
            else
                break;
        }

        if (!MATCHES(sequence(SEPARATOR_CURLYBRACKETOPEN)))
            throwParseError("expected '{' at start of namespace");

        while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
            auto newStatements = parseStatements();
            std::copy(newStatements.begin(), newStatements.end(), std::back_inserter(statements));
        }

        this->m_currNamespace.pop_back();

        return statements;
    }

    ASTNode* Parser::parsePlacement() {
        auto type = parseType();

        if (MATCHES(sequence(IDENTIFIER, SEPARATOR_SQUAREBRACKETOPEN)))
            return parseArrayVariablePlacement(type);
        else if (MATCHES(sequence(IDENTIFIER)))
            return parseVariablePlacement(type);
        else if (MATCHES(sequence(OPERATOR_STAR, IDENTIFIER, OPERATOR_INHERIT)))
            return parsePointerVariablePlacement(type);
        else throwParseError("invalid sequence", 0);
    }

    /* Program */

    // <(parseUsingDeclaration)|(parseVariablePlacement)|(parseStruct)>
    std::vector<ASTNode*> Parser::parseStatements() {
        ASTNode *statement;

        if (MATCHES(sequence(KEYWORD_USING, IDENTIFIER)))
            statement = parseUsingDeclaration();
        else if (peek(IDENTIFIER)) {
            auto originalPos = this->m_curr;
            this->m_curr++;
            parseNamespaceResolution();
            bool isFunction = peek(SEPARATOR_ROUNDBRACKETOPEN);
            this->m_curr = originalPos;

            if (isFunction) {
                this->m_curr++;
                statement = parseFunctionCall();
            }
            else
                statement = parsePlacement();
        }
        else if (peek(KEYWORD_BE) || peek(KEYWORD_LE) || peek(VALUETYPE_ANY))
            statement = parsePlacement();
        else if (MATCHES(sequence(KEYWORD_STRUCT, IDENTIFIER)))
            statement = parseStruct();
        else if (MATCHES(sequence(KEYWORD_UNION, IDENTIFIER, SEPARATOR_CURLYBRACKETOPEN)))
            statement = parseUnion();
        else if (MATCHES(sequence(KEYWORD_ENUM, IDENTIFIER, OPERATOR_INHERIT)))
            statement = parseEnum();
        else if (MATCHES(sequence(KEYWORD_BITFIELD, IDENTIFIER, SEPARATOR_CURLYBRACKETOPEN)))
            statement = parseBitfield();
        else if (MATCHES(sequence(KEYWORD_FUNCTION, IDENTIFIER, SEPARATOR_ROUNDBRACKETOPEN)))
            statement = parseFunctionDefinition();
        else if (MATCHES(sequence(KEYWORD_NAMESPACE)))
            return parseNamespace();
        else throwParseError("invalid sequence", 0);

        if (MATCHES(sequence(SEPARATOR_SQUAREBRACKETOPEN, SEPARATOR_SQUAREBRACKETOPEN)))
            parseAttribute(dynamic_cast<Attributable *>(statement));

        if (!MATCHES(sequence(SEPARATOR_ENDOFEXPRESSION)))
            throwParseError("missing ';' at end of expression", -1);

        // Consume superfluous semicolons
        while (MATCHES(sequence(SEPARATOR_ENDOFEXPRESSION)));

        return { statement };
    }

    ASTNodeTypeDecl* Parser::addType(const std::string &name, ASTNode *node, std::optional<std::endian> endian) {
        auto typeName = getNamespacePrefixedName(name);

        if (this->m_types.contains(typeName))
            throwParseError(hex::format("redefinition of type '{}'", typeName));

        auto typeDecl = create(new ASTNodeTypeDecl(typeName, node, endian));
        this->m_types.insert({ typeName,  typeDecl });

        return typeDecl;
    }

    // <(parseNamespace)...> EndOfProgram
    std::optional<std::vector<ASTNode*>> Parser::parse(const std::vector<Token> &tokens) {
        this->m_curr = tokens.begin();

        this->m_types.clear();

        this->m_currNamespace.clear();
        this->m_currNamespace.emplace_back();

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