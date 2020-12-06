#pragma once

#include <hex.hpp>
#include "token.hpp"
#include "ast_node.hpp"

#include <vector>

namespace hex::lang {

    class Parser {
    public:
        Parser();

        using TokenIter = std::vector<Token>::const_iterator;

        std::pair<Result, std::vector<ASTNode*>> parse(const std::vector<Token> &tokens);

        const std::pair<u32, std::string>& getError() { return this->m_error; }

    private:
        std::pair<u32, std::string> m_error;


        ASTNode* parseBuiltinVariableDecl(TokenIter &curr, bool hasEndianDef);
        ASTNode* parseCustomTypeVariableDecl(TokenIter &curr, bool hasEndianDef);
        ASTNode* parseBuiltinPointerVariableDecl(TokenIter &curr, bool hasEndianDef);
        ASTNode* parseCustomTypePointerVariableDecl(TokenIter &curr, bool hasEndianDef);
        ASTNode* parseBuiltinArrayDecl(TokenIter &curr, bool hasEndianDef);
        ASTNode* parseCustomTypeArrayDecl(TokenIter &curr, bool hasEndianDef);
        ASTNode* parseBuiltinVariableArrayDecl(TokenIter &curr, bool hasEndianDef);
        ASTNode* parseCustomTypeVariableArrayDecl(TokenIter &curr, bool hasEndianDef);
        ASTNode* parsePaddingDecl(TokenIter &curr);
        ASTNode* parseFreeBuiltinVariableDecl(TokenIter &curr, bool hasEndianDef);
        ASTNode* parseFreeCustomTypeVariableDecl(TokenIter &curr, bool hasEndianDef);

        ASTNode* parseStruct(TokenIter &curr);
        ASTNode* parseUnion(TokenIter &curr);
        ASTNode* parseEnum(TokenIter &curr);
        ASTNode *parseBitField(TokenIter &curr);
        ASTNode *parseScope(TokenIter &curr);
        std::optional<ASTNode*> parseUsingDeclaration(TokenIter &curr);
        std::optional<std::vector<ASTNode*>> parseStatement(TokenIter &curr);

        std::vector<ASTNode*> parseTillToken(TokenIter &curr, Token::Type endTokenType);
        bool tryConsume(TokenIter &curr, std::initializer_list<Token::Type> tokenTypes);
    };

}