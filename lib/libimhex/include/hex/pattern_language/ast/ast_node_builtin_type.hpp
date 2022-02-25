#pragma once

#include <hex/pattern_language/ast/ast_node.hpp>

namespace hex::pl {

    class ASTNodeBuiltinType : public ASTNode {
    public:
        constexpr explicit ASTNodeBuiltinType(Token::ValueType type)
            : ASTNode(), m_type(type) { }

        [[nodiscard]] constexpr const auto &getType() const { return this->m_type; }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeBuiltinType(*this));
        }

        [[nodiscard]] std::vector<std::unique_ptr<PatternData>> createPatterns(Evaluator *evaluator) const override {
            auto offset = evaluator->dataOffset();
            auto size   = Token::getTypeSize(this->m_type);

            evaluator->dataOffset() += size;

            std::unique_ptr<PatternData> pattern;
            if (Token::isUnsigned(this->m_type))
                pattern = std::unique_ptr<PatternData>(new PatternDataUnsigned(evaluator, offset, size));
            else if (Token::isSigned(this->m_type))
                pattern = std::unique_ptr<PatternData>(new PatternDataSigned(evaluator, offset, size));
            else if (Token::isFloatingPoint(this->m_type))
                pattern = std::unique_ptr<PatternData>(new PatternDataFloat(evaluator, offset, size));
            else if (this->m_type == Token::ValueType::Boolean)
                pattern = std::unique_ptr<PatternData>(new PatternDataBoolean(evaluator, offset));
            else if (this->m_type == Token::ValueType::Character)
                pattern = std::unique_ptr<PatternData>(new PatternDataCharacter(evaluator, offset));
            else if (this->m_type == Token::ValueType::Character16)
                pattern = std::unique_ptr<PatternData>(new PatternDataCharacter16(evaluator, offset));
            else if (this->m_type == Token::ValueType::Padding)
                pattern = std::unique_ptr<PatternData>(new PatternDataPadding(evaluator, offset, 1));
            else if (this->m_type == Token::ValueType::String)
                pattern = std::unique_ptr<PatternData>(new PatternDataString(evaluator, offset, 1));
            else if (this->m_type == Token::ValueType::Auto)
                return {};
            else
                LogConsole::abortEvaluation("invalid built-in type", this);

            pattern->setTypeName(Token::getTypeName(this->m_type));

            return hex::moveToVector(std::move(pattern));
        }

    private:
        const Token::ValueType m_type;
    };

}