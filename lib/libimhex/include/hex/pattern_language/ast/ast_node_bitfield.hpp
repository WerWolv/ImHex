#pragma once

#include <hex/pattern_language/ast/ast_node.hpp>
#include <hex/pattern_language/ast/ast_node_attribute.hpp>

#include <hex/pattern_language/patterns/pattern_bitfield.hpp>

namespace hex::pl {

    class ASTNodeBitfield : public ASTNode,
                            public Attributable {
    public:
        ASTNodeBitfield() : ASTNode() { }

        ASTNodeBitfield(const ASTNodeBitfield &other) : ASTNode(other), Attributable(other) {
            for (const auto &[name, entry] : other.getEntries())
                this->m_entries.emplace_back(name, entry->clone());
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeBitfield(*this));
        }

        [[nodiscard]] const std::vector<std::pair<std::string, std::unique_ptr<ASTNode>>> &getEntries() const { return this->m_entries; }
        void addEntry(const std::string &name, std::unique_ptr<ASTNode> &&size) { this->m_entries.emplace_back(name, std::move(size)); }

        [[nodiscard]] std::vector<std::unique_ptr<Pattern>> createPatterns(Evaluator *evaluator) const override {
            auto pattern = std::make_unique<PatternBitfield>(evaluator, evaluator->dataOffset(), 0);

            size_t bitOffset = 0;
            std::vector<std::shared_ptr<Pattern>> fields;

            bool isLeftToRight = false;
            if (this->hasAttribute("left_to_right", false))
                isLeftToRight = true;
            else if (this->hasAttribute("right_to_left", false))
                isLeftToRight = false;

            std::vector<std::pair<std::string, ASTNode *>> entries;
            for (const auto &[name, entry] : this->m_entries)
                entries.push_back({ name, entry.get() });

            if (isLeftToRight)
                std::reverse(entries.begin(), entries.end());

            evaluator->pushScope(pattern.get(), fields);
            ON_SCOPE_EXIT {
                evaluator->popScope();
            };

            for (auto &[name, bitSizeNode] : entries) {
                auto literal = bitSizeNode->evaluate(evaluator);

                u8 bitSize = std::visit(overloaded {
                                            [this](const std::string &) -> u8 { LogConsole::abortEvaluation("bitfield field size cannot be a string", this); },
                                            [this](const std::shared_ptr<Pattern> &) -> u8 { LogConsole::abortEvaluation("bitfield field size cannot be a custom type", this); },
                                            [](auto &&offset) -> u8 { return static_cast<u8>(offset); } },
                    dynamic_cast<ASTNodeLiteral *>(literal.get())->getValue());

                // If a field is named padding, it was created through a padding expression and only advances the bit position
                if (name != "padding") {
                    auto field = std::make_unique<PatternBitfieldField>(evaluator, evaluator->dataOffset(), bitOffset, bitSize, pattern.get());
                    field->setVariableName(name);

                    fields.push_back(std::move(field));
                }

                bitOffset += bitSize;
            }

            pattern->setSize((bitOffset + 7) / 8);
            pattern->setFields(std::move(fields));

            evaluator->dataOffset() += pattern->getSize();

            applyTypeAttributes(evaluator, this, pattern.get());

            return hex::moveToVector<std::unique_ptr<Pattern>>(std::move(pattern));
        }

    private:
        std::vector<std::pair<std::string, std::unique_ptr<ASTNode>>> m_entries;
    };

}