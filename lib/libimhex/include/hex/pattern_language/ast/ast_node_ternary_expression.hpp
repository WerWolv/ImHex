#pragma once

#include <hex/pattern_language/ast/ast_node.hpp>

namespace hex::pl {

    class ASTNodeTernaryExpression : public ASTNode {
    public:
        ASTNodeTernaryExpression(std::unique_ptr<ASTNode> &&first, std::unique_ptr<ASTNode> &&second, std::unique_ptr<ASTNode> &&third, Token::Operator op)
            : ASTNode(), m_first(std::move(first)), m_second(std::move(second)), m_third(std::move(third)), m_operator(op) { }

        ASTNodeTernaryExpression(const ASTNodeTernaryExpression &other) : ASTNode(other) {
            this->m_operator = other.m_operator;
            this->m_first    = other.m_first->clone();
            this->m_second   = other.m_second->clone();
            this->m_third    = other.m_third->clone();
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeTernaryExpression(*this));
        }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override {
            if (this->getFirstOperand() == nullptr || this->getSecondOperand() == nullptr || this->getThirdOperand() == nullptr)
                LogConsole::abortEvaluation("attempted to use void expression in mathematical expression", this);

            auto firstNode  = this->getFirstOperand()->evaluate(evaluator);
            auto secondNode = this->getSecondOperand()->evaluate(evaluator);
            auto thirdNode  = this->getThirdOperand()->evaluate(evaluator);

            auto *first  = dynamic_cast<ASTNodeLiteral *>(firstNode.get());
            auto *second = dynamic_cast<ASTNodeLiteral *>(secondNode.get());
            auto *third  = dynamic_cast<ASTNodeLiteral *>(thirdNode.get());

            auto condition = std::visit(overloaded {
                                            [](const std::string &value) -> bool { return !value.empty(); },
                                            [this](const std::shared_ptr<Pattern> &) -> bool { LogConsole::abortEvaluation("cannot cast custom type to bool", this); },
                                            [](auto &&value) -> bool { return bool(value); } },
                first->getValue());

            return std::visit(overloaded {
                                  [condition]<typename T>(const T &second, const T &third) -> std::unique_ptr<ASTNode> { return std::unique_ptr<ASTNode>(new ASTNodeLiteral(condition ? second : third)); },
                                  [this](auto &&second, auto &&third) -> std::unique_ptr<ASTNode> { LogConsole::abortEvaluation("operands to ternary expression have different types", this); } },
                second->getValue(),
                third->getValue());
        }

        [[nodiscard]] const std::unique_ptr<ASTNode> &getFirstOperand() const { return this->m_first; }
        [[nodiscard]] const std::unique_ptr<ASTNode> &getSecondOperand() const { return this->m_second; }
        [[nodiscard]] const std::unique_ptr<ASTNode> &getThirdOperand() const { return this->m_third; }
        [[nodiscard]] Token::Operator getOperator() const { return this->m_operator; }

    private:
        std::unique_ptr<ASTNode> m_first, m_second, m_third;
        Token::Operator m_operator;
    };

}