#pragma once

#include <hex/pattern_language/ast/ast_node.hpp>
#include <hex/pattern_language/ast/ast_node_parameter_pack.hpp>
#include <hex/pattern_language/ast/ast_node_mathematical_expression.hpp>
#include <hex/pattern_language/ast/ast_node_literal.hpp>

#include <thread>

namespace hex::pl {

    class ASTNodeFunctionCall : public ASTNode {
    public:
        explicit ASTNodeFunctionCall(std::string functionName, std::vector<std::unique_ptr<ASTNode>> &&params)
            : ASTNode(), m_functionName(std::move(functionName)), m_params(std::move(params)) { }

        ASTNodeFunctionCall(const ASTNodeFunctionCall &other) : ASTNode(other) {
            this->m_functionName = other.m_functionName;

            for (auto &param : other.m_params)
                this->m_params.push_back(param->clone());
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeFunctionCall(*this));
        }

        [[nodiscard]] const std::string &getFunctionName() {
            return this->m_functionName;
        }

        [[nodiscard]] const std::vector<std::unique_ptr<ASTNode>> &getParams() const {
            return this->m_params;
        }

        [[nodiscard]] std::vector<std::unique_ptr<Pattern>> createPatterns(Evaluator *evaluator) const override {

            this->execute(evaluator);

            return {};
        }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override {
            auto startOffset = evaluator->dataOffset();
            ON_SCOPE_EXIT { evaluator->dataOffset() = startOffset; };

            std::vector<Token::Literal> evaluatedParams;
            for (auto &param : this->m_params) {
                const auto expression = param->evaluate(evaluator)->evaluate(evaluator);

                if (auto literal = dynamic_cast<ASTNodeLiteral *>(expression.get())) {
                    evaluatedParams.push_back(literal->getValue());
                } else if (auto parameterPack = dynamic_cast<ASTNodeParameterPack *>(expression.get())) {
                    for (auto &value : parameterPack->getValues()) {
                        evaluatedParams.push_back(value);
                    }
                }
            }

            auto &customFunctions = evaluator->getCustomFunctions();
            auto functions        = ContentRegistry::PatternLanguage::getFunctions();

            for (auto &func : customFunctions)
                functions.insert(func);

            if (!functions.contains(this->m_functionName)) {
                if (this->m_functionName.starts_with("std::")) {
                    evaluator->getConsole().log(LogConsole::Level::Warning, "This function might be part of the standard library.\nYou can install the standard library though\nthe Content Store found under Help -> Content Store and then\ninclude the correct file.");
                }

                LogConsole::abortEvaluation(hex::format("call to unknown function '{}'", this->m_functionName), this);
            }

            using namespace ContentRegistry::PatternLanguage;

            auto function = functions[this->m_functionName];
            const auto &[min, max] = function.parameterCount;

            if (evaluatedParams.size() >= min && evaluatedParams.size() < max) {
                while (true) {
                    auto remainingParams = max - evaluatedParams.size();
                    if (remainingParams <= 0)
                        break;

                    auto offset = evaluatedParams.size() - min;
                    if (offset >= function.defaultParameters.size())
                        break;

                    evaluatedParams.push_back(function.defaultParameters[offset]);
                }
            }

            if (evaluatedParams.size() < min)
                LogConsole::abortEvaluation(hex::format("too few parameters for function '{0}'. Expected {1} at least", this->m_functionName, min), this);
            else if (evaluatedParams.size() > max)
                LogConsole::abortEvaluation(hex::format("too many parameters for function '{0}'. Expected {1} at most", this->m_functionName, max), this);

            try {
                if (function.dangerous && evaluator->getDangerousFunctionPermission() != DangerousFunctionPermission::Allow) {
                    evaluator->dangerousFunctionCalled();

                    while (evaluator->getDangerousFunctionPermission() == DangerousFunctionPermission::Ask) {
                        using namespace std::literals::chrono_literals;

                        std::this_thread::sleep_for(100ms);
                    }

                    if (evaluator->getDangerousFunctionPermission() == DangerousFunctionPermission::Deny) {
                        LogConsole::abortEvaluation(hex::format("calling of dangerous function '{}' is not allowed", this->m_functionName), this);
                    }
                }

                auto result = function.func(evaluator, evaluatedParams);

                if (result.has_value())
                    return std::unique_ptr<ASTNode>(new ASTNodeLiteral(std::move(result.value())));
                else
                    return std::unique_ptr<ASTNode>(new ASTNodeMathematicalExpression(nullptr, nullptr, Token::Operator::Plus));
            } catch (std::string &error) {
                LogConsole::abortEvaluation(error, this);
            }

            return nullptr;
        }

        FunctionResult execute(Evaluator *evaluator) const override {
            (void)this->evaluate(evaluator);

            return {};
        }

    private:
        std::string m_functionName;
        std::vector<std::unique_ptr<ASTNode>> m_params;
    };

}