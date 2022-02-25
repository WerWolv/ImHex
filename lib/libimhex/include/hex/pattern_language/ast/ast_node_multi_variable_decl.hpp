#pragma once

#include <hex/pattern_language/ast/ast_node.hpp>

#include <hex/pattern_language/ast/ast_node_variable_decl.hpp>

namespace hex::pl {

    class ASTNodeMultiVariableDecl : public ASTNode {
    public:
        explicit ASTNodeMultiVariableDecl(std::vector<std::unique_ptr<ASTNode>> &&variables) : m_variables(std::move(variables)) { }

        ASTNodeMultiVariableDecl(const ASTNodeMultiVariableDecl &other) : ASTNode(other) {
            for (auto &variable : other.m_variables)
                this->m_variables.push_back(variable->clone());
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeMultiVariableDecl(*this));
        }

        [[nodiscard]] const std::vector<std::unique_ptr<ASTNode>> &getVariables() {
            return this->m_variables;
        }

        [[nodiscard]] std::vector<std::unique_ptr<PatternData>> createPatterns(Evaluator *evaluator) const override {
            std::vector<std::unique_ptr<PatternData>> patterns;

            for (auto &node : this->m_variables) {
                auto newPatterns = node->createPatterns(evaluator);
                std::move(newPatterns.begin(), newPatterns.end(), std::back_inserter(patterns));
            }

            return patterns;
        }

        FunctionResult execute(Evaluator *evaluator) const override {
            for (auto &variable : this->m_variables) {
                auto variableDecl = dynamic_cast<ASTNodeVariableDecl *>(variable.get());
                auto variableType = variableDecl->getType()->evaluate(evaluator);

                evaluator->createVariable(variableDecl->getName(), variableType.get());
            }

            return std::nullopt;
        }

    private:
        std::vector<std::unique_ptr<ASTNode>> m_variables;
    };

}