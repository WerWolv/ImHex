#pragma once

#include <optional>
#include <vector>

#include <hex/pattern_language/token.hpp>

namespace hex::pl {

    class ASTNode;
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

    class Clonable {
    public:
        [[nodiscard]]
        virtual ASTNode* clone() const = 0;
    };

    class ASTNode : public Clonable {
    public:
        constexpr ASTNode() = default;

        constexpr virtual ~ASTNode() = default;

        constexpr ASTNode(const ASTNode &) = default;

        [[nodiscard]] constexpr u32 getLineNumber() const { return this->m_lineNumber; }

        [[maybe_unused]] constexpr void setLineNumber(u32 lineNumber) { this->m_lineNumber = lineNumber; }

        [[nodiscard]] virtual ASTNode *evaluate(Evaluator *evaluator) const { return this->clone(); }

        [[nodiscard]] virtual std::vector<PatternData *> createPatterns(Evaluator *evaluator) const { return {}; }

        using FunctionResult = std::pair<bool, std::optional<Token::Literal>>;
        virtual FunctionResult execute(Evaluator *evaluator) { throw std::pair<u32, std::string>(this->getLineNumber(), "cannot execute non-function statement"); }

    private:
        u32 m_lineNumber = 1;
    };

}