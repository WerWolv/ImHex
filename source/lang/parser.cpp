#include "lang/parser.hpp"

#include <optional>

namespace hex::lang {

    Parser::Parser() {

    }

    using TokenIter = std::vector<Token>::const_iterator;

    std::vector<ASTNode*> parseTillToken(TokenIter &curr, Token::Type endTokenType);

    bool tryConsume(TokenIter &curr, std::initializer_list<Token::Type> tokenTypes) {
        std::vector<Token>::const_iterator originalPosition = curr;

        for (const auto& type : tokenTypes) {
            if (curr->type != type) {
                curr = originalPosition;
                return false;
            }
            curr++;
        }

        return true;
    }


    ASTNode* parseBuiltinVariableDecl(TokenIter &curr) {
        return new ASTNodeVariableDecl(curr[-3].typeToken.type, curr[-2].identifierToken.identifier);
    }

    ASTNode* parseCustomTypeVariableDecl(TokenIter &curr) {
        return new ASTNodeVariableDecl(Token::TypeToken::Type::CustomType, curr[-2].identifierToken.identifier, curr[-3].identifierToken.identifier);
    }

    ASTNode* parseBuiltinArrayDecl(TokenIter &curr) {
        return new ASTNodeVariableDecl(curr[-6].typeToken.type, curr[-5].identifierToken.identifier, "", { }, curr[-3].integerToken.integer);
    }

    ASTNode* parseCustomTypeArrayDecl(TokenIter &curr) {
        return new ASTNodeVariableDecl(Token::TypeToken::Type::CustomType, curr[-5].identifierToken.identifier, curr[-6].identifierToken.identifier, { }, curr[-3].integerToken.integer);
    }

    ASTNode* parseFreeBuiltinVariableDecl(TokenIter &curr) {
        return new ASTNodeVariableDecl(curr[-5].typeToken.type, curr[-4].identifierToken.identifier, "", curr[-2].integerToken.integer);
    }

    ASTNode* parseFreeCustomTypeVariableDecl(TokenIter &curr) {
        return new ASTNodeVariableDecl(Token::TypeToken::Type::CustomType, curr[-4].identifierToken.identifier, curr[-5].identifierToken.identifier, curr[-2].integerToken.integer);
    }

    ASTNode* parseStruct(TokenIter &curr) {
        const std::string &structName = curr[-2].identifierToken.identifier;
        std::vector<ASTNode*> nodes;

        while (!tryConsume(curr, {Token::Type::ScopeClose})) {
            if (tryConsume(curr, {Token::Type::Type, Token::Type::Identifier, Token::Type::EndOfExpression}))
                nodes.push_back(parseBuiltinVariableDecl(curr));
            else if (tryConsume(curr, {Token::Type::Identifier, Token::Type::Identifier, Token::Type::EndOfExpression}))
                nodes.push_back(parseCustomTypeVariableDecl(curr));
            else if (tryConsume(curr, {Token::Type::Type, Token::Type::Identifier, Token::Type::ArrayOpen, Token::Type::Integer, Token::Type::ArrayClose, Token::Type::EndOfExpression}))
                nodes.push_back(parseBuiltinArrayDecl(curr));
            else if (tryConsume(curr, {Token::Type::Identifier, Token::Type::Identifier, Token::Type::ArrayOpen, Token::Type::Integer, Token::Type::ArrayClose, Token::Type::EndOfExpression}))
                nodes.push_back(parseCustomTypeArrayDecl(curr));
            else break;
        }

        if (!tryConsume(curr, {Token::Type::EndOfExpression})) {
            for(auto &node : nodes) delete node;
            return nullptr;
        }

        return new ASTNodeStruct(structName, nodes);
    }

    ASTNode* parseEnum(TokenIter &curr) {
        const std::string &enumName = curr[-4].identifierToken.identifier;
        const Token::TypeToken::Type underlyingType = curr[-2].typeToken.type;

        if (curr[-3].operatorToken.op != Token::OperatorToken::Operator::Inherit)
            return nullptr;

        if ((static_cast<u32>(underlyingType) & 0x0F) != 0x00)
            return nullptr;

        auto enumNode = new ASTNodeEnum(underlyingType, enumName);

        while (!tryConsume(curr, {Token::Type::ScopeClose})) {
            if (tryConsume(curr, { Token::Type::Identifier, Token::Type::Separator }) || tryConsume(curr, { Token::Type::Identifier, Token::Type::ScopeClose })) {
                u64 value;
                if (enumNode->getValues().empty())
                    value = 0;
                else
                    value = enumNode->getValues().back().first + 1;

                enumNode->getValues().emplace_back(value, curr[-2].identifierToken.identifier);

                if (curr[-1].type == Token::Type::ScopeClose)
                    break;
            }
            else if (tryConsume(curr, { Token::Type::Identifier, Token::Type::Operator, Token::Type::Integer, Token::Type::Separator})) {
                enumNode->getValues().emplace_back(curr[-2].integerToken.integer, curr[-4].identifierToken.identifier);
            }
            else if (tryConsume(curr, { Token::Type::Identifier, Token::Type::Operator, Token::Type::Integer, Token::Type::ScopeClose})) {
                enumNode->getValues().emplace_back(curr[-2].integerToken.integer, curr[-4].identifierToken.identifier);
                break;
            }
            else {
                delete enumNode;
                return nullptr;
            }
        }

        if (!tryConsume(curr, {Token::Type::EndOfExpression})) {
            delete enumNode;
            return nullptr;
        }

        return enumNode;
    }

    ASTNode *parseScope(TokenIter &curr) {
        return new ASTNodeScope(parseTillToken(curr, Token::Type::ScopeClose));
    }

    std::optional<ASTNode*> parseUsingDeclaration(TokenIter &curr) {
        auto keyword = curr[-5].keywordToken;
        auto name = curr[-4].identifierToken;
        auto op = curr[-3].operatorToken;

        if (keyword.keyword != Token::KeywordToken::Keyword::Using)
            return { };

        if (op.op != Token::OperatorToken::Operator::Assignment)
            return { };

        if (curr[-2].type == Token::Type::Type) {
            auto type = curr[-2].typeToken;

            return new ASTNodeTypeDecl(type.type, name.identifier);
        } else if (curr[-2].type == Token::Type::Identifier) {
            auto customType = curr[-2].identifierToken;

            return new ASTNodeTypeDecl(Token::TypeToken::Type::CustomType, name.identifier, customType.identifier);
        }

        return { };
    }

    std::optional<std::vector<ASTNode*>> parseStatement(TokenIter &curr) {
        std::vector<ASTNode*> program;

        // Struct
        if (tryConsume(curr, { Token::Type::Keyword, Token::Type::Identifier, Token::Type::ScopeOpen })) {
            if (curr[-3].keywordToken.keyword == Token::KeywordToken::Keyword::Struct) {
                auto structAst = parseStruct(curr);

                if (structAst == nullptr) {
                    for(auto &node : program) delete node;
                    return { };
                }

                program.push_back(structAst);
            }

            return program;

        } // Enum
        if (tryConsume(curr, { Token::Type::Keyword, Token::Type::Identifier, Token::Type::Operator, Token::Type::Type, Token::Type::ScopeOpen })) {
            if (curr[-5].keywordToken.keyword == Token::KeywordToken::Keyword::Enum) {
                auto enumAst = parseEnum(curr);

                if (enumAst == nullptr) {
                    for(auto &node : program) delete node;
                    return { };
                }

                program.push_back(enumAst);
            }

            return program;

        // Scope
        } else if (tryConsume(curr, { Token::Type::ScopeOpen })) {
            program.push_back(parseScope(curr));

            return program;

        // Using declaration with built-in type
        } else if (tryConsume(curr, { Token::Type::Keyword, Token::Type::Identifier, Token::Type::Operator, Token::Type::Type, Token::Type::EndOfExpression})) {
            auto usingDecl = parseUsingDeclaration(curr);

            if (!usingDecl.has_value()) {
                for(auto &node : program) delete node;
                return { };
            }

            program.push_back(usingDecl.value());

            return program;

        // Using declaration with custom type
        } else if (tryConsume(curr, { Token::Type::Keyword, Token::Type::Identifier, Token::Type::Operator, Token::Type::Identifier, Token::Type::EndOfExpression})) {
            auto usingDecl = parseUsingDeclaration(curr);

            if (!usingDecl.has_value()) {
                for(auto &node : program) delete node;
                return { };
            }

            program.push_back(usingDecl.value());

            return program;
        // Variable declaration with built-in type
        } else if (tryConsume(curr, { Token::Type::Type, Token::Type::Identifier, Token::Type::Operator, Token::Type::Integer, Token::Type::EndOfExpression})) {
            auto variableDecl = parseFreeBuiltinVariableDecl(curr);

            program.push_back(variableDecl);

            return program;

        // Variable declaration with custom type
        } else if (tryConsume(curr, { Token::Type::Identifier, Token::Type::Identifier, Token::Type::Operator, Token::Type::Integer, Token::Type::EndOfExpression})) {
            auto variableDecl = parseFreeCustomTypeVariableDecl(curr);

            program.push_back(variableDecl);

            return program;
        }
        else {
            for(auto &node : program) delete node;
            return { };
        }
    }

    std::vector<ASTNode*> parseTillToken(TokenIter &curr, Token::Type endTokenType) {
        std::vector<ASTNode*> program;

        while (curr->type != endTokenType) {
            auto newTokens = parseStatement(curr);

            if (!newTokens.has_value())
                break;

            program.insert(program.end(), newTokens->begin(), newTokens->end());
        }

        curr++;

        return program;
    }

    std::pair<Result, std::vector<ASTNode*>> Parser::parse(const std::vector<Token> &tokens) {
        auto currentToken = tokens.begin();

        auto program = parseTillToken(currentToken, Token::Type::EndOfProgram);

        if (program.empty() || currentToken != tokens.end())
            return { ResultParseError, { } };

        return { ResultSuccess, program };
    }

}