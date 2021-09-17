#pragma once

#include <hex/pattern_language/token.hpp>
#include <hex/pattern_language/evaluator.hpp>
#include <hex/pattern_language/pattern_data.hpp>

#include <bit>
#include <optional>
#include <map>
#include <variant>
#include <vector>

#include <hex/pattern_language/ast_node_base.hpp>

namespace hex::pl {

    class PatternData;

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
                [this](std::string left, auto right) -> ASTNode* {
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
                [this](auto left, std::string right) -> ASTNode* { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
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
                        case Token::Operator::BoolAnd:
                            return new ASTNodeLiteral(left && right);
                        case Token::Operator::BoolXor:
                            return new ASTNodeLiteral(left && !right || !left && right);
                        case Token::Operator::BoolOr:
                            return new ASTNodeLiteral(left || right);
                        case Token::Operator::BoolNot:
                            return new ASTNodeLiteral(!right);
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

            return std::visit(overloaded {
                [this](std::string first, auto &&second, auto &&third) -> ASTNode * { LogConsole::abortEvaluation("invalid ternary operation", this); },
                [this](std::string first, std::string second, auto &&third) -> ASTNode * { LogConsole::abortEvaluation("invalid ternary operation", this); },
                [this](std::string first, auto &&second, std::string third) -> ASTNode * { LogConsole::abortEvaluation("invalid ternary operation", this); },
                [this](std::string first, std::string second, std::string third) -> ASTNode * { LogConsole::abortEvaluation("invalid ternary operation", this); },
                [this](auto &&first, std::string second, auto &&third) -> ASTNode * { LogConsole::abortEvaluation("invalid ternary operation", this); },
                [this](auto &&first, auto &&second, std::string third) -> ASTNode * { LogConsole::abortEvaluation("invalid ternary operation", this); },
                [this](auto &&first, std::string second, std::string third) -> ASTNode * { LogConsole::abortEvaluation("invalid ternary operation", this); },

                [this](auto &&first, auto &&second, auto &&third) -> ASTNode * {
                    switch (this->getOperator()) {
                    case Token::Operator::TernaryConditional:
                        return new ASTNodeLiteral(first ? second : third);
                    default:
                        LogConsole::abortEvaluation("invalid ternary operator used in mathematical expression");
                    }
                }
            }, first->getValue(), second->getValue(), third->getValue());
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
                pattern = new PatternDataUnsigned(offset, size);
            else if (Token::isSigned(this->m_type))
                pattern = new PatternDataSigned(offset, size);
            else if (Token::isFloatingPoint(this->m_type))
                pattern = new PatternDataFloat(offset, size);
            else if (this->m_type == Token::ValueType::Boolean)
                pattern = new PatternDataBoolean(offset, size);
            else if (this->m_type == Token::ValueType::Character)
                pattern = new PatternDataCharacter(offset);
            else if (this->m_type == Token::ValueType::Character16)
                pattern = new PatternDataCharacter16(offset);
            else if (this->m_type == Token::ValueType::Padding)
                pattern = new PatternDataPadding(offset, 1);
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

    class ASTNodeVariableDecl : public ASTNode, public Attributable {
    public:
        ASTNodeVariableDecl(std::string name, ASTNode *type, ASTNode *placementOffset = nullptr)
                : ASTNode(), m_name(std::move(name)), m_type(type), m_placementOffset(placementOffset) { }

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

        [[nodiscard]] std::vector<PatternData*> createPatterns(Evaluator *evaluator) const override {
            if (this->m_placementOffset != nullptr) {
                auto offset = dynamic_cast<ASTNodeLiteral *>(this->m_placementOffset->evaluate(evaluator));
                ON_SCOPE_EXIT { delete offset; };

                evaluator->dataOffset() = std::visit(overloaded {
                    [this](std::string){ LogConsole::abortEvaluation("placement offset cannot be a string", this); return u64(0); },
                    [](auto &&offset) { return u64(offset); }
                }, offset->getValue());
            }

            auto pattern = this->m_type->createPatterns(evaluator).front();
            pattern->setVariableName(this->m_name);

            return { pattern };
        }

        FunctionResult execute(Evaluator *evaluator) override {
            evaluator->createVariable(this->getName(), this->getType());

            return { false, { } };
        }

    private:
        std::string m_name;
        ASTNode *m_type;
        ASTNode *m_placementOffset;
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
                        [this](std::string){ LogConsole::abortEvaluation("placement offset cannot be a string", this); return u64(0); },
                        [](auto &&offset) { return u64(offset); }
                }, offset->getValue());
            }

            auto type = this->m_type->evaluate(evaluator);
            ON_SCOPE_EXIT { delete type; };

            if (dynamic_cast<ASTNodeBuiltinType*>(type))
                return { createStaticArray(evaluator) };
            else if (auto attributable = dynamic_cast<Attributable*>(type)) {
                auto &attributes = attributable->getAttributes();

                bool isStaticType = std::any_of(attributes.begin(), attributes.end(), [](ASTNodeAttribute *attribute) {
                    return attribute->getAttribute() == "static" && !attribute->getValue().has_value();
                });

                if (isStaticType)
                    return { createStaticArray(evaluator) };
                else
                    return { createDynamicArray(evaluator) };
            }

            LogConsole::abortEvaluation("invalid type used in array", this);
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

            u128 entryCount = 0;

            // TODO: Implement while and unsized arrays
            if (this->m_size != nullptr) {
                auto sizeNode = this->m_size->evaluate(evaluator);
                ON_SCOPE_EXIT { delete sizeNode; };

                if (auto literal = dynamic_cast<ASTNodeLiteral*>(sizeNode)) {
                    entryCount = std::visit(overloaded {
                            [this](std::string) { LogConsole::abortEvaluation("cannot use string to index array", this); return u128(0); },
                            [](auto &&size) { return u128(size); }
                    }, literal->getValue());
                }
            }

            PatternData *outputPattern;
            if (dynamic_cast<PatternDataPadding*>(templatePattern)) {
                outputPattern = new PatternDataPadding(startOffset, 0);
            } else if (dynamic_cast<PatternDataCharacter*>(templatePattern)) {
                outputPattern = new PatternDataString(startOffset, 0);
            } else if (dynamic_cast<PatternDataCharacter16*>(templatePattern)) {
                outputPattern = new PatternDataString16(startOffset, 0);
            } else {
                auto arrayPattern = new PatternDataStaticArray(startOffset, 0);
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
            auto arrayPattern = new PatternDataDynamicArray(evaluator->dataOffset(), 0);
            arrayPattern->setVariableName(this->m_name);

            // TODO: Implement while and unsized arrays
            if (this->m_size != nullptr) {
                auto sizeNode = this->m_size->evaluate(evaluator);
                ON_SCOPE_EXIT { delete sizeNode; };

                if (auto literal = dynamic_cast<ASTNodeLiteral*>(sizeNode)) {
                    auto entryCount = std::visit(overloaded {
                            [this](std::string) { LogConsole::abortEvaluation("cannot use string to index array", this); return u128(0); },
                            [](auto &&size) { return u128(size); }
                    }, literal->getValue());

                    {
                        auto templatePattern = this->m_type->createPatterns(evaluator).front();
                        ON_SCOPE_EXIT { delete templatePattern; };

                        arrayPattern->setTypeName(templatePattern->getTypeName());
                        evaluator->dataOffset() -= templatePattern->getSize();
                    }

                    std::vector<PatternData*> entries;
                    size_t size = 0;
                    for (u64 i = 0; i < entryCount; i++) {
                        auto pattern = this->m_type->createPatterns(evaluator).front();

                        pattern->setVariableName(hex::format("[{}]", i));
                        pattern->setEndian(arrayPattern->getEndian());
                        pattern->setColor(arrayPattern->getColor());
                        entries.push_back(pattern);

                        size += pattern->getSize();
                    }
                    arrayPattern->setEntries(entries);
                    arrayPattern->setSize(size);
                }
            }

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
                        [this](std::string){ LogConsole::abortEvaluation("placement offset cannot be a string", this); return u64(0); },
                        [](auto &&offset) { return u64(offset); }
                }, offset->getValue());
            }

            auto sizePattern = this->m_sizeType->createPatterns(evaluator).front();
            ON_SCOPE_EXIT { delete sizePattern; };

            auto pattern = new PatternDataPointer(evaluator->dataOffset(), sizePattern->getSize());
            pattern->setPointedAtPattern(this->m_type->createPatterns(evaluator).front());
            pattern->setVariableName(this->m_name);

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

            private:
        std::vector<ASTNode*> m_variables;
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

        [[nodiscard]] std::vector<PatternData*> createPatterns(Evaluator *evaluator) const override {
            auto pattern = new PatternDataStruct(evaluator->dataOffset(), 0);

            u64 startOffset = evaluator->dataOffset();
            std::vector<PatternData*> memberPatterns;

            evaluator->pushScope(memberPatterns);
            for (auto member : this->m_members) {
                for (auto &memberPattern : member->createPatterns(evaluator)) {
                    memberPatterns.push_back(memberPattern);
                }
            }
            evaluator->popScope();

            pattern->setMembers(memberPatterns);
            pattern->setSize(evaluator->dataOffset() - startOffset);

            return { pattern };
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

        [[nodiscard]] std::vector<PatternData*> createPatterns(Evaluator *evaluator) const override {
            auto pattern = new PatternDataUnion(evaluator->dataOffset(), 0);

            size_t size = 0;
            std::vector<PatternData*> memberPatterns;
            u64 startOffset = evaluator->dataOffset();

            evaluator->pushScope(memberPatterns);
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
            auto pattern = new PatternDataEnum(evaluator->dataOffset(), 0);

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
            auto pattern = new PatternDataBitfield(evaluator->dataOffset(), 0);

            size_t bitOffset = 0;
            std::vector<PatternData*> fields;
            evaluator->pushScope(fields);
            for (auto [name, bitSizeNode] : this->m_entries) {
                auto literal = bitSizeNode->evaluate(evaluator);
                ON_SCOPE_EXIT { delete literal; };

                u8 bitSize = std::visit(overloaded {
                        [](std::string){ return static_cast<u8>(0); },
                        [](auto &&offset) { return static_cast<u8>(offset); }
                }, dynamic_cast<ASTNodeLiteral*>(literal)->getValue());

                auto field = new PatternDataBitfieldField(evaluator->dataOffset(), bitOffset, bitSize);
                field->setVariableName(name);

                bitOffset += bitSize;
                fields.push_back(field);
            }
            evaluator->popScope();

            pattern->setSize((bitOffset + 7) / 8);
            pattern->setFields(fields);

            evaluator->dataOffset() += pattern->getSize();

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

            auto readValue = [&evaluator](auto &value, PatternData *pattern) {
                if (pattern->isLocal())
                    std::memcpy(&value, &evaluator->getStack()[pattern->getOffset()], pattern->getSize());
                else
                    evaluator->getProvider()->read(pattern->getOffset(), &value, pattern->getSize());
            };

            Token::Literal literal;
            if (dynamic_cast<PatternDataUnsigned*>(pattern)) {
                u128 value = 0;
                readValue(value, pattern);
                literal = value;
            } else if (dynamic_cast<PatternDataSigned*>(pattern)) {
                s128 value = 0;
                readValue(value, pattern);
                literal = value;
            } else if (dynamic_cast<PatternDataFloat*>(pattern)) {
                if (pattern->getSize() == sizeof(u16)) {
                    u16 value = 0;
                    readValue(value, pattern);
                    literal = double(float16ToFloat32(value));
                } else if (pattern->getSize() == sizeof(float)) {
                    float value = 0;
                    readValue(value, pattern);
                    literal = double(value);
                } else if (pattern->getSize() == sizeof(double)) {
                    double value = 0;
                    readValue(value, pattern);
                    literal = value;
                } else LogConsole::abortEvaluation("invalid floating point type access", this);
            } else if (dynamic_cast<PatternDataCharacter*>(pattern)) {
                char value = 0;
                readValue(value, pattern);
                literal = value;
            } else if (dynamic_cast<PatternDataBoolean*>(pattern)) {
                bool value = 0;
                readValue(value, pattern);
                literal = value;
            } else if (dynamic_cast<PatternDataString*>(pattern)) {
                std::string value(pattern->getSize(), '\x00');
                readValue(value, pattern);
                literal = value;
            } else if (auto bitfieldFieldPattern = dynamic_cast<PatternDataBitfieldField*>(pattern)) {
                u64 value = 0;
                readValue(value, pattern);
                literal = u128(hex::extract(bitfieldFieldPattern->getBitOffset() + (bitfieldFieldPattern->getBitSize() - 1), bitfieldFieldPattern->getBitOffset(), value));
            } else LogConsole::abortEvaluation("invalid type access", this);

            return new ASTNodeLiteral(literal);
        }

        [[nodiscard]] std::vector<PatternData*> createPatterns(Evaluator *evaluator) const override {
            s32 scopeIndex = 0;

            auto searchScope = evaluator->getScope(scopeIndex);
            PatternData *currPattern = nullptr;

            for (const auto &part : this->getPath()) {

                if (part.index() == 0) {
                    // Variable access
                    auto name = std::get<std::string>(part);

                    if (name == "parent") {
                        scopeIndex--;
                        searchScope = evaluator->getScope(scopeIndex);
                    } else {
                        bool found = false;
                        for (const auto &variable : searchScope) {
                            if (variable->getVariableName() == name) {
                                auto newPattern = variable->clone();
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
                        [](std::string) { },
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

                                auto newPattern = searchScope[0]->clone();
                                delete currPattern;
                                currPattern = newPattern;
                                currPattern->setOffset(staticArrayPattern->getOffset() + index * staticArrayPattern->getSize());
                            }
                        }
                    }, index->getValue());
                }

                if (auto pointerPattern = dynamic_cast<PatternDataPointer*>(currPattern)) {
                    auto newPattern = pointerPattern->getPointedAtPattern()->clone();
                    delete currPattern;
                    currPattern = newPattern;
                }

                if (auto structPattern = dynamic_cast<PatternDataStruct*>(currPattern))
                    searchScope = structPattern->getMembers();
                else if (auto unionPattern = dynamic_cast<PatternDataUnion*>(currPattern))
                    searchScope = unionPattern->getMembers();
                else if (auto bitfieldPattern = dynamic_cast<PatternDataBitfield*>(currPattern))
                    searchScope = bitfieldPattern->getFields();
                else if (auto dynamicArrayPattern = dynamic_cast<PatternDataDynamicArray*>(currPattern))
                    searchScope = dynamicArrayPattern->getEntries();
                else if (auto staticArrayPattern = dynamic_cast<PatternDataStaticArray*>(currPattern))
                    searchScope = { staticArrayPattern->getTemplate() };

            }

            return { currPattern };
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

        [[nodiscard]] std::vector<PatternData*> createPatterns(Evaluator *evaluator) const override {
            std::vector<PatternData *> patterns;

            auto &body = evaluateCondition(evaluator) ? this->m_trueBody : this->m_falseBody;

            for (auto &node : body) {
                auto newPatterns = node->createPatterns(evaluator);
                patterns.insert(patterns.end(), newPatterns.begin(), newPatterns.end());
            }

            return patterns;
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

        FunctionResult execute(Evaluator *evaluator) override {
            auto &body = evaluateCondition(evaluator) ? this->m_trueBody : this->m_falseBody;

            std::vector<PatternData*> variables = evaluator->getScope(0);
            u32 startSize = variables.size();
            ON_SCOPE_EXIT {
                for (u32 i = startSize; i < variables.size(); i++)
                    delete variables[i];
            };

            evaluator->pushScope(variables);
            ON_SCOPE_EXIT { evaluator->popScope(); };
            for (auto &statement : body) {
                auto [executionStopped, result] = statement->execute(evaluator);
                if (executionStopped) {
                    return { true, result };
                }
            }

            return { false, { } };
        }

    private:
        [[nodiscard]]
        bool evaluateCondition(Evaluator *evaluator) const {
            auto literal = dynamic_cast<ASTNodeLiteral*>(this->m_condition->evaluate(evaluator));
            ON_SCOPE_EXIT { delete literal; };

            return std::visit(overloaded {
                [](std::string) { return false; },
                [](auto &&value) { return value != 0; }
                }, literal->getValue());
        }

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

        FunctionResult execute(Evaluator *evaluator) override {

            while (evaluateCondition(evaluator)) {
                std::vector<PatternData*> variables = evaluator->getScope(0);
                u32 startSize = variables.size();
                ON_SCOPE_EXIT {
                    for (u32 i = startSize; i < variables.size(); i++)
                        delete variables[i];
                };

                evaluator->pushScope(variables);
                ON_SCOPE_EXIT { evaluator->popScope(); };

                for (auto &statement : this->m_body) {
                    auto [executionStopped, result] = statement->execute(evaluator);
                    if (executionStopped) {
                        return { true, result };
                    }
                }
            }

            return { false, { } };
        }

    private:
        ASTNode *m_condition;
        std::vector<ASTNode*> m_body;

        [[nodiscard]]
        bool evaluateCondition(Evaluator *evaluator) const {
            auto literal = dynamic_cast<ASTNodeLiteral*>(this->m_condition->evaluate(evaluator));
            ON_SCOPE_EXIT { delete literal; };

            return std::visit(overloaded {
                    [](std::string) { return false; },
                    [](auto &&value) { return value != 0; }
            }, literal->getValue());
        }
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
            auto functions = ContentRegistry::PatternLanguageFunctions::getEntries();

            for (auto &func : customFunctions)
                functions.insert(func);

            if (!functions.contains(this->m_functionName))
                LogConsole::abortEvaluation(hex::format("call to unknown function '{}'", this->m_functionName), this);

            auto function = functions[this->m_functionName];
            if (function.parameterCount == ContentRegistry::PatternLanguageFunctions::UnlimitedParameters) {
                ; // Don't check parameter count
            }
            else if (function.parameterCount & ContentRegistry::PatternLanguageFunctions::LessParametersThan) {
                if (evaluatedParams.size() >= (function.parameterCount & ~ContentRegistry::PatternLanguageFunctions::LessParametersThan))
                    LogConsole::abortEvaluation(hex::format("too many parameters for function '{0}'. Expected {1}", this->m_functionName, function.parameterCount & ~ContentRegistry::PatternLanguageFunctions::LessParametersThan), this);
            } else if (function.parameterCount & ContentRegistry::PatternLanguageFunctions::MoreParametersThan) {
                if (evaluatedParams.size() <= (function.parameterCount & ~ContentRegistry::PatternLanguageFunctions::MoreParametersThan))
                    LogConsole::abortEvaluation(hex::format("too few parameters for function '{0}'. Expected {1}", this->m_functionName, function.parameterCount & ~ContentRegistry::PatternLanguageFunctions::MoreParametersThan), this);
            } else if (function.parameterCount != evaluatedParams.size()) {
                LogConsole::abortEvaluation(hex::format("invalid number of parameters for function '{0}'. Expected {1}", this->m_functionName, function.parameterCount), this);
            }

            try {
                auto result = functions[this->m_functionName].func(evaluator, evaluatedParams);

                if (result.has_value())
                    return new ASTNodeLiteral(result.value());
                else
                    return new ASTNodeMathematicalExpression(nullptr, nullptr, Token::Operator::Plus);
            } catch (std::string &error) {
                LogConsole::abortEvaluation(error, this);
            }

            return nullptr;
        }

        FunctionResult execute(Evaluator *evaluator) override {
            delete this->evaluate(evaluator);

            return { false, { } };
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
        // TODO: Implement this
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

        FunctionResult execute(Evaluator *evaluator) override {
            auto literal = dynamic_cast<ASTNodeLiteral*>(this->getRValue()->evaluate(evaluator));
            ON_SCOPE_EXIT { delete literal; };

            evaluator->setVariable(this->getLValueName(), literal->getValue());

            return { false, { } };
        }

    private:
        std::string m_lvalueName;
        ASTNode *m_rvalue;
    };

    class ASTNodeReturnStatement : public ASTNode {
    public:
        // TODO: Implement this
        explicit ASTNodeReturnStatement(ASTNode *rvalue) : m_rvalue(rvalue) {

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

        [[nodiscard]] ASTNode* getReturnValue() const {
            return this->m_rvalue;
        }

        FunctionResult execute(Evaluator *evaluator) override {
            auto returnValue = this->getReturnValue();

            if (returnValue == nullptr)
                return { true, std::nullopt };
            else {
                auto literal = dynamic_cast<ASTNodeLiteral*>(returnValue->evaluate(evaluator));
                ON_SCOPE_EXIT { delete literal; };

                return { true, literal->getValue() };
            }
        }

    private:
        ASTNode *m_rvalue;
    };

    class ASTNodeFunctionDefinition : public ASTNode {
    public:
        // TODO: Implement this
        ASTNodeFunctionDefinition(std::string name, std::map<std::string, ASTNode*> params, std::vector<ASTNode*> body)
            : m_name(std::move(name)), m_params(std::move(params)), m_body(std::move(body)) {

        }

        ASTNodeFunctionDefinition(const ASTNodeFunctionDefinition &other) : ASTNode(other) {
            this->m_name = other.m_name;
            this->m_params = other.m_params;

            for (const auto &[name, type] : other.m_params) {
                this->m_params.emplace(name, type->clone());
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

                ctx->pushScope(variables);
                ON_SCOPE_EXIT { ctx->popScope(); };

                u32 paramIndex = 0;
                for (const auto &[name, type] : this->m_params) {
                    ctx->createVariable(name, type);
                    ctx->setVariable(name, params[paramIndex]);

                    paramIndex++;
                }

                for (auto statement : this->m_body) {
                    auto [executionStopped, result] = statement->execute(ctx);

                    if (executionStopped) {
                        return result;
                    }
                }

                return { };
            });

            return nullptr;
        }


    private:
        std::string m_name;
        std::map<std::string, ASTNode*> m_params;
        std::vector<ASTNode*> m_body;
    };

}