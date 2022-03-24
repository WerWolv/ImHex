#include <hex/pattern_language/parser.hpp>

#include <hex/pattern_language/ast/ast_node_array_variable_decl.hpp>
#include <hex/pattern_language/ast/ast_node_assignment.hpp>
#include <hex/pattern_language/ast/ast_node_attribute.hpp>
#include <hex/pattern_language/ast/ast_node_bitfield.hpp>
#include <hex/pattern_language/ast/ast_node_builtin_type.hpp>
#include <hex/pattern_language/ast/ast_node_cast.hpp>
#include <hex/pattern_language/ast/ast_node_compound_statement.hpp>
#include <hex/pattern_language/ast/ast_node_conditional_statement.hpp>
#include <hex/pattern_language/ast/ast_node_control_flow_statement.hpp>
#include <hex/pattern_language/ast/ast_node_enum.hpp>
#include <hex/pattern_language/ast/ast_node_function_call.hpp>
#include <hex/pattern_language/ast/ast_node_function_definition.hpp>
#include <hex/pattern_language/ast/ast_node_literal.hpp>
#include <hex/pattern_language/ast/ast_node_mathematical_expression.hpp>
#include <hex/pattern_language/ast/ast_node_multi_variable_decl.hpp>
#include <hex/pattern_language/ast/ast_node_parameter_pack.hpp>
#include <hex/pattern_language/ast/ast_node_pointer_variable_decl.hpp>
#include <hex/pattern_language/ast/ast_node_rvalue.hpp>
#include <hex/pattern_language/ast/ast_node_scope_resolution.hpp>
#include <hex/pattern_language/ast/ast_node_struct.hpp>
#include <hex/pattern_language/ast/ast_node_ternary_expression.hpp>
#include <hex/pattern_language/ast/ast_node_type_decl.hpp>
#include <hex/pattern_language/ast/ast_node_type_operator.hpp>
#include <hex/pattern_language/ast/ast_node_union.hpp>
#include <hex/pattern_language/ast/ast_node_variable_decl.hpp>
#include <hex/pattern_language/ast/ast_node_while_statement.hpp>

#include <optional>

#define MATCHES(x) (begin() && resetIfFailed(x))

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
    std::unique_ptr<ASTNode> Parser::parseFunctionCall() {
        std::string functionName = parseNamespaceResolution();

        if (!MATCHES(sequence(SEPARATOR_ROUNDBRACKETOPEN)))
            throwParserError("expected '(' after function name");

        std::vector<std::unique_ptr<ASTNode>> params;

        while (!MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE))) {
            params.push_back(parseMathematicalExpression());

            if (MATCHES(sequence(SEPARATOR_COMMA, SEPARATOR_ROUNDBRACKETCLOSE)))
                throwParserError("unexpected ',' at end of function parameter list", -1);
            else if (MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE)))
                break;
            else if (!MATCHES(sequence(SEPARATOR_COMMA)))
                throwParserError("missing ',' between parameters", -1);
        }

        return create(new ASTNodeFunctionCall(functionName, std::move(params)));
    }

    std::unique_ptr<ASTNode> Parser::parseStringLiteral() {
        return create(new ASTNodeLiteral(getValue<Token::Literal>(-1)));
    }

    std::string Parser::parseNamespaceResolution() {
        std::string name;

        while (true) {
            name += getValue<Token::Identifier>(-1).get();

            if (MATCHES(sequence(OPERATOR_SCOPERESOLUTION, IDENTIFIER))) {
                name += "::";
                continue;
            } else
                break;
        }

        return name;
    }

    std::unique_ptr<ASTNode> Parser::parseScopeResolution() {
        std::string typeName;

        while (true) {
            typeName += getValue<Token::Identifier>(-1).get();

            if (MATCHES(sequence(OPERATOR_SCOPERESOLUTION, IDENTIFIER))) {
                if (peek(OPERATOR_SCOPERESOLUTION, 0) && peek(IDENTIFIER, 1)) {
                    typeName += "::";
                    continue;
                } else {
                    if (!this->m_types.contains(typeName))
                        throwParserError(hex::format("cannot access scope of invalid type '{}'", typeName), -1);

                    return create(new ASTNodeScopeResolution(this->m_types[typeName]->clone(), getValue<Token::Identifier>(-1).get()));
                }
            } else
                break;
        }

        throwParserError("failed to parse scope resolution. Expected 'TypeName::Identifier'");
    }

    std::unique_ptr<ASTNode> Parser::parseRValue() {
        ASTNodeRValue::Path path;
        return this->parseRValue(path);
    }

    // <Identifier[.]...>
    std::unique_ptr<ASTNode> Parser::parseRValue(ASTNodeRValue::Path &path) {
        if (peek(IDENTIFIER, -1))
            path.push_back(getValue<Token::Identifier>(-1).get());
        else if (peek(KEYWORD_PARENT, -1))
            path.emplace_back("parent");
        else if (peek(KEYWORD_THIS, -1))
            path.emplace_back("this");

        if (MATCHES(sequence(SEPARATOR_SQUAREBRACKETOPEN))) {
            path.push_back(parseMathematicalExpression());
            if (!MATCHES(sequence(SEPARATOR_SQUAREBRACKETCLOSE)))
                throwParserError("expected closing ']' at end of array indexing");
        }

        if (MATCHES(sequence(SEPARATOR_DOT))) {
            if (MATCHES(oneOf(IDENTIFIER, KEYWORD_PARENT)))
                return this->parseRValue(path);
            else
                throwParserError("expected member name or 'parent' keyword", -1);
        } else
            return create(new ASTNodeRValue(std::move(path)));
    }

    // <Integer|((parseMathematicalExpression))>
    std::unique_ptr<ASTNode> Parser::parseFactor() {
        if (MATCHES(sequence(INTEGER)))
            return create(new ASTNodeLiteral(getValue<Token::Literal>(-1)));
        else if (peek(OPERATOR_PLUS) || peek(OPERATOR_MINUS) || peek(OPERATOR_BITNOT) || peek(OPERATOR_BOOLNOT))
            return this->parseMathematicalExpression();
        else if (MATCHES(sequence(SEPARATOR_ROUNDBRACKETOPEN))) {
            auto node = this->parseMathematicalExpression();
            if (!MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE)))
                throwParserError("expected closing parenthesis");

            return node;
        } else if (MATCHES(sequence(IDENTIFIER))) {
            auto originalPos = this->m_curr;
            parseNamespaceResolution();
            bool isFunction = peek(SEPARATOR_ROUNDBRACKETOPEN);
            this->m_curr    = originalPos;


            if (isFunction) {
                return this->parseFunctionCall();
            } else if (peek(OPERATOR_SCOPERESOLUTION, 0)) {
                return this->parseScopeResolution();
            } else {
                return this->parseRValue();
            }
        } else if (MATCHES(oneOf(KEYWORD_PARENT, KEYWORD_THIS))) {
            return this->parseRValue();
        } else if (MATCHES(sequence(OPERATOR_DOLLAR))) {
            return create(new ASTNodeRValue(hex::moveToVector<ASTNodeRValue::PathSegment>("$")));
        } else if (MATCHES(oneOf(OPERATOR_ADDRESSOF, OPERATOR_SIZEOF) && sequence(SEPARATOR_ROUNDBRACKETOPEN))) {
            auto op = getValue<Token::Operator>(-2);

            std::unique_ptr<ASTNode> result;

            if (MATCHES(oneOf(IDENTIFIER, KEYWORD_PARENT, KEYWORD_THIS))) {
                result = create(new ASTNodeTypeOperator(op, this->parseRValue()));
            } else if (MATCHES(sequence(VALUETYPE_ANY))) {
                auto type = getValue<Token::ValueType>(-1);

                result = create(new ASTNodeLiteral(u128(Token::getTypeSize(type))));
            } else {
                throwParserError("expected rvalue identifier or built-in type");
            }

            if (!MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE)))
                throwParserError("expected closing parenthesis");

            return result;
        } else
            throwParserError("expected value or parenthesis");
    }

    std::unique_ptr<ASTNode> Parser::parseCastExpression() {
        if (peek(KEYWORD_BE) || peek(KEYWORD_LE) || peek(VALUETYPE_ANY)) {
            auto type        = parseType(true);
            auto builtinType = dynamic_cast<ASTNodeBuiltinType *>(type->getType().get());

            if (builtinType == nullptr)
                throwParserError("invalid type used for pointer size", -1);

            if (!peek(SEPARATOR_ROUNDBRACKETOPEN))
                throwParserError("expected '(' before cast expression", -1);

            auto node = parseFactor();

            return create(new ASTNodeCast(std::move(node), std::move(type)));
        } else return parseFactor();
    }

    // <+|-|!|~> (parseFactor)
    std::unique_ptr<ASTNode> Parser::parseUnaryExpression() {
        if (MATCHES(oneOf(OPERATOR_PLUS, OPERATOR_MINUS, OPERATOR_BOOLNOT, OPERATOR_BITNOT))) {
            auto op = getValue<Token::Operator>(-1);

            return create(new ASTNodeMathematicalExpression(create(new ASTNodeLiteral(0)), this->parseCastExpression(), op));
        } else if (MATCHES(sequence(STRING))) {
            return this->parseStringLiteral();
        }

        return this->parseCastExpression();
    }

    // (parseUnaryExpression) <*|/|%> (parseUnaryExpression)
    std::unique_ptr<ASTNode> Parser::parseMultiplicativeExpression() {
        auto node = this->parseUnaryExpression();

        while (MATCHES(oneOf(OPERATOR_STAR, OPERATOR_SLASH, OPERATOR_PERCENT))) {
            auto op = getValue<Token::Operator>(-1);
            node    = create(new ASTNodeMathematicalExpression(std::move(node), this->parseUnaryExpression(), op));
        }

        return node;
    }

    // (parseMultiplicativeExpression) <+|-> (parseMultiplicativeExpression)
    std::unique_ptr<ASTNode> Parser::parseAdditiveExpression() {
        auto node = this->parseMultiplicativeExpression();

        while (MATCHES(variant(OPERATOR_PLUS, OPERATOR_MINUS))) {
            auto op = getValue<Token::Operator>(-1);
            node    = create(new ASTNodeMathematicalExpression(std::move(node), this->parseMultiplicativeExpression(), op));
        }

        return node;
    }

    // (parseAdditiveExpression) < >>|<< > (parseAdditiveExpression)
    std::unique_ptr<ASTNode> Parser::parseShiftExpression() {
        auto node = this->parseAdditiveExpression();

        while (MATCHES(variant(OPERATOR_SHIFTLEFT, OPERATOR_SHIFTRIGHT))) {
            auto op = getValue<Token::Operator>(-1);
            node    = create(new ASTNodeMathematicalExpression(std::move(node), this->parseAdditiveExpression(), op));
        }

        return node;
    }

    // (parseShiftExpression) & (parseShiftExpression)
    std::unique_ptr<ASTNode> Parser::parseBinaryAndExpression() {
        auto node = this->parseShiftExpression();

        while (MATCHES(sequence(OPERATOR_BITAND))) {
            node = create(new ASTNodeMathematicalExpression(std::move(node), this->parseShiftExpression(), Token::Operator::BitAnd));
        }

        return node;
    }

    // (parseBinaryAndExpression) ^ (parseBinaryAndExpression)
    std::unique_ptr<ASTNode> Parser::parseBinaryXorExpression() {
        auto node = this->parseBinaryAndExpression();

        while (MATCHES(sequence(OPERATOR_BITXOR))) {
            node = create(new ASTNodeMathematicalExpression(std::move(node), this->parseBinaryAndExpression(), Token::Operator::BitXor));
        }

        return node;
    }

    // (parseBinaryXorExpression) | (parseBinaryXorExpression)
    std::unique_ptr<ASTNode> Parser::parseBinaryOrExpression() {
        auto node = this->parseBinaryXorExpression();

        while (MATCHES(sequence(OPERATOR_BITOR))) {
            node = create(new ASTNodeMathematicalExpression(std::move(node), this->parseBinaryXorExpression(), Token::Operator::BitOr));
        }

        return node;
    }

    // (parseBinaryOrExpression) < >=|<=|>|< > (parseBinaryOrExpression)
    std::unique_ptr<ASTNode> Parser::parseRelationExpression() {
        auto node = this->parseBinaryOrExpression();

        while (MATCHES(sequence(OPERATOR_BOOLGREATERTHAN) || sequence(OPERATOR_BOOLLESSTHAN) || sequence(OPERATOR_BOOLGREATERTHANOREQUALS) || sequence(OPERATOR_BOOLLESSTHANOREQUALS))) {
            auto op = getValue<Token::Operator>(-1);
            node    = create(new ASTNodeMathematicalExpression(std::move(node), this->parseBinaryOrExpression(), op));
        }

        return node;
    }

    // (parseRelationExpression) <==|!=> (parseRelationExpression)
    std::unique_ptr<ASTNode> Parser::parseEqualityExpression() {
        auto node = this->parseRelationExpression();

        while (MATCHES(sequence(OPERATOR_BOOLEQUALS) || sequence(OPERATOR_BOOLNOTEQUALS))) {
            auto op = getValue<Token::Operator>(-1);
            node    = create(new ASTNodeMathematicalExpression(std::move(node), this->parseRelationExpression(), op));
        }

        return node;
    }

    // (parseEqualityExpression) && (parseEqualityExpression)
    std::unique_ptr<ASTNode> Parser::parseBooleanAnd() {
        auto node = this->parseEqualityExpression();

        while (MATCHES(sequence(OPERATOR_BOOLAND))) {
            node = create(new ASTNodeMathematicalExpression(std::move(node), this->parseEqualityExpression(), Token::Operator::BoolAnd));
        }

        return node;
    }

    // (parseBooleanAnd) ^^ (parseBooleanAnd)
    std::unique_ptr<ASTNode> Parser::parseBooleanXor() {
        auto node = this->parseBooleanAnd();

        while (MATCHES(sequence(OPERATOR_BOOLXOR))) {
            node = create(new ASTNodeMathematicalExpression(std::move(node), this->parseBooleanAnd(), Token::Operator::BoolXor));
        }

        return node;
    }

    // (parseBooleanXor) || (parseBooleanXor)
    std::unique_ptr<ASTNode> Parser::parseBooleanOr() {
        auto node = this->parseBooleanXor();

        while (MATCHES(sequence(OPERATOR_BOOLOR))) {
            node = create(new ASTNodeMathematicalExpression(std::move(node), this->parseBooleanXor(), Token::Operator::BoolOr));
        }

        return node;
    }

    // (parseBooleanOr) ? (parseBooleanOr) : (parseBooleanOr)
    std::unique_ptr<ASTNode> Parser::parseTernaryConditional() {
        auto node = this->parseBooleanOr();

        while (MATCHES(sequence(OPERATOR_TERNARYCONDITIONAL))) {
            auto second = this->parseBooleanOr();

            if (!MATCHES(sequence(OPERATOR_INHERIT)))
                throwParserError("expected ':' in ternary expression");

            auto third = this->parseBooleanOr();
            node       = create(new ASTNodeTernaryExpression(std::move(node), std::move(second), std::move(third), Token::Operator::TernaryConditional));
        }

        return node;
    }

    // (parseTernaryConditional)
    std::unique_ptr<ASTNode> Parser::parseMathematicalExpression() {
        return this->parseTernaryConditional();
    }

    // [[ <Identifier[( (parseStringLiteral) )], ...> ]]
    void Parser::parseAttribute(Attributable *currNode) {
        if (currNode == nullptr)
            throwParserError("tried to apply attribute to invalid statement");

        do {
            if (!MATCHES(sequence(IDENTIFIER)))
                throwParserError("expected attribute expression");

            auto attribute = getValue<Token::Identifier>(-1).get();

            if (MATCHES(sequence(SEPARATOR_ROUNDBRACKETOPEN, STRING, SEPARATOR_ROUNDBRACKETCLOSE))) {
                auto value  = getValue<Token::Literal>(-2);
                auto string = std::get_if<std::string>(&value);

                if (string == nullptr)
                    throwParserError("expected string attribute argument");

                currNode->addAttribute(create(new ASTNodeAttribute(attribute, *string)));
            } else
                currNode->addAttribute(create(new ASTNodeAttribute(attribute)));

        } while (MATCHES(sequence(SEPARATOR_COMMA)));

        if (!MATCHES(sequence(SEPARATOR_SQUAREBRACKETCLOSE, SEPARATOR_SQUAREBRACKETCLOSE)))
            throwParserError("unfinished attribute. Expected ']]'");
    }

    /* Functions */

    std::unique_ptr<ASTNode> Parser::parseFunctionDefinition() {
        const auto &functionName = getValue<Token::Identifier>(-2).get();
        std::vector<std::pair<std::string, std::unique_ptr<ASTNode>>> params;
        std::optional<std::string> parameterPack;

        // Parse parameter list
        bool hasParams        = !peek(SEPARATOR_ROUNDBRACKETCLOSE);
        u32 unnamedParamCount = 0;
        while (hasParams) {
            if (MATCHES(sequence(VALUETYPE_AUTO, SEPARATOR_DOT, SEPARATOR_DOT, SEPARATOR_DOT, IDENTIFIER))) {
                parameterPack = getValue<Token::Identifier>(-1).get();

                if (MATCHES(sequence(SEPARATOR_COMMA)))
                    throwParserError("parameter pack can only appear at end of parameter list");

                break;
            } else {
                auto type = parseType(true);

                if (MATCHES(sequence(IDENTIFIER)))
                    params.emplace_back(getValue<Token::Identifier>(-1).get(), std::move(type));
                else {
                    params.emplace_back(std::to_string(unnamedParamCount), std::move(type));
                    unnamedParamCount++;
                }

                if (!MATCHES(sequence(SEPARATOR_COMMA))) {
                    break;
                }
            }
        }

        if (!MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE)))
            throwParserError("expected closing ')' after parameter list");

        if (!MATCHES(sequence(SEPARATOR_CURLYBRACKETOPEN)))
            throwParserError("expected opening '{' after function definition");


        // Parse function body
        std::vector<std::unique_ptr<ASTNode>> body;

        while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
            body.push_back(this->parseFunctionStatement());
        }

        return create(new ASTNodeFunctionDefinition(getNamespacePrefixedName(functionName), std::move(params), std::move(body), parameterPack));
    }

    std::unique_ptr<ASTNode> Parser::parseFunctionVariableDecl() {
        std::unique_ptr<ASTNode> statement;
        auto type = parseType(true);

        if (MATCHES(sequence(IDENTIFIER))) {
            auto identifier = getValue<Token::Identifier>(-1).get();
            statement       = parseMemberVariable(std::move(type));

            if (MATCHES(sequence(OPERATOR_ASSIGNMENT))) {
                auto expression = parseMathematicalExpression();

                std::vector<std::unique_ptr<ASTNode>> compoundStatement;
                {
                    compoundStatement.push_back(std::move(statement));
                    compoundStatement.push_back(create(new ASTNodeAssignment(identifier, std::move(expression))));
                }

                statement = create(new ASTNodeCompoundStatement(std::move(compoundStatement)));
            }
        } else
            throwParserError("invalid variable declaration");

        return statement;
    }

    std::unique_ptr<ASTNode> Parser::parseFunctionStatement() {
        bool needsSemicolon = true;
        std::unique_ptr<ASTNode> statement;

        if (MATCHES(sequence(IDENTIFIER, OPERATOR_ASSIGNMENT)))
            statement = parseFunctionVariableAssignment(getValue<Token::Identifier>(-2).get());
        else if (MATCHES(sequence(OPERATOR_DOLLAR, OPERATOR_ASSIGNMENT)))
            statement = parseFunctionVariableAssignment("$");
        else if (MATCHES(oneOf(IDENTIFIER) && oneOf(OPERATOR_PLUS, OPERATOR_MINUS, OPERATOR_STAR, OPERATOR_SLASH, OPERATOR_PERCENT, OPERATOR_SHIFTLEFT, OPERATOR_SHIFTRIGHT, OPERATOR_BITOR, OPERATOR_BITAND, OPERATOR_BITXOR) && sequence(OPERATOR_ASSIGNMENT)))
            statement = parseFunctionVariableCompoundAssignment(getValue<Token::Identifier>(-3).get());
        else if (MATCHES(oneOf(OPERATOR_DOLLAR) && oneOf(OPERATOR_PLUS, OPERATOR_MINUS, OPERATOR_STAR, OPERATOR_SLASH, OPERATOR_PERCENT, OPERATOR_SHIFTLEFT, OPERATOR_SHIFTRIGHT, OPERATOR_BITOR, OPERATOR_BITAND, OPERATOR_BITXOR) && sequence(OPERATOR_ASSIGNMENT)))
            statement = parseFunctionVariableCompoundAssignment("$");
        else if (MATCHES(oneOf(KEYWORD_RETURN, KEYWORD_BREAK, KEYWORD_CONTINUE)))
            statement = parseFunctionControlFlowStatement();
        else if (MATCHES(sequence(KEYWORD_IF, SEPARATOR_ROUNDBRACKETOPEN))) {
            statement      = parseFunctionConditional();
            needsSemicolon = false;
        } else if (MATCHES(sequence(KEYWORD_WHILE, SEPARATOR_ROUNDBRACKETOPEN))) {
            statement      = parseFunctionWhileLoop();
            needsSemicolon = false;
        } else if (MATCHES(sequence(KEYWORD_FOR, SEPARATOR_ROUNDBRACKETOPEN))) {
            statement      = parseFunctionForLoop();
            needsSemicolon = false;
        } else if (MATCHES(sequence(IDENTIFIER))) {
            auto originalPos = this->m_curr;
            parseNamespaceResolution();
            bool isFunction = peek(SEPARATOR_ROUNDBRACKETOPEN);

            if (isFunction) {
                this->m_curr = originalPos;
                statement    = parseFunctionCall();
            } else {
                this->m_curr = originalPos - 1;
                statement    = parseFunctionVariableDecl();
            }
        } else if (peek(KEYWORD_BE) || peek(KEYWORD_LE) || peek(VALUETYPE_ANY)) {
            statement = parseFunctionVariableDecl();
        } else
            throwParserError("invalid sequence", 0);

        if (needsSemicolon && !MATCHES(sequence(SEPARATOR_ENDOFEXPRESSION)))
            throwParserError("missing ';' at end of expression", -1);

        // Consume superfluous semicolons
        while (needsSemicolon && MATCHES(sequence(SEPARATOR_ENDOFEXPRESSION)))
            ;

        return statement;
    }

    std::unique_ptr<ASTNode> Parser::parseFunctionVariableAssignment(const std::string &lvalue) {
        auto rvalue = this->parseMathematicalExpression();

        return create(new ASTNodeAssignment(lvalue, std::move(rvalue)));
    }

    std::unique_ptr<ASTNode> Parser::parseFunctionVariableCompoundAssignment(const std::string &lvalue) {
        const auto &op = getValue<Token::Operator>(-2);

        auto rvalue = this->parseMathematicalExpression();

        return create(new ASTNodeAssignment(lvalue, create(new ASTNodeMathematicalExpression(create(new ASTNodeRValue(hex::moveToVector<ASTNodeRValue::PathSegment>(lvalue))), std::move(rvalue), op))));
    }

    std::unique_ptr<ASTNode> Parser::parseFunctionControlFlowStatement() {
        ControlFlowStatement type;
        if (peek(KEYWORD_RETURN, -1))
            type = ControlFlowStatement::Return;
        else if (peek(KEYWORD_BREAK, -1))
            type = ControlFlowStatement::Break;
        else if (peek(KEYWORD_CONTINUE, -1))
            type = ControlFlowStatement::Continue;
        else
            throwParserError("invalid control flow statement. Expected 'return', 'break' or 'continue'");

        if (peek(SEPARATOR_ENDOFEXPRESSION))
            return create(new ASTNodeControlFlowStatement(type, nullptr));
        else
            return create(new ASTNodeControlFlowStatement(type, this->parseMathematicalExpression()));
    }

    std::vector<std::unique_ptr<ASTNode>> Parser::parseStatementBody() {
        std::vector<std::unique_ptr<ASTNode>> body;

        if (MATCHES(sequence(SEPARATOR_CURLYBRACKETOPEN))) {
            while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
                body.push_back(parseFunctionStatement());
            }
        } else {
            body.push_back(parseFunctionStatement());
        }

        return body;
    }

    std::unique_ptr<ASTNode> Parser::parseFunctionConditional() {
        auto condition = parseMathematicalExpression();
        std::vector<std::unique_ptr<ASTNode>> trueBody, falseBody;

        if (!MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE)))
            throwParserError("expected closing ')' after statement head");

        trueBody = parseStatementBody();

        if (MATCHES(sequence(KEYWORD_ELSE)))
            falseBody = parseStatementBody();

        return create(new ASTNodeConditionalStatement(std::move(condition), std::move(trueBody), std::move(falseBody)));
    }

    std::unique_ptr<ASTNode> Parser::parseFunctionWhileLoop() {
        auto condition = parseMathematicalExpression();
        std::vector<std::unique_ptr<ASTNode>> body;

        if (!MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE)))
            throwParserError("expected closing ')' after statement head");

        body = parseStatementBody();

        return create(new ASTNodeWhileStatement(std::move(condition), std::move(body)));
    }

    std::unique_ptr<ASTNode> Parser::parseFunctionForLoop() {
        auto variable = parseFunctionVariableDecl();

        if (!MATCHES(sequence(SEPARATOR_COMMA)))
            throwParserError("expected ',' after for loop variable declaration");

        auto condition = parseMathematicalExpression();

        if (!MATCHES(sequence(SEPARATOR_COMMA)))
            throwParserError("expected ',' after for loop condition");

        std::unique_ptr<ASTNode> postExpression = nullptr;
        if (MATCHES(sequence(IDENTIFIER, OPERATOR_ASSIGNMENT)))
            postExpression = parseFunctionVariableAssignment(getValue<Token::Identifier>(-2).get());
        else if (MATCHES(sequence(OPERATOR_DOLLAR, OPERATOR_ASSIGNMENT)))
            postExpression = parseFunctionVariableAssignment("$");
        else if (MATCHES(oneOf(IDENTIFIER) && oneOf(OPERATOR_PLUS, OPERATOR_MINUS, OPERATOR_STAR, OPERATOR_SLASH, OPERATOR_PERCENT, OPERATOR_SHIFTLEFT, OPERATOR_SHIFTRIGHT, OPERATOR_BITOR, OPERATOR_BITAND, OPERATOR_BITXOR) && sequence(OPERATOR_ASSIGNMENT)))
            postExpression = parseFunctionVariableCompoundAssignment(getValue<Token::Identifier>(-3).get());
        else if (MATCHES(oneOf(OPERATOR_DOLLAR) && oneOf(OPERATOR_PLUS, OPERATOR_MINUS, OPERATOR_STAR, OPERATOR_SLASH, OPERATOR_PERCENT, OPERATOR_SHIFTLEFT, OPERATOR_SHIFTRIGHT, OPERATOR_BITOR, OPERATOR_BITAND, OPERATOR_BITXOR) && sequence(OPERATOR_ASSIGNMENT)))
            postExpression = parseFunctionVariableCompoundAssignment("$");
        else
            throwParserError("expected variable assignment in for loop post expression");

        std::vector<std::unique_ptr<ASTNode>> body;


        if (!MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE)))
            throwParserError("expected closing ')' after statement head");

        body = parseStatementBody();

        std::vector<std::unique_ptr<ASTNode>> compoundStatement;
        {
            compoundStatement.push_back(std::move(variable));
            compoundStatement.push_back(create(new ASTNodeWhileStatement(std::move(condition), std::move(body), std::move(postExpression))));
        }

        return create(new ASTNodeCompoundStatement(std::move(compoundStatement), true));
    }

    /* Control flow */

    // if ((parseMathematicalExpression)) { (parseMember) }
    std::unique_ptr<ASTNode> Parser::parseConditional() {
        auto condition = parseMathematicalExpression();
        std::vector<std::unique_ptr<ASTNode>> trueBody, falseBody;

        if (MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE, SEPARATOR_CURLYBRACKETOPEN))) {
            while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
                trueBody.push_back(parseMember());
            }
        } else if (MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE))) {
            trueBody.push_back(parseMember());
        } else
            throwParserError("expected body of conditional statement");

        if (MATCHES(sequence(KEYWORD_ELSE, SEPARATOR_CURLYBRACKETOPEN))) {
            while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
                falseBody.push_back(parseMember());
            }
        } else if (MATCHES(sequence(KEYWORD_ELSE))) {
            falseBody.push_back(parseMember());
        }

        return create(new ASTNodeConditionalStatement(std::move(condition), std::move(trueBody), std::move(falseBody)));
    }

    // while ((parseMathematicalExpression))
    std::unique_ptr<ASTNode> Parser::parseWhileStatement() {
        auto condition = parseMathematicalExpression();

        if (!MATCHES(sequence(SEPARATOR_ROUNDBRACKETCLOSE)))
            throwParserError("expected closing ')' after while head");

        return create(new ASTNodeWhileStatement(std::move(condition), {}));
    }

    /* Type declarations */

    // [be|le] <Identifier|u8|u16|u32|u64|u128|s8|s16|s32|s64|s128|float|double|str>
    std::unique_ptr<ASTNodeTypeDecl> Parser::parseType(bool allowFunctionTypes) {
        std::optional<std::endian> endian;

        if (MATCHES(sequence(KEYWORD_LE)))
            endian = std::endian::little;
        else if (MATCHES(sequence(KEYWORD_BE)))
            endian = std::endian::big;

        if (MATCHES(sequence(IDENTIFIER))) {    // Custom type
            std::string typeName = parseNamespaceResolution();

            if (this->m_types.contains(typeName))
                return create(new ASTNodeTypeDecl({}, this->m_types[typeName], endian));
            else if (this->m_types.contains(getNamespacePrefixedName(typeName)))
                return create(new ASTNodeTypeDecl({}, this->m_types[getNamespacePrefixedName(typeName)], endian));
            else
                throwParserError(hex::format("unknown type '{}'", typeName));
        } else if (MATCHES(sequence(VALUETYPE_ANY))) {    // Builtin type
            auto type = getValue<Token::ValueType>(-1);
            if (!allowFunctionTypes) {
                if (type == Token::ValueType::String)
                    throwParserError("cannot use 'str' in this context. Use a character array instead");
                else if (type == Token::ValueType::Auto)
                    throwParserError("cannot use 'auto' in this context");
            }

            return create(new ASTNodeTypeDecl({}, create(new ASTNodeBuiltinType(type)), endian));
        } else throwParserError("failed to parse type. Expected identifier or builtin type");
    }

    // using Identifier = (parseType)
    std::shared_ptr<ASTNodeTypeDecl> Parser::parseUsingDeclaration() {
        auto name = getNamespacePrefixedName(getValue<Token::Identifier>(-2).get());

        auto type = parseType();

        auto endian = type->getEndian();
        return addType(name, std::move(type), endian);
    }

    // padding[(parseMathematicalExpression)]
    std::unique_ptr<ASTNode> Parser::parsePadding() {
        auto size = parseMathematicalExpression();

        if (!MATCHES(sequence(SEPARATOR_SQUAREBRACKETCLOSE)))
            throwParserError("expected closing ']' at end of array declaration", -1);

        return create(new ASTNodeArrayVariableDecl({}, create(new ASTNodeTypeDecl({}, create(new ASTNodeBuiltinType(Token::ValueType::Padding)))), std::move(size)));
    }

    // (parseType) Identifier
    std::unique_ptr<ASTNode> Parser::parseMemberVariable(const std::shared_ptr<ASTNodeTypeDecl> &type) {
        if (peek(SEPARATOR_COMMA)) {

            std::vector<std::unique_ptr<ASTNode>> variables;

            do {
                variables.push_back(create(new ASTNodeVariableDecl(getValue<Token::Identifier>(-1).get(), type)));
            } while (MATCHES(sequence(SEPARATOR_COMMA, IDENTIFIER)));

            return create(new ASTNodeMultiVariableDecl(std::move(variables)));
        } else if (MATCHES(sequence(OPERATOR_AT)))
            return create(new ASTNodeVariableDecl(getValue<Token::Identifier>(-2).get(), type, parseMathematicalExpression()));
        else
            return create(new ASTNodeVariableDecl(getValue<Token::Identifier>(-1).get(), type));
    }

    // (parseType) Identifier[(parseMathematicalExpression)]
    std::unique_ptr<ASTNode> Parser::parseMemberArrayVariable(const std::shared_ptr<ASTNodeTypeDecl> &type) {
        auto name = getValue<Token::Identifier>(-2).get();

        std::unique_ptr<ASTNode> size;

        if (!MATCHES(sequence(SEPARATOR_SQUAREBRACKETCLOSE))) {
            if (MATCHES(sequence(KEYWORD_WHILE, SEPARATOR_ROUNDBRACKETOPEN)))
                size = parseWhileStatement();
            else
                size = parseMathematicalExpression();

            if (!MATCHES(sequence(SEPARATOR_SQUAREBRACKETCLOSE)))
                throwParserError("expected closing ']' at end of array declaration", -1);
        }

        if (MATCHES(sequence(OPERATOR_AT)))
            return create(new ASTNodeArrayVariableDecl(name, type, std::move(size), parseMathematicalExpression()));
        else
            return create(new ASTNodeArrayVariableDecl(name, type, std::move(size)));
    }

    // (parseType) *Identifier : (parseType)
    std::unique_ptr<ASTNode> Parser::parseMemberPointerVariable(const std::shared_ptr<ASTNodeTypeDecl> &type) {
        auto name = getValue<Token::Identifier>(-2).get();

        auto sizeType = parseType();

        {
            auto builtinType = dynamic_cast<ASTNodeBuiltinType *>(sizeType->getType().get());

            if (builtinType == nullptr || !Token::isUnsigned(builtinType->getType()))
                throwParserError("invalid type used for pointer size", -1);
        }

        if (MATCHES(sequence(OPERATOR_AT)))
            return create(new ASTNodePointerVariableDecl(name, type, std::move(sizeType), parseMathematicalExpression()));
        else
            return create(new ASTNodePointerVariableDecl(name, type, std::move(sizeType)));
    }

    // [(parsePadding)|(parseMemberVariable)|(parseMemberArrayVariable)|(parseMemberPointerVariable)]
    std::unique_ptr<ASTNode> Parser::parseMember() {
        std::unique_ptr<ASTNode> member;

        if (MATCHES(sequence(OPERATOR_DOLLAR, OPERATOR_ASSIGNMENT)))
            member = parseFunctionVariableAssignment("$");
        else if (MATCHES(sequence(OPERATOR_DOLLAR) && oneOf(OPERATOR_PLUS, OPERATOR_MINUS, OPERATOR_STAR, OPERATOR_SLASH, OPERATOR_PERCENT, OPERATOR_SHIFTLEFT, OPERATOR_SHIFTRIGHT, OPERATOR_BITOR, OPERATOR_BITAND, OPERATOR_BITXOR) && sequence(OPERATOR_ASSIGNMENT)))
            member = parseFunctionVariableCompoundAssignment("$");
        else if (MATCHES(sequence(IDENTIFIER, OPERATOR_ASSIGNMENT)))
            member = parseFunctionVariableAssignment(getValue<Token::Identifier>(-2).get());
        else if (MATCHES(sequence(IDENTIFIER) && oneOf(OPERATOR_PLUS, OPERATOR_MINUS, OPERATOR_STAR, OPERATOR_SLASH, OPERATOR_PERCENT, OPERATOR_SHIFTLEFT, OPERATOR_SHIFTRIGHT, OPERATOR_BITOR, OPERATOR_BITAND, OPERATOR_BITXOR) && sequence(OPERATOR_ASSIGNMENT)))
            member = parseFunctionVariableCompoundAssignment(getValue<Token::Identifier>(-3).get());
        else if (peek(KEYWORD_BE) || peek(KEYWORD_LE) || peek(VALUETYPE_ANY) || peek(IDENTIFIER)) {
            // Some kind of variable definition

            bool isFunction = false;

            if (peek(IDENTIFIER)) {
                auto originalPos = this->m_curr;
                this->m_curr++;
                parseNamespaceResolution();
                isFunction   = peek(SEPARATOR_ROUNDBRACKETOPEN);
                this->m_curr = originalPos;

                if (isFunction) {
                    this->m_curr++;
                    member = parseFunctionCall();
                }
            }


            if (!isFunction) {
                auto type = parseType();

                if (MATCHES(sequence(IDENTIFIER, SEPARATOR_SQUAREBRACKETOPEN) && sequence<Not>(SEPARATOR_SQUAREBRACKETOPEN)))
                    member = parseMemberArrayVariable(std::move(type));
                else if (MATCHES(sequence(IDENTIFIER)))
                    member = parseMemberVariable(std::move(type));
                else if (MATCHES(sequence(OPERATOR_STAR, IDENTIFIER, OPERATOR_INHERIT)))
                    member = parseMemberPointerVariable(std::move(type));
                else
                    throwParserError("invalid variable declaration");
            }
        } else if (MATCHES(sequence(VALUETYPE_PADDING, SEPARATOR_SQUAREBRACKETOPEN)))
            member = parsePadding();
        else if (MATCHES(sequence(KEYWORD_IF, SEPARATOR_ROUNDBRACKETOPEN)))
            return parseConditional();
        else if (MATCHES(sequence(SEPARATOR_ENDOFPROGRAM)))
            throwParserError("unexpected end of program", -2);
        else if (MATCHES(sequence(KEYWORD_BREAK)))
            member = create(new ASTNodeControlFlowStatement(ControlFlowStatement::Break, nullptr));
        else if (MATCHES(sequence(KEYWORD_CONTINUE)))
            member = create(new ASTNodeControlFlowStatement(ControlFlowStatement::Continue, nullptr));
        else
            throwParserError("invalid struct member", 0);

        if (MATCHES(sequence(SEPARATOR_SQUAREBRACKETOPEN, SEPARATOR_SQUAREBRACKETOPEN)))
            parseAttribute(dynamic_cast<Attributable *>(member.get()));

        if (!MATCHES(sequence(SEPARATOR_ENDOFEXPRESSION)))
            throwParserError("missing ';' at end of expression", -1);

        // Consume superfluous semicolons
        while (MATCHES(sequence(SEPARATOR_ENDOFEXPRESSION)))
            ;

        return member;
    }

    // struct Identifier { <(parseMember)...> }
    std::shared_ptr<ASTNodeTypeDecl> Parser::parseStruct() {
        const auto &typeName = getValue<Token::Identifier>(-1).get();

        auto typeDecl   = addType(typeName, create(new ASTNodeStruct()));
        auto structNode = static_cast<ASTNodeStruct *>(typeDecl->getType().get());

        if (MATCHES(sequence(OPERATOR_INHERIT, IDENTIFIER))) {
            // Inheritance

            do {
                auto inheritedTypeName = getValue<Token::Identifier>(-1).get();
                if (!this->m_types.contains(inheritedTypeName))
                    throwParserError(hex::format("cannot inherit from unknown type '{}'", inheritedTypeName), -1);

                structNode->addInheritance(this->m_types[inheritedTypeName]->clone());
            } while (MATCHES(sequence(SEPARATOR_COMMA, IDENTIFIER)));

        } else if (MATCHES(sequence(OPERATOR_INHERIT, VALUETYPE_ANY))) {
            throwParserError("cannot inherit from builtin type");
        }

        if (!MATCHES(sequence(SEPARATOR_CURLYBRACKETOPEN)))
            throwParserError("expected '{' after struct definition", -1);

        while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
            structNode->addMember(parseMember());
        }

        return typeDecl;
    }

    // union Identifier { <(parseMember)...> }
    std::shared_ptr<ASTNodeTypeDecl> Parser::parseUnion() {
        const auto &typeName = getValue<Token::Identifier>(-2).get();

        auto typeDecl  = addType(typeName, create(new ASTNodeUnion()));
        auto unionNode = static_cast<ASTNodeStruct *>(typeDecl->getType().get());

        while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
            unionNode->addMember(parseMember());
        }

        return typeDecl;
    }

    // enum Identifier : (parseType) { <<Identifier|Identifier = (parseMathematicalExpression)[,]>...> }
    std::shared_ptr<ASTNodeTypeDecl> Parser::parseEnum() {
        auto typeName = getValue<Token::Identifier>(-2).get();

        auto underlyingType = parseType();
        if (underlyingType->getEndian().has_value()) throwParserError("underlying type may not have an endian specification", -2);

        auto typeDecl = addType(typeName, create(new ASTNodeEnum(std::move(underlyingType))));
        auto enumNode = static_cast<ASTNodeEnum *>(typeDecl->getType().get());

        if (!MATCHES(sequence(SEPARATOR_CURLYBRACKETOPEN)))
            throwParserError("expected '{' after enum definition", -1);

        std::unique_ptr<ASTNode> lastEntry;
        while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
            if (MATCHES(sequence(IDENTIFIER, OPERATOR_ASSIGNMENT))) {
                auto name  = getValue<Token::Identifier>(-2).get();
                auto value = parseMathematicalExpression();

                lastEntry = value->clone();
                enumNode->addEntry(name, std::move(value));
            } else if (MATCHES(sequence(IDENTIFIER))) {
                std::unique_ptr<ASTNode> valueExpr;
                auto name = getValue<Token::Identifier>(-1).get();
                if (enumNode->getEntries().empty())
                    valueExpr = create(new ASTNodeLiteral(u128(0)));
                else
                    valueExpr = create(new ASTNodeMathematicalExpression(lastEntry->clone(), create(new ASTNodeLiteral(u128(1))), Token::Operator::Plus));

                lastEntry = valueExpr->clone();
                enumNode->addEntry(name, std::move(valueExpr));
            } else if (MATCHES(sequence(SEPARATOR_ENDOFPROGRAM)))
                throwParserError("unexpected end of program", -2);
            else
                throwParserError("invalid enum entry", -1);

            if (!MATCHES(sequence(SEPARATOR_COMMA))) {
                if (MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE)))
                    break;
                else
                    throwParserError("missing ',' between enum entries", -1);
            }
        }

        return typeDecl;
    }

    // bitfield Identifier { <Identifier : (parseMathematicalExpression)[;]...> }
    std::shared_ptr<ASTNodeTypeDecl> Parser::parseBitfield() {
        std::string typeName = getValue<Token::Identifier>(-2).get();

        auto typeDecl     = addType(typeName, create(new ASTNodeBitfield()));
        auto bitfieldNode = static_cast<ASTNodeBitfield *>(typeDecl->getType().get());

        while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
            if (MATCHES(sequence(IDENTIFIER, OPERATOR_INHERIT))) {
                auto name = getValue<Token::Identifier>(-2).get();
                bitfieldNode->addEntry(name, parseMathematicalExpression());
            } else if (MATCHES(sequence(VALUETYPE_PADDING, OPERATOR_INHERIT))) {
                bitfieldNode->addEntry("padding", parseMathematicalExpression());
            } else if (MATCHES(sequence(SEPARATOR_ENDOFPROGRAM)))
                throwParserError("unexpected end of program", -2);
            else
                throwParserError("invalid bitfield member", 0);

            if (!MATCHES(sequence(SEPARATOR_ENDOFEXPRESSION)))
                throwParserError("missing ';' at end of expression", -1);

            // Consume superfluous semicolons
            while (MATCHES(sequence(SEPARATOR_ENDOFEXPRESSION)))
                ;
        }

        return typeDecl;
    }

    // using Identifier;
    void Parser::parseForwardDeclaration() {
        std::string typeName = getNamespacePrefixedName(getValue<Token::Identifier>(-1).get());

        if (this->m_types.contains(typeName))
            return;

        this->m_types.insert({ typeName, create(new ASTNodeTypeDecl(typeName) )});
    }

    // (parseType) Identifier @ Integer
    std::unique_ptr<ASTNode> Parser::parseVariablePlacement(const std::shared_ptr<ASTNodeTypeDecl> &type) {
        bool inVariable  = false;
        bool outVariable = false;

        auto name = getValue<Token::Identifier>(-1).get();

        std::unique_ptr<ASTNode> placementOffset;
        if (MATCHES(sequence(OPERATOR_AT))) {
            placementOffset = parseMathematicalExpression();
        } else if (MATCHES(sequence(KEYWORD_IN))) {
            inVariable = true;
        } else if (MATCHES(sequence(KEYWORD_OUT))) {
            outVariable = true;
        }

        return create(new ASTNodeVariableDecl(name, type, std::move(placementOffset), inVariable, outVariable));
    }

    // (parseType) Identifier[[(parseMathematicalExpression)]] @ Integer
    std::unique_ptr<ASTNode> Parser::parseArrayVariablePlacement(const std::shared_ptr<ASTNodeTypeDecl> &type) {
        auto name = getValue<Token::Identifier>(-2).get();

        std::unique_ptr<ASTNode> size;

        if (!MATCHES(sequence(SEPARATOR_SQUAREBRACKETCLOSE))) {
            if (MATCHES(sequence(KEYWORD_WHILE, SEPARATOR_ROUNDBRACKETOPEN)))
                size = parseWhileStatement();
            else
                size = parseMathematicalExpression();

            if (!MATCHES(sequence(SEPARATOR_SQUAREBRACKETCLOSE)))
                throwParserError("expected closing ']' at end of array declaration", -1);
        }

        if (!MATCHES(sequence(OPERATOR_AT)))
            throwParserError("expected placement instruction", -1);

        auto placementOffset = parseMathematicalExpression();

        return create(new ASTNodeArrayVariableDecl(name, type, std::move(size), std::move(placementOffset)));
    }

    // (parseType) *Identifier : (parseType) @ Integer
    std::unique_ptr<ASTNode> Parser::parsePointerVariablePlacement(const std::shared_ptr<ASTNodeTypeDecl> &type) {
        auto name = getValue<Token::Identifier>(-2).get();

        auto sizeType = parseType();

        {
            auto builtinType = dynamic_cast<ASTNodeBuiltinType *>(sizeType->getType().get());

            if (builtinType == nullptr || !Token::isUnsigned(builtinType->getType()))
                throwParserError("invalid type used for pointer size", -1);
        }

        if (!MATCHES(sequence(OPERATOR_AT)))
            throwParserError("expected placement instruction", -1);

        auto placementOffset = parseMathematicalExpression();

        return create(new ASTNodePointerVariableDecl(name, type, std::move(sizeType), std::move(placementOffset)));
    }

    std::vector<std::shared_ptr<ASTNode>> Parser::parseNamespace() {
        std::vector<std::shared_ptr<ASTNode>> statements;

        if (!MATCHES(sequence(IDENTIFIER)))
            throwParserError("expected namespace identifier");

        this->m_currNamespace.push_back(this->m_currNamespace.back());

        while (true) {
            this->m_currNamespace.back().push_back(getValue<Token::Identifier>(-1).get());

            if (MATCHES(sequence(OPERATOR_SCOPERESOLUTION, IDENTIFIER)))
                continue;
            else
                break;
        }

        if (!MATCHES(sequence(SEPARATOR_CURLYBRACKETOPEN)))
            throwParserError("expected '{' at start of namespace");

        while (!MATCHES(sequence(SEPARATOR_CURLYBRACKETCLOSE))) {
            auto newStatements = parseStatements();
            std::move(newStatements.begin(), newStatements.end(), std::back_inserter(statements));
        }

        this->m_currNamespace.pop_back();

        return statements;
    }

    std::unique_ptr<ASTNode> Parser::parsePlacement() {
        auto type = parseType();

        if (MATCHES(sequence(IDENTIFIER, SEPARATOR_SQUAREBRACKETOPEN)))
            return parseArrayVariablePlacement(std::move(type));
        else if (MATCHES(sequence(IDENTIFIER)))
            return parseVariablePlacement(std::move(type));
        else if (MATCHES(sequence(OPERATOR_STAR, IDENTIFIER, OPERATOR_INHERIT)))
            return parsePointerVariablePlacement(std::move(type));
        else throwParserError("invalid sequence", 0);
    }

    /* Program */

    // <(parseUsingDeclaration)|(parseVariablePlacement)|(parseStruct)>
    std::vector<std::shared_ptr<ASTNode>> Parser::parseStatements() {
        std::shared_ptr<ASTNode> statement;

        if (MATCHES(sequence(KEYWORD_USING, IDENTIFIER, OPERATOR_ASSIGNMENT)))
            statement = parseUsingDeclaration();
        else if (MATCHES(sequence(KEYWORD_USING, IDENTIFIER)))
            parseForwardDeclaration();
        else if (peek(IDENTIFIER)) {
            auto originalPos = this->m_curr;
            this->m_curr++;
            parseNamespaceResolution();
            bool isFunction = peek(SEPARATOR_ROUNDBRACKETOPEN);
            this->m_curr    = originalPos;

            if (isFunction) {
                this->m_curr++;
                statement = parseFunctionCall();
            } else
                statement = parsePlacement();
        } else if (peek(KEYWORD_BE) || peek(KEYWORD_LE) || peek(VALUETYPE_ANY))
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
        else throwParserError("invalid sequence", 0);

        if (statement && MATCHES(sequence(SEPARATOR_SQUAREBRACKETOPEN, SEPARATOR_SQUAREBRACKETOPEN)))
            parseAttribute(dynamic_cast<Attributable *>(statement.get()));

        if (!MATCHES(sequence(SEPARATOR_ENDOFEXPRESSION)))
            throwParserError("missing ';' at end of expression", -1);

        // Consume superfluous semicolons
        while (MATCHES(sequence(SEPARATOR_ENDOFEXPRESSION)))
            ;

        if (!statement)
            return { };

        return hex::moveToVector(std::move(statement));
    }

    std::shared_ptr<ASTNodeTypeDecl> Parser::addType(const std::string &name, std::unique_ptr<ASTNode> &&node, std::optional<std::endian> endian) {
        auto typeName = getNamespacePrefixedName(name);

        if (this->m_types.contains(typeName) && this->m_types.at(typeName)->isForwardDeclared()) {
            this->m_types.at(typeName)->setType(std::move(node));

            return this->m_types.at(typeName);
        } else {
            if (this->m_types.contains(typeName))
                throwParserError(hex::format("redefinition of type '{}'", typeName));

            std::shared_ptr typeDecl = create(new ASTNodeTypeDecl(typeName, std::move(node), endian));
            this->m_types.insert({ typeName, typeDecl });

            return typeDecl;
        }
    }

    // <(parseNamespace)...> EndOfProgram
    std::optional<std::vector<std::shared_ptr<ASTNode>>> Parser::parse(const std::vector<Token> &tokens) {
        this->m_curr = tokens.begin();

        this->m_types.clear();

        this->m_currNamespace.clear();
        this->m_currNamespace.emplace_back();

        try {
            auto program = parseTillToken(SEPARATOR_ENDOFPROGRAM);

            if (program.empty() || this->m_curr != tokens.end())
                throwParserError("program is empty!", -1);

            return program;
        } catch (PatternLanguageError &e) {
            this->m_error = e;

            return std::nullopt;
        }
    }

}