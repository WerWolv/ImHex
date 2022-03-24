#pragma once

#include <hex/pattern_language/ast/ast_node.hpp>

namespace hex::pl {

    class ASTNodeFunctionDefinition : public ASTNode {
    public:
        ASTNodeFunctionDefinition(std::string name, std::vector<std::pair<std::string, std::unique_ptr<ASTNode>>> &&params, std::vector<std::unique_ptr<ASTNode>> &&body, std::optional<std::string> parameterPack, std::vector<std::unique_ptr<ASTNode>> &&defaultParameters)
            : m_name(std::move(name)), m_params(std::move(params)), m_body(std::move(body)), m_parameterPack(std::move(parameterPack)), m_defaultParameters(std::move(defaultParameters)) {
        }

        ASTNodeFunctionDefinition(const ASTNodeFunctionDefinition &other) : ASTNode(other) {
            this->m_name = other.m_name;
            this->m_parameterPack = other.m_parameterPack;

            for (const auto &[name, type] : other.m_params) {
                this->m_params.emplace_back(name, type->clone());
            }

            for (auto &statement : other.m_body) {
                this->m_body.push_back(statement->clone());
            }

            for (auto &statement : other.m_defaultParameters) {
                this->m_body.push_back(statement->clone());
            }
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeFunctionDefinition(*this));
        }

        [[nodiscard]] const std::string &getName() const {
            return this->m_name;
        }

        [[nodiscard]] const auto &getParams() const {
            return this->m_params;
        }

        [[nodiscard]] const auto &getBody() const {
            return this->m_body;
        }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override {
            using namespace ContentRegistry::PatternLanguage;

            ParameterCount paramCount;

            if (this->m_parameterPack.has_value())
                paramCount = ParameterCount::atLeast(this->m_params.size());
            else if (!this->m_defaultParameters.empty())
                paramCount = ParameterCount::between(this->m_params.size() - this->m_defaultParameters.size(), this->m_params.size());
            else
                paramCount = ParameterCount::exactly(this->m_params.size());

            std::vector<Token::Literal> evaluatedDefaultParams;
            for (const auto &param : this->m_defaultParameters) {
                const auto expression = param->evaluate(evaluator)->evaluate(evaluator);

                if (auto literal = dynamic_cast<ASTNodeLiteral *>(expression.get())) {
                    evaluatedDefaultParams.push_back(literal->getValue());
                } else {
                    LogConsole::abortEvaluation(hex::format("invalid default parameter for function {}", this->m_name), expression);
                }
            }

            evaluator->addCustomFunction(this->m_name, paramCount, evaluatedDefaultParams, [this](Evaluator *ctx, const std::vector<Token::Literal> &params) -> std::optional<Token::Literal> {
                std::vector<std::shared_ptr<Pattern>> variables;

                auto startOffset = ctx->dataOffset();
                ctx->pushScope(nullptr, variables);
                ON_SCOPE_EXIT {
                    ctx->popScope();
                    ctx->dataOffset() = startOffset;
                };

                if (this->m_parameterPack.has_value()) {
                    std::vector<Token::Literal> parameterPackContent;
                    for (u32 paramIndex = this->m_params.size(); paramIndex < params.size(); paramIndex++)
                        parameterPackContent.push_back(params[paramIndex]);

                    ctx->createParameterPack(this->m_parameterPack.value(), parameterPackContent);
                }

                for (u32 paramIndex = 0; paramIndex < this->m_params.size(); paramIndex++) {
                    const auto &[name, type] = this->m_params[paramIndex];

                    ctx->createVariable(name, type.get(), params[paramIndex]);
                    ctx->setVariable(name, params[paramIndex]);
                }

                for (auto &statement : this->m_body) {
                    auto result = statement->execute(ctx);

                    if (ctx->getCurrentControlFlowStatement() != ControlFlowStatement::None) {
                        switch (ctx->getCurrentControlFlowStatement()) {
                            case ControlFlowStatement::Break:
                                LogConsole::abortEvaluation("break statement not within a loop", statement);
                            case ControlFlowStatement::Continue:
                                LogConsole::abortEvaluation("continue statement not within a loop", statement);
                            default:
                                break;
                        }

                        ctx->setCurrentControlFlowStatement(ControlFlowStatement::None);
                        return result;
                    }
                }

                return {};
            });

            return nullptr;
        }


    private:
        std::string m_name;
        std::vector<std::pair<std::string, std::unique_ptr<ASTNode>>> m_params;
        std::vector<std::unique_ptr<ASTNode>> m_body;
        std::optional<std::string> m_parameterPack;
        std::vector<std::unique_ptr<ASTNode>> m_defaultParameters;
    };

}