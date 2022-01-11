#pragma once

#include <hex/pattern_language/token.hpp>
#include <hex/pattern_language/evaluator.hpp>
#include <hex/pattern_language/pattern_data.hpp>

#include <algorithm>
#include <bit>
#include <chrono>
#include <optional>
#include <map>
#include <thread>
#include <variant>
#include <vector>

namespace hex::pl {

    class ASTNode;
    class ASTNodeAttribute;

    class PatternData;
    class Evaluator;

    class Attributable {
    protected:
        Attributable() = default;

        Attributable(const Attributable &) = default;

    public:

        void addAttribute(ASTNodeAttribute *attribute) {
            this->m_attributes.push_back(attribute);
        }

        [[nodiscard]] const auto &getAttributes() const {
            return this->m_attributes;
        }

    private:
        std::vector<ASTNodeAttribute *> m_attributes;
    };

    class Clonable {
    public:
        [[nodiscard]]
        virtual ASTNode* clone() const = 0;
    };

    class ASTNode : public Clonable {
    public:
        constexpr ASTNode() = default;

        constexpr virtual ~ASTNode() = default;

        constexpr ASTNode(const ASTNode &) = default;

        [[nodiscard]] constexpr u32 getLineNumber() const { return this->m_lineNumber; }

        [[maybe_unused]] constexpr void setLineNumber(u32 lineNumber) { this->m_lineNumber = lineNumber; }

        [[nodiscard]] virtual ASTNode *evaluate(Evaluator *evaluator) const { return this->clone(); }

        [[nodiscard]] virtual std::vector<PatternData *> createPatterns(Evaluator *evaluator) const { return {}; }

        using FunctionResult = std::optional<Token::Literal>;
        virtual FunctionResult execute(Evaluator *evaluator) const { evaluator->getConsole().abortEvaluation("cannot execute non-function statement", this); }

    private:
        u32 m_lineNumber = 1;
    };

    class ASTNodeAttribute : public ASTNode {
    public:
        explicit ASTNodeAttribute(std::string attribute, std::optional<std::string> value = std::nullopt)
                : ASTNode(), m_attribute(std::move(attribute)), m_value(std::move(value)) { }

        ~ASTNodeAttribute() override = default;

        ASTNodeAttribute(const ASTNodeAttribute &other) : ASTNode(other) {
            this->m_attribute = other.m_attribute;
            this->m_value = other.m_value;
        }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeAttribute(*this);
        }

        [[nodiscard]] const std::string& getAttribute() const {
            return this->m_attribute;
        }

        [[nodiscard]] const std::optional<std::string>& getValue() const {
            return this->m_value;
        }

    private:
        std::string m_attribute;
        std::optional<std::string> m_value;
    };

    class ASTNodeLiteral : public ASTNode {
    public:
        explicit ASTNodeLiteral(Token::Literal literal) : ASTNode(), m_literal(literal) { }

        ASTNodeLiteral(const ASTNodeLiteral&) = default;

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeLiteral(*this);
        }

        [[nodiscard]] const auto& getValue() const {
            return this->m_literal;
        }

    private:
        Token::Literal m_literal;
    };

    class ASTNodeMathematicalExpression : public ASTNode {
        #define FLOAT_BIT_OPERATION(name) \
            auto name(hex::floating_point auto left, auto right) const { LogConsole::abortEvaluation("invalid floating point operation", this); return 0; } \
            auto name(auto left, hex::floating_point auto right) const { LogConsole::abortEvaluation("invalid floating point operation", this); return 0; } \
            auto name(hex::floating_point auto left, hex::floating_point auto right) const { LogConsole::abortEvaluation("invalid floating point operation", this); return 0; } \
            auto name(hex::integral auto left, hex::integral auto right) const

        FLOAT_BIT_OPERATION(shiftLeft) {
            return left << right;
        }

        FLOAT_BIT_OPERATION(shiftRight) {
            return left >> right;
        }

        FLOAT_BIT_OPERATION(bitAnd) {
            return left & right;
        }

        FLOAT_BIT_OPERATION(bitOr) {
            return left | right;
        }

        FLOAT_BIT_OPERATION(bitXor) {
            return left ^ right;
        }

        FLOAT_BIT_OPERATION(bitNot) {
            return ~right;
        }

        FLOAT_BIT_OPERATION(modulus) {
            return left % right;
        }

    #undef FLOAT_BIT_OPERATION
    public:
        ASTNodeMathematicalExpression(ASTNode *left, ASTNode *right, Token::Operator op)
                : ASTNode(), m_left(left), m_right(right), m_operator(op) { }

        ~ASTNodeMathematicalExpression() override {
            delete this->m_left;
            delete this->m_right;
        }

        ASTNodeMathematicalExpression(const ASTNodeMathematicalExpression &other) : ASTNode(other) {
            this->m_operator = other.m_operator;
            this->m_left = other.m_left->clone();
            this->m_right = other.m_right->clone();
        }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeMathematicalExpression(*this);
        }

        [[nodiscard]] ASTNode* evaluate(Evaluator *evaluator) const override {
            if (this->getLeftOperand() == nullptr || this->getRightOperand() == nullptr)
                LogConsole::abortEvaluation("attempted to use void expression in mathematical expression", this);

            auto *left = dynamic_cast<ASTNodeLiteral*>(this->getLeftOperand()->evaluate(evaluator));
            auto *right = dynamic_cast<ASTNodeLiteral*>(this->getRightOperand()->evaluate(evaluator));
            ON_SCOPE_EXIT { delete left; delete right; };

            return std::visit(overloaded {
                // TODO: :notlikethis:
                [this](u128 left, PatternData * const &right) -> ASTNode*           { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                [this](s128 left, PatternData * const &right) -> ASTNode*           { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                [this](double left, PatternData * const &right) -> ASTNode*         { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                [this](char left, PatternData * const &right) -> ASTNode*           { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                [this](bool left, PatternData * const &right) -> ASTNode*           { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                [this](std::string left, PatternData * const &right) -> ASTNode*    { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                [this](PatternData * const &left, u128 right) -> ASTNode*           { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                [this](PatternData * const &left, s128 right) -> ASTNode*           { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                [this](PatternData * const &left, double right) -> ASTNode*         { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                [this](PatternData * const &left, char right) -> ASTNode*           { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                [this](PatternData * const &left, bool right) -> ASTNode*           { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                [this](PatternData * const &left, std::string right) -> ASTNode*    { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                [this](PatternData * const &left, PatternData *right) -> ASTNode*   { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },

                [this](auto&& left, std::string right) -> ASTNode*          { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                [this](std::string left, auto&& right) -> ASTNode* {
                    switch (this->getOperator()) {
                        case Token::Operator::Star: {
                            std::string result;
                            for (auto i = 0; i < right; i++)
                                result += left;
                            return new ASTNodeLiteral(result);
                        }
                        default:
                            LogConsole::abortEvaluation("invalid operand used in mathematical expression", this);
                    }
                },
                [this](std::string left, std::string right) -> ASTNode* {
                    switch (this->getOperator()) {
                        case Token::Operator::Plus:
                            return new ASTNodeLiteral(left + right);
                        case Token::Operator::BoolEquals:
                            return new ASTNodeLiteral(left == right);
                        case Token::Operator::BoolNotEquals:
                            return new ASTNodeLiteral(left != right);
                        case Token::Operator::BoolGreaterThan:
                            return new ASTNodeLiteral(left > right);
                        case Token::Operator::BoolLessThan:
                            return new ASTNodeLiteral(left < right);
                        case Token::Operator::BoolGreaterThanOrEquals:
                            return new ASTNodeLiteral(left >= right);
                        case Token::Operator::BoolLessThanOrEquals:
                            return new ASTNodeLiteral(left <= right);
                        default:
                            LogConsole::abortEvaluation("invalid operand used in mathematical expression", this);
                    }
                },
                [this](std::string left, char right) -> ASTNode* {
                    switch (this->getOperator()) {
                        case Token::Operator::Plus:
                            return new ASTNodeLiteral(left + right);
                        default:
                            LogConsole::abortEvaluation("invalid operand used in mathematical expression", this);
                    }
                },
                [this](char left, std::string right) -> ASTNode* {
                        switch (this->getOperator()) {
                            case Token::Operator::Plus:
                                return new ASTNodeLiteral(left + right);
                            default:
                                LogConsole::abortEvaluation("invalid operand used in mathematical expression", this);
                        }
                    },
                [this](auto &&left, auto &&right) -> ASTNode* {
                    switch (this->getOperator()) {
                        case Token::Operator::Plus:
                            return new ASTNodeLiteral(left + right);
                        case Token::Operator::Minus:
                            return new ASTNodeLiteral(left - right);
                        case Token::Operator::Star:
                            return new ASTNodeLiteral(left * right);
                        case Token::Operator::Slash:
                            if (right == 0) LogConsole::abortEvaluation("division by zero!", this);
                            return new ASTNodeLiteral(left / right);
                        case Token::Operator::Percent:
                            if (right == 0) LogConsole::abortEvaluation("division by zero!", this);
                            return new ASTNodeLiteral(modulus(left, right));
                        case Token::Operator::ShiftLeft:
                            return new ASTNodeLiteral(shiftLeft(left, right));
                        case Token::Operator::ShiftRight:
                            return new ASTNodeLiteral(shiftRight(left, right));
                        case Token::Operator::BitAnd:
                            return new ASTNodeLiteral(bitAnd(left, right));
                        case Token::Operator::BitXor:
                            return new ASTNodeLiteral(bitXor(left, right));
                        case Token::Operator::BitOr:
                            return new ASTNodeLiteral(bitOr(left, right));
                        case Token::Operator::BitNot:
                            return new ASTNodeLiteral(bitNot(left, right));
                        case Token::Operator::BoolEquals:
                            return new ASTNodeLiteral(bool(left == right));
                        case Token::Operator::BoolNotEquals:
                            return new ASTNodeLiteral(bool(left != right));
                        case Token::Operator::BoolGreaterThan:
                            return new ASTNodeLiteral(bool(left > right));
                        case Token::Operator::BoolLessThan:
                            return new ASTNodeLiteral(bool(left < right));
                        case Token::Operator::BoolGreaterThanOrEquals:
                            return new ASTNodeLiteral(bool(left >= right));
                        case Token::Operator::BoolLessThanOrEquals:
                            return new ASTNodeLiteral(bool(left <= right));
                        case Token::Operator::BoolAnd:
                            return new ASTNodeLiteral(bool(left && right));
                        case Token::Operator::BoolXor:
                            return new ASTNodeLiteral(bool(left && !right || !left && right));
                        case Token::Operator::BoolOr:
                            return new ASTNodeLiteral(bool(left || right));
                        case Token::Operator::BoolNot:
                            return new ASTNodeLiteral(bool(!right));
                        default:
                            LogConsole::abortEvaluation("invalid operand used in mathematical expression", this);
                    }
                }
            }, left->getValue(), right->getValue());
        }

        [[nodiscard]] ASTNode *getLeftOperand() const { return this->m_left; }
        [[nodiscard]] ASTNode *getRightOperand() const { return this->m_right; }
        [[nodiscard]] Token::Operator getOperator() const { return this->m_operator; }

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

        [[nodiscard]] ASTNode* evaluate(Evaluator *evaluator) const override {
            if (this->getFirstOperand() == nullptr || this->getSecondOperand() == nullptr || this->getThirdOperand() == nullptr)
                LogConsole::abortEvaluation("attempted to use void expression in mathematical expression", this);

            auto *first = dynamic_cast<ASTNodeLiteral*>(this->getFirstOperand()->evaluate(evaluator));
            auto *second = dynamic_cast<ASTNodeLiteral*>(this->getSecondOperand()->evaluate(evaluator));
            auto *third = dynamic_cast<ASTNodeLiteral*>(this->getThirdOperand()->evaluate(evaluator));
            ON_SCOPE_EXIT { delete first; delete second; delete third; };

            auto condition = std::visit(overloaded {
                [this](std::string value) -> bool { return !value.empty(); },
                [this](PatternData * const &) -> bool { LogConsole::abortEvaluation("cannot cast custom type to bool", this); },
                [](auto &&value) -> bool { return bool(value); }
            }, first->getValue());

            return std::visit(overloaded {
                [condition]<typename T>(const T &second, const T &third) -> ASTNode* { return new ASTNodeLiteral(condition ? second : third); },
                [this](auto &&second, auto &&third) -> ASTNode* { LogConsole::abortEvaluation("operands to ternary expression have different types", this); }
            }, second->getValue(), third->getValue());
        }

        [[nodiscard]] ASTNode *getFirstOperand() const { return this->m_first; }
        [[nodiscard]] ASTNode *getSecondOperand() const { return this->m_second; }
        [[nodiscard]] ASTNode *getThirdOperand() const { return this->m_third; }
        [[nodiscard]] Token::Operator getOperator() const { return this->m_operator; }

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

        [[nodiscard]] std::vector<PatternData*> createPatterns(Evaluator *evaluator) const override {
            auto offset = evaluator->dataOffset();
            auto size = Token::getTypeSize(this->m_type);

            evaluator->dataOffset() += size;

            PatternData *pattern;
            if (Token::isUnsigned(this->m_type))
                pattern = new PatternDataUnsigned(offset, size, evaluator);
            else if (Token::isSigned(this->m_type))
                pattern = new PatternDataSigned(offset, size, evaluator);
            else if (Token::isFloatingPoint(this->m_type))
                pattern = new PatternDataFloat(offset, size, evaluator);
            else if (this->m_type == Token::ValueType::Boolean)
                pattern = new PatternDataBoolean(offset, evaluator);
            else if (this->m_type == Token::ValueType::Character)
                pattern = new PatternDataCharacter(offset, evaluator);
            else if (this->m_type == Token::ValueType::Character16)
                pattern = new PatternDataCharacter16(offset, evaluator);
            else if (this->m_type == Token::ValueType::Padding)
                pattern = new PatternDataPadding(offset, 1, evaluator);
            else if (this->m_type == Token::ValueType::String)
                pattern = new PatternDataString(offset, 1, evaluator);
            else if (this->m_type == Token::ValueType::Auto)
                return { nullptr };
            else
                LogConsole::abortEvaluation("invalid built-in type", this);

            pattern->setTypeName(Token::getTypeName(this->m_type));

            return { pattern };
        }

    private:
        const Token::ValueType m_type;
    };

    class ASTNodeTypeDecl : public ASTNode, public Attributable {
    public:
        ASTNodeTypeDecl(std::string name, ASTNode *type, std::optional<std::endian> endian = std::nullopt)
                : ASTNode(), m_name(std::move(name)), m_type(type), m_endian(endian) { }

        ASTNodeTypeDecl(const ASTNodeTypeDecl& other) : ASTNode(other), Attributable(other) {
            this->m_name = other.m_name;
            this->m_type = other.m_type;
            this->m_endian = other.m_endian;
        }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeTypeDecl(*this);
        }

        void setName(const std::string &name) { this->m_name = name; }
        [[nodiscard]] const std::string& getName() const { return this->m_name; }
        [[nodiscard]] ASTNode* getType() { return this->m_type; }
        [[nodiscard]] std::optional<std::endian> getEndian() const { return this->m_endian; }

        [[nodiscard]] ASTNode *evaluate(Evaluator *evaluator) const override {
            return this->m_type->evaluate(evaluator);
        }

        [[nodiscard]] std::vector<PatternData*> createPatterns(Evaluator *evaluator) const override {
            auto patterns = this->m_type->createPatterns(evaluator);

            for (auto &pattern : patterns) {
                if (pattern == nullptr)
                    continue;

                if (!this->m_name.empty())
                    pattern->setTypeName(this->m_name);
                pattern->setEndian(this->m_endian.value_or(evaluator->getDefaultEndian()));
            }

            return patterns;
        }

    private:
        std::string m_name;
        ASTNode *m_type;
        std::optional<std::endian> m_endian;
    };

    class ASTNodeCast : public ASTNode {
    public:
        ASTNodeCast(ASTNode *value, ASTNode *type) : m_value(value), m_type(type) { }
        ASTNodeCast(const ASTNodeCast &other) {
            this->m_value = other.m_value->clone();
            this->m_type = other.m_type->clone();
        }

        ~ASTNodeCast() override {
            delete this->m_value;
            delete this->m_type;
        }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeCast(*this);
        }

        [[nodiscard]] ASTNode* evaluate(Evaluator *evaluator) const override {
            auto literal = dynamic_cast<ASTNodeLiteral*>(this->m_value->evaluate(evaluator));
            auto type = dynamic_cast<ASTNodeBuiltinType*>(this->m_type->evaluate(evaluator))->getType();

            auto startOffset= evaluator->dataOffset();

            auto typePattern = this->m_type->createPatterns(evaluator).front();
            ON_SCOPE_EXIT {
                evaluator->dataOffset() = startOffset;
                delete typePattern;
            };

            return std::visit(overloaded {
                    [&, this](PatternData * value) -> ASTNode* { LogConsole::abortEvaluation(hex::format("cannot cast custom type '{}' to '{}'", value->getTypeName(), Token::getTypeName(type)), this); },
                    [&, this](const std::string&) -> ASTNode* { LogConsole::abortEvaluation(hex::format("cannot cast string to '{}'", Token::getTypeName(type)), this); },
                    [&, this](auto &&value) -> ASTNode* {
                        auto endianAdjustedValue = hex::changeEndianess(value, typePattern->getSize(), typePattern->getEndian());
                        switch (type) {
                            case Token::ValueType::Unsigned8Bit:
                                return new ASTNodeLiteral(u128(u8(endianAdjustedValue)));
                            case Token::ValueType::Unsigned16Bit:
                                return new ASTNodeLiteral(u128(u16(endianAdjustedValue)));
                            case Token::ValueType::Unsigned32Bit:
                                return new ASTNodeLiteral(u128(u32(endianAdjustedValue)));
                            case Token::ValueType::Unsigned64Bit:
                                return new ASTNodeLiteral(u128(u64(endianAdjustedValue)));
                            case Token::ValueType::Unsigned128Bit:
                                return new ASTNodeLiteral(u128(endianAdjustedValue));
                            case Token::ValueType::Signed8Bit:
                                return new ASTNodeLiteral(s128(s8(endianAdjustedValue)));
                            case Token::ValueType::Signed16Bit:
                                return new ASTNodeLiteral(s128(s16(endianAdjustedValue)));
                            case Token::ValueType::Signed32Bit:
                                return new ASTNodeLiteral(s128(s32(endianAdjustedValue)));
                            case Token::ValueType::Signed64Bit:
                                return new ASTNodeLiteral(s128(s64(endianAdjustedValue)));
                            case Token::ValueType::Signed128Bit:
                                return new ASTNodeLiteral(s128(endianAdjustedValue));
                            case Token::ValueType::Float:
                                return new ASTNodeLiteral(double(float(endianAdjustedValue)));
                            case Token::ValueType::Double:
                                return new ASTNodeLiteral(double(endianAdjustedValue));
                            case Token::ValueType::Character:
                                return new ASTNodeLiteral(char(endianAdjustedValue));
                            case Token::ValueType::Character16:
                                return new ASTNodeLiteral(u128(char16_t(endianAdjustedValue)));
                            case Token::ValueType::Boolean:
                                return new ASTNodeLiteral(bool(endianAdjustedValue));
                            case Token::ValueType::String:
                            {
                                std::string string(sizeof(value), '\x00');
                                std::memcpy(string.data(), &value, string.size());
                                hex::trim(string);

                                if (typePattern->getEndian() != std::endian::native)
                                    std::reverse(string.begin(), string.end());

                                return new ASTNodeLiteral(string);
                            }
                            default:
                                LogConsole::abortEvaluation(hex::format("cannot cast value to '{}'", Token::getTypeName(type)), this);
                        }
                    },
            }, literal->getValue());
        }

    private:
        ASTNode *m_value;
        ASTNode *m_type;
    };

    class ASTNodeWhileStatement : public ASTNode {
    public:
        explicit ASTNodeWhileStatement(ASTNode *condition, std::vector<ASTNode*> body, ASTNode *postExpression = nullptr)
                : ASTNode(), m_condition(condition), m_body(std::move(body)), m_postExpression(postExpression) { }

        ~ASTNodeWhileStatement() override {
            delete this->m_condition;

            for (auto &statement : this->m_body)
                delete statement;

            delete this->m_postExpression;
        }

        ASTNodeWhileStatement(const ASTNodeWhileStatement &other) : ASTNode(other) {
            this->m_condition = other.m_condition->clone();

            for (auto &statement : other.m_body)
                this->m_body.push_back(statement->clone());

            if (other.m_postExpression != nullptr)
                this->m_postExpression = other.m_postExpression->clone();
            else
                this->m_postExpression = nullptr;
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

        FunctionResult execute(Evaluator *evaluator) const override {

            u64 loopIterations = 0;
            while (evaluateCondition(evaluator)) {
                evaluator->handleAbort();

                auto variables = *evaluator->getScope(0).scope;
                u32 startVariableCount = variables.size();
                ON_SCOPE_EXIT {
                    s64 stackSize = evaluator->getStack().size();
                    for (u32 i = startVariableCount; i < variables.size(); i++) {
                        stackSize--;
                        delete variables[i];
                    }
                    if (stackSize < 0) LogConsole::abortEvaluation("stack pointer underflow!", this);
                    evaluator->getStack().resize(stackSize);
                };

                evaluator->pushScope(nullptr, variables);
                ON_SCOPE_EXIT { evaluator->popScope(); };

                auto ctrlFlow = ControlFlowStatement::None;
                for (auto &statement : this->m_body) {
                    auto result = statement->execute(evaluator);

                    ctrlFlow = evaluator->getCurrentControlFlowStatement();
                    evaluator->setCurrentControlFlowStatement(ControlFlowStatement::None);
                    if (ctrlFlow == ControlFlowStatement::Return)
                        return result;
                    else if (ctrlFlow != ControlFlowStatement::None)
                        break;
                }

                if (this->m_postExpression != nullptr)
                    this->m_postExpression->execute(evaluator);

                loopIterations++;
                if (loopIterations >= evaluator->getLoopLimit())
                    LogConsole::abortEvaluation(hex::format("loop iterations exceeded limit of {}", evaluator->getLoopLimit()), this);

                evaluator->handleAbort();

                if (ctrlFlow == ControlFlowStatement::Break)
                    break;
                else if (ctrlFlow == ControlFlowStatement::Continue)
                    continue;
            }

            return { };
        }

        [[nodiscard]]
        bool evaluateCondition(Evaluator *evaluator) const {
            auto literal = dynamic_cast<ASTNodeLiteral*>(this->m_condition->evaluate(evaluator));
            ON_SCOPE_EXIT { delete literal; };

            return std::visit(overloaded {
                    [](std::string value) -> bool { return !value.empty(); },
                    [this](PatternData * const &) -> bool { LogConsole::abortEvaluation("cannot cast custom type to bool", this); },
                    [](auto &&value) -> bool { return value != 0; }
            }, literal->getValue());
        }

    private:
        ASTNode *m_condition;
        std::vector<ASTNode*> m_body;
        ASTNode *m_postExpression;
    };

    inline void applyVariableAttributes(Evaluator *evaluator, const Attributable *attributable, PatternData *pattern) {
        auto endOffset = evaluator->dataOffset();
        evaluator->dataOffset() = pattern->getOffset();
        ON_SCOPE_EXIT { evaluator->dataOffset() = endOffset; };

        for (ASTNodeAttribute *attribute : attributable->getAttributes()) {
            auto &name = attribute->getAttribute();
            auto value = attribute->getValue();

            auto node = reinterpret_cast<const ASTNode*>(attributable);

            auto requiresValue = [&]() {
                if (!value.has_value())
                    LogConsole::abortEvaluation(hex::format("used attribute '{}' without providing a value", name), node);
                return true;
            };

            auto noValue = [&]() {
                if (value.has_value())
                    LogConsole::abortEvaluation(hex::format("provided a value to attribute '{}' which doesn't take one", name), node);
                return true;
            };

            if (name == "color" && requiresValue()) {
                u32 color = strtoul(value->c_str(), nullptr, 16);
                pattern->setColor(hex::changeEndianess(color, std::endian::big) >> 8);
            } else if (name == "name" && requiresValue()) {
                pattern->setDisplayName(*value);
            } else if (name == "comment" && requiresValue()) {
                pattern->setComment(*value);
            } else if (name == "hidden" && noValue()) {
                pattern->setHidden(true);
            } else if (name == "inline" && noValue()) {
                auto inlinable = dynamic_cast<Inlinable*>(pattern);

                if (inlinable == nullptr)
                    LogConsole::abortEvaluation("inline attribute can only be applied to nested types", node);
                else
                    inlinable->setInlined(true);

            } else if (name == "format" && requiresValue()) {
                auto functions = evaluator->getCustomFunctions();
                if (!functions.contains(*value))
                    LogConsole::abortEvaluation(hex::format("cannot find formatter function '{}'", *value), node);

                const auto &function = functions[*value];
                if (function.parameterCount != 1)
                    LogConsole::abortEvaluation("formatter function needs exactly one parameter", node);

                pattern->setFormatterFunction(function);
            } else if (name == "transform" && requiresValue()) {
                auto functions = evaluator->getCustomFunctions();
                if (!functions.contains(*value))
                    LogConsole::abortEvaluation(hex::format("cannot find transform function '{}'", *value), node);

                const auto &function = functions[*value];
                if (function.parameterCount != 1)
                    LogConsole::abortEvaluation("transform function needs exactly one parameter", node);

                pattern->setTransformFunction(function);
            } else if (name == "pointer_base" && requiresValue()) {
                auto functions = evaluator->getCustomFunctions();
                if (!functions.contains(*value))
                    LogConsole::abortEvaluation(hex::format("cannot find pointer base function '{}'", *value), node);

                const auto &function = functions[*value];
                if (function.parameterCount != 1)
                    LogConsole::abortEvaluation("pointer base function needs exactly one parameter", node);

                if (auto pointerPattern = dynamic_cast<PatternDataPointer*>(pattern)) {
                    u128 pointerValue = pointerPattern->getPointedAtAddress();

                    auto result = function.func(evaluator, { pointerValue });

                    if (!result.has_value())
                        LogConsole::abortEvaluation("pointer base function did not return a value", node);

                    pointerPattern->setPointedAtAddress(Token::literalToUnsigned(result.value()) + pointerValue);
                } else {
                    LogConsole::abortEvaluation("pointer_base attribute may only be applied to a pointer");
                }
            }
        }
    }

    class ASTNodeVariableDecl : public ASTNode, public Attributable {
    public:
        ASTNodeVariableDecl(std::string name, ASTNode *type, ASTNode *placementOffset = nullptr, bool inVariable = false, bool outVariable = false)
                : ASTNode(), m_name(std::move(name)), m_type(type), m_placementOffset(placementOffset), m_inVariable(inVariable), m_outVariable(outVariable) { }

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

        [[nodiscard]] const std::string& getName() const { return this->m_name; }
        [[nodiscard]] constexpr ASTNode* getType() const { return this->m_type; }
        [[nodiscard]] constexpr auto getPlacementOffset() const { return this->m_placementOffset; }

        [[nodiscard]] constexpr bool isInVariable() const { return this->m_inVariable; }
        [[nodiscard]] constexpr bool isOutVariable() const { return this->m_outVariable; }

        [[nodiscard]] std::vector<PatternData*> createPatterns(Evaluator *evaluator) const override {
            if (this->m_placementOffset != nullptr) {
                auto offset = dynamic_cast<ASTNodeLiteral *>(this->m_placementOffset->evaluate(evaluator));
                ON_SCOPE_EXIT { delete offset; };

                evaluator->dataOffset() = std::visit(overloaded {
                    [this](std::string) -> u64 { LogConsole::abortEvaluation("placement offset cannot be a string", this); },
                    [this](PatternData * const &) -> u64 { LogConsole::abortEvaluation("placement offset cannot be a custom type", this); },
                    [](auto &&offset) -> u64 { return offset; }
                }, offset->getValue());
            }

            auto pattern = this->m_type->createPatterns(evaluator).front();
            pattern->setVariableName(this->m_name);

            applyVariableAttributes(evaluator, this, pattern);

            return { pattern };
        }

        FunctionResult execute(Evaluator *evaluator) const override {
            evaluator->createVariable(this->getName(), this->getType());

            return { };
        }

    private:
        std::string m_name;
        ASTNode *m_type;
        ASTNode *m_placementOffset;

        bool m_inVariable, m_outVariable;
    };

    class ASTNodeArrayVariableDecl : public ASTNode, public Attributable {
    public:
        ASTNodeArrayVariableDecl(std::string name, ASTNode *type, ASTNode *size, ASTNode *placementOffset = nullptr)
                : ASTNode(), m_name(std::move(name)), m_type(type), m_size(size), m_placementOffset(placementOffset) { }

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

        [[nodiscard]] std::vector<PatternData*> createPatterns(Evaluator *evaluator) const override {
            if (this->m_placementOffset != nullptr) {
                auto offset = dynamic_cast<ASTNodeLiteral*>(this->m_placementOffset->evaluate(evaluator));
                ON_SCOPE_EXIT { delete offset; };

                evaluator->dataOffset() = std::visit(overloaded {
                        [this](std::string) -> u64 { LogConsole::abortEvaluation("placement offset cannot be a string", this); },
                        [this](PatternData * const &) -> u64 { LogConsole::abortEvaluation("placement offset cannot be a custom type", this); },
                        [](auto &&offset) -> u64 { return offset; }
                }, offset->getValue());
            }

            auto type = this->m_type->evaluate(evaluator);
            ON_SCOPE_EXIT { delete type; };

            PatternData *pattern;
            if (dynamic_cast<ASTNodeBuiltinType*>(type))
                pattern = createStaticArray(evaluator);
            else if (auto attributable = dynamic_cast<Attributable*>(type)) {
                auto &attributes = attributable->getAttributes();

                bool isStaticType = std::any_of(attributes.begin(), attributes.end(), [](ASTNodeAttribute *attribute) {
                    return attribute->getAttribute() == "static" && !attribute->getValue().has_value();
                });

                if (isStaticType)
                    pattern = createStaticArray(evaluator);
                else
                    pattern = createDynamicArray(evaluator);
            } else {
                LogConsole::abortEvaluation("invalid type used in array", this);
            }

            applyVariableAttributes(evaluator, this, pattern);
            return { pattern };
        }

        [[nodiscard]] const std::string& getName() const { return this->m_name; }
        [[nodiscard]] constexpr ASTNode* getType() const { return this->m_type; }
        [[nodiscard]] constexpr ASTNode* getSize() const { return this->m_size; }
        [[nodiscard]] constexpr auto getPlacementOffset() const { return this->m_placementOffset; }

    private:
        std::string m_name;
        ASTNode *m_type;
        ASTNode *m_size;
        ASTNode *m_placementOffset;

        PatternData* createStaticArray(Evaluator *evaluator) const {
            u64 startOffset = evaluator->dataOffset();

            PatternData *templatePattern = this->m_type->createPatterns(evaluator).front();
            ON_SCOPE_EXIT { delete templatePattern; };

            evaluator->dataOffset() = startOffset;

            u128 entryCount = 0;

            if (this->m_size != nullptr) {
                auto sizeNode = this->m_size->evaluate(evaluator);
                ON_SCOPE_EXIT { delete sizeNode; };

                if (auto literal = dynamic_cast<ASTNodeLiteral*>(sizeNode)) {
                    entryCount = std::visit(overloaded {
                            [this](std::string) -> u128  { LogConsole::abortEvaluation("cannot use string to index array", this); },
                            [this](PatternData*) -> u128 { LogConsole::abortEvaluation("cannot use custom type to index array", this); },
                            [](auto &&size) -> u128 { return size; }
                    }, literal->getValue());
                } else if (auto whileStatement = dynamic_cast<ASTNodeWhileStatement*>(sizeNode)) {
                    while (whileStatement->evaluateCondition(evaluator)) {
                        entryCount++;
                        evaluator->dataOffset() += templatePattern->getSize();
                        evaluator->handleAbort();
                    }
                }
            } else {
                std::vector<u8> buffer(templatePattern->getSize());
                while (true) {
                    if (evaluator->dataOffset() >= evaluator->getProvider()->getActualSize() - buffer.size())
                        LogConsole::abortEvaluation("reached end of file before finding end of unsized array", this);

                    evaluator->getProvider()->read(evaluator->dataOffset(), buffer.data(), buffer.size());
                    evaluator->dataOffset() += buffer.size();

                    entryCount++;

                    bool reachedEnd = true;
                    for (u8 &byte : buffer) {
                        if (byte != 0x00) {
                            reachedEnd = false;
                            break;
                        }
                    }

                    if (reachedEnd) break;
                    evaluator->handleAbort();
                }
            }

            PatternData *outputPattern;
            if (dynamic_cast<PatternDataPadding*>(templatePattern)) {
                outputPattern = new PatternDataPadding(startOffset, 0, evaluator);
            } else if (dynamic_cast<PatternDataCharacter*>(templatePattern)) {
                outputPattern = new PatternDataString(startOffset, 0, evaluator);
            } else if (dynamic_cast<PatternDataCharacter16*>(templatePattern)) {
                outputPattern = new PatternDataString16(startOffset, 0, evaluator);
            } else {
                auto arrayPattern = new PatternDataStaticArray(startOffset, 0, evaluator);
                arrayPattern->setEntries(templatePattern->clone(), entryCount);
                outputPattern = arrayPattern;
            }

            outputPattern->setVariableName(this->m_name);
            outputPattern->setEndian(templatePattern->getEndian());
            outputPattern->setColor(templatePattern->getColor());
            outputPattern->setTypeName(templatePattern->getTypeName());
            outputPattern->setSize(templatePattern->getSize() * entryCount);

            evaluator->dataOffset() = startOffset + outputPattern->getSize();

            return outputPattern;
        }

        PatternData* createDynamicArray(Evaluator *evaluator) const {
            auto arrayPattern = new PatternDataDynamicArray(evaluator->dataOffset(), 0, evaluator);
            arrayPattern->setVariableName(this->m_name);

            std::vector<PatternData *> entries;
            auto arrayCleanup = SCOPE_GUARD {
                for (auto entry : entries)
                    delete entry;
            };

            size_t size = 0;
            u64 entryIndex = 0;

            auto addEntry = [&](PatternData *pattern) {
                pattern->setVariableName(hex::format("[{}]", entryIndex));
                pattern->setEndian(arrayPattern->getEndian());
                pattern->setColor(arrayPattern->getColor());
                entries.push_back(pattern);

                size += pattern->getSize();
                entryIndex++;

                evaluator->handleAbort();
            };

            auto discardEntry = [&] {
                delete entries.back();
                entries.pop_back();
                entryIndex--;

            };

            if (this->m_size != nullptr) {
                auto sizeNode = this->m_size->evaluate(evaluator);
                ON_SCOPE_EXIT { delete sizeNode; };

                if (auto literal = dynamic_cast<ASTNodeLiteral*>(sizeNode)) {
                    auto entryCount = std::visit(overloaded{
                            [this](std::string) -> u128  { LogConsole::abortEvaluation("cannot use string to index array", this); },
                            [this](PatternData*) -> u128 { LogConsole::abortEvaluation("cannot use custom type to index array", this); },
                            [](auto &&size) -> u128 { return size; }
                    }, literal->getValue());

                    auto limit = evaluator->getArrayLimit();
                    if (entryCount > limit)
                        LogConsole::abortEvaluation(hex::format("array grew past set limit of {}", limit), this);

                    for (u64 i = 0; i < entryCount; i++) {
                        auto patterns = this->m_type->createPatterns(evaluator);

                        if (!patterns.empty())
                            addEntry(patterns.front());

                        auto ctrlFlow = evaluator->getCurrentControlFlowStatement();
                        if (ctrlFlow == ControlFlowStatement::Break)
                            break;
                        else if (ctrlFlow == ControlFlowStatement::Continue) {
                            discardEntry();
                            continue;
                        }
                    }
                } else if (auto whileStatement = dynamic_cast<ASTNodeWhileStatement*>(sizeNode)) {
                    while (whileStatement->evaluateCondition(evaluator)) {
                        auto limit = evaluator->getArrayLimit();
                        if (entryIndex > limit)
                            LogConsole::abortEvaluation(hex::format("array grew past set limit of {}", limit), this);

                        auto patterns = this->m_type->createPatterns(evaluator);

                        if (!patterns.empty())
                            addEntry(patterns.front());

                        auto ctrlFlow = evaluator->getCurrentControlFlowStatement();
                        if (ctrlFlow == ControlFlowStatement::Break)
                            break;
                        else if (ctrlFlow == ControlFlowStatement::Continue) {
                            discardEntry();
                            continue;
                        }
                    }
                }
            } else {
                while (true) {
                    auto limit = evaluator->getArrayLimit();
                    if (entryIndex > limit)
                        LogConsole::abortEvaluation(hex::format("array grew past set limit of {}", limit), this);

                    auto patterns = this->m_type->createPatterns(evaluator);

                    if (!patterns.empty()) {
                        auto pattern = patterns.front();

                        std::vector<u8> buffer(pattern->getSize());

                        if (evaluator->dataOffset() >= evaluator->getProvider()->getActualSize() - buffer.size()) {
                            delete pattern;
                            LogConsole::abortEvaluation("reached end of file before finding end of unsized array", this);
                        }

                        addEntry(pattern);

                        auto ctrlFlow = evaluator->getCurrentControlFlowStatement();
                        if (ctrlFlow == ControlFlowStatement::Break)
                            break;
                        else if (ctrlFlow == ControlFlowStatement::Continue) {
                            discardEntry();
                            continue;
                        }

                        evaluator->getProvider()->read(evaluator->dataOffset() - pattern->getSize(), buffer.data(), buffer.size());
                        bool reachedEnd = true;
                        for (u8 &byte : buffer) {
                            if (byte != 0x00) {
                                reachedEnd = false;
                                break;
                            }
                        }

                        if (reachedEnd) break;
                    }
                }
            }

            arrayPattern->setEntries(entries);
            arrayPattern->setSize(size);

            if (auto &entries = arrayPattern->getEntries(); !entries.empty())
                arrayPattern->setTypeName(entries.front()->getTypeName());

            arrayCleanup.release();

            return arrayPattern;
        }
    };

    class ASTNodePointerVariableDecl : public ASTNode, public Attributable {
    public:
        ASTNodePointerVariableDecl(std::string name, ASTNode *type, ASTNode *sizeType, ASTNode *placementOffset = nullptr)
                : ASTNode(), m_name(std::move(name)), m_type(type), m_sizeType(sizeType), m_placementOffset(placementOffset) { }

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

        [[nodiscard]] const std::string& getName() const { return this->m_name; }
        [[nodiscard]] constexpr ASTNode* getType() const { return this->m_type; }
        [[nodiscard]] constexpr ASTNode* getSizeType() const { return this->m_sizeType; }
        [[nodiscard]] constexpr auto getPlacementOffset() const { return this->m_placementOffset; }

        [[nodiscard]] std::vector<PatternData*> createPatterns(Evaluator *evaluator) const override {
            if (this->m_placementOffset != nullptr) {
                auto offset = dynamic_cast<ASTNodeLiteral *>(this->m_placementOffset->evaluate(evaluator));
                ON_SCOPE_EXIT { delete offset; };

                evaluator->dataOffset() = std::visit(overloaded {
                        [this](std::string) -> u64 { LogConsole::abortEvaluation("placement offset cannot be a string", this); },
                        [this](PatternData*) -> u64 { LogConsole::abortEvaluation("placement offset cannot be a custom type", this); },
                        [](auto &&offset) -> u64   { return u64(offset); }
                }, offset->getValue());
            }

            auto startOffset = evaluator->dataOffset();

            auto sizePattern = this->m_sizeType->createPatterns(evaluator).front();
            ON_SCOPE_EXIT { delete sizePattern; };

            auto pattern = new PatternDataPointer(startOffset, sizePattern->getSize(), evaluator);
            pattern->setVariableName(this->m_name);

            auto endOffset = evaluator->dataOffset();

            {
                u128 pointerAddress = 0;
                evaluator->getProvider()->read(pattern->getOffset(), &pointerAddress, pattern->getSize());
                pointerAddress = hex::changeEndianess(pointerAddress, sizePattern->getSize(), sizePattern->getEndian());

                evaluator->dataOffset() = startOffset;

                pattern->setPointedAtAddress(pointerAddress);
                applyVariableAttributes(evaluator, this, pattern);

                evaluator->dataOffset() = pattern->getPointedAtAddress();

                auto pointedAtPattern = this->m_type->createPatterns(evaluator).front();

                pattern->setPointedAtPattern(pointedAtPattern);
                pattern->setEndian(sizePattern->getEndian());
            }

            evaluator->dataOffset() = endOffset;

            return { pattern };
        }

    private:
        std::string m_name;
        ASTNode *m_type;
        ASTNode *m_sizeType;
        ASTNode *m_placementOffset;
    };

    class ASTNodeMultiVariableDecl : public ASTNode {
    public:
        explicit ASTNodeMultiVariableDecl(std::vector<ASTNode*> variables) : m_variables(std::move(variables)) { }

        ASTNodeMultiVariableDecl(const ASTNodeMultiVariableDecl &other) : ASTNode(other) {
            for (auto &variable : other.m_variables)
                this->m_variables.push_back(variable->clone());
        }

        ~ASTNodeMultiVariableDecl() override {
            for (auto &variable : this->m_variables)
                delete variable;
        }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeMultiVariableDecl(*this);
        }

        [[nodiscard]] std::vector<ASTNode*> getVariables() {
            return this->m_variables;
        }

        [[nodiscard]] std::vector<PatternData*> createPatterns(Evaluator *evaluator) const override {
            std::vector<PatternData*> patterns;

            for (auto &node : this->m_variables) {
                auto newPatterns = node->createPatterns(evaluator);
                patterns.insert(patterns.end(), newPatterns.begin(), newPatterns.end());
            }

            return patterns;
        }

        FunctionResult execute(Evaluator *evaluator) const override {
            for (auto &variable : this->m_variables) {
                auto variableDecl = dynamic_cast<ASTNodeVariableDecl*>(variable);

                evaluator->createVariable(variableDecl->getName(), variableDecl->getType()->evaluate(evaluator));
            }

            return { };
        }

    private:
        std::vector<ASTNode*> m_variables;
    };

    class ASTNodeStruct : public ASTNode, public Attributable {
    public:
        ASTNodeStruct() : ASTNode() { }

        ASTNodeStruct(const ASTNodeStruct &other) : ASTNode(other), Attributable(other) {
            for (const auto &otherMember : other.getMembers())
                this->m_members.push_back(otherMember->clone());
            for (const auto &otherInheritance : other.getInheritance())
                this->m_inheritance.push_back(otherInheritance->clone());
        }

        ~ASTNodeStruct() override {
            for (auto &member : this->m_members)
                delete member;
            for (auto &inheritance : this->m_inheritance)
                delete inheritance;
        }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeStruct(*this);
        }

        [[nodiscard]] std::vector<PatternData*> createPatterns(Evaluator *evaluator) const override {
            auto pattern = new PatternDataStruct(evaluator->dataOffset(), 0, evaluator);

            u64 startOffset = evaluator->dataOffset();
            std::vector<PatternData*> memberPatterns;
            auto structCleanup = SCOPE_GUARD {
                delete pattern;
                for (auto member : memberPatterns)
                    delete member;
            };

            evaluator->pushScope(pattern, memberPatterns);

            for (auto inheritance : this->m_inheritance) {
                auto inheritancePatterns = inheritance->createPatterns(evaluator).front();
                ON_SCOPE_EXIT {
                    delete inheritancePatterns;
                };

                if (auto structPattern = dynamic_cast<PatternDataStruct*>(inheritancePatterns)) {
                    for (auto member : structPattern->getMembers()) {
                        memberPatterns.push_back(member->clone());
                    }
                }
            }

            for (auto member : this->m_members) {
                for (auto &memberPattern : member->createPatterns(evaluator)) {
                    memberPatterns.push_back(memberPattern);
                }
            }

            evaluator->popScope();

            pattern->setMembers(memberPatterns);
            pattern->setSize(evaluator->dataOffset() - startOffset);

            structCleanup.release();

            return { pattern };
        }

        [[nodiscard]] const std::vector<ASTNode*>& getMembers() const { return this->m_members; }
        void addMember(ASTNode *node) { this->m_members.push_back(node); }

        [[nodiscard]] const std::vector<ASTNode*>& getInheritance() const { return this->m_inheritance; }
        void addInheritance(ASTNode *node) { this->m_inheritance.push_back(node); }

    private:
        std::vector<ASTNode*> m_members;
        std::vector<ASTNode*> m_inheritance;
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

        [[nodiscard]] std::vector<PatternData*> createPatterns(Evaluator *evaluator) const override {
            auto pattern = new PatternDataUnion(evaluator->dataOffset(), 0, evaluator);

            size_t size = 0;
            std::vector<PatternData*> memberPatterns;
            u64 startOffset = evaluator->dataOffset();

            auto unionCleanup = SCOPE_GUARD {
                delete pattern;
                for (auto member : memberPatterns)
                    delete member;
            };

            evaluator->pushScope(pattern, memberPatterns);
            for (auto member : this->m_members) {
                for (auto &memberPattern : member->createPatterns(evaluator)) {
                    memberPattern->setOffset(startOffset);
                    memberPatterns.push_back(memberPattern);
                    size = std::max(memberPattern->getSize(), size);
                }
            }
            evaluator->popScope();

            evaluator->dataOffset() = startOffset + size;
            pattern->setMembers(memberPatterns);
            pattern->setSize(size);

            unionCleanup.release();

            return { pattern };
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
                this->m_entries.emplace(name, entry->clone());
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

        [[nodiscard]] std::vector<PatternData*> createPatterns(Evaluator *evaluator) const override {
            auto pattern = new PatternDataEnum(evaluator->dataOffset(), 0, evaluator);
            auto enumCleanup = SCOPE_GUARD { delete pattern; };


            std::vector<std::pair<Token::Literal, std::string>> enumEntries;
            for (const auto &[name, value] : this->m_entries) {
                auto literal = dynamic_cast<ASTNodeLiteral*>(value->evaluate(evaluator));
                ON_SCOPE_EXIT { delete literal; };

                enumEntries.emplace_back(literal->getValue(), name);
            }

            pattern->setEnumValues(enumEntries);

            auto underlying = this->m_underlyingType->createPatterns(evaluator).front();
            ON_SCOPE_EXIT { delete underlying; };
            pattern->setSize(underlying->getSize());
            pattern->setEndian(underlying->getEndian());

            enumCleanup.release();

            return { pattern };
        }

        [[nodiscard]] const std::map<std::string, ASTNode*>& getEntries() const { return this->m_entries; }
        void addEntry(const std::string &name, ASTNode* expression) { this->m_entries.insert({ name, expression }); }

        [[nodiscard]] ASTNode *getUnderlyingType() { return this->m_underlyingType; }

    private:
        std::map<std::string, ASTNode*> m_entries;
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

        [[nodiscard]] std::vector<PatternData*> createPatterns(Evaluator *evaluator) const override {
            auto pattern = new PatternDataBitfield(evaluator->dataOffset(), 0, evaluator);

            size_t bitOffset = 0;
            std::vector<PatternData*> fields;

            auto bitfieldCleanup = SCOPE_GUARD {
                delete pattern;
                for (auto field : fields)
                    delete field;
            };

            evaluator->pushScope(pattern, fields);
            for (auto [name, bitSizeNode] : this->m_entries) {
                auto literal = bitSizeNode->evaluate(evaluator);
                ON_SCOPE_EXIT { delete literal; };

                u8 bitSize = std::visit(overloaded {
                        [this](std::string) -> u8 { LogConsole::abortEvaluation("bitfield field size cannot be a string", this); },
                        [this](PatternData*) -> u8 { LogConsole::abortEvaluation("bitfield field size cannot be a custom type", this); },
                        [](auto &&offset) -> u8 { return static_cast<u8>(offset); }
                }, dynamic_cast<ASTNodeLiteral*>(literal)->getValue());

                // If a field is named padding, it was created through a padding expression and only advances the bit position
                if (name != "padding") {
                    auto field = new PatternDataBitfieldField(evaluator->dataOffset(), bitOffset, bitSize, evaluator);
                    field->setVariableName(name);
                    fields.push_back(field);
                }

                bitOffset += bitSize;
            }
            evaluator->popScope();

            pattern->setSize((bitOffset + 7) / 8);
            pattern->setFields(fields);

            evaluator->dataOffset() += pattern->getSize();

            bitfieldCleanup.release();

            return { pattern };
        }

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

        [[nodiscard]]
        const Path& getPath() const {
            return this->m_path;
        }

        [[nodiscard]] ASTNode* evaluate(Evaluator *evaluator) const override {
            if (this->getPath().size() == 1) {
                if (auto name = std::get_if<std::string>(&this->getPath().front()); name != nullptr) {
                    if (*name == "$") return new ASTNodeLiteral(u128(evaluator->dataOffset()));
                }
            }

            auto pattern = this->createPatterns(evaluator).front();
            ON_SCOPE_EXIT { delete pattern; };

            Token::Literal literal;
            if (dynamic_cast<PatternDataUnsigned*>(pattern) || dynamic_cast<PatternDataEnum*>(pattern)) {
                u128 value = 0;
                readVariable(evaluator, value, pattern);
                literal = value;
            } else if (dynamic_cast<PatternDataSigned*>(pattern)) {
                s128 value = 0;
                readVariable(evaluator, value, pattern);
                value = hex::signExtend(pattern->getSize() * 8, value);
                literal = value;
            } else if (dynamic_cast<PatternDataFloat*>(pattern)) {
                if (pattern->getSize() == sizeof(u16)) {
                    u16 value = 0;
                    readVariable(evaluator, value, pattern);
                    literal = double(float16ToFloat32(value));
                } else if (pattern->getSize() == sizeof(float)) {
                    float value = 0;
                    readVariable(evaluator, value, pattern);
                    literal = double(value);
                } else if (pattern->getSize() == sizeof(double)) {
                    double value = 0;
                    readVariable(evaluator, value, pattern);
                    literal = value;
                } else LogConsole::abortEvaluation("invalid floating point type access", this);
            } else if (dynamic_cast<PatternDataCharacter*>(pattern)) {
                char value = 0;
                readVariable(evaluator, value, pattern);
                literal = value;
            } else if (dynamic_cast<PatternDataBoolean*>(pattern)) {
                bool value = false;
                readVariable(evaluator, value, pattern);
                literal = value;
            } else if (dynamic_cast<PatternDataString*>(pattern)) {
                std::string value;

                if (pattern->isLocal()) {
                    auto &literal = evaluator->getStack()[pattern->getOffset()];

                    std::visit(overloaded {
                            [&](char assignmentValue) { if (assignmentValue != 0x00) value = std::string({ assignmentValue }); },
                            [&](std::string assignmentValue) { value = assignmentValue; },
                            [&, this](PatternData * const &assignmentValue) {
                                if (!dynamic_cast<PatternDataString*>(assignmentValue) && !dynamic_cast<PatternDataCharacter*>(assignmentValue))
                                    LogConsole::abortEvaluation(hex::format("cannot assign '{}' to string", pattern->getTypeName()), this);

                                readVariable(evaluator, value, assignmentValue);
                            },
                            [&, this](auto &&assignmentValue) { LogConsole::abortEvaluation(hex::format("cannot assign '{}' to string", pattern->getTypeName()), this); }
                    }, literal);
                }
                else {
                    value.resize(pattern->getSize());
                    evaluator->getProvider()->read(pattern->getOffset(), value.data(), value.size());
                    value.erase(std::find(value.begin(), value.end(), '\0'), value.end());
                }

                literal = value;
            } else if (auto bitfieldFieldPattern = dynamic_cast<PatternDataBitfieldField*>(pattern)) {
                u64 value = 0;
                readVariable(evaluator, value, pattern);
                literal = u128(hex::extract(bitfieldFieldPattern->getBitOffset() + (bitfieldFieldPattern->getBitSize() - 1), bitfieldFieldPattern->getBitOffset(), value));
            } else {
                literal = pattern->clone();
            }

            if (auto transformFunc = pattern->getTransformFunction(); transformFunc.has_value() && pattern->getEvaluator() != nullptr) {
                auto result = transformFunc->func(evaluator, { literal });

                if (!result.has_value())
                    LogConsole::abortEvaluation("transform function did not return a value", this);
                literal = result.value();
            }

            return new ASTNodeLiteral(literal);
        }

        [[nodiscard]] std::vector<PatternData*> createPatterns(Evaluator *evaluator) const override {
            std::vector<PatternData*> searchScope;
            PatternData *currPattern = nullptr;
            s32 scopeIndex = 0;

            if (!evaluator->isGlobalScope()){
                auto globalScope = evaluator->getGlobalScope().scope;
                std::copy(globalScope->begin(), globalScope->end(), std::back_inserter(searchScope));
            }

            {
                auto currScope = evaluator->getScope(scopeIndex).scope;
                std::copy(currScope->begin(), currScope->end(), std::back_inserter(searchScope));
            }

            for (const auto &part : this->getPath()) {

                if (part.index() == 0) {
                    // Variable access
                    auto name = std::get<std::string>(part);

                    if (name == "parent") {
                        scopeIndex--;

                        if (-scopeIndex >= evaluator->getScopeCount())
                            LogConsole::abortEvaluation("cannot access parent of global scope", this);

                        searchScope = *evaluator->getScope(scopeIndex).scope;
                        auto currParent = evaluator->getScope(scopeIndex).parent;

                        if (currParent == nullptr) {
                            currPattern = nullptr;
                        } else {
                            currPattern = currParent->clone();
                        }

                        continue;
                    } else if (name == "this") {
                        searchScope = *evaluator->getScope(scopeIndex).scope;

                        auto currParent = evaluator->getScope(0).parent;

                        if (currParent == nullptr)
                            LogConsole::abortEvaluation("invalid use of 'this' outside of struct-like type", this);

                        currPattern = currParent->clone();
                        continue;
                    } else {
                        bool found = false;
                        for (auto iter = searchScope.crbegin(); iter != searchScope.crend(); ++iter) {
                            if ((*iter)->getVariableName() == name) {
                                auto newPattern = (*iter)->clone();
                                delete currPattern;
                                currPattern = newPattern;
                                found = true;
                                break;
                            }
                        }

                        if (name == "$")
                            LogConsole::abortEvaluation("invalid use of placeholder operator in rvalue");

                        if (!found)
                            LogConsole::abortEvaluation(hex::format("no variable named '{}' found", name), this);
                    }
                } else {
                    // Array indexing
                    auto index = dynamic_cast<ASTNodeLiteral*>(std::get<ASTNode*>(part)->evaluate(evaluator));
                    ON_SCOPE_EXIT { delete index; };

                    std::visit(overloaded {
                        [](std::string) { throw std::string("cannot use string to index array"); },
                        [](PatternData * const &) { throw std::string("cannot use custom type to index array"); },
                        [&, this](auto &&index) {
                            if (auto dynamicArrayPattern = dynamic_cast<PatternDataDynamicArray*>(currPattern)) {
                                if (index >= searchScope.size() || index < 0)
                                    LogConsole::abortEvaluation("array index out of bounds", this);

                                auto newPattern = searchScope[index]->clone();
                                delete currPattern;
                                currPattern = newPattern;
                            }
                            else if (auto staticArrayPattern = dynamic_cast<PatternDataStaticArray*>(currPattern)) {
                                if (index >= staticArrayPattern->getEntryCount() || index < 0)
                                    LogConsole::abortEvaluation("array index out of bounds", this);

                                auto newPattern = searchScope.front()->clone();
                                newPattern->setOffset(staticArrayPattern->getOffset() + index * staticArrayPattern->getTemplate()->getSize());
                                delete currPattern;
                                currPattern = newPattern;
                            }
                        }
                    }, index->getValue());
                }

                if (currPattern == nullptr)
                    break;

                if (auto pointerPattern = dynamic_cast<PatternDataPointer*>(currPattern)) {
                    auto newPattern = pointerPattern->getPointedAtPattern()->clone();
                    delete currPattern;
                    currPattern = newPattern;
                }

                PatternData *indexPattern;
                if (currPattern->isLocal()) {
                    auto stackLiteral = evaluator->getStack()[currPattern->getOffset()];
                    if (auto stackPattern = std::get_if<PatternData*>(&stackLiteral); stackPattern != nullptr)
                        indexPattern = *stackPattern;
                    else
                        return { currPattern };
                }
                else
                    indexPattern = currPattern;

                if (auto structPattern = dynamic_cast<PatternDataStruct*>(indexPattern))
                    searchScope = structPattern->getMembers();
                else if (auto unionPattern = dynamic_cast<PatternDataUnion*>(indexPattern))
                    searchScope = unionPattern->getMembers();
                else if (auto bitfieldPattern = dynamic_cast<PatternDataBitfield*>(indexPattern))
                    searchScope = bitfieldPattern->getFields();
                else if (auto dynamicArrayPattern = dynamic_cast<PatternDataDynamicArray*>(indexPattern))
                    searchScope = dynamicArrayPattern->getEntries();
                else if (auto staticArrayPattern = dynamic_cast<PatternDataStaticArray*>(indexPattern))
                    searchScope = { staticArrayPattern->getTemplate() };

            }

            if (currPattern == nullptr)
                LogConsole::abortEvaluation("cannot reference global scope", this);

            return { currPattern };
        }

    private:
        Path m_path;

        void readVariable(Evaluator *evaluator, auto &value, PatternData *variablePattern) const {
            constexpr bool isString = std::same_as<std::remove_cvref_t<decltype(value)>, std::string>;

            if (variablePattern->isLocal()) {
                auto &literal = evaluator->getStack()[variablePattern->getOffset()];

                std::visit(overloaded {
                        [&](std::string &assignmentValue) {
                            if constexpr (isString) value = assignmentValue;
                        },
                        [&](PatternData *assignmentValue) { readVariable(evaluator, value, assignmentValue); },
                        [&](auto &&assignmentValue) { value = assignmentValue; }
                }, literal);
            }
            else {
                if constexpr (isString) {
                    value.resize(variablePattern->getSize());
                    evaluator->getProvider()->read(variablePattern->getOffset(), value.data(), value.size());
                    value.erase(std::find(value.begin(), value.end(), '\0'), value.end());
                } else {
                    evaluator->getProvider()->read(variablePattern->getOffset(), &value, variablePattern->getSize());
                }
            }

            if constexpr (!isString)
                value = hex::changeEndianess(value, variablePattern->getSize(), variablePattern->getEndian());
        }
    };

    class ASTNodeScopeResolution : public ASTNode {
    public:
        explicit ASTNodeScopeResolution(ASTNode *type, std::string name) : ASTNode(), m_type(type), m_name(std::move(name)) { }

        ASTNodeScopeResolution(const ASTNodeScopeResolution &other) {
            this->m_type = other.m_type->clone();
            this->m_name = other.m_name;
        }

        ~ASTNodeScopeResolution() override {
            delete this->m_type;
        }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeScopeResolution(*this);
        }

        [[nodiscard]] ASTNode* evaluate(Evaluator *evaluator) const override {
            auto type = this->m_type->evaluate(evaluator);
            ON_SCOPE_EXIT { delete type; };

            if (auto enumType = dynamic_cast<ASTNodeEnum*>(type)) {
                for (auto &[name, value] : enumType->getEntries()) {
                    if (name == this->m_name)
                        return value->evaluate(evaluator);
                }
            } else {
                LogConsole::abortEvaluation("invalid scope resolution. Cannot access this type");
            }

            LogConsole::abortEvaluation(hex::format("could not find constant '{}'", this->m_name), this);
        }

    private:
        ASTNode *m_type;
        std::string m_name;
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

        [[nodiscard]] std::vector<PatternData*> createPatterns(Evaluator *evaluator) const override {
            auto &scope = *evaluator->getScope(0).scope;
            auto &body = evaluateCondition(evaluator) ? this->m_trueBody : this->m_falseBody;

            for (auto &node : body) {
                auto newPatterns = node->createPatterns(evaluator);
                for (auto &pattern : newPatterns)
                    scope.push_back(pattern->clone());
            }

            return { };
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

        FunctionResult execute(Evaluator *evaluator) const override {
            auto &body = evaluateCondition(evaluator) ? this->m_trueBody : this->m_falseBody;

            auto variables = *evaluator->getScope(0).scope;
            u32 startVariableCount = variables.size();
            ON_SCOPE_EXIT {
                s64 stackSize = evaluator->getStack().size();
                for (u32 i = startVariableCount; i < variables.size(); i++) {
                    stackSize--;
                    delete variables[i];
                }
                if (stackSize < 0) LogConsole::abortEvaluation("stack pointer underflow!", this);
                    evaluator->getStack().resize(stackSize);
            };

            evaluator->pushScope(nullptr, variables);
            ON_SCOPE_EXIT { evaluator->popScope(); };
            for (auto &statement : body) {
                auto result = statement->execute(evaluator);
                if (auto ctrlStatement = evaluator->getCurrentControlFlowStatement(); ctrlStatement != ControlFlowStatement::None) {
                    return result;
                }
            }

            return { };
        }

    private:
        [[nodiscard]]
        bool evaluateCondition(Evaluator *evaluator) const {
            auto literal = dynamic_cast<ASTNodeLiteral*>(this->m_condition->evaluate(evaluator));
            ON_SCOPE_EXIT { delete literal; };

            return std::visit(overloaded {
                [](std::string value) -> bool { return !value.empty(); },
                [this](PatternData * const &) -> bool { LogConsole::abortEvaluation("cannot cast custom type to bool", this); },
                [](auto &&value) -> bool { return value != 0; }
                }, literal->getValue());
        }

        ASTNode *m_condition;
        std::vector<ASTNode*> m_trueBody, m_falseBody;
    };

    class ASTNodeFunctionCall : public ASTNode {
    public:
        explicit ASTNodeFunctionCall(std::string functionName, std::vector<ASTNode*> params)
                : ASTNode(), m_functionName(std::move(functionName)), m_params(std::move(params)) { }

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

        [[nodiscard]] const std::string& getFunctionName() {
            return this->m_functionName;
        }

        [[nodiscard]] const std::vector<ASTNode*>& getParams() const {
            return this->m_params;
        }

        [[nodiscard]] std::vector<PatternData*> createPatterns(Evaluator *evaluator) const override {

            this->execute(evaluator);

            return { };
        }

        [[nodiscard]] ASTNode* evaluate(Evaluator *evaluator) const override {
            std::vector<Token::Literal> evaluatedParams;
            for (auto param : this->m_params) {
                auto expression = param->evaluate(evaluator);
                ON_SCOPE_EXIT { delete expression; };

                auto literal = dynamic_cast<ASTNodeLiteral*>(expression->evaluate(evaluator));
                ON_SCOPE_EXIT { delete literal; };

                evaluatedParams.push_back(literal->getValue());
            }

            auto &customFunctions = evaluator->getCustomFunctions();
            auto functions = ContentRegistry::PatternLanguage::getFunctions();

            for (auto &func : customFunctions)
                functions.insert(func);

            if (!functions.contains(this->m_functionName))
                LogConsole::abortEvaluation(hex::format("call to unknown function '{}'", this->m_functionName), this);

            auto function = functions[this->m_functionName];
            if (function.parameterCount == ContentRegistry::PatternLanguage::UnlimitedParameters) {
                ; // Don't check parameter count
            }
            else if (function.parameterCount & ContentRegistry::PatternLanguage::LessParametersThan) {
                if (evaluatedParams.size() >= (function.parameterCount & ~ContentRegistry::PatternLanguage::LessParametersThan))
                    LogConsole::abortEvaluation(hex::format("too many parameters for function '{0}'. Expected {1}", this->m_functionName, function.parameterCount & ~ContentRegistry::PatternLanguage::LessParametersThan), this);
            } else if (function.parameterCount & ContentRegistry::PatternLanguage::MoreParametersThan) {
                if (evaluatedParams.size() <= (function.parameterCount & ~ContentRegistry::PatternLanguage::MoreParametersThan))
                    LogConsole::abortEvaluation(hex::format("too few parameters for function '{0}'. Expected {1}", this->m_functionName, function.parameterCount & ~ContentRegistry::PatternLanguage::MoreParametersThan), this);
            } else if (function.parameterCount != evaluatedParams.size()) {
                LogConsole::abortEvaluation(hex::format("invalid number of parameters for function '{0}'. Expected {1}", this->m_functionName, function.parameterCount), this);
            }

            try {
                auto &function = functions[this->m_functionName];

                if (function.dangerous && evaluator->getDangerousFunctionPermission() != DangerousFunctionPermission::Allow) {
                    evaluator->dangerousFunctionCalled();

                    while (evaluator->getDangerousFunctionPermission() == DangerousFunctionPermission::Ask) {
                        using namespace std::literals::chrono_literals;

                        std::this_thread::sleep_for(100ms);
                    }

                    if (evaluator->getDangerousFunctionPermission() == DangerousFunctionPermission::Deny) {
                        LogConsole::abortEvaluation(hex::format("calling of dangerous function '{}' is not allowed", this->m_functionName), this);
                    }
                }

                auto result = function.func(evaluator, evaluatedParams);

                if (result.has_value())
                    return new ASTNodeLiteral(result.value());
                else
                    return new ASTNodeMathematicalExpression(nullptr, nullptr, Token::Operator::Plus);
            } catch (std::string &error) {
                LogConsole::abortEvaluation(error, this);
            }

            return nullptr;
        }

        FunctionResult execute(Evaluator *evaluator) const override {
            delete this->evaluate(evaluator);

            return { };
        }

    private:
        std::string m_functionName;
        std::vector<ASTNode*> m_params;
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

        [[nodiscard]]
        Token::Operator getOperator() const {
            return this->m_op;
        }

        [[nodiscard]]
        ASTNode* getExpression() const {
            return this->m_expression;
        }

        [[nodiscard]]
        ASTNode* evaluate(Evaluator *evaluator) const override {
            auto pattern = this->m_expression->createPatterns(evaluator).front();
            ON_SCOPE_EXIT { delete pattern; };

            switch (this->getOperator()) {
                case Token::Operator::AddressOf:
                    return new ASTNodeLiteral(u128(pattern->getOffset()));
                case Token::Operator::SizeOf:
                    return new ASTNodeLiteral(u128(pattern->getSize()));
                default:
                    LogConsole::abortEvaluation("invalid type operator", this);
            }
        }


    private:
        Token::Operator m_op;
        ASTNode *m_expression;
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

        [[nodiscard]] const std::string& getLValueName() const {
            return this->m_lvalueName;
        }

        [[nodiscard]] ASTNode* getRValue() const {
            return this->m_rvalue;
        }

        FunctionResult execute(Evaluator *evaluator) const override {
            auto literal = dynamic_cast<ASTNodeLiteral*>(this->getRValue()->evaluate(evaluator));
            ON_SCOPE_EXIT { delete literal; };

            evaluator->setVariable(this->getLValueName(), literal->getValue());

            return { };
        }

    private:
        std::string m_lvalueName;
        ASTNode *m_rvalue;
    };

    class ASTNodeControlFlowStatement : public ASTNode {
    public:
        explicit ASTNodeControlFlowStatement(ControlFlowStatement type, ASTNode *rvalue) : m_type(type), m_rvalue(rvalue) {

        }

        ASTNodeControlFlowStatement(const ASTNodeControlFlowStatement &other) : ASTNode(other) {
            this->m_type = other.m_type;

            if (other.m_rvalue != nullptr)
                this->m_rvalue = other.m_rvalue->clone();
            else
                this->m_rvalue = nullptr;
        }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeControlFlowStatement(*this);
        }

        ~ASTNodeControlFlowStatement() override {
            delete this->m_rvalue;
        }

        [[nodiscard]] ASTNode* getReturnValue() const {
            return this->m_rvalue;
        }

        [[nodiscard]] std::vector<PatternData*> createPatterns(Evaluator *evaluator) const override {

            this->execute(evaluator);

            return { };
        }

        FunctionResult execute(Evaluator *evaluator) const override {
            auto returnValue = this->getReturnValue();

            evaluator->setCurrentControlFlowStatement(this->m_type);

            if (returnValue == nullptr)
                return std::nullopt;
            else {
                auto literal = dynamic_cast<ASTNodeLiteral*>(returnValue->evaluate(evaluator));
                ON_SCOPE_EXIT { delete literal; };

                return literal->getValue();
            }
        }

    private:
        ControlFlowStatement m_type;
        ASTNode *m_rvalue;
    };

    class ASTNodeFunctionDefinition : public ASTNode {
    public:
        ASTNodeFunctionDefinition(std::string name, std::vector<std::pair<std::string, ASTNode*>> params, std::vector<ASTNode*> body)
            : m_name(std::move(name)), m_params(std::move(params)), m_body(std::move(body)) {

        }

        ASTNodeFunctionDefinition(const ASTNodeFunctionDefinition &other) : ASTNode(other) {
            this->m_name = other.m_name;
            this->m_params = other.m_params;

            for (const auto &[name, type] : other.m_params) {
                this->m_params.emplace_back(name, type->clone());
            }

            for (auto statement : other.m_body) {
                this->m_body.push_back(statement->clone());
            }
        }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeFunctionDefinition(*this);
        }

        ~ASTNodeFunctionDefinition() override {
            for (auto &[name, type] : this->m_params)
                delete type;
            for (auto statement : this->m_body)
                delete statement;
        }

        [[nodiscard]] const std::string& getName() const {
            return this->m_name;
        }

        [[nodiscard]] const auto& getParams() const {
            return this->m_params;
        }

        [[nodiscard]] const auto& getBody() const {
            return this->m_body;
        }

        [[nodiscard]] ASTNode* evaluate(Evaluator *evaluator) const override {

            evaluator->addCustomFunction(this->m_name, this->m_params.size(), [this](Evaluator *ctx, const std::vector<Token::Literal>& params) -> std::optional<Token::Literal> {
                std::vector<PatternData*> variables;

                ctx->pushScope(nullptr, variables);
                ON_SCOPE_EXIT {
                    for (auto variable : variables)
                        delete variable;

                    ctx->popScope();
                };

                u32 paramIndex = 0;
                for (const auto &[name, type] : this->m_params) {
                    ctx->createVariable(name, type, params[paramIndex]);
                    ctx->setVariable(name, params[paramIndex]);

                    paramIndex++;
                }

                for (auto statement : this->m_body) {
                    auto result = statement->execute(ctx);

                    if (ctx->getCurrentControlFlowStatement() != ControlFlowStatement::None) {
                        switch (ctx->getCurrentControlFlowStatement()) {
                            case ControlFlowStatement::Break:
                                ctx->getConsole().abortEvaluation("break statement not within a loop", statement);
                            case ControlFlowStatement::Continue:
                                ctx->getConsole().abortEvaluation("continue statement not within a loop", statement);
                            default: break;
                        }

                        ctx->setCurrentControlFlowStatement(ControlFlowStatement::None);
                        return result;
                    }
                }

                return { };
            });

            return nullptr;
        }


    private:
        std::string m_name;
        std::vector<std::pair<std::string, ASTNode*>> m_params;
        std::vector<ASTNode*> m_body;
    };

    class ASTNodeCompoundStatement : public ASTNode {
    public:

        ASTNodeCompoundStatement(std::vector<ASTNode*> statements, bool newScope = false) : m_statements(std::move(statements)), m_newScope(newScope) {

        }

        ASTNodeCompoundStatement(const ASTNodeCompoundStatement &other) : ASTNode(other) {
            for (const auto &statement : other.m_statements) {
                this->m_statements.push_back(statement->clone());
            }
        }

        [[nodiscard]] ASTNode* clone() const override {
            return new ASTNodeCompoundStatement(*this);
        }

        ~ASTNodeCompoundStatement() override {
            for (const auto &statement : this->m_statements) {
                delete statement;
            }
        }

        [[nodiscard]] ASTNode* evaluate(Evaluator *evaluator) const override {
            ASTNode *result = nullptr;

            for (const auto &statement : this->m_statements) {
                delete result;
                result = statement->evaluate(evaluator);
            }

            return result;
        }

        [[nodiscard]] std::vector<PatternData*> createPatterns(Evaluator *evaluator) const override {
            std::vector<PatternData*> result;

            for (const auto &statement : this->m_statements) {
                auto patterns = statement->createPatterns(evaluator);
                std::copy(patterns.begin(), patterns.end(), std::back_inserter(result));
            }

            return result;
        }

        FunctionResult execute(Evaluator *evaluator) const override {
            FunctionResult result;

            auto variables = *evaluator->getScope(0).scope;
            u32 startVariableCount = variables.size();

            if (this->m_newScope) {
                evaluator->pushScope(nullptr, variables);
            }

            for (const auto &statement : this->m_statements) {
                result = statement->execute(evaluator);
                if (evaluator->getCurrentControlFlowStatement() != ControlFlowStatement::None)
                    return result;
            }

            if (this->m_newScope) {
                s64 stackSize = evaluator->getStack().size();
                for (u32 i = startVariableCount; i < variables.size(); i++) {
                    stackSize--;
                    delete variables[i];
                }
                if (stackSize < 0) LogConsole::abortEvaluation("stack pointer underflow!", this);
                evaluator->getStack().resize(stackSize);

                evaluator->popScope();
            }

            return result;
        }

    public:
        std::vector<ASTNode*> m_statements;
        bool m_newScope;
    };

};