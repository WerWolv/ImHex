#pragma once

#include <hex/pattern_language/ast/ast_node.hpp>
#include <hex/pattern_language/ast/ast_node_attribute.hpp>

#include <hex/pattern_language/patterns/pattern_pointer.hpp>

namespace hex::pl {

    class ASTNodePointerVariableDecl : public ASTNode,
                                       public Attributable {
    public:
        ASTNodePointerVariableDecl(std::string name, std::shared_ptr<ASTNodeTypeDecl> type, std::shared_ptr<ASTNodeTypeDecl> sizeType, std::unique_ptr<ASTNode> &&placementOffset = nullptr)
            : ASTNode(), m_name(std::move(name)), m_type(std::move(type)), m_sizeType(std::move(sizeType)), m_placementOffset(std::move(placementOffset)) { }

        ASTNodePointerVariableDecl(const ASTNodePointerVariableDecl &other) : ASTNode(other), Attributable(other) {
            this->m_name     = other.m_name;
            this->m_type     = other.m_type;
            this->m_sizeType = other.m_sizeType;

            if (other.m_placementOffset != nullptr)
                this->m_placementOffset = other.m_placementOffset->clone();
            else
                this->m_placementOffset = nullptr;
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodePointerVariableDecl(*this));
        }

        [[nodiscard]] const std::string &getName() const { return this->m_name; }
        [[nodiscard]] constexpr const std::shared_ptr<ASTNodeTypeDecl> &getType() const { return this->m_type; }
        [[nodiscard]] constexpr const std::shared_ptr<ASTNodeTypeDecl> &getSizeType() const { return this->m_sizeType; }
        [[nodiscard]] constexpr const std::unique_ptr<ASTNode> &getPlacementOffset() const { return this->m_placementOffset; }

        [[nodiscard]] std::vector<std::unique_ptr<Pattern>> createPatterns(Evaluator *evaluator) const override {
            auto startOffset = evaluator->dataOffset();

            if (this->m_placementOffset != nullptr) {
                const auto node   = this->m_placementOffset->evaluate(evaluator);
                const auto offset = dynamic_cast<ASTNodeLiteral *>(node.get());

                evaluator->dataOffset() = std::visit(overloaded {
                                                         [this](const std::string &) -> u64 { LogConsole::abortEvaluation("placement offset cannot be a string", this); },
                                                         [this](const std::shared_ptr<Pattern> &) -> u64 { LogConsole::abortEvaluation("placement offset cannot be a custom type", this); },
                                                         [](auto &&offset) -> u64 { return u64(offset); } },
                    offset->getValue());
            }

            auto pointerStartOffset = evaluator->dataOffset();

            const auto sizePatterns = this->m_sizeType->createPatterns(evaluator);
            const auto &sizePattern = sizePatterns.front();

            auto pattern = std::make_unique<PatternPointer>(evaluator, pointerStartOffset, sizePattern->getSize());
            pattern->setVariableName(this->m_name);

            auto pointerEndOffset = evaluator->dataOffset();

            {
                u128 pointerAddress = 0;
                evaluator->getProvider()->read(pattern->getOffset(), &pointerAddress, pattern->getSize());
                pointerAddress = hex::changeEndianess(pointerAddress, sizePattern->getSize(), sizePattern->getEndian());

                evaluator->dataOffset() = pointerStartOffset;

                pattern->setPointedAtAddress(pointerAddress);
                applyVariableAttributes(evaluator, this, pattern.get());

                evaluator->dataOffset() = pattern->getPointedAtAddress();

                auto pointedAtPatterns = this->m_type->createPatterns(evaluator);
                auto &pointedAtPattern = pointedAtPatterns.front();

                pattern->setPointedAtPattern(std::move(pointedAtPattern));
                pattern->setEndian(sizePattern->getEndian());
            }

            if (this->m_placementOffset != nullptr && !evaluator->isGlobalScope()) {
                evaluator->dataOffset() = startOffset;
            } else {
                evaluator->dataOffset() = pointerEndOffset;
            }

            return hex::moveToVector<std::unique_ptr<Pattern>>(std::move(pattern));
        }

    private:
        std::string m_name;
        std::shared_ptr<ASTNodeTypeDecl> m_type;
        std::shared_ptr<ASTNodeTypeDecl> m_sizeType;
        std::unique_ptr<ASTNode> m_placementOffset;
    };

}