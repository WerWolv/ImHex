#pragma once

#include <hex/pattern_language/ast/ast_node.hpp>

namespace hex::pl {

    class ASTNodeCast : public ASTNode {
    public:
        ASTNodeCast(std::unique_ptr<ASTNode> &&value, std::unique_ptr<ASTNode> &&type) : m_value(std::move(value)), m_type(std::move(type)) { }

        ASTNodeCast(const ASTNodeCast &other) : ASTNode(other) {
            this->m_value = other.m_value->clone();
            this->m_type  = other.m_type->clone();
        }

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeCast(*this));
        }

        [[nodiscard]] std::unique_ptr<ASTNode> evaluate(Evaluator *evaluator) const override {
            auto evaluatedValue = this->m_value->evaluate(evaluator);
            auto evaluatedType  = this->m_type->evaluate(evaluator);

            auto literal = dynamic_cast<ASTNodeLiteral *>(evaluatedValue.get());
            auto type    = dynamic_cast<ASTNodeBuiltinType *>(evaluatedType.get())->getType();

            auto startOffset = evaluator->dataOffset();

            auto typePatterns = this->m_type->createPatterns(evaluator);
            auto &typePattern = typePatterns.front();

            return std::unique_ptr<ASTNode>(std::visit(overloaded {
                                                           [&, this](Pattern *value) -> ASTNode * { LogConsole::abortEvaluation(hex::format("cannot cast custom type '{}' to '{}'", value->getTypeName(), Token::getTypeName(type)), this); },
                                                           [&, this](const std::string &) -> ASTNode * { LogConsole::abortEvaluation(hex::format("cannot cast string to '{}'", Token::getTypeName(type)), this); },
                                                           [&, this](auto &&value) -> ASTNode * {
                                                               auto endianAdjustedValue = hex::changeEndianess(value, typePattern->getSize(), typePattern->getEndian());
                                                               switch (type) {
                                                                   case Token::ValueType::Unsigned8Bit:
                                                                       return new ASTNodeLiteral(u128(u8(endianAdjustedValue)));
                                                                   case Token::ValueType::Unsigned16Bit:
                                                                       return new ASTNodeLiteral(u128(u16(endianAdjustedValue)));
                                                                   case Token::ValueType::Unsigned32Bit:
                                                                       return new ASTNodeLiteral(u128(u32(endianAdjustedValue)));
                                                                   case Token::ValueType::Unsigned64Bit:
                                                                       return new ASTNodeLiteral(u128(u64(endianAdjustedValue)));
                                                                   case Token::ValueType::Unsigned128Bit:
                                                                       return new ASTNodeLiteral(u128(endianAdjustedValue));
                                                                   case Token::ValueType::Signed8Bit:
                                                                       return new ASTNodeLiteral(i128(i8(endianAdjustedValue)));
                                                                   case Token::ValueType::Signed16Bit:
                                                                       return new ASTNodeLiteral(i128(i16(endianAdjustedValue)));
                                                                   case Token::ValueType::Signed32Bit:
                                                                       return new ASTNodeLiteral(i128(i32(endianAdjustedValue)));
                                                                   case Token::ValueType::Signed64Bit:
                                                                       return new ASTNodeLiteral(i128(i64(endianAdjustedValue)));
                                                                   case Token::ValueType::Signed128Bit:
                                                                       return new ASTNodeLiteral(i128(endianAdjustedValue));
                                                                   case Token::ValueType::Float:
                                                                       return new ASTNodeLiteral(double(float(endianAdjustedValue)));
                                                                   case Token::ValueType::Double:
                                                                       return new ASTNodeLiteral(double(endianAdjustedValue));
                                                                   case Token::ValueType::Character:
                                                                       return new ASTNodeLiteral(char(endianAdjustedValue));
                                                                   case Token::ValueType::Character16:
                                                                       return new ASTNodeLiteral(u128(char16_t(endianAdjustedValue)));
                                                                   case Token::ValueType::Boolean:
                                                                       return new ASTNodeLiteral(bool(endianAdjustedValue));
                                                                   case Token::ValueType::String:
                                                                       {
                                                                           std::string string(sizeof(value), '\x00');
                                                                           std::memcpy(string.data(), &value, string.size());
                                                                           hex::trim(string);

                                                                           if (typePattern->getEndian() != std::endian::native)
                                                                               std::reverse(string.begin(), string.end());

                                                                           return new ASTNodeLiteral(string);
                                                                       }
                                                                   default:
                                                                       LogConsole::abortEvaluation(hex::format("cannot cast value to '{}'", Token::getTypeName(type)), this);
                                                               }
                                                           },
                                                       },
                literal->getValue()));
        }

    private:
        std::unique_ptr<ASTNode> m_value;
        std::unique_ptr<ASTNode> m_type;
    };

}