#pragma once

#include <hex/pattern_language/ast/ast_node.hpp>

namespace hex::pl {

#define FLOAT_BIT_OPERATION(name)                                                    \
    auto name(hex::floating_point auto left, auto right) const {                     \
        LogConsole::abortEvaluation("invalid floating point operation", this);       \
        return 0;                                                                    \
    }                                                                                \
    auto name(auto left, hex::floating_point auto right) const {                     \
        LogConsole::abortEvaluation("invalid floating point operation", this);       \
        return 0;                                                                    \
    }                                                                                \
    auto name(hex::floating_point auto left, hex::floating_point auto right) const { \
        LogConsole::abortEvaluation("invalid floating point operation", this);       \
        return 0;                                                                    \
    }                                                                                \
    auto name(hex::integral auto left, hex::integral auto right) const

    class ASTNodeMathematicalExpression : public ASTNode {

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
        ASTNodeMathematicalExpression(std::unique_ptr<ASTNode> &&left, std::unique_ptr<ASTNode> &&right, Token::Operator op)
            : ASTNode(), m_left(std::move(left)), m_right(std::move(right)), m_operator(op) { }

        ASTNodeMathematicalExpression(const ASTNodeMathematicalExpression &other) : ASTNode(other) {
            this->m_operator = other.m_operator;
            this->m_left     = other.m_left->clone();
            this->m_right    = other.m_right->clone();
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeMathematicalExpression(*this));
        }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override {
            if (this->getLeftOperand() == nullptr || this->getRightOperand() == nullptr)
                LogConsole::abortEvaluation("attempted to use void expression in mathematical expression", this);

            auto leftValue  = this->getLeftOperand()->evaluate(evaluator);
            auto rightValue = this->getRightOperand()->evaluate(evaluator);

            auto *left  = dynamic_cast<ASTNodeLiteral *>(leftValue.get());
            auto *right = dynamic_cast<ASTNodeLiteral *>(rightValue.get());

            return std::unique_ptr<ASTNode>(std::visit(overloaded {
                                                           // TODO: :notlikethis:
                                                           [this](u128 left, const std::shared_ptr<PatternData> &right) -> ASTNode * { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                                                           [this](i128 left, const std::shared_ptr<PatternData> &right) -> ASTNode * { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                                                           [this](double left, const std::shared_ptr<PatternData> &right) -> ASTNode * { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                                                           [this](char left, const std::shared_ptr<PatternData> &right) -> ASTNode * { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                                                           [this](bool left, const std::shared_ptr<PatternData> &right) -> ASTNode * { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                                                           [this](const std::string &left, const std::shared_ptr<PatternData> &right) -> ASTNode * { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                                                           [this](const std::shared_ptr<PatternData> &left, u128 right) -> ASTNode * { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                                                           [this](const std::shared_ptr<PatternData> &left, i128 right) -> ASTNode * { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                                                           [this](const std::shared_ptr<PatternData> &left, double right) -> ASTNode * { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                                                           [this](const std::shared_ptr<PatternData> &left, char right) -> ASTNode * { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                                                           [this](const std::shared_ptr<PatternData> &left, bool right) -> ASTNode * { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                                                           [this](const std::shared_ptr<PatternData> &left, const std::string &right) -> ASTNode * { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                                                           [this](const std::shared_ptr<PatternData> &left, const std::shared_ptr<PatternData> &right) -> ASTNode * { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },

                                                           [this](auto &&left, const std::string &right) -> ASTNode * { LogConsole::abortEvaluation("invalid operand used in mathematical expression", this); },
                                                           [this](const std::string &left, auto &&right) -> ASTNode * {
                                                               switch (this->getOperator()) {
                                                                   case Token::Operator::Star:
                                                                       {
                                                                           std::string result;
                                                                           for (auto i = 0; i < right; i++)
                                                                               result += left;
                                                                           return new ASTNodeLiteral(result);
                                                                       }
                                                                   default:
                                                                       LogConsole::abortEvaluation("invalid operand used in mathematical expression", this);
                                                               }
                                                           },
                                                           [this](const std::string &left, const std::string &right) -> ASTNode * {
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
                                                           [this](const std::string &left, char right) -> ASTNode * {
                                                               switch (this->getOperator()) {
                                                                   case Token::Operator::Plus:
                                                                       return new ASTNodeLiteral(left + right);
                                                                   default:
                                                                       LogConsole::abortEvaluation("invalid operand used in mathematical expression", this);
                                                               }
                                                           },
                                                           [this](char left, const std::string &right) -> ASTNode * {
                                                               switch (this->getOperator()) {
                                                                   case Token::Operator::Plus:
                                                                       return new ASTNodeLiteral(left + right);
                                                                   default:
                                                                       LogConsole::abortEvaluation("invalid operand used in mathematical expression", this);
                                                               }
                                                           },
                                                           [this](auto &&left, auto &&right) -> ASTNode * {
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
                                                           } },
                left->getValue(),
                right->getValue()));
        }

        [[nodiscard]] const std::unique_ptr<ASTNode> &getLeftOperand() const { return this->m_left; }
        [[nodiscard]] const std::unique_ptr<ASTNode> &getRightOperand() const { return this->m_right; }
        [[nodiscard]] Token::Operator getOperator() const { return this->m_operator; }

    private:
        std::unique_ptr<ASTNode> m_left, m_right;
        Token::Operator m_operator;
    };

#undef FLOAT_BIT_OPERATION

}