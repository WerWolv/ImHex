#pragma once

#include <hex/pattern_language/ast/ast_node.hpp>
#include <hex/pattern_language/ast/ast_node_attribute.hpp>

#include <hex/pattern_language/patterns/pattern_enum.hpp>

namespace hex::pl {

    class ASTNodeEnum : public ASTNode,
                        public Attributable {
    public:
        explicit ASTNodeEnum(std::unique_ptr<ASTNode> &&underlyingType) : ASTNode(), m_underlyingType(std::move(underlyingType)) { }

        ASTNodeEnum(const ASTNodeEnum &other) : ASTNode(other), Attributable(other) {
            for (const auto &[name, entry] : other.getEntries())
                this->m_entries.emplace(name, entry->clone());
            this->m_underlyingType = other.m_underlyingType->clone();
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeEnum(*this));
        }

        [[nodiscard]] std::vector<std::unique_ptr<Pattern>> createPatterns(Evaluator *evaluator) const override {
            auto pattern = std::make_unique<PatternEnum>(evaluator, evaluator->dataOffset(), 0);

            std::vector<std::pair<Token::Literal, std::string>> enumEntries;
            for (const auto &[name, value] : this->m_entries) {
                const auto node = value->evaluate(evaluator);
                auto literal    = dynamic_cast<ASTNodeLiteral *>(node.get());

                enumEntries.emplace_back(literal->getValue(), name);
            }

            pattern->setEnumValues(enumEntries);

            const auto nodes = this->m_underlyingType->createPatterns(evaluator);
            auto &underlying = nodes.front();

            pattern->setSize(underlying->getSize());
            pattern->setEndian(underlying->getEndian());

            applyTypeAttributes(evaluator, this, pattern.get());

            return hex::moveToVector<std::unique_ptr<Pattern>>(std::move(pattern));
        }

        [[nodiscard]] const std::map<std::string, std::unique_ptr<ASTNode>> &getEntries() const { return this->m_entries; }
        void addEntry(const std::string &name, std::unique_ptr<ASTNode> &&expression) { this->m_entries.insert({ name, std::move(expression) }); }

        [[nodiscard]] const std::unique_ptr<ASTNode> &getUnderlyingType() { return this->m_underlyingType; }

    private:
        std::map<std::string, std::unique_ptr<ASTNode>> m_entries;
        std::unique_ptr<ASTNode> m_underlyingType;
    };

}