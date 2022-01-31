#pragma once

#include <hex.hpp>
#include <hex/helpers/utils.hpp>

#include <hex/pattern_language/error.hpp>
#include <hex/pattern_language/token.hpp>
#include <hex/pattern_language/ast_node.hpp>

#include <unordered_map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace hex::pl {

    class Parser {
    public:
        using TokenIter = std::vector<Token>::const_iterator;

        Parser() = default;
        ~Parser() = default;

        std::optional<std::vector<ASTNode *>> parse(const std::vector<Token> &tokens);
        const std::optional<PatternLanguageError> &getError() { return this->m_error; }

    private:
        std::optional<PatternLanguageError> m_error;
        TokenIter m_curr;
        TokenIter m_originalPosition, m_partOriginalPosition;

        std::unordered_map<std::string, ASTNode *> m_types;
        std::vector<TokenIter> m_matchedOptionals;
        std::vector<std::vector<std::string>> m_currNamespace;

        u32 getLineNumber(i32 index) const {
            return this->m_curr[index].lineNumber;
        }

        auto *create(auto *node) {
            node->setLineNumber(this->getLineNumber(-1));
            return node;
        }

        template<typename T>
        const T &getValue(i32 index) const {
            auto value = std::get_if<T>(&this->m_curr[index].value);

            if (value == nullptr)
                throwParserError("failed to decode token. Invalid type.", getLineNumber(index));

            return *value;
        }

        Token::Type getType(i32 index) const {
            return this->m_curr[index].type;
        }

        std::string getNamespacePrefixedName(const std::string &name) {
            std::string result;
            for (const auto &part : this->m_currNamespace.back()) {
                result += part + "::";
            }

            result += name;

            return result;
        }

        ASTNode *parseFunctionCall();
        ASTNode *parseStringLiteral();
        std::string parseNamespaceResolution();
        ASTNode *parseScopeResolution();
        ASTNode *parseRValue();
        ASTNode *parseRValue(ASTNodeRValue::Path &path);
        ASTNode *parseFactor();
        ASTNode *parseCastExpression();
        ASTNode *parseUnaryExpression();
        ASTNode *parseMultiplicativeExpression();
        ASTNode *parseAdditiveExpression();
        ASTNode *parseShiftExpression();
        ASTNode *parseRelationExpression();
        ASTNode *parseEqualityExpression();
        ASTNode *parseBinaryAndExpression();
        ASTNode *parseBinaryXorExpression();
        ASTNode *parseBinaryOrExpression();
        ASTNode *parseBooleanAnd();
        ASTNode *parseBooleanXor();
        ASTNode *parseBooleanOr();
        ASTNode *parseTernaryConditional();
        ASTNode *parseMathematicalExpression();

        ASTNode *parseFunctionDefinition();
        ASTNode *parseFunctionVariableDecl();
        ASTNode *parseFunctionStatement();
        ASTNode *parseFunctionVariableAssignment(const std::string &lvalue);
        ASTNode *parseFunctionVariableCompoundAssignment(const std::string &lvalue);
        ASTNode *parseFunctionControlFlowStatement();
        std::vector<ASTNode *> parseStatementBody();
        ASTNode *parseFunctionConditional();
        ASTNode *parseFunctionWhileLoop();
        ASTNode *parseFunctionForLoop();

        void parseAttribute(Attributable *currNode);
        ASTNode *parseConditional();
        ASTNode *parseWhileStatement();
        ASTNodeTypeDecl *parseType(bool allowFunctionTypes = false);
        ASTNode *parseUsingDeclaration();
        ASTNode *parsePadding();
        ASTNode *parseMemberVariable(ASTNodeTypeDecl *type);
        ASTNode *parseMemberArrayVariable(ASTNodeTypeDecl *type);
        ASTNode *parseMemberPointerVariable(ASTNodeTypeDecl *type);
        ASTNode *parseMember();
        ASTNode *parseStruct();
        ASTNode *parseUnion();
        ASTNode *parseEnum();
        ASTNode *parseBitfield();
        ASTNode *parseVariablePlacement(ASTNodeTypeDecl *type);
        ASTNode *parseArrayVariablePlacement(ASTNodeTypeDecl *type);
        ASTNode *parsePointerVariablePlacement(ASTNodeTypeDecl *type);
        ASTNode *parsePlacement();
        std::vector<ASTNode *> parseNamespace();
        std::vector<ASTNode *> parseStatements();

        ASTNodeTypeDecl *addType(const std::string &name, ASTNode *node, std::optional<std::endian> endian = std::nullopt);

        std::vector<ASTNode *> parseTillToken(Token::Type endTokenType, const auto value) {
            std::vector<ASTNode *> program;
            auto guard = SCOPE_GUARD {
                for (auto &node : program)
                    delete node;
            };

            while (this->m_curr->type != endTokenType || (*this->m_curr) != value) {
                for (auto statement : parseStatements())
                    program.push_back(statement);
            }

            this->m_curr++;

            guard.release();

            return program;
        }

        [[noreturn]] void throwParserError(const std::string &error, i32 token = -1) const {
            throw PatternLanguageError(this->m_curr[token].lineNumber, "Parser: " + error);
        }

        /* Token consuming */

        enum class Setting { };
        constexpr static auto Normal = static_cast<Setting>(0);
        constexpr static auto Not = static_cast<Setting>(1);

        bool begin() {
            this->m_originalPosition = this->m_curr;
            this->m_matchedOptionals.clear();

            return true;
        }

        bool partBegin() {
            this->m_partOriginalPosition = this->m_curr;
            this->m_matchedOptionals.clear();

            return true;
        }

        void reset() {
            this->m_curr = this->m_originalPosition;
        }

        void partReset() {
            this->m_curr = this->m_partOriginalPosition;
        }

        bool resetIfFailed(bool value) {
            if (!value) reset();

            return value;
        }

        template<Setting S = Normal>
        bool sequenceImpl() {
            if constexpr (S == Normal)
                return true;
            else if constexpr (S == Not)
                return false;
            else
                __builtin_unreachable();
        }

        template<Setting S = Normal>
        bool sequenceImpl(Token::Type type, auto value, auto... args) {
            if constexpr (S == Normal) {
                if (!peek(type, value)) {
                    partReset();
                    return false;
                }

                this->m_curr++;

                if (!sequenceImpl<Normal>(args...)) {
                    partReset();
                    return false;
                }

                return true;
            } else if constexpr (S == Not) {
                if (!peek(type, value))
                    return true;

                this->m_curr++;

                if (!sequenceImpl<Normal>(args...))
                    return true;

                partReset();
                return false;
            } else
                __builtin_unreachable();
        }

        template<Setting S = Normal>
        bool sequence(Token::Type type, auto value, auto... args) {
            return partBegin() && sequenceImpl<S>(type, value, args...);
        }

        template<Setting S = Normal>
        bool oneOfImpl() {
            if constexpr (S == Normal)
                return false;
            else if constexpr (S == Not)
                return true;
            else
                __builtin_unreachable();
        }

        template<Setting S = Normal>
        bool oneOfImpl(Token::Type type, auto value, auto... args) {
            if constexpr (S == Normal)
                return sequenceImpl<Normal>(type, value) || oneOfImpl(args...);
            else if constexpr (S == Not)
                return sequenceImpl<Not>(type, value) && oneOfImpl(args...);
            else
                __builtin_unreachable();
        }

        template<Setting S = Normal>
        bool oneOf(Token::Type type, auto value, auto... args) {
            return partBegin() && oneOfImpl<S>(type, value, args...);
        }

        bool variantImpl(Token::Type type1, auto value1, Token::Type type2, auto value2) {
            if (!peek(type1, value1)) {
                if (!peek(type2, value2)) {
                    partReset();
                    return false;
                }
            }

            this->m_curr++;

            return true;
        }

        bool variant(Token::Type type1, auto value1, Token::Type type2, auto value2) {
            return partBegin() && variantImpl(type1, value1, type2, value2);
        }

        bool optionalImpl(Token::Type type, auto value) {
            if (peek(type, value)) {
                this->m_matchedOptionals.push_back(this->m_curr);
                this->m_curr++;
            }

            return true;
        }

        bool optional(Token::Type type, auto value) {
            return partBegin() && optionalImpl(type, value);
        }

        bool peek(Token::Type type, auto value, i32 index = 0) {
            return this->m_curr[index].type == type && this->m_curr[index] == value;
        }
    };

}