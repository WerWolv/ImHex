#pragma once

#include <hex/pattern_language/ast/ast_node.hpp>

namespace hex::pl {

    class ASTNodeTypeOperator : public ASTNode {
    public:
        ASTNodeTypeOperator(Token::Operator op, std::unique_ptr<ASTNode> &&expression) : m_op(op), m_expression(std::move(expression)) {
        }

        ASTNodeTypeOperator(const ASTNodeTypeOperator &other) : ASTNode(other) {
            this->m_op         = other.m_op;
            this->m_expression = other.m_expression->clone();
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeTypeOperator(*this));
        }

        [[nodiscard]] Token::Operator getOperator() const {
            return this->m_op;
        }

        [[nodiscard]] const std::unique_ptr<ASTNode> &getExpression() const {
            return this->m_expression;
        }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override {
            auto patterns = this->m_expression->createPatterns(evaluator);
            auto &pattern = patterns.front();

            switch (this->getOperator()) {
                case Token::Operator::AddressOf:
                    return std::unique_ptr<ASTNode>(new ASTNodeLiteral(u128(pattern->getOffset())));
                case Token::Operator::SizeOf:
                    return std::unique_ptr<ASTNode>(new ASTNodeLiteral(u128(pattern->getSize())));
                default:
                    LogConsole::abortEvaluation("invalid type operator", this);
            }
        }


    private:
        Token::Operator m_op;
        std::unique_ptr<ASTNode> m_expression;
    };

}