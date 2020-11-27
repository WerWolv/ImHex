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


        ASTNode* parseBuiltinVariableDecl(TokenIter &curr);
        ASTNode* parseCustomTypeVariableDecl(TokenIter &curr);
        ASTNode* parseBuiltinPointerVariableDecl(TokenIter &curr);
        ASTNode* parseCustomTypePointerVariableDecl(TokenIter &curr);
        ASTNode* parseBuiltinArrayDecl(TokenIter &curr);
        ASTNode* parseCustomTypeArrayDecl(TokenIter &curr);
        ASTNode* parseBuiltinVariableArrayDecl(TokenIter &curr);
        ASTNode* parseCustomTypeVariableArrayDecl(TokenIter &curr);
        ASTNode* parsePaddingDecl(TokenIter &curr);
        ASTNode* parseFreeBuiltinVariableDecl(TokenIter &curr);
        ASTNode* parseFreeCustomTypeVariableDecl(TokenIter &curr);
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