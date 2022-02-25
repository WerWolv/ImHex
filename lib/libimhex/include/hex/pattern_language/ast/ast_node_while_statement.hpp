#pragma once

#include <hex/pattern_language/ast/ast_node.hpp>

namespace hex::pl {

    class ASTNodeWhileStatement : public ASTNode {
    public:
        explicit ASTNodeWhileStatement(std::unique_ptr<ASTNode> &&condition, std::vector<std::unique_ptr<ASTNode>> &&body, std::unique_ptr<ASTNode> &&postExpression = nullptr)
            : ASTNode(), m_condition(std::move(condition)), m_body(std::move(body)), m_postExpression(std::move(postExpression)) { }

        ASTNodeWhileStatement(const ASTNodeWhileStatement &other) : ASTNode(other) {
            this->m_condition = other.m_condition->clone();

            for (auto &statement : other.m_body)
                this->m_body.push_back(statement->clone());

            if (other.m_postExpression != nullptr)
                this->m_postExpression = other.m_postExpression->clone();
            else
                this->m_postExpression = nullptr;
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeWhileStatement(*this));
        }

        [[nodiscard]] const std::unique_ptr<ASTNode> &getCondition() {
            return this->m_condition;
        }

        [[nodiscard]] const std::vector<std::unique_ptr<ASTNode>> &getBody() {
            return this->m_body;
        }

        FunctionResult execute(Evaluator *evaluator) const override {

            u64 loopIterations = 0;
            while (evaluateCondition(evaluator)) {
                evaluator->handleAbort();

                auto variables         = *evaluator->getScope(0).scope;
                auto parameterPack     = evaluator->getScope(0).parameterPack;
                u32 startVariableCount = variables.size();
                ON_SCOPE_EXIT {
                    ssize_t stackSize = evaluator->getStack().size();
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

            return std::nullopt;
        }

        [[nodiscard]] bool evaluateCondition(Evaluator *evaluator) const {
            const auto node    = this->m_condition->evaluate(evaluator);
            const auto literal = dynamic_cast<ASTNodeLiteral *>(node.get());

            return std::visit(overloaded {
                                  [](const std::string &value) -> bool { return !value.empty(); },
                                  [this](PatternData *const &) -> bool { LogConsole::abortEvaluation("cannot cast custom type to bool", this); },
                                  [](auto &&value) -> bool { return value != 0; } },
                literal->getValue());
        }

    private:
        std::unique_ptr<ASTNode> m_condition;
        std::vector<std::unique_ptr<ASTNode>> m_body;
        std::unique_ptr<ASTNode> m_postExpression;
    };

}