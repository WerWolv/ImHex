#pragma once

#include <vector>

namespace hex::pl {

    class ASTNodeAttribute;

    class PatternData;
    class Evaluator;

    class Attributable {
    protected:
        Attributable() = default;

        Attributable(const Attributable &) = default;

    public:

        void addAttribute(ASTNodeAttribute *attribute) {
            this->m_attributes.push_back(attribute);
        }

        [[nodiscard]] const auto &getAttributes() const {
            return this->m_attributes;
        }

    private:
        std::vector<ASTNodeAttribute *> m_attributes;
    };

    class ASTNode {
    public:
        constexpr ASTNode() = default;

        constexpr virtual ~ASTNode() = default;

        constexpr ASTNode(const ASTNode &) = default;

        [[nodiscard]] constexpr u32 getLineNumber() const { return this->m_lineNumber; }

        [[maybe_unused]] constexpr void setLineNumber(u32 lineNumber) { this->m_lineNumber = lineNumber; }

        [[nodiscard]] virtual ASTNode *clone() const = 0;

        [[nodiscard]] virtual ASTNode *evaluate(Evaluator *evaluator) const { return this->clone(); }

        [[nodiscard]] virtual std::vector<PatternData *> createPatterns(Evaluator *evaluator) const { return {}; }

    private:
        u32 m_lineNumber = 1;
    };

}