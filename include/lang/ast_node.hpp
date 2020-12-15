#pragma once

#include "token.hpp"

#include <bit>
#include <optional>
#include <map>
#include <vector>

namespace hex::lang {

    class ASTNode {
    public:
        constexpr ASTNode() = default;
        constexpr virtual ~ASTNode() = default;
        constexpr ASTNode(const ASTNode &) = default;

        [[nodiscard]] constexpr u32 getLineNumber() const { return this->m_lineNumber; }
        constexpr void setLineNumber(u32 lineNumber) { this->m_lineNumber = lineNumber; }

        virtual ASTNode* clone() = 0;

    private:
        u32 m_lineNumber = 0;
    };

    class ASTNodeIntegerLiteral : public ASTNode {
    public:
        ASTNodeIntegerLiteral(std::variant<u128, s128> value, Token::ValueType type)
            : ASTNode(), m_value(value), m_type(type) {

        }
        ASTNodeIntegerLiteral(const ASTNodeIntegerLiteral&) = default;

        ASTNode* clone() override {
            return new ASTNodeIntegerLiteral(*this);
        }

        [[nodiscard]] const auto& getValue() const {
            return this->m_value;
        }

        [[nodiscard]] Token::ValueType getType() const {
            return this->m_type;
        }

    private:
        std::variant<u128, s128> m_value;
        Token::ValueType m_type;
    };

    class ASTNodeNumericExpression : public ASTNode {
    public:
        ASTNodeNumericExpression(ASTNode *left, ASTNode *right, Token::Operator op)
            : ASTNode(), m_left(left), m_right(right), m_operator(op) { }

        ~ASTNodeNumericExpression() override {
            delete this->m_left;
            delete this->m_right;
        }

        ASTNodeNumericExpression(const ASTNodeNumericExpression &other) {
            this->m_operator = other.m_operator;
            this->m_left = other.m_left->clone();
            this->m_right = other.m_right->clone();
        }

        ASTNode* clone() override {
            return new ASTNodeNumericExpression(*this);
        }

        ASTNode *getLeftOperand() { return this->m_left; }
        ASTNode *getRightOperand() { return this->m_right; }
        Token::Operator getOperator() { return this->m_operator; }

        [[nodiscard]] ASTNodeIntegerLiteral* evaluate() {
            ASTNodeIntegerLiteral *leftInteger, *rightInteger;

            if (auto leftExprLiteral = dynamic_cast<ASTNodeIntegerLiteral*>(this->m_left); leftExprLiteral != nullptr)
                leftInteger = leftExprLiteral;
            else if (auto leftExprExpression = dynamic_cast<ASTNodeNumericExpression*>(this->m_left); leftExprExpression != nullptr)
                leftInteger = leftExprExpression->evaluate();
            else
                return nullptr;

            if (auto rightExprLiteral = dynamic_cast<ASTNodeIntegerLiteral*>(this->m_right); rightExprLiteral != nullptr)
                rightInteger = rightExprLiteral;
            else if (auto rightExprExpression = dynamic_cast<ASTNodeNumericExpression*>(this->m_right); rightExprExpression != nullptr)
                rightInteger = rightExprExpression->evaluate();
            else
                return nullptr;

            return this->evaluateOperator(leftInteger, rightInteger);
        }

    private:
        ASTNode *m_left, *m_right;
        Token::Operator m_operator;

        ASTNodeIntegerLiteral* evaluateOperator(ASTNodeIntegerLiteral *left, ASTNodeIntegerLiteral *right) {
            return std::visit([&](auto &&leftValue, auto &&rightValue) -> ASTNodeIntegerLiteral* {

                auto newType = [&] {
                    #define CHECK_TYPE(type) if (left->getType() == (type) || right->getType() == (type)) return (type)
                    #define DEFAULT_TYPE(type) return (type)

                    CHECK_TYPE(Token::ValueType::Double);
                    CHECK_TYPE(Token::ValueType::Float);
                    CHECK_TYPE(Token::ValueType::Unsigned128Bit);
                    CHECK_TYPE(Token::ValueType::Signed128Bit);
                    CHECK_TYPE(Token::ValueType::Unsigned64Bit);
                    CHECK_TYPE(Token::ValueType::Signed64Bit);
                    CHECK_TYPE(Token::ValueType::Unsigned32Bit);
                    CHECK_TYPE(Token::ValueType::Signed32Bit);
                    CHECK_TYPE(Token::ValueType::Unsigned16Bit);
                    CHECK_TYPE(Token::ValueType::Signed16Bit);
                    CHECK_TYPE(Token::ValueType::Unsigned8Bit);
                    CHECK_TYPE(Token::ValueType::Signed8Bit);
                    DEFAULT_TYPE(Token::ValueType::Signed32Bit);

                    #undef CHECK_TYPE
                    #undef DEFAULT_TYPE
                }();

                switch (this->m_operator) {
                    case Token::Operator::Plus:
                        return new ASTNodeIntegerLiteral(leftValue + rightValue, newType);
                    case Token::Operator::Minus:
                        return new ASTNodeIntegerLiteral(leftValue - rightValue, newType);
                    case Token::Operator::Star:
                        return new ASTNodeIntegerLiteral(leftValue * rightValue, newType);
                    case Token::Operator::Slash:
                        return new ASTNodeIntegerLiteral(leftValue / rightValue, newType);
                    case Token::Operator::ShiftLeft:
                        return new ASTNodeIntegerLiteral(leftValue << rightValue, newType);
                    case Token::Operator::ShiftRight:
                        return new ASTNodeIntegerLiteral(leftValue >> rightValue, newType);
                    case Token::Operator::BitAnd:
                        return new ASTNodeIntegerLiteral(leftValue & rightValue, newType);
                    case Token::Operator::BitXor:
                        return new ASTNodeIntegerLiteral(leftValue ^ rightValue, newType);
                    case Token::Operator::BitOr:
                        return new ASTNodeIntegerLiteral(leftValue | rightValue, newType);
                    default: return nullptr;

                }

            }, left->getValue(), right->getValue());
        }
    };

    class ASTNodeBuiltinType : public ASTNode {
    public:
        constexpr explicit ASTNodeBuiltinType(Token::ValueType type)
            : ASTNode(), m_type(type) { }

        [[nodiscard]] constexpr const auto& getType() const { return this->m_type; }

        ASTNode* clone() override {
            return new ASTNodeBuiltinType(*this);
        }

    private:
        const Token::ValueType m_type;
    };

    class ASTNodeTypeDecl : public ASTNode {
    public:
        ASTNodeTypeDecl(std::string_view name, ASTNode *type, std::optional<std::endian> endian = { })
            : ASTNode(), m_name(name), m_type(type), m_endian(endian) { }

        ASTNodeTypeDecl(const ASTNodeTypeDecl& other) {
            this->m_name = other.m_name;
            this->m_type = other.m_type->clone();
            this->m_endian = other.m_endian;
        }

        ~ASTNodeTypeDecl() override {
            delete this->m_type;
        }

        ASTNode* clone() override {
            return new ASTNodeTypeDecl(*this);
        }

        [[nodiscard]] std::string_view getName() const { return this->m_name; }
        [[nodiscard]] ASTNode* getType() { return this->m_type; }
        [[nodiscard]] std::optional<std::endian> getEndian() const { return this->m_endian; }

    private:
        std::string m_name;
        ASTNode *m_type;
        std::optional<std::endian> m_endian;
    };

    class ASTNodeVariableDecl : public ASTNode {
    public:
        ASTNodeVariableDecl(std::string_view name, ASTNode *type, std::optional<u64> placementOffset = { })
            : ASTNode(), m_name(name), m_type(type), m_placementOffset(placementOffset) { }

        ASTNodeVariableDecl(const ASTNodeVariableDecl &other) {
            this->m_name = other.m_name;
            this->m_type = other.m_type->clone();
            this->m_placementOffset = other.m_placementOffset;
        }

        ~ASTNodeVariableDecl() override {
            delete this->m_type;
        }

        ASTNode* clone() override {
            return new ASTNodeVariableDecl(*this);
        }

        [[nodiscard]] std::string_view getName() const { return this->m_name; }
        [[nodiscard]] constexpr ASTNode* getType() const { return this->m_type; }
        [[nodiscard]] constexpr auto getPlacementOffset() const { return this->m_placementOffset; }

    private:
        std::string m_name;
        ASTNode *m_type;
        std::optional<u64> m_placementOffset;
    };

    class ASTNodeArrayVariableDecl : public ASTNode {
    public:
        ASTNodeArrayVariableDecl(std::string_view name, ASTNode *type, ASTNode *size, std::optional<u64> placementOffset = { })
                : ASTNode(), m_name(name), m_type(type), m_size(size) { }

        ASTNodeArrayVariableDecl(const ASTNodeArrayVariableDecl &other) {
            this->m_name = other.m_name;
            this->m_type = other.m_type->clone();
            this->m_size = other.m_size->clone();
            this->m_placementOffset = other.m_placementOffset;
        }

        ~ASTNodeArrayVariableDecl() override {
            delete this->m_type;
            delete this->m_size;
        }

        ASTNode* clone() override {
            return new ASTNodeArrayVariableDecl(*this);
        }

        [[nodiscard]] std::string_view getName() const { return this->m_name; }
        [[nodiscard]] constexpr ASTNode* getType() const { return this->m_type; }
        [[nodiscard]] constexpr ASTNode* getSize() const { return this->m_size; }
        [[nodiscard]] constexpr auto getPlacementOffset() const { return this->m_placementOffset; }

    private:
        std::string m_name;
        ASTNode *m_type;
        ASTNode *m_size;
        std::optional<u64> m_placementOffset;
    };

    class ASTNodeStruct : public ASTNode {
    public:
        ASTNodeStruct() : ASTNode() { }

        ASTNodeStruct(const ASTNodeStruct &other) {
            for (const auto &otherMember : other.getMembers())
                this->m_members.push_back(otherMember->clone());
        }

        ~ASTNodeStruct() override {
            for (auto &member : this->m_members)
                delete member;
        }

        ASTNode* clone() override {
            return new ASTNodeStruct(*this);
        }

        [[nodiscard]] const std::vector<ASTNode*>& getMembers() const { return this->m_members; }
        void addMember(ASTNode *node) { this->m_members.push_back(node); }

    private:
        std::vector<ASTNode*> m_members;
    };

    class ASTNodeUnion : public ASTNode {
    public:
        ASTNodeUnion() : ASTNode() { }

        ASTNodeUnion(const ASTNodeUnion &other) {
            for (const auto &otherMember : other.getMembers())
                this->m_members.push_back(otherMember->clone());
        }

        ~ASTNodeUnion() override {
            for (auto &member : this->m_members)
                delete member;
        }

        ASTNode* clone() override {
            return new ASTNodeUnion(*this);
        }

        [[nodiscard]] const  std::vector<ASTNode*>& getMembers() const { return this->m_members; }
        void addMember(ASTNode *node) { this->m_members.push_back(node); }

    private:
        std::vector<ASTNode*> m_members;
    };

    class ASTNodeEnum : public ASTNode {
    public:
        ASTNodeEnum(ASTNode *underlyingType) : ASTNode(), m_underlyingType(underlyingType) { }

        ASTNodeEnum(const ASTNodeEnum &other) {
            for (const auto &[name, entry] : other.getEntries())
                this->m_entries.emplace_back(name, entry->clone());
            this->m_underlyingType = other.m_underlyingType->clone();
        }

        ~ASTNodeEnum() override {
            for (auto &[name, expr] : this->m_entries)
                delete expr;
            delete this->m_underlyingType;
        }

        ASTNode* clone() override {
            return new ASTNodeEnum(*this);
        }

        [[nodiscard]] const std::vector<std::pair<std::string, ASTNode*>>& getEntries() const { return this->m_entries; }
        void addEntry(std::string name, ASTNode* expression) { this->m_entries.emplace_back(name, expression); }

        [[nodiscard]] const ASTNode *getUnderlyingType() const { return this->m_underlyingType; }

    private:
        std::vector<std::pair<std::string, ASTNode*>> m_entries;
        ASTNode *m_underlyingType;
    };

}