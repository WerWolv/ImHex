#pragma once

#include <hex.hpp>

#include "token.hpp"
#include "ast_node.hpp"

#include "helpers/utils.hpp"

#include <stdexcept>
#include <utility>
#include <vector>

namespace hex::lang {

    class Parser {
    public:
        Parser();

        using TokenIter = std::vector<Token>::const_iterator;

        std::optional<std::vector<ASTNode*>> parse(const std::vector<Token> &tokens);

        const std::pair<u32, std::string>& getError() { return this->m_error; }

    private:
        using ParseError = std::pair<u32, std::string>;

        ParseError m_error;
        TokenIter m_curr;
        TokenIter m_originalPosition;

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



        ASTNode* parseUsingDeclaration();
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

        void throwParseError(std::string_view error, u32 token = 1) const {
            throw ParseError(this->m_curr[-token].lineNumber, error);
        }

        /* Token consuming */

        bool begin() {
            this->m_originalPosition = this->m_curr;
            return true;
        }

        bool sequence() {
            return true;
        }

        bool sequence(Token::Type type, auto value, auto ... args) {
            if (this->m_curr->type != type || (*this->m_curr) != value) {
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
            if (this->m_curr->type != type1 || (*this->m_curr) != value1) {
                if (this->m_curr->type != type2 || (*this->m_curr) != value2) {
                    this->m_curr = this->m_originalPosition;
                    return false;
                }
            }

            this->m_curr++;

            return true;
        }

        bool optional(Token::Type type, auto value, Token::Type nextType, auto nextValue) {
            if (this->m_curr->type != type || (*this->m_curr) != value)
                this->m_curr++;

            return sequence(nextType, nextValue);
        }
    };

}