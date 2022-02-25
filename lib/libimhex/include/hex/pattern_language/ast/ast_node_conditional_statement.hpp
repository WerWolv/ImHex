#pragma once

#include <hex/pattern_language/ast/ast_node.hpp>

namespace hex::pl {

    class ASTNodeConditionalStatement : public ASTNode {
    public:
        explicit ASTNodeConditionalStatement(std::unique_ptr<ASTNode> condition, std::vector<std::unique_ptr<ASTNode>> &&trueBody, std::vector<std::unique_ptr<ASTNode>> &&falseBody)
            : ASTNode(), m_condition(std::move(condition)), m_trueBody(std::move(trueBody)), m_falseBody(std::move(falseBody)) { }


        ASTNodeConditionalStatement(const ASTNodeConditionalStatement &other) : ASTNode(other) {
            this->m_condition = other.m_condition->clone();

            for (auto &statement : other.m_trueBody)
                this->m_trueBody.push_back(statement->clone());
            for (auto &statement : other.m_falseBody)
                this->m_falseBody.push_back(statement->clone());
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeConditionalStatement(*this));
        }

        [[nodiscard]] std::vector<std::unique_ptr<PatternData>> createPatterns(Evaluator *evaluator) const override {
            auto &scope = *evaluator->getScope(0).scope;
            auto &body  = evaluateCondition(evaluator) ? this->m_trueBody : this->m_falseBody;

            for (auto &node : body) {
                auto newPatterns = node->createPatterns(evaluator);
                for (auto &pattern : newPatterns) {
                    scope.push_back(std::move(pattern));
                }
            }

            return {};
        }

        [[nodiscard]] const std::unique_ptr<ASTNode> &getCondition() {
            return this->m_condition;
        }

        FunctionResult execute(Evaluator *evaluator) const override {
            auto &body = evaluateCondition(evaluator) ? this->m_trueBody : this->m_falseBody;

            auto variables     = *evaluator->getScope(0).scope;
            auto parameterPack = evaluator->getScope(0).parameterPack;

            u32 startVariableCount = variables.size();
            ON_SCOPE_EXIT {
                i64 stackSize = evaluator->getStack().size();
                for (u32 i = startVariableCount; i < variables.size(); i++) {
                    stackSize--;
                }
                if (stackSize < 0) LogConsole::abortEvaluation("stack pointer underflow!", this);
                evaluator->getStack().resize(stackSize);
            };

            evaluator->pushScope(nullptr, variables);
            evaluator->getScope(0).parameterPack = parameterPack;
            ON_SCOPE_EXIT {
                evaluator->popScope();
            };

            for (auto &statement : body) {
                auto result = statement->execute(evaluator);
                if (auto ctrlStatement = evaluator->getCurrentControlFlowStatement(); ctrlStatement != ControlFlowStatement::None) {
                    return result;
                }
            }

            return std::nullopt;
        }

    private:
        [[nodiscard]] bool evaluateCondition(Evaluator *evaluator) const {
            const auto node    = this->m_condition->evaluate(evaluator);
            const auto literal = dynamic_cast<ASTNodeLiteral *>(node.get());

            return std::visit(overloaded {
                                  [](const std::string &value) -> bool { return !value.empty(); },
                                  [this](PatternData *const &) -> bool { LogConsole::abortEvaluation("cannot cast custom type to bool", this); },
                                  [](auto &&value) -> bool { return value != 0; } },
                literal->getValue());
        }

        std::unique_ptr<ASTNode> m_condition;
        std::vector<std::unique_ptr<ASTNode>> m_trueBody, m_falseBody;
    };

}