#pragma once

#include <hex.hpp>

#include "token.hpp"
#include "ast_node.hpp"

#include "helpers/utils.hpp"

#include <unordered_map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace hex::lang {

    class Parser {
    public:
        Parser() = default;
        ~Parser() = default;

        using TokenIter = std::vector<Token>::const_iterator;

        std::optional<std::vector<ASTNode*>> parse(const std::vector<Token> &tokens);

        const std::pair<u32, std::string>& getError() { return this->m_error; }

    private:
        using ParseError = std::pair<u32, std::string>;

        ParseError m_error;
        TokenIter m_curr;
        TokenIter m_originalPosition;

        std::unordered_map<std::string, ASTNode*> m_types;
        std::vector<TokenIter> m_matchedOptionals;

        u32 getLineNumber(s32 index) const {
            return this->m_curr[index].lineNumber;
        }

        template<typename T>
        const T& getValue(s32 index) const {
            auto value = std::get_if<T>(&this->m_curr[index].value);

            if (value == nullptr)
                throwParseError("Failed to decode token. Invalid type.", getLineNumber(index));

            return *value;
        }

        Token::Type getType(s32 index) const {
            return this->m_curr[index].type;
        }

        ASTNode* parseFactor();
        ASTNode* parseMultiplicativeExpression();
        ASTNode* parseAdditiveExpression();
        ASTNode* parseShiftExpression();
        ASTNode* parseBinaryAndExpression();
        ASTNode* parseBinaryXorExpression();
        ASTNode* parseBinaryOrExpression();
        ASTNode* parseMathematicalExpression();

        ASTNode* parseType(s32 startIndex);
        ASTNode* parseUsingDeclaration();
        ASTNode* parsePadding();
        ASTNode* parseMemberVariable();
        ASTNode* parseMemberArrayVariable();
        ASTNode* parseStruct();
        ASTNode* parseUnion();
        ASTNode* parseEnum();
        ASTNode* parseVariablePlacement();
        ASTNode* parseArrayVariablePlacement();
        ASTNode* parseStatement();

        std::vector<ASTNode*> parseTillToken(Token::Type endTokenType, const auto value) {
            std::vector<ASTNode*> program;
            ScopeExit guard([&]{ for (auto &node : program) delete node; });

            while (this->m_curr->type != endTokenType || (*this->m_curr) != value) {
                program.push_back(parseStatement());
            }

            this->m_curr++;

            guard.release();

            return program;
        }

        [[noreturn]] void throwParseError(std::string_view error, s32 token = -1) const {
            throw ParseError(this->m_curr[token].lineNumber, "Parser: " + std::string(error));
        }

        /* Token consuming */

        bool begin() {
            this->m_originalPosition = this->m_curr;
            this->m_matchedOptionals.clear();

            return true;
        }

        bool sequence() {
            return true;
        }

        bool sequence(Token::Type type, auto value, auto ... args) {
            if (!matches(type, value)) {
                this->m_curr = this->m_originalPosition;
                return false;
            }

            this->m_curr++;

            if (!sequence(args...)) {
                this->m_curr = this->m_originalPosition;
                return false;
            }

            return true;
        }

        bool variant(Token::Type type1, auto value1, Token::Type type2, auto value2) {
            if (!matches(type1, value1)) {
                if (!matches(type2, value2)) {
                    this->m_curr = this->m_originalPosition;
                    return false;
                }
            }

            this->m_curr++;

            return true;
        }

        bool optional(Token::Type type, auto value) {
            if (matches(type, value)) {
                this->m_matchedOptionals.push_back(this->m_curr);
                this->m_curr++;
            }

            return true;
        }

        bool matches(Token::Type type, auto value, s32 index = 0) {
            return this->m_curr[index].type == type && this->m_curr[index] == value;
        }

        bool matchesOptional(Token::Type type, auto value, u32 index = 0) {
            if (index >= this->m_matchedOptionals.size())
                return false;
            return matches(type, value, std::distance(this->m_curr, this->m_matchedOptionals[index]));
        }

    };

}