#pragma once

#include <hex/pattern_language/token.hpp>
#include <hex/pattern_language/evaluator.hpp>
#include <hex/pattern_language/pattern_data.hpp>
#include <hex/helpers/concepts.hpp>

namespace hex::pl {

    class PatternData;
    class Evaluator;

    class ASTNode : public Cloneable<ASTNode> {
    public:
        constexpr ASTNode() = default;

        constexpr virtual ~ASTNode() = default;

        constexpr ASTNode(const ASTNode &) = default;

        [[nodiscard]] constexpr u32 getLineNumber() const { return this->m_lineNumber; }

        [[maybe_unused]] constexpr void setLineNumber(u32 lineNumber) { this->m_lineNumber = lineNumber; }

        [[nodiscard]] virtual std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const { return this->clone(); }

        [[nodiscard]] virtual std::vector<std::unique_ptr<PatternData>> createPatterns(Evaluator *evaluator) const { return {}; }

        using FunctionResult = std::optional<Token::Literal>;
        virtual FunctionResult execute(Evaluator *evaluator) const { LogConsole::abortEvaluation("cannot execute non-function statement", this); }

    private:
        u32 m_lineNumber = 1;
    };

}