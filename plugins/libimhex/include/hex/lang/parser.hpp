#pragma once

#include <hex.hpp>

#include "token.hpp"
#include "ast_node.hpp"

#include <hex/helpers/utils.hpp>

#include <unordered_map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace hex::lang {

    class Parser {
    public:
        using TokenIter = std::vector<Token>::const_iterator;
        using ParseError = std::pair<u32, std::string>;

        Parser() = default;
        ~Parser() = default;

        std::optional<std::vector<ASTNode*>> parse(const std::vector<Token> &tokens);
        const ParseError& getError() { return this->m_error; }

    private:
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
                throwParseError("failed to decode token. Invalid type.", getLineNumber(index));

            return *value;
        }

        Token::Type getType(s32 index) const {
            return this->m_curr[index].type;
        }

        ASTNode* parseFunctionCall();
        ASTNode* parseStringLiteral();
        ASTNode* parseScopeResolution(std::vector<std::string> &path);
        ASTNode* parseRValue(ASTNodeRValue::Path &path);
        ASTNode* parseFactor();
        ASTNode* parseUnaryExpression();
        ASTNode* parseMultiplicativeExpression();
        ASTNode* parseAdditiveExpression();
        ASTNode* parseShiftExpression();
        ASTNode* parseRelationExpression();
        ASTNode* parseEqualityExpression();
        ASTNode* parseBinaryAndExpression();
        ASTNode* parseBinaryXorExpression();
        ASTNode* parseBinaryOrExpression();
        ASTNode* parseBooleanAnd();
        ASTNode* parseBooleanXor();
        ASTNode* parseBooleanOr();
        ASTNode* parseTernaryConditional();
        ASTNode* parseMathematicalExpression();

        ASTNode* parseFunctionDefintion();
        ASTNode* parseFunctionStatement();
        ASTNode* parseFunctionVariableAssignment();
        ASTNode* parseFunctionReturnStatement();
        ASTNode* parseFunctionConditional();
        ASTNode* parseFunctionWhileLoop();

        void parseAttribute(Attributable *currNode);
        ASTNode* parseConditional();
        ASTNode* parseWhileStatement();
        ASTNode* parseType(s32 startIndex);
        ASTNode* parseUsingDeclaration();
        ASTNode* parsePadding();
        ASTNode* parseMemberVariable();
        ASTNode* parseMemberArrayVariable();
        ASTNode* parseMemberPointerVariable();
        ASTNode* parseMember();
        ASTNode* parseStruct();
        ASTNode* parseUnion();
        ASTNode* parseEnum();
        ASTNode* parseBitfield();
        ASTNode* parseVariablePlacement();
        ASTNode* parseArrayVariablePlacement();
        ASTNode* parsePointerVariablePlacement();
        ASTNode* parseStatement();

        std::vector<ASTNode*> parseTillToken(Token::Type endTokenType, const auto value) {
            std::vector<ASTNode*> program;
            auto guard = SCOPE_GUARD {
                for (auto &node : program)
                    delete node;
            };

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

        enum class Setting{ };
        constexpr static auto Normal = static_cast<Setting>(0);
        constexpr static auto Not = static_cast<Setting>(1);

        bool begin() {
            this->m_originalPosition = this->m_curr;
            this->m_matchedOptionals.clear();

            return true;
        }

        template<Setting S = Normal>
        bool sequence() {
            if constexpr (S == Normal)
                return true;
            else if constexpr (S == Not)
                return false;
            else
                __builtin_unreachable();
        }

        template<Setting S = Normal>
        bool sequence(Token::Type type, auto value, auto ... args) {
            if constexpr (S == Normal) {
                if (!peek(type, value)) {
                    this->m_curr = this->m_originalPosition;
                    return false;
                }

                this->m_curr++;

                if (!sequence<Normal>(args...)) {
                    this->m_curr = this->m_originalPosition;
                    return false;
                }

                return true;
            } else if constexpr (S == Not) {
                if (!peek(type, value))
                    return true;

                this->m_curr++;

                if (!sequence<Normal>(args...))
                    return true;

                this->m_curr = this->m_originalPosition;
                return false;
            } else
                __builtin_unreachable();
        }

        template<Setting S = Normal>
        bool oneOf() {
            if constexpr (S == Normal)
                return false;
            else if constexpr (S == Not)
                return true;
            else
                __builtin_unreachable();
        }

        template<Setting S = Normal>
        bool oneOf(Token::Type type, auto value, auto ... args) {
            if constexpr (S == Normal)
                return sequence<Normal>(type, value) || oneOf(args...);
            else if constexpr (S == Not)
                return sequence<Not>(type, value) && oneOf(args...);
            else
                __builtin_unreachable();
        }

        bool variant(Token::Type type1, auto value1, Token::Type type2, auto value2) {
            if (!peek(type1, value1)) {
                if (!peek(type2, value2)) {
                    this->m_curr = this->m_originalPosition;
                    return false;
                }
            }

            this->m_curr++;

            return true;
        }

        bool optional(Token::Type type, auto value) {
            if (peek(type, value)) {
                this->m_matchedOptionals.push_back(this->m_curr);
                this->m_curr++;
            }

            return true;
        }

        bool peek(Token::Type type, auto value, s32 index = 0) {
            return this->m_curr[index].type == type && this->m_curr[index] == value;
        }

        bool peekOptional(Token::Type type, auto value, u32 index = 0) {
            if (index >= this->m_matchedOptionals.size())
                return false;
            return peek(type, value, std::distance(this->m_curr, this->m_matchedOptionals[index]));
        }

    };

}