#pragma once

#include "token.hpp"

#include <bit>
#include <optional>
#include <unordered_map>
#include <variant>
#include <vector>

namespace hex::lang {

    class ASTNodeAttribute;

    class Attributable {
    protected:
        Attributable() = default;
        Attributable(const Attributable&) = default;
    public:

        void addAttribute(ASTNodeAttribute *attribute) {
            this->m_attributes.push_back(attribute);
        }

        [[nodiscard]] const auto& getAttributes() const {
            return this->m_attributes;
        }

    private:
        std::vector<ASTNodeAttribute*> m_attributes;
    };

    class ASTNode {
    public:
        constexpr ASTNode() = default;
        constexpr virtual ~ASTNode() = default;
        constexpr ASTNode(const ASTNode &) = default;

        [[nodiscard]] constexpr u32 getLineNumber() const { return this->m_lineNumber; }
        [[maybe_unused]] constexpr void setLineNumber(u32 lineNumber) { this->m_lineNumber = lineNumber; }

        [[nodiscard]] virtual ASTNode* clone() const = 0;

    private:
        u32 m_lineNumber = 1;
    };

    class ASTNodeIntegerLiteral : public ASTNode {
    public:
        explicit ASTNodeIntegerLiteral(Token::IntegerLiteral literal) : ASTNode(), m_literal(std::move(literal)) { }

        ASTNodeIntegerLiteral(const ASTNodeIntegerLiteral&) = default;

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeIntegerLiteral(*this);
        }

        [[nodiscard]] const auto& getValue() const {
            return this->m_literal;
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

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeNumericExpression(*this);
        }

        ASTNode *getLeftOperand() { return this->m_left; }
        ASTNode *getRightOperand() { return this->m_right; }
        Token::Operator getOperator() { return this->m_operator; }

    private:
        ASTNode *m_left, *m_right;
        Token::Operator m_operator;
    };

    class ASTNodeTernaryExpression : public ASTNode {
    public:
        ASTNodeTernaryExpression(ASTNode *first, ASTNode *second, ASTNode *third, Token::Operator op)
                : ASTNode(), m_first(first), m_second(second), m_third(third), m_operator(op) { }

        ~ASTNodeTernaryExpression() override {
            delete this->m_first;
            delete this->m_second;
            delete this->m_third;
        }

        ASTNodeTernaryExpression(const ASTNodeTernaryExpression &other) : ASTNode(other) {
            this->m_operator = other.m_operator;
            this->m_first = other.m_first->clone();
            this->m_second = other.m_second->clone();
            this->m_third = other.m_third->clone();
        }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeTernaryExpression(*this);
        }

        ASTNode *getFirstOperand() { return this->m_first; }
        ASTNode *getSecondOperand() { return this->m_second; }
        ASTNode *getThirdOperand() { return this->m_third; }
        Token::Operator getOperator() { return this->m_operator; }

    private:
        ASTNode *m_first, *m_second, *m_third;
        Token::Operator m_operator;
    };

    class ASTNodeBuiltinType : public ASTNode {
    public:
        constexpr explicit ASTNodeBuiltinType(Token::ValueType type)
                : ASTNode(), m_type(type) { }

        [[nodiscard]] constexpr const auto& getType() const { return this->m_type; }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeBuiltinType(*this);
        }

    private:
        const Token::ValueType m_type;
    };

    class ASTNodeTypeDecl : public ASTNode, public Attributable {
    public:
        ASTNodeTypeDecl(std::string_view name, ASTNode *type, std::optional<std::endian> endian = { })
                : ASTNode(), m_name(name), m_type(type), m_endian(endian) { }

        ASTNodeTypeDecl(const ASTNodeTypeDecl& other) : ASTNode(other), Attributable(other) {
            this->m_name = other.m_name;
            if (other.m_type != nullptr)
                this->m_type = other.m_type->clone();
            else
                this->m_type = nullptr;
            this->m_endian = other.m_endian;
        }

        ~ASTNodeTypeDecl() override {
            delete this->m_type;
        }

        [[nodiscard]] ASTNode* clone() const override {
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

    class ASTNodeVariableDecl : public ASTNode, public Attributable {
    public:
        ASTNodeVariableDecl(std::string_view name, ASTNode *type, ASTNode *placementOffset = nullptr)
                : ASTNode(), m_name(name), m_type(type), m_placementOffset(placementOffset) { }

        ASTNodeVariableDecl(const ASTNodeVariableDecl &other) : ASTNode(other), Attributable(other) {
            this->m_name = other.m_name;
            this->m_type = other.m_type->clone();

            if (other.m_placementOffset != nullptr)
                this->m_placementOffset = other.m_placementOffset->clone();
            else
                this->m_placementOffset = nullptr;
        }

        ~ASTNodeVariableDecl() override {
            delete this->m_type;
            delete this->m_placementOffset;
        }

        [[nodiscard]] ASTNode* clone() const override {
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

    class ASTNodeArrayVariableDecl : public ASTNode, public Attributable {
    public:
        ASTNodeArrayVariableDecl(std::string_view name, ASTNode *type, ASTNode *size, ASTNode *placementOffset = nullptr)
                : ASTNode(), m_name(name), m_type(type), m_size(size), m_placementOffset(placementOffset) { }

        ASTNodeArrayVariableDecl(const ASTNodeArrayVariableDecl &other) : ASTNode(other), Attributable(other) {
            this->m_name = other.m_name;
            this->m_type = other.m_type->clone();
            if (other.m_size != nullptr)
                this->m_size = other.m_size->clone();
            else
                this->m_size = nullptr;

            if (other.m_placementOffset != nullptr)
                this->m_placementOffset = other.m_placementOffset->clone();
            else
                this->m_placementOffset = nullptr;
        }

        ~ASTNodeArrayVariableDecl() override {
            delete this->m_type;
            delete this->m_size;
            delete this->m_placementOffset;
        }

        [[nodiscard]] ASTNode* clone() const override {
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

    class ASTNodePointerVariableDecl : public ASTNode, public Attributable {
    public:
        ASTNodePointerVariableDecl(std::string_view name, ASTNode *type, ASTNode *sizeType, ASTNode *placementOffset = nullptr)
                : ASTNode(), m_name(name), m_type(type), m_sizeType(sizeType), m_placementOffset(placementOffset) { }

        ASTNodePointerVariableDecl(const ASTNodePointerVariableDecl &other) : ASTNode(other), Attributable(other) {
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
            delete this->m_sizeType;
            delete this->m_placementOffset;
        }

        [[nodiscard]] ASTNode* clone() const override {
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

    class ASTNodeStruct : public ASTNode, public Attributable {
    public:
        ASTNodeStruct() : ASTNode() { }

        ASTNodeStruct(const ASTNodeStruct &other) : ASTNode(other), Attributable(other) {
            for (const auto &otherMember : other.getMembers())
                this->m_members.push_back(otherMember->clone());
        }

        ~ASTNodeStruct() override {
            for (auto &member : this->m_members)
                delete member;
        }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeStruct(*this);
        }

        [[nodiscard]] const std::vector<ASTNode*>& getMembers() const { return this->m_members; }
        void addMember(ASTNode *node) { this->m_members.push_back(node); }

    private:
        std::vector<ASTNode*> m_members;
    };

    class ASTNodeUnion : public ASTNode, public Attributable {
    public:
        ASTNodeUnion() : ASTNode() { }

        ASTNodeUnion(const ASTNodeUnion &other) : ASTNode(other), Attributable(other) {
            for (const auto &otherMember : other.getMembers())
                this->m_members.push_back(otherMember->clone());
        }

        ~ASTNodeUnion() override {
            for (auto &member : this->m_members)
                delete member;
        }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeUnion(*this);
        }

        [[nodiscard]] const  std::vector<ASTNode*>& getMembers() const { return this->m_members; }
        void addMember(ASTNode *node) { this->m_members.push_back(node); }

    private:
        std::vector<ASTNode*> m_members;
    };

    class ASTNodeEnum : public ASTNode, public Attributable {
    public:
        explicit ASTNodeEnum(ASTNode *underlyingType) : ASTNode(), m_underlyingType(underlyingType) { }

        ASTNodeEnum(const ASTNodeEnum &other) : ASTNode(other), Attributable(other) {
            for (const auto &[name, entry] : other.getEntries())
                this->m_entries.insert({ name, entry->clone() });
            this->m_underlyingType = other.m_underlyingType->clone();
        }

        ~ASTNodeEnum() override {
            for (auto &[name, expr] : this->m_entries)
                delete expr;
            delete this->m_underlyingType;
        }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeEnum(*this);
        }

        [[nodiscard]] const std::unordered_map<std::string, ASTNode*>& getEntries() const { return this->m_entries; }
        void addEntry(const std::string &name, ASTNode* expression) { this->m_entries.insert({ name, expression }); }

        [[nodiscard]] ASTNode *getUnderlyingType() { return this->m_underlyingType; }

    private:
        std::unordered_map<std::string, ASTNode*> m_entries;
        ASTNode *m_underlyingType;
    };

    class ASTNodeBitfield : public ASTNode, public Attributable {
    public:
        ASTNodeBitfield() : ASTNode() { }

        ASTNodeBitfield(const ASTNodeBitfield &other) : ASTNode(other), Attributable(other) {
            for (const auto &[name, entry] : other.getEntries())
                this->m_entries.emplace_back(name, entry->clone());
        }

        ~ASTNodeBitfield() override {
            for (auto &[name, expr] : this->m_entries)
                delete expr;
        }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeBitfield(*this);
        }

        [[nodiscard]] const std::vector<std::pair<std::string, ASTNode*>>& getEntries() const { return this->m_entries; }
        void addEntry(const std::string &name, ASTNode* size) { this->m_entries.emplace_back(name, size); }

    private:
        std::vector<std::pair<std::string, ASTNode*>> m_entries;
    };

    class ASTNodeRValue : public ASTNode {
    public:
        using Path = std::vector<std::variant<std::string, ASTNode*>>;

        explicit ASTNodeRValue(Path path) : ASTNode(), m_path(std::move(path)) { }

        ASTNodeRValue(const ASTNodeRValue&) = default;

        ~ASTNodeRValue() override {
            for (auto &part : this->m_path) {
                if (auto node = std::get_if<ASTNode*>(&part); node != nullptr)
                    delete *node;
            }
        }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeRValue(*this);
        }

        const Path& getPath() {
            return this->m_path;
        }

    private:
        Path m_path;
    };

    class ASTNodeScopeResolution : public ASTNode {
    public:
        explicit ASTNodeScopeResolution(std::vector<std::string> path) : ASTNode(), m_path(std::move(path)) { }

        ASTNodeScopeResolution(const ASTNodeScopeResolution&) = default;

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeScopeResolution(*this);
        }

        const std::vector<std::string>& getPath() {
            return this->m_path;
        }

    private:
        std::vector<std::string> m_path;
    };

    class ASTNodeConditionalStatement : public ASTNode {
    public:
        explicit ASTNodeConditionalStatement(ASTNode *condition, std::vector<ASTNode*> trueBody, std::vector<ASTNode*> falseBody)
            : ASTNode(), m_condition(condition), m_trueBody(std::move(trueBody)), m_falseBody(std::move(falseBody)) { }

        ~ASTNodeConditionalStatement() override {
            delete this->m_condition;

            for (auto &statement : this->m_trueBody)
                delete statement;
            for (auto &statement : this->m_falseBody)
                delete statement;
        }

        ASTNodeConditionalStatement(const ASTNodeConditionalStatement &other) : ASTNode(other) {
            this->m_condition = other.m_condition->clone();

            for (auto &statement : other.m_trueBody)
                this->m_trueBody.push_back(statement->clone());
            for (auto &statement : other.m_falseBody)
                this->m_falseBody.push_back(statement->clone());
        }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeConditionalStatement(*this);
        }

        [[nodiscard]] ASTNode* getCondition() {
            return this->m_condition;
        }

        [[nodiscard]] const std::vector<ASTNode*>& getTrueBody() const {
            return this->m_trueBody;
        }

        [[nodiscard]] const std::vector<ASTNode*>& getFalseBody() const {
            return this->m_falseBody;
        }

    private:
        ASTNode *m_condition;
        std::vector<ASTNode*> m_trueBody, m_falseBody;
    };

    class ASTNodeWhileStatement : public ASTNode {
    public:
        explicit ASTNodeWhileStatement(ASTNode *condition, std::vector<ASTNode*> body)
            : ASTNode(), m_condition(condition), m_body(std::move(body)) { }

        ~ASTNodeWhileStatement() override {
            delete this->m_condition;

            for (auto &statement : this->m_body)
                delete statement;
        }

        ASTNodeWhileStatement(const ASTNodeWhileStatement &other) : ASTNode(other) {
            this->m_condition = other.m_condition->clone();
        }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeWhileStatement(*this);
        }

        [[nodiscard]] ASTNode* getCondition() {
            return this->m_condition;
        }

        [[nodiscard]] const std::vector<ASTNode*>& getBody() {
            return this->m_body;
        }

    private:
        ASTNode *m_condition;
        std::vector<ASTNode*> m_body;
    };

    class ASTNodeFunctionCall : public ASTNode {
    public:
        explicit ASTNodeFunctionCall(std::string_view functionName, std::vector<ASTNode*> params)
                : ASTNode(), m_functionName(functionName), m_params(std::move(params)) { }

        ~ASTNodeFunctionCall() override {
            for (auto &param : this->m_params)
                delete param;
        }

        ASTNodeFunctionCall(const ASTNodeFunctionCall &other) : ASTNode(other) {
            this->m_functionName = other.m_functionName;

            for (auto &param : other.m_params)
                this->m_params.push_back(param->clone());
        }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeFunctionCall(*this);
        }

        [[nodiscard]] std::string_view getFunctionName() {
            return this->m_functionName;
        }

        [[nodiscard]] const std::vector<ASTNode*>& getParams() const {
            return this->m_params;
        }

    private:
        std::string m_functionName;
        std::vector<ASTNode*> m_params;
    };

    class ASTNodeStringLiteral : public ASTNode {
    public:
        explicit ASTNodeStringLiteral(std::string_view string) : ASTNode(), m_string(string) { }

        ~ASTNodeStringLiteral() override = default;

        ASTNodeStringLiteral(const ASTNodeStringLiteral &other) : ASTNode(other) {
            this->m_string = other.m_string;
        }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeStringLiteral(*this);
        }

        [[nodiscard]] std::string_view getString() {
            return this->m_string;
        }

    private:
        std::string m_string;
    };

    class ASTNodeAttribute : public ASTNode {
    public:
        explicit ASTNodeAttribute(std::string_view attribute, std::optional<std::string_view> value = { })
            : ASTNode(), m_attribute(attribute), m_value(value) { }

        ~ASTNodeAttribute() override = default;

        ASTNodeAttribute(const ASTNodeAttribute &other) : ASTNode(other) {
            this->m_attribute = other.m_attribute;
            this->m_value = other.m_value;
        }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeAttribute(*this);
        }

        [[nodiscard]] std::string_view getAttribute() const {
            return this->m_attribute;
        }

        [[nodiscard]] const std::optional<std::string>& getValue() const {
            return this->m_value;
        }

    private:
        std::string m_attribute;
        std::optional<std::string> m_value;
    };

    class ASTNodeTypeOperator : public ASTNode {
    public:
        ASTNodeTypeOperator(Token::Operator op, ASTNode *expression) : m_op(op), m_expression(expression) {

        }

        ASTNodeTypeOperator(const ASTNodeTypeOperator &other) : ASTNode(other) {
            this->m_op = other.m_op;
            this->m_expression = other.m_expression->clone();
        }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeTypeOperator(*this);
        }

        ~ASTNodeTypeOperator() override {
            delete this->m_expression;
        }

        Token::Operator getOperator() const {
            return this->m_op;
        }

        ASTNode* getExpression() const {
            return this->m_expression;
        }

    private:
        Token::Operator m_op;
        ASTNode *m_expression;
    };

    class ASTNodeFunctionDefinition : public ASTNode {
    public:
        ASTNodeFunctionDefinition(std::string name, std::vector<std::string> params, std::vector<ASTNode*> body)
            : m_name(std::move(name)), m_params(std::move(params)), m_body(std::move(body)) {

        }

        ASTNodeFunctionDefinition(const ASTNodeFunctionDefinition &other) : ASTNode(other) {
            this->m_name = other.m_name;
            this->m_params = other.m_params;

            for (auto statement : other.m_body) {
                this->m_body.push_back(statement->clone());
            }
        }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeFunctionDefinition(*this);
        }

        ~ASTNodeFunctionDefinition() override {
            for (auto statement : this->m_body)
                delete statement;
        }

        [[nodiscard]] std::string_view getName() const {
            return this->m_name;
        }

        [[nodiscard]] const auto& getParams() const {
            return this->m_params;
        }

        [[nodiscard]] const auto& getBody() const {
            return this->m_body;
        }

    private:
        std::string m_name;
        std::vector<std::string> m_params;
        std::vector<ASTNode*> m_body;
    };

    class ASTNodeAssignment : public ASTNode {
    public:
        ASTNodeAssignment(std::string lvalueName, ASTNode *rvalue) : m_lvalueName(std::move(lvalueName)), m_rvalue(rvalue) {

        }

        ASTNodeAssignment(const ASTNodeAssignment &other) : ASTNode(other) {
            this->m_lvalueName = other.m_lvalueName;
            this->m_rvalue = other.m_rvalue->clone();
        }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeAssignment(*this);
        }

        ~ASTNodeAssignment() override {
            delete this->m_rvalue;
        }

        [[nodiscard]] std::string_view getLValueName() const {
            return this->m_lvalueName;
        }

        [[nodiscard]] ASTNode* getRValue() const {
            return this->m_rvalue;
        }

    private:
        std::string m_lvalueName;
        ASTNode *m_rvalue;
    };

    class ASTNodeReturnStatement : public ASTNode {
    public:
        ASTNodeReturnStatement(ASTNode *rvalue) : m_rvalue(rvalue) {

        }

        ASTNodeReturnStatement(const ASTNodeReturnStatement &other) : ASTNode(other) {
            this->m_rvalue = other.m_rvalue->clone();
        }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeReturnStatement(*this);
        }

        ~ASTNodeReturnStatement() override {
            delete this->m_rvalue;
        }

        [[nodiscard]] ASTNode* getRValue() const {
            return this->m_rvalue;
        }

    private:
        ASTNode *m_rvalue;
    };
}