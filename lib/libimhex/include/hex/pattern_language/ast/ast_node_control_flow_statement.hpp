#pragma once

#include <hex/pattern_language/ast/ast_node.hpp>

namespace hex::pl {

    class ASTNodeControlFlowStatement : public ASTNode {
    public:
        explicit ASTNodeControlFlowStatement(ControlFlowStatement type, std::unique_ptr<ASTNode> &&rvalue) : m_type(type), m_rvalue(std::move(rvalue)) {
        }

        ASTNodeControlFlowStatement(const ASTNodeControlFlowStatement &other) : ASTNode(other) {
            this->m_type = other.m_type;

            if (other.m_rvalue != nullptr)
                this->m_rvalue = other.m_rvalue->clone();
            else
                this->m_rvalue = nullptr;
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeControlFlowStatement(*this));
        }

        [[nodiscard]] std::vector<std::unique_ptr<Pattern>> createPatterns(Evaluator *evaluator) const override {

            this->execute(evaluator);

            return {};
        }

        FunctionResult execute(Evaluator *evaluator) const override {
            if (this->m_rvalue == nullptr) {
                evaluator->setCurrentControlFlowStatement(this->m_type);
                return std::nullopt;
            } else {
                auto returnValue = this->m_rvalue->evaluate(evaluator);
                auto literal     = dynamic_cast<ASTNodeLiteral *>(returnValue.get());

                evaluator->setCurrentControlFlowStatement(this->m_type);

                if (literal == nullptr)
                    return std::nullopt;
                else
                    return literal->getValue();
            }
        }

    private:
        ControlFlowStatement m_type;
        std::unique_ptr<ASTNode> m_rvalue;
    };

}