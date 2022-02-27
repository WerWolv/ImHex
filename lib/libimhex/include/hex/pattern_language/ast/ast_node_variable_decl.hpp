#pragma once

#include <hex/pattern_language/ast/ast_node.hpp>
#include <hex/pattern_language/ast/ast_node_attribute.hpp>
#include <hex/pattern_language/ast/ast_node_literal.hpp>

namespace hex::pl {

    class ASTNodeVariableDecl : public ASTNode,
                                public Attributable {
    public:
        ASTNodeVariableDecl(std::string name, std::unique_ptr<ASTNode> &&type, std::unique_ptr<ASTNode> &&placementOffset = nullptr, bool inVariable = false, bool outVariable = false)
            : ASTNode(), m_name(std::move(name)), m_type(std::move(type)), m_placementOffset(std::move(placementOffset)), m_inVariable(inVariable), m_outVariable(outVariable) { }

        ASTNodeVariableDecl(const ASTNodeVariableDecl &other) : ASTNode(other), Attributable(other) {
            this->m_name = other.m_name;
            this->m_type = other.m_type->clone();

            if (other.m_placementOffset != nullptr)
                this->m_placementOffset = other.m_placementOffset->clone();
            else
                this->m_placementOffset = nullptr;

            this->m_inVariable  = other.m_inVariable;
            this->m_outVariable = other.m_outVariable;
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeVariableDecl(*this));
        }

        [[nodiscard]] const std::string &getName() const { return this->m_name; }
        [[nodiscard]] constexpr const std::unique_ptr<ASTNode> &getType() const { return this->m_type; }
        [[nodiscard]] constexpr const std::unique_ptr<ASTNode> &getPlacementOffset() const { return this->m_placementOffset; }

        [[nodiscard]] constexpr bool isInVariable() const { return this->m_inVariable; }
        [[nodiscard]] constexpr bool isOutVariable() const { return this->m_outVariable; }

        [[nodiscard]] std::vector<std::unique_ptr<Pattern>> createPatterns(Evaluator *evaluator) const override {
            u64 startOffset = evaluator->dataOffset();

            if (this->m_placementOffset != nullptr) {
                const auto node   = this->m_placementOffset->evaluate(evaluator);
                const auto offset = dynamic_cast<ASTNodeLiteral *>(node.get());

                evaluator->dataOffset() = std::visit(overloaded {
                                                         [this](const std::string &) -> u64 { LogConsole::abortEvaluation("placement offset cannot be a string", this); },
                                                         [this](const std::shared_ptr<Pattern> &) -> u64 { LogConsole::abortEvaluation("placement offset cannot be a custom type", this); },
                                                         [](auto &&offset) -> u64 { return offset; } },
                    offset->getValue());
            }

            auto patterns = this->m_type->createPatterns(evaluator);
            auto &pattern = patterns.front();
            pattern->setVariableName(this->m_name);

            applyVariableAttributes(evaluator, this, pattern.get());

            if (this->m_placementOffset != nullptr && !evaluator->isGlobalScope()) {
                evaluator->dataOffset() = startOffset;
            }

            return hex::moveToVector<std::unique_ptr<Pattern>>(std::move(pattern));
        }

        FunctionResult execute(Evaluator *evaluator) const override {
            evaluator->createVariable(this->getName(), this->getType().get());

            return std::nullopt;
        }

    private:
        std::string m_name;
        std::unique_ptr<ASTNode> m_type;
        std::unique_ptr<ASTNode> m_placementOffset;

        bool m_inVariable = false, m_outVariable = false;
    };

}