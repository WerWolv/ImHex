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

        void throwParseError(std::string_view error) {
            throw ParseError(this->m_curr[-1].lineNumber, error);
        }

        std::vector<ASTNode*> parseStatement();

        std::vector<ASTNode*> parseTillToken(Token::Type endTokenType, const auto value) {
            std::vector<ASTNode*> program;
            SCOPE_EXIT( for (auto &node : program) delete node; );

            while (this->m_curr->type != endTokenType || (*this->m_curr) != value) {
                auto newTokens = parseStatement();

                program.insert(program.end(), newTokens.begin(), newTokens.end());
            }

            this->m_curr++;

            return program;
        }

        bool tryConsume() {
            return true;
        }

        bool tryConsume(Token::Type type, auto value, auto ... args) {
            auto originalPosition = this->m_curr;

            if (this->m_curr->type != type || (*this->m_curr) != value) {
                this->m_curr = originalPosition;
                return false;
            }

            this->m_curr++;

            if (!tryConsume(args...)) {
                this->m_curr = originalPosition;
                return false;
            }

            return true;
        }
    };

}