#pragma once

#include <hex/pattern_language/ast/ast_node.hpp>
#include <hex/pattern_language/ast/ast_node_literal.hpp>

namespace hex::pl {

    class ASTNodeAssignment : public ASTNode {
    public:
        ASTNodeAssignment(std::string lvalueName, std::unique_ptr<ASTNode> &&rvalue) : m_lvalueName(std::move(lvalueName)), m_rvalue(std::move(rvalue)) {
        }

        ASTNodeAssignment(const ASTNodeAssignment &other) : ASTNode(other) {
            this->m_lvalueName = other.m_lvalueName;
            this->m_rvalue     = other.m_rvalue->clone();
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeAssignment(*this));
        }

        [[nodiscard]] const std::string &getLValueName() const {
            return this->m_lvalueName;
        }

        [[nodiscard]] const std::unique_ptr<ASTNode> &getRValue() const {
            return this->m_rvalue;
        }

        [[nodiscard]] std::vector<std::unique_ptr<PatternData>> createPatterns(Evaluator *evaluator) const override {
            this->execute(evaluator);

            return {};
        }

        FunctionResult execute(Evaluator *evaluator) const override {
            const auto node    = this->getRValue()->evaluate(evaluator);
            const auto literal = dynamic_cast<ASTNodeLiteral *>(node.get());

            if (this->getLValueName() == "$")
                evaluator->dataOffset() = Token::literalToUnsigned(literal->getValue());
            else
                evaluator->setVariable(this->getLValueName(), literal->getValue());

            return {};
        }

    private:
        std::string m_lvalueName;
        std::unique_ptr<ASTNode> m_rvalue;
    };

}