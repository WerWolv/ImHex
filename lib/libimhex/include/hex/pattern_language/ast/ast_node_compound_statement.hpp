#pragma once

#include <hex/pattern_language/ast/ast_node.hpp>

namespace hex::pl {

    class ASTNodeCompoundStatement : public ASTNode {
    public:
        explicit ASTNodeCompoundStatement(std::vector<std::unique_ptr<ASTNode>> &&statements, bool newScope = false) : m_statements(std::move(statements)), m_newScope(newScope) {
        }

        ASTNodeCompoundStatement(const ASTNodeCompoundStatement &other) : ASTNode(other) {
            for (const auto &statement : other.m_statements) {
                this->m_statements.push_back(statement->clone());
            }

            this->m_newScope = other.m_newScope;
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeCompoundStatement(*this));
        }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override {
            std::unique_ptr<ASTNode> result = nullptr;

            for (const auto &statement : this->m_statements) {
                result = statement->evaluate(evaluator);
            }

            return result;
        }

        [[nodiscard]] std::vector<std::unique_ptr<Pattern>> createPatterns(Evaluator *evaluator) const override {
            std::vector<std::unique_ptr<Pattern>> result;

            for (const auto &statement : this->m_statements) {
                auto patterns = statement->createPatterns(evaluator);
                std::move(patterns.begin(), patterns.end(), std::back_inserter(result));
            }

            return result;
        }

        FunctionResult execute(Evaluator *evaluator) const override {
            FunctionResult result;

            auto variables         = *evaluator->getScope(0).scope;
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
                i64 stackSize = evaluator->getStack().size();
                for (u32 i = startVariableCount; i < variables.size(); i++) {
                    stackSize--;
                }
                if (stackSize < 0) LogConsole::abortEvaluation("stack pointer underflow!", this);
                evaluator->getStack().resize(stackSize);

                evaluator->popScope();
            }

            return result;
        }

    public:
        std::vector<std::unique_ptr<ASTNode>> m_statements;
        bool m_newScope = false;
    };

}