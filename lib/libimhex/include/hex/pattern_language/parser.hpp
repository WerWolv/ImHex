#pragma once

#include <hex.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/intrinsics.hpp>

#include <hex/pattern_language/error.hpp>
#include <hex/pattern_language/token.hpp>

#include <hex/pattern_language/ast/ast_node.hpp>
#include <hex/pattern_language/ast/ast_node_rvalue.hpp>
#include <hex/pattern_language/ast/ast_node_attribute.hpp>
#include <hex/pattern_language/ast/ast_node_type_decl.hpp>

#include <unordered_map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace hex::pl {

    class Parser {
    public:
        using TokenIter = std::vector<Token>::const_iterator;

        Parser()  = default;
        ~Parser() = default;

        std::optional<std::vector<std::shared_ptr<ASTNode>>> parse(const std::vector<Token> &tokens);
        const std::optional<PatternLanguageError> &getError() { return this->m_error; }

    private:
        std::optional<PatternLanguageError> m_error;
        TokenIter m_curr;
        TokenIter m_originalPosition, m_partOriginalPosition;

        std::unordered_map<std::string, std::shared_ptr<ASTNode>> m_types;
        std::vector<TokenIter> m_matchedOptionals;
        std::vector<std::vector<std::string>> m_currNamespace;

        u32 getLineNumber(i32 index) const {
            return this->m_curr[index].lineNumber;
        }

        template<typename T>
        std::unique_ptr<T> create(T *node) {
            node->setLineNumber(this->getLineNumber(-1));
            return std::unique_ptr<T>(node);
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

        std::unique_ptr<ASTNode> parseFunctionCall();
        std::unique_ptr<ASTNode> parseStringLiteral();
        std::string parseNamespaceResolution();
        std::unique_ptr<ASTNode> parseScopeResolution();
        std::unique_ptr<ASTNode> parseRValue();
        std::unique_ptr<ASTNode> parseRValue(ASTNodeRValue::Path &path);
        std::unique_ptr<ASTNode> parseFactor();
        std::unique_ptr<ASTNode> parseCastExpression();
        std::unique_ptr<ASTNode> parseUnaryExpression();
        std::unique_ptr<ASTNode> parseMultiplicativeExpression();
        std::unique_ptr<ASTNode> parseAdditiveExpression();
        std::unique_ptr<ASTNode> parseShiftExpression();
        std::unique_ptr<ASTNode> parseBinaryAndExpression();
        std::unique_ptr<ASTNode> parseBinaryXorExpression();
        std::unique_ptr<ASTNode> parseBinaryOrExpression();
        std::unique_ptr<ASTNode> parseBooleanAnd();
        std::unique_ptr<ASTNode> parseBooleanXor();
        std::unique_ptr<ASTNode> parseBooleanOr();
        std::unique_ptr<ASTNode> parseRelationExpression();
        std::unique_ptr<ASTNode> parseEqualityExpression();
        std::unique_ptr<ASTNode> parseTernaryConditional();
        std::unique_ptr<ASTNode> parseMathematicalExpression();

        std::unique_ptr<ASTNode> parseFunctionDefinition();
        std::unique_ptr<ASTNode> parseFunctionVariableDecl();
        std::unique_ptr<ASTNode> parseFunctionStatement();
        std::unique_ptr<ASTNode> parseFunctionVariableAssignment(const std::string &lvalue);
        std::unique_ptr<ASTNode> parseFunctionVariableCompoundAssignment(const std::string &lvalue);
        std::unique_ptr<ASTNode> parseFunctionControlFlowStatement();
        std::vector<std::unique_ptr<ASTNode>> parseStatementBody();
        std::unique_ptr<ASTNode> parseFunctionConditional();
        std::unique_ptr<ASTNode> parseFunctionWhileLoop();
        std::unique_ptr<ASTNode> parseFunctionForLoop();

        void parseAttribute(Attributable *currNode);
        std::unique_ptr<ASTNode> parseConditional();
        std::unique_ptr<ASTNode> parseWhileStatement();
        std::unique_ptr<ASTNodeTypeDecl> parseType(bool allowFunctionTypes = false);
        std::shared_ptr<ASTNodeTypeDecl> parseUsingDeclaration();
        std::unique_ptr<ASTNode> parsePadding();
        std::unique_ptr<ASTNode> parseMemberVariable(const std::shared_ptr<ASTNodeTypeDecl> &type);
        std::unique_ptr<ASTNode> parseMemberArrayVariable(const std::shared_ptr<ASTNodeTypeDecl> &type);
        std::unique_ptr<ASTNode> parseMemberPointerVariable(const std::shared_ptr<ASTNodeTypeDecl> &type);
        std::unique_ptr<ASTNode> parseMember();
        std::shared_ptr<ASTNodeTypeDecl> parseStruct();
        std::shared_ptr<ASTNodeTypeDecl> parseUnion();
        std::shared_ptr<ASTNodeTypeDecl> parseEnum();
        std::shared_ptr<ASTNodeTypeDecl> parseBitfield();
        std::unique_ptr<ASTNode> parseVariablePlacement(const std::shared_ptr<ASTNodeTypeDecl> &type);
        std::unique_ptr<ASTNode> parseArrayVariablePlacement(const std::shared_ptr<ASTNodeTypeDecl> &type);
        std::unique_ptr<ASTNode> parsePointerVariablePlacement(const std::shared_ptr<ASTNodeTypeDecl> &type);
        std::unique_ptr<ASTNode> parsePlacement();
        std::vector<std::shared_ptr<ASTNode>> parseNamespace();
        std::vector<std::shared_ptr<ASTNode>> parseStatements();

        std::shared_ptr<ASTNodeTypeDecl> addType(const std::string &name, std::unique_ptr<ASTNode> &&node, std::optional<std::endian> endian = std::nullopt);

        std::vector<std::shared_ptr<ASTNode>> parseTillToken(Token::Type endTokenType, const auto value) {
            std::vector<std::shared_ptr<ASTNode>> program;

            while (this->m_curr->type != endTokenType || (*this->m_curr) != value) {
                for (auto &statement : parseStatements())
                    program.push_back(std::move(statement));
            }

            this->m_curr++;

            return program;
        }

        [[noreturn]] void throwParserError(const std::string &error, i32 token = -1) const {
            throw PatternLanguageError(this->m_curr[token].lineNumber, "Parser: " + error);
        }

        /* Token consuming */

        enum class Setting
        {
        };
        constexpr static auto Normal = static_cast<Setting>(0);
        constexpr static auto Not    = static_cast<Setting>(1);

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
                hex::unreachable();
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
                hex::unreachable();
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
                hex::unreachable();
        }

        template<Setting S = Normal>
        bool oneOfImpl(Token::Type type, auto value, auto... args) {
            if constexpr (S == Normal)
                return sequenceImpl<Normal>(type, value) || oneOfImpl(args...);
            else if constexpr (S == Not)
                return sequenceImpl<Not>(type, value) && oneOfImpl(args...);
            else
                hex::unreachable();
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