#include "lang/parser.hpp"

#include "helpers/utils.hpp"

#include <optional>

namespace hex::lang {

    Parser::Parser() {

    }

    bool Parser::tryConsume(TokenIter &curr, std::initializer_list<Token::Type> tokenTypes) {
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


    ASTNode* Parser::parseBuiltinVariableDecl(TokenIter &curr, bool hasEndianDef) {
        if (hasEndianDef) {
            std::endian endianess;

            if (curr[-4].keywordToken.keyword == Token::KeywordToken::Keyword::LittleEndian)
                endianess = std::endian::little;
            else if (curr[-4].keywordToken.keyword == Token::KeywordToken::Keyword::BigEndian)
                endianess = std::endian::big;
            else {
                this->m_error = { curr->lineNumber, "Expected be or le identifier" };
                return nullptr;
            }

            return new ASTNodeVariableDecl(curr[-4].lineNumber, curr[-3].typeToken.type, curr[-2].identifierToken.identifier, "", {}, 1, {}, {}, endianess);
        }
        else
            return new ASTNodeVariableDecl(curr[-3].lineNumber, curr[-3].typeToken.type, curr[-2].identifierToken.identifier);
    }

    ASTNode* Parser::parseCustomTypeVariableDecl(TokenIter &curr, bool hasEndianDef) {
        if (hasEndianDef) {
            std::endian endianess;

            if (curr[-4].keywordToken.keyword == Token::KeywordToken::Keyword::LittleEndian)
                endianess = std::endian::little;
            else if (curr[-4].keywordToken.keyword == Token::KeywordToken::Keyword::BigEndian)
                endianess = std::endian::big;
            else return nullptr;

            return new ASTNodeVariableDecl(curr[-4].lineNumber, Token::TypeToken::Type::CustomType, curr[-2].identifierToken.identifier, curr[-3].identifierToken.identifier, {}, 1, {}, {}, endianess);
        }
        else
            return new ASTNodeVariableDecl(curr[-3].lineNumber, Token::TypeToken::Type::CustomType, curr[-2].identifierToken.identifier, curr[-3].identifierToken.identifier);
    }

    ASTNode* Parser::parseBuiltinPointerVariableDecl(TokenIter &curr, bool hasEndianDef) {
        auto pointerType = curr[-2].typeToken.type;

        if (!isUnsigned(pointerType)) {
            this->m_error = { curr->lineNumber, "Pointer size needs to be a unsigned type" };
            return nullptr;
        }

        if (curr[-5].operatorToken.op != Token::OperatorToken::Operator::Star) {
            this->m_error = { curr->lineNumber, "Expected '*' for pointer definition" };
            return nullptr;
        }

        if (curr[-3].operatorToken.op != Token::OperatorToken::Operator::Inherit) {
            this->m_error = { curr->lineNumber, "Expected ':' after member name" };
            return nullptr;
        }


        if (hasEndianDef) {
            std::endian endianess;

            if (curr[-7].keywordToken.keyword == Token::KeywordToken::Keyword::LittleEndian)
                endianess = std::endian::little;
            else if (curr[-7].keywordToken.keyword == Token::KeywordToken::Keyword::BigEndian)
                endianess = std::endian::big;
            else return nullptr;

            return new ASTNodeVariableDecl(curr[-7].lineNumber, curr[-6].typeToken.type, curr[-4].identifierToken.identifier, "", { }, 1, { }, getTypeSize(pointerType), endianess);
        }
        else
            return new ASTNodeVariableDecl(curr[-6].lineNumber, curr[-6].typeToken.type, curr[-4].identifierToken.identifier, "", { }, 1, { }, getTypeSize(pointerType));
    }

    ASTNode* Parser::parseCustomTypePointerVariableDecl(TokenIter &curr, bool hasEndianDef) {
        auto pointerType = curr[-2].typeToken.type;

        if (!isUnsigned(pointerType)) {
            this->m_error = { curr->lineNumber, "Pointer size needs to be a unsigned type" };
            return nullptr;
        }

        if (curr[-5].operatorToken.op != Token::OperatorToken::Operator::Star) {
            this->m_error = { curr->lineNumber, "Expected '*' for pointer definition" };
            return nullptr;
        }

        if (curr[-3].operatorToken.op != Token::OperatorToken::Operator::Inherit) {
            this->m_error = { curr->lineNumber, "Expected ':' after member name" };
            return nullptr;
        }

        if (hasEndianDef) {
            std::endian endianess;

            if (curr[-7].keywordToken.keyword == Token::KeywordToken::Keyword::LittleEndian)
                endianess = std::endian::little;
            else if (curr[-7].keywordToken.keyword == Token::KeywordToken::Keyword::BigEndian)
                endianess = std::endian::big;
            else {
                this->m_error = { curr->lineNumber, "Expected be or le identifier" };
                return nullptr;
            }

            return new ASTNodeVariableDecl(curr[-7].lineNumber,Token::TypeToken::Type::CustomType, curr[-4].identifierToken.identifier, curr[-6].identifierToken.identifier, { }, 1, { }, getTypeSize(pointerType), endianess);
        }
        else
            return new ASTNodeVariableDecl(curr[-6].lineNumber, Token::TypeToken::Type::CustomType, curr[-4].identifierToken.identifier, curr[-6].identifierToken.identifier, { }, 1, { }, getTypeSize(pointerType));
    }

    ASTNode* Parser::parseBuiltinArrayDecl(TokenIter &curr, bool hasEndianDef) {
        if (hasEndianDef) {
            std::endian endianess;

            if (curr[-7].keywordToken.keyword == Token::KeywordToken::Keyword::LittleEndian)
                endianess = std::endian::little;
            else if (curr[-7].keywordToken.keyword == Token::KeywordToken::Keyword::BigEndian)
                endianess = std::endian::big;
            else {
                this->m_error = { curr->lineNumber, "Expected be or le identifier" };
                return nullptr;
            }

            return new ASTNodeVariableDecl(curr[-7].lineNumber, curr[-6].typeToken.type, curr[-5].identifierToken.identifier, "", { }, curr[-3].integerToken.integer, { }, { }, endianess);
        }
        else
            return new ASTNodeVariableDecl(curr[-6].lineNumber, curr[-6].typeToken.type, curr[-5].identifierToken.identifier, "", { }, curr[-3].integerToken.integer);
    }

    ASTNode* Parser::parseCustomTypeArrayDecl(TokenIter &curr, bool hasEndianDef) {
        if (hasEndianDef) {
            std::endian endianess;

            if (curr[-7].keywordToken.keyword == Token::KeywordToken::Keyword::LittleEndian)
                endianess = std::endian::little;
            else if (curr[-7].keywordToken.keyword == Token::KeywordToken::Keyword::BigEndian)
                endianess = std::endian::big;
            else {
                this->m_error = { curr->lineNumber, "Expected be or le identifier" };
                return nullptr;
            }

            return new ASTNodeVariableDecl(curr[-7].lineNumber, Token::TypeToken::Type::CustomType, curr[-5].identifierToken.identifier, curr[-6].identifierToken.identifier, { }, curr[-3].integerToken.integer, { }, { }, endianess);
        }
        else
            return new ASTNodeVariableDecl(curr[-6].lineNumber, Token::TypeToken::Type::CustomType, curr[-5].identifierToken.identifier, curr[-6].identifierToken.identifier, { }, curr[-3].integerToken.integer);
    }

    ASTNode* Parser::parseBuiltinVariableArrayDecl(TokenIter &curr, bool hasEndianDef) {
        if (hasEndianDef) {
            std::endian endianess;

            if (curr[-7].keywordToken.keyword == Token::KeywordToken::Keyword::LittleEndian)
                endianess = std::endian::little;
            else if (curr[-7].keywordToken.keyword == Token::KeywordToken::Keyword::BigEndian)
                endianess = std::endian::big;
            else {
                this->m_error = { curr->lineNumber, "Expected be or le identifier" };
                return nullptr;
            }

            return new ASTNodeVariableDecl(curr[-7].lineNumber, curr[-6].typeToken.type, curr[-5].identifierToken.identifier, "", { }, 0, curr[-3].identifierToken.identifier, { }, endianess);
        }
        else
            return new ASTNodeVariableDecl(curr[-6].lineNumber, curr[-6].typeToken.type, curr[-5].identifierToken.identifier, "", { }, 0, curr[-3].identifierToken.identifier);
    }

    ASTNode* Parser::parseCustomTypeVariableArrayDecl(TokenIter &curr, bool hasEndianDef) {
        if (hasEndianDef) {
            std::endian endianess;

            if (curr[-7].keywordToken.keyword == Token::KeywordToken::Keyword::LittleEndian)
                endianess = std::endian::little;
            else if (curr[-7].keywordToken.keyword == Token::KeywordToken::Keyword::BigEndian)
                endianess = std::endian::big;
            else {
                this->m_error = { curr->lineNumber, "Expected be or le identifier" };
                return nullptr;
            }

            return new ASTNodeVariableDecl(curr[-7].lineNumber, Token::TypeToken::Type::CustomType, curr[-5].identifierToken.identifier, curr[-6].identifierToken.identifier, { }, 0, curr[-3].identifierToken.identifier, { }, endianess);
        }
        else
            return new ASTNodeVariableDecl(curr[-6].lineNumber, Token::TypeToken::Type::CustomType, curr[-5].identifierToken.identifier, curr[-6].identifierToken.identifier, { }, 0, curr[-3].identifierToken.identifier);
    }

    ASTNode* Parser::parsePaddingDecl(TokenIter &curr) {
            return new ASTNodeVariableDecl(curr[-5].lineNumber, curr[-5].typeToken.type, "", "", { }, curr[-3].integerToken.integer);
    }

    ASTNode* Parser::parseFreeBuiltinVariableDecl(TokenIter &curr, bool hasEndianDef) {
        if (hasEndianDef) {
            std::endian endianess;

            if (curr[-6].keywordToken.keyword == Token::KeywordToken::Keyword::LittleEndian)
                endianess = std::endian::little;
            else if (curr[-6].keywordToken.keyword == Token::KeywordToken::Keyword::BigEndian)
                endianess = std::endian::big;
            else {
                this->m_error = { curr->lineNumber, "Expected be or le identifier" };
                return nullptr;
            }

            return new ASTNodeVariableDecl(curr[-6].lineNumber, curr[-5].typeToken.type, curr[-4].identifierToken.identifier, "", curr[-2].integerToken.integer, 1, { }, { }, endianess);
        }
        else
            return new ASTNodeVariableDecl(curr[-5].lineNumber, curr[-5].typeToken.type, curr[-4].identifierToken.identifier, "", curr[-2].integerToken.integer);
    }

    ASTNode* Parser::parseFreeCustomTypeVariableDecl(TokenIter &curr, bool hasEndianDef) {
        if (hasEndianDef) {
            std::endian endianess;

            if (curr[-6].keywordToken.keyword == Token::KeywordToken::Keyword::LittleEndian)
                endianess = std::endian::little;
            else if (curr[-6].keywordToken.keyword == Token::KeywordToken::Keyword::BigEndian)
                endianess = std::endian::big;
            else {
                this->m_error = { curr->lineNumber, "Expected be or le identifier" };
                return nullptr;
            }

            return new ASTNodeVariableDecl(curr[-6].lineNumber, Token::TypeToken::Type::CustomType, curr[-4].identifierToken.identifier, curr[-5].identifierToken.identifier, curr[-2].integerToken.integer, 1, { }, { }, endianess);
        }
        else
            return new ASTNodeVariableDecl(curr[-5].lineNumber, Token::TypeToken::Type::CustomType, curr[-4].identifierToken.identifier, curr[-5].identifierToken.identifier, curr[-2].integerToken.integer);
    }

    ASTNode* Parser::parseStruct(TokenIter &curr) {
        const std::string &structName = curr[-2].identifierToken.identifier;
        std::vector<ASTNode*> nodes;

        u32 startLineNumber = curr[-3].lineNumber;

        while (!tryConsume(curr, {Token::Type::ScopeClose})) {
            if (tryConsume(curr, {Token::Type::Type, Token::Type::Identifier, Token::Type::EndOfExpression}))
                nodes.push_back(parseBuiltinVariableDecl(curr, false));
            else if (tryConsume(curr, {Token::Type::Identifier, Token::Type::Identifier, Token::Type::EndOfExpression}))
                nodes.push_back(parseCustomTypeVariableDecl(curr, false));
            else if (tryConsume(curr, {Token::Type::Type, Token::Type::Identifier, Token::Type::ArrayOpen, Token::Type::Integer, Token::Type::ArrayClose, Token::Type::EndOfExpression}))
                nodes.push_back(parseBuiltinArrayDecl(curr, false));
            else if (tryConsume(curr, {Token::Type::Identifier, Token::Type::Identifier, Token::Type::ArrayOpen, Token::Type::Integer, Token::Type::ArrayClose, Token::Type::EndOfExpression}))
                nodes.push_back(parseCustomTypeArrayDecl(curr, false));
            else if (tryConsume(curr, {Token::Type::Type, Token::Type::Identifier, Token::Type::ArrayOpen, Token::Type::Identifier, Token::Type::ArrayClose, Token::Type::EndOfExpression}))
                nodes.push_back(parseBuiltinVariableArrayDecl(curr, false));
            else if (tryConsume(curr, {Token::Type::Identifier, Token::Type::Identifier, Token::Type::ArrayOpen, Token::Type::Identifier, Token::Type::ArrayClose, Token::Type::EndOfExpression}))
                nodes.push_back(parseCustomTypeVariableArrayDecl(curr, false));
            else if (tryConsume(curr, {Token::Type::Type, Token::Type::ArrayOpen, Token::Type::Integer, Token::Type::ArrayClose, Token::Type::EndOfExpression})) {
                if (curr[-5].typeToken.type != Token::TypeToken::Type::Padding) {
                    for(auto &node : nodes) delete node;

                    this->m_error = { curr[-5].lineNumber, "No member name provided" };
                    return nullptr;
                }
                nodes.push_back(parsePaddingDecl(curr));
            } else if (tryConsume(curr, {Token::Type::Type, Token::Type::Operator, Token::Type::Identifier, Token::Type::Operator, Token::Type::Type, Token::Type::EndOfExpression}))
                nodes.push_back(parseBuiltinPointerVariableDecl(curr, false));
            else if (tryConsume(curr, {Token::Type::Identifier, Token::Type::Operator, Token::Type::Identifier, Token::Type::Operator, Token::Type::Type, Token::Type::EndOfExpression}))
                nodes.push_back(parseCustomTypePointerVariableDecl(curr, false));
            else if (tryConsume(curr, {Token::Type::Keyword, Token::Type::Type, Token::Type::Identifier, Token::Type::EndOfExpression}))
                nodes.push_back(parseBuiltinVariableDecl(curr, true));
            else if (tryConsume(curr, {Token::Type::Keyword, Token::Type::Identifier, Token::Type::Identifier, Token::Type::EndOfExpression}))
                nodes.push_back(parseCustomTypeVariableDecl(curr, true));
            else if (tryConsume(curr, {Token::Type::Keyword, Token::Type::Type, Token::Type::Identifier, Token::Type::ArrayOpen, Token::Type::Integer, Token::Type::ArrayClose, Token::Type::EndOfExpression}))
                nodes.push_back(parseBuiltinArrayDecl(curr, true));
            else if (tryConsume(curr, {Token::Type::Keyword, Token::Type::Identifier, Token::Type::Identifier, Token::Type::ArrayOpen, Token::Type::Integer, Token::Type::ArrayClose, Token::Type::EndOfExpression}))
                nodes.push_back(parseCustomTypeArrayDecl(curr, true));
            else if (tryConsume(curr, {Token::Type::Keyword, Token::Type::Type, Token::Type::Identifier, Token::Type::ArrayOpen, Token::Type::Identifier, Token::Type::ArrayClose, Token::Type::EndOfExpression}))
                nodes.push_back(parseBuiltinVariableArrayDecl(curr, true));
            else if (tryConsume(curr, {Token::Type::Keyword, Token::Type::Identifier, Token::Type::Identifier, Token::Type::ArrayOpen, Token::Type::Identifier, Token::Type::ArrayClose, Token::Type::EndOfExpression}))
                nodes.push_back(parseCustomTypeVariableArrayDecl(curr, true));
            else if (tryConsume(curr, {Token::Type::Keyword, Token::Type::Type, Token::Type::Operator, Token::Type::Identifier, Token::Type::Operator, Token::Type::Type, Token::Type::EndOfExpression}))
                nodes.push_back(parseBuiltinPointerVariableDecl(curr, true));
            else if (tryConsume(curr, {Token::Type::Keyword, Token::Type::Identifier, Token::Type::Operator, Token::Type::Identifier, Token::Type::Operator, Token::Type::Type, Token::Type::EndOfExpression}))
                nodes.push_back(parseCustomTypePointerVariableDecl(curr, true));
            else {
                for(auto &node : nodes) delete node;
                this->m_error = { curr[-1].lineNumber, "Invalid sequence, expected member declaration" };
                return nullptr;
            }
        }

        if (!tryConsume(curr, {Token::Type::EndOfExpression})) {
            this->m_error = { curr[-1].lineNumber, "Expected ';' after struct definition" };
            for(auto &node : nodes) delete node;
            return nullptr;
        }

        return new ASTNodeStruct(startLineNumber, structName, nodes);
    }

    ASTNode* Parser::parseUnion(TokenIter &curr) {
        const std::string &unionName = curr[-2].identifierToken.identifier;
        std::vector<ASTNode*> nodes;

        u32 startLineNumber = curr[-3].lineNumber;

        while (!tryConsume(curr, {Token::Type::ScopeClose})) {
            if (tryConsume(curr, {Token::Type::Type, Token::Type::Identifier, Token::Type::EndOfExpression}))
                nodes.push_back(parseBuiltinVariableDecl(curr, false));
            else if (tryConsume(curr, {Token::Type::Identifier, Token::Type::Identifier, Token::Type::EndOfExpression}))
                nodes.push_back(parseCustomTypeVariableDecl(curr, false));
            else if (tryConsume(curr, {Token::Type::Type, Token::Type::Identifier, Token::Type::ArrayOpen, Token::Type::Integer, Token::Type::ArrayClose, Token::Type::EndOfExpression}))
                nodes.push_back(parseBuiltinArrayDecl(curr, false));
            else if (tryConsume(curr, {Token::Type::Identifier, Token::Type::Identifier, Token::Type::ArrayOpen, Token::Type::Integer, Token::Type::ArrayClose, Token::Type::EndOfExpression}))
                nodes.push_back(parseCustomTypeArrayDecl(curr, false));
            else if (tryConsume(curr, {Token::Type::Type, Token::Type::Operator, Token::Type::Identifier, Token::Type::Operator, Token::Type::Type, Token::Type::EndOfExpression}))
                nodes.push_back(parseBuiltinPointerVariableDecl(curr, false));
            else if (tryConsume(curr, {Token::Type::Identifier, Token::Type::Operator, Token::Type::Identifier, Token::Type::Operator, Token::Type::Type, Token::Type::EndOfExpression}))
                nodes.push_back(parseCustomTypePointerVariableDecl(curr, false));
            else if (tryConsume(curr, {Token::Type::Keyword, Token::Type::Type, Token::Type::Identifier, Token::Type::EndOfExpression}))
                nodes.push_back(parseBuiltinVariableDecl(curr, true));
            else if (tryConsume(curr, {Token::Type::Keyword, Token::Type::Identifier, Token::Type::Identifier, Token::Type::EndOfExpression}))
                nodes.push_back(parseCustomTypeVariableDecl(curr, true));
            else if (tryConsume(curr, {Token::Type::Keyword, Token::Type::Type, Token::Type::Identifier, Token::Type::ArrayOpen, Token::Type::Integer, Token::Type::ArrayClose, Token::Type::EndOfExpression}))
                nodes.push_back(parseBuiltinArrayDecl(curr, true));
            else if (tryConsume(curr, {Token::Type::Keyword, Token::Type::Identifier, Token::Type::Identifier, Token::Type::ArrayOpen, Token::Type::Integer, Token::Type::ArrayClose, Token::Type::EndOfExpression}))
                nodes.push_back(parseCustomTypeArrayDecl(curr, true));
            else if (tryConsume(curr, {Token::Type::Keyword, Token::Type::Type, Token::Type::Operator, Token::Type::Identifier, Token::Type::Operator, Token::Type::Type, Token::Type::EndOfExpression}))
                nodes.push_back(parseBuiltinPointerVariableDecl(curr, true));
            else if (tryConsume(curr, {Token::Type::Keyword, Token::Type::Identifier, Token::Type::Operator, Token::Type::Identifier, Token::Type::Operator, Token::Type::Type, Token::Type::EndOfExpression}))
                nodes.push_back(parseCustomTypePointerVariableDecl(curr, true));
            else {
                for(auto &node : nodes) delete node;
                this->m_error = { curr[-1].lineNumber, "Invalid sequence, expected member declaration" };
                return nullptr;
            }
        }

        if (!tryConsume(curr, {Token::Type::EndOfExpression})) {
            for(auto &node : nodes) delete node;
            this->m_error = { curr[-1].lineNumber, "Expected ';' after union definition" };
            return nullptr;
        }

        return new ASTNodeUnion(startLineNumber, unionName, nodes);
    }

    ASTNode* Parser::parseEnum(TokenIter &curr) {
        const std::string &enumName = curr[-4].identifierToken.identifier;
        const Token::TypeToken::Type underlyingType = curr[-2].typeToken.type;

        u32 startLineNumber = curr[-5].lineNumber;

        if (curr[-3].operatorToken.op != Token::OperatorToken::Operator::Inherit) {
            this->m_error = { curr[-3].lineNumber, "Expected ':' after enum name" };
            return nullptr;
        }

        if (!isUnsigned(underlyingType)) {
            this->m_error = { curr[-3].lineNumber, "Underlying type needs to be an unsigned type" };
            return nullptr;
        }

        auto enumNode = new ASTNodeEnum(startLineNumber, underlyingType, enumName);

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
                this->m_error = { curr->lineNumber, "Expected constant identifier" };
                return nullptr;
            }
        }

        if (!tryConsume(curr, {Token::Type::EndOfExpression})) {
            delete enumNode;
            this->m_error = { curr[-1].lineNumber, "Expected ';' after enum definition" };
            return nullptr;
        }

        return enumNode;
    }

    ASTNode* Parser::parseBitField(TokenIter &curr) {
        const std::string &bitfieldName = curr[-2].identifierToken.identifier;
        std::vector<std::pair<std::string, size_t>> fields;

        u32 startLineNumber = curr[-3].lineNumber;

        while (!tryConsume(curr, {Token::Type::ScopeClose})) {
            if (tryConsume(curr, {Token::Type::Identifier, Token::Type::Operator, Token::Type::Integer, Token::Type::EndOfExpression})) {
                if (curr[-3].operatorToken.op != Token::OperatorToken::Operator::Inherit) {
                    this->m_error = { curr[-3].lineNumber, "Expected ':' after member name" };
                    return nullptr;
                }

                fields.emplace_back(curr[-4].identifierToken.identifier, curr[-2].integerToken.integer);
            }
            else {
                this->m_error = { curr[-1].lineNumber, "Invalid sequence, expected member declaration" };
                return nullptr;
            }
        }

        if (!tryConsume(curr, {Token::Type::EndOfExpression})) {
            this->m_error = { curr[-1].lineNumber, "Expected ';' after bitfield definition" };
            return nullptr;
        }

        return new ASTNodeBitField(startLineNumber, bitfieldName, fields);
    }

    ASTNode* Parser::parseScope(TokenIter &curr) {
        return new ASTNodeScope(curr[-1].lineNumber, parseTillToken(curr, Token::Type::ScopeClose));
    }

    std::optional<ASTNode*> Parser::parseUsingDeclaration(TokenIter &curr) {
        auto keyword = curr[-5].keywordToken;
        auto name = curr[-4].identifierToken;
        auto op = curr[-3].operatorToken;

        if (keyword.keyword != Token::KeywordToken::Keyword::Using) {
            this->m_error = { curr[-5].lineNumber, "Invalid keyword. Expected 'using'" };
            return { };
        }

        if (op.op != Token::OperatorToken::Operator::Assignment) {
            this->m_error = { curr[-3].lineNumber, "Invalid operator. Expected '='" };
            return { };
        }

        if (curr[-2].type == Token::Type::Type) {
            auto type = curr[-2].typeToken;

            return new ASTNodeTypeDecl(curr[-2].lineNumber, type.type, name.identifier);
        } else if (curr[-2].type == Token::Type::Identifier) {
            auto customType = curr[-2].identifierToken;

            return new ASTNodeTypeDecl(curr[-2].lineNumber, Token::TypeToken::Type::CustomType, name.identifier, customType.identifier);
        }

        this->m_error = { curr[-2].lineNumber, hex::format("'%s' does not name a type") };
        return { };
    }

    std::optional<std::vector<ASTNode*>> Parser::parseStatement(TokenIter &curr) {
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
            } else if (curr[-3].keywordToken.keyword == Token::KeywordToken::Keyword::Union) {
                auto unionAst = parseUnion(curr);

                if (unionAst == nullptr) {
                    for(auto &node : program) delete node;
                    return { };
                }

                program.push_back(unionAst);
            } else if (curr[-3].keywordToken.keyword == Token::KeywordToken::Keyword::Bitfield) {
                auto bitfieldAst = parseBitField(curr);

                if (bitfieldAst == nullptr) {
                    for(auto &node : program) delete node;
                    return { };
                }

                program.push_back(bitfieldAst);
            }

            return program;

        } // Enum
        else if (tryConsume(curr, { Token::Type::Keyword, Token::Type::Identifier, Token::Type::Operator, Token::Type::Type, Token::Type::ScopeOpen })) {
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
        // Variable placement declaration with built-in type
        } else if (tryConsume(curr, { Token::Type::Type, Token::Type::Identifier, Token::Type::Operator, Token::Type::Integer, Token::Type::EndOfExpression})) {
            if (curr[-3].operatorToken.op != Token::OperatorToken::Operator::AtDeclaration) {
                this->m_error = { curr[-3].lineNumber, "Expected '@' after variable placement declaration" };
                for(auto &node : program) delete node;
                return { };
            }

            auto variableDecl = parseFreeBuiltinVariableDecl(curr, false);

            program.push_back(variableDecl);

            return program;

        // Variable placement declaration with custom type
        } else if (tryConsume(curr, { Token::Type::Identifier, Token::Type::Identifier, Token::Type::Operator, Token::Type::Integer, Token::Type::EndOfExpression})) {
            if (curr[-3].operatorToken.op != Token::OperatorToken::Operator::AtDeclaration) {
                this->m_error = { curr[-3].lineNumber, "Expected '@' after variable placement declaration" };
                for(auto &node : program) delete node;
                return { };
            }

            auto variableDecl = parseFreeCustomTypeVariableDecl(curr, false);

            program.push_back(variableDecl);

            return program;

            // Variable placement declaration with built-in type and big/little endian setting
        } else if (tryConsume(curr, { Token::Type::Keyword, Token::Type::Type, Token::Type::Identifier, Token::Type::Operator, Token::Type::Integer, Token::Type::EndOfExpression})) {
            if (curr[-3].operatorToken.op != Token::OperatorToken::Operator::AtDeclaration) {
                this->m_error = { curr[-3].lineNumber, "Expected '@' after variable placement declaration" };
                for(auto &node : program) delete node;
                return { };
            }

            if (curr[-6].keywordToken.keyword != Token::KeywordToken::Keyword::LittleEndian && curr[-6].keywordToken.keyword != Token::KeywordToken::Keyword::BigEndian) {
                this->m_error = { curr[-3].lineNumber, "Expected endianess identifier" };
                for(auto &node : program) delete node;
                return { };
            }

            auto variableDecl = parseFreeBuiltinVariableDecl(curr, true);

            program.push_back(variableDecl);

            return program;

            // Variable placement declaration with custom type and big/little endian setting
        } else if (tryConsume(curr, { Token::Type::Keyword, Token::Type::Identifier, Token::Type::Identifier, Token::Type::Operator, Token::Type::Integer, Token::Type::EndOfExpression})) {
            if (curr[-3].operatorToken.op != Token::OperatorToken::Operator::AtDeclaration) {
                this->m_error = { curr[-3].lineNumber, "Expected '@' after variable placement declaration" };
                for(auto &node : program) delete node;
                return { };
            }

            if (curr[-6].keywordToken.keyword != Token::KeywordToken::Keyword::LittleEndian && curr[-6].keywordToken.keyword != Token::KeywordToken::Keyword::BigEndian) {
                this->m_error = { curr[-6].lineNumber, "Expected endianess identifier" };
                for(auto &node : program) delete node;
                return { };
            }

            auto variableDecl = parseFreeCustomTypeVariableDecl(curr, true);

            program.push_back(variableDecl);

            return program;
        }
        else {
            for(auto &node : program) delete node;
            this->m_error = { curr->lineNumber, "Invalid sequence" };
            return { };
        }
    }

    std::vector<ASTNode*> Parser::parseTillToken(TokenIter &curr, Token::Type endTokenType) {
        std::vector<ASTNode*> program;

        while (curr->type != endTokenType) {
            auto newTokens = parseStatement(curr);

            if (!newTokens.has_value()) {
                for (auto &node : program) delete node;
                return { };
            }

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