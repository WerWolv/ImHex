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

        ~ASTNodeFunctionCall() override {
        }

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

        [[nodiscard]] std::vector<std::unique_ptr<PatternData>> createPatterns(Evaluator *evaluator) const override {

            this->execute(evaluator);

            return {};
        }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override {
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

            auto function = functions[this->m_functionName];
            if (function.parameterCount == ContentRegistry::PatternLanguage::UnlimitedParameters) {
                ;    // Don't check parameter count
            } else if (function.parameterCount & ContentRegistry::PatternLanguage::LessParametersThan) {
                if (evaluatedParams.size() >= (function.parameterCount & ~ContentRegistry::PatternLanguage::LessParametersThan))
                    LogConsole::abortEvaluation(hex::format("too many parameters for function '{0}'. Expected less than {1}", this->m_functionName, function.parameterCount & ~ContentRegistry::PatternLanguage::LessParametersThan), this);
            } else if (function.parameterCount & ContentRegistry::PatternLanguage::MoreParametersThan) {
                if (evaluatedParams.size() <= (function.parameterCount & ~ContentRegistry::PatternLanguage::MoreParametersThan))
                    LogConsole::abortEvaluation(hex::format("too few parameters for function '{0}'. Expected more than {1}", this->m_functionName, function.parameterCount & ~ContentRegistry::PatternLanguage::MoreParametersThan), this);
            } else if (function.parameterCount & ContentRegistry::PatternLanguage::ExactlyOrMoreParametersThan) {
                if (evaluatedParams.size() < (function.parameterCount & ~ContentRegistry::PatternLanguage::ExactlyOrMoreParametersThan))
                    LogConsole::abortEvaluation(hex::format("too few parameters for function '{0}'. Expected more than {1}", this->m_functionName, (function.parameterCount - 1) & ~ContentRegistry::PatternLanguage::ExactlyOrMoreParametersThan), this);
            } else if (function.parameterCount != evaluatedParams.size()) {
                LogConsole::abortEvaluation(hex::format("invalid number of parameters for function '{0}'. Expected {1}", this->m_functionName, function.parameterCount), this);
            }

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
                    return std::unique_ptr<ASTNode>(new ASTNodeLiteral(result.value()));
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