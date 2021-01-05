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
        u32 m_lineNumber = 1;
    };

    class ASTNodeIntegerLiteral : public ASTNode {
    public:
        ASTNodeIntegerLiteral(Token::IntegerLiteral literal) : ASTNode(), m_literal(literal) { }

        ASTNodeIntegerLiteral(const ASTNodeIntegerLiteral&) = default;

        ASTNode* clone() override {
            return new ASTNodeIntegerLiteral(*this);
        }

        [[nodiscard]] const auto& getValue() const {
            return this->m_literal.second;
        }

        [[nodiscard]] Token::ValueType getType() const {
            return this->m_literal.first;
        }

    private:
        Token::IntegerLiteral m_literal;
    };

    class ASTNodeNumericExpression : public ASTNode {
    public:
        ASTNodeNumericExpression(ASTNode *left, ASTNode *right, Token::Operator op)
                : ASTNode(), m_left(left), m_right(right), m_operator(op) { }

        ~ASTNodeNumericExpression() override {
            delete this->m_left;
            delete this->m_right;
        }

        ASTNodeNumericExpression(const ASTNodeNumericExpression &other) : ASTNode(other) {
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

    private:
        ASTNode *m_left, *m_right;
        Token::Operator m_operator;
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

        ASTNodeTypeDecl(const ASTNodeTypeDecl& other) : ASTNode(other) {
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
        ASTNodeVariableDecl(std::string_view name, ASTNode *type, ASTNode *placementOffset = nullptr)
                : ASTNode(), m_name(name), m_type(type), m_placementOffset(placementOffset) { }

        ASTNodeVariableDecl(const ASTNodeVariableDecl &other) : ASTNode(other) {
            this->m_name = other.m_name;
            this->m_type = other.m_type->clone();

            if (other.m_placementOffset != nullptr)
                this->m_placementOffset = other.m_placementOffset->clone();
            else
                this->m_placementOffset = nullptr;
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
        ASTNode *m_placementOffset;
    };

    class ASTNodeArrayVariableDecl : public ASTNode {
    public:
        ASTNodeArrayVariableDecl(std::string_view name, ASTNode *type, ASTNode *size, ASTNode *placementOffset = nullptr)
                : ASTNode(), m_name(name), m_type(type), m_size(size), m_placementOffset(placementOffset) { }

        ASTNodeArrayVariableDecl(const ASTNodeArrayVariableDecl &other) : ASTNode(other) {
            this->m_name = other.m_name;
            this->m_type = other.m_type->clone();
            this->m_size = other.m_size->clone();

            if (other.m_placementOffset != nullptr)
                this->m_placementOffset = other.m_placementOffset->clone();
            else
                this->m_placementOffset = nullptr;
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
        ASTNode *m_placementOffset;
    };

    class ASTNodePointerVariableDecl : public ASTNode {
    public:
        ASTNodePointerVariableDecl(std::string_view name, ASTNode *type, ASTNode *sizeType, ASTNode *placementOffset = nullptr)
                : ASTNode(), m_name(name), m_type(type), m_sizeType(sizeType), m_placementOffset(placementOffset) { }

        ASTNodePointerVariableDecl(const ASTNodePointerVariableDecl &other) : ASTNode(other) {
            this->m_name = other.m_name;
            this->m_type = other.m_type->clone();
            this->m_sizeType = other.m_sizeType->clone();

            if (other.m_placementOffset != nullptr)
                this->m_placementOffset = other.m_placementOffset->clone();
            else
                this->m_placementOffset = nullptr;
        }

        ~ASTNodePointerVariableDecl() override {
            delete this->m_type;
        }

        ASTNode* clone() override {
            return new ASTNodePointerVariableDecl(*this);
        }

        [[nodiscard]] std::string_view getName() const { return this->m_name; }
        [[nodiscard]] constexpr ASTNode* getType() const { return this->m_type; }
        [[nodiscard]] constexpr ASTNode* getSizeType() const { return this->m_sizeType; }
        [[nodiscard]] constexpr auto getPlacementOffset() const { return this->m_placementOffset; }

    private:
        std::string m_name;
        ASTNode *m_type;
        ASTNode *m_sizeType;
        ASTNode *m_placementOffset;
    };

    class ASTNodeStruct : public ASTNode {
    public:
        ASTNodeStruct() : ASTNode() { }

        ASTNodeStruct(const ASTNodeStruct &other) : ASTNode(other) {
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

        ASTNodeUnion(const ASTNodeUnion &other) : ASTNode(other) {
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
        explicit ASTNodeEnum(ASTNode *underlyingType) : ASTNode(), m_underlyingType(underlyingType) { }

        ASTNodeEnum(const ASTNodeEnum &other) : ASTNode(other) {
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
        void addEntry(const std::string &name, ASTNode* expression) { this->m_entries.emplace_back(name, expression); }

        [[nodiscard]] const ASTNode *getUnderlyingType() const { return this->m_underlyingType; }

    private:
        std::vector<std::pair<std::string, ASTNode*>> m_entries;
        ASTNode *m_underlyingType;
    };

    class ASTNodeBitfield : public ASTNode {
    public:
        ASTNodeBitfield() : ASTNode() { }

        ASTNodeBitfield(const ASTNodeBitfield &other) : ASTNode(other) {
            for (const auto &[name, entry] : other.getEntries())
                this->m_entries.emplace_back(name, entry->clone());
        }

        ~ASTNodeBitfield() override {
            for (auto &[name, expr] : this->m_entries)
                delete expr;
        }

        ASTNode* clone() override {
            return new ASTNodeBitfield(*this);
        }

        [[nodiscard]] const std::vector<std::pair<std::string, ASTNode*>>& getEntries() const { return this->m_entries; }
        void addEntry(const std::string &name, ASTNode* size) { this->m_entries.emplace_back(name, size); }

    private:
        std::vector<std::pair<std::string, ASTNode*>> m_entries;
    };

    class ASTNodeRValue : public ASTNode {
    public:
        explicit ASTNodeRValue(std::vector<std::string> path) : ASTNode(), m_path(std::move(path)) { }

        ASTNodeRValue(const ASTNodeRValue&) = default;

        ASTNode* clone() override {
            return new ASTNodeRValue(*this);
        }

        const std::vector<std::string>& getPath() {
            return this->m_path;
        }

    private:
        std::vector<std::string> m_path;
    };

    class ASTNodeScopeResolution : public ASTNode {
    public:
        explicit ASTNodeScopeResolution(std::vector<std::string> path) : ASTNode(), m_path(std::move(path)) { }

        ASTNodeScopeResolution(const ASTNodeScopeResolution&) = default;

        ASTNode* clone() override {
            return new ASTNodeScopeResolution(*this);
        }

        const std::vector<std::string>& getPath() {
            return this->m_path;
        }

    private:
        std::vector<std::string> m_path;
    };

}