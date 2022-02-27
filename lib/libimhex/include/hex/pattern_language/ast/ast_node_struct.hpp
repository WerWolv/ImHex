#pragma once

#include <hex/pattern_language/ast/ast_node.hpp>
#include <hex/pattern_language/ast/ast_node_attribute.hpp>

#include <hex/pattern_language/patterns/pattern_struct.hpp>

namespace hex::pl {

    class ASTNodeStruct : public ASTNode,
                          public Attributable {
    public:
        ASTNodeStruct() : ASTNode() { }

        ASTNodeStruct(const ASTNodeStruct &other) : ASTNode(other), Attributable(other) {
            for (const auto &otherMember : other.getMembers())
                this->m_members.push_back(otherMember->clone());
            for (const auto &otherInheritance : other.getInheritance())
                this->m_inheritance.push_back(otherInheritance->clone());
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeStruct(*this));
        }

        [[nodiscard]] std::vector<std::unique_ptr<Pattern>> createPatterns(Evaluator *evaluator) const override {
            auto pattern = std::make_unique<PatternStruct>(evaluator, evaluator->dataOffset(), 0);

            u64 startOffset = evaluator->dataOffset();
            std::vector<std::shared_ptr<Pattern>> memberPatterns;

            evaluator->pushScope(pattern.get(), memberPatterns);
            ON_SCOPE_EXIT {
                evaluator->popScope();
            };

            for (auto &inheritance : this->m_inheritance) {
                auto inheritancePatterns = inheritance->createPatterns(evaluator);
                auto &inheritancePattern = inheritancePatterns.front();

                if (auto structPattern = dynamic_cast<PatternStruct *>(inheritancePattern.get())) {
                    for (auto &member : structPattern->getMembers()) {
                        memberPatterns.push_back(member->clone());
                    }
                }
            }

            for (auto &member : this->m_members) {
                for (auto &memberPattern : member->createPatterns(evaluator)) {
                    memberPatterns.push_back(std::move(memberPattern));
                }
            }

            pattern->setMembers(std::move(memberPatterns));
            pattern->setSize(evaluator->dataOffset() - startOffset);

            applyTypeAttributes(evaluator, this, pattern.get());

            return hex::moveToVector<std::unique_ptr<Pattern>>(std::move(pattern));
        }

        [[nodiscard]] const std::vector<std::shared_ptr<ASTNode>> &getMembers() const { return this->m_members; }
        void addMember(std::shared_ptr<ASTNode> &&node) { this->m_members.push_back(std::move(node)); }

        [[nodiscard]] const std::vector<std::shared_ptr<ASTNode>> &getInheritance() const { return this->m_inheritance; }
        void addInheritance(std::shared_ptr<ASTNode> &&node) { this->m_inheritance.push_back(std::move(node)); }

    private:
        std::vector<std::shared_ptr<ASTNode>> m_members;
        std::vector<std::shared_ptr<ASTNode>> m_inheritance;
    };

}