#include "lang/evaluator.hpp"

namespace hex::lang {

    #define BUILTIN_FUNCTION(name) ASTNodeIntegerLiteral* Evaluator::name(std::vector<ASTNodeIntegerLiteral*> params)

    #define LITERAL_COMPARE(literal, cond) std::visit([&, this](auto &&literal) { return (cond) != 0; }, literal)

    BUILTIN_FUNCTION(findSequence) {
        auto& occurrenceIndex = params[0]->getValue();
        std::vector<u8> sequence;
        for (u32 i = 1; i < params.size(); i++) {
            sequence.push_back(std::visit([](auto &&value) -> u8 {
                if (value <= 0xFF)
                    return value;
                else
                    throwEvaluateError("sequence bytes need to fit into 1 byte", 1);
            }, params[i]->getValue()));
        }

        std::vector<u8> bytes(sequence.size(), 0x00);
        u32 occurrences = 0;
        for (u64 offset = 0; offset < this->m_provider->getSize() - sequence.size(); offset++) {
            this->m_provider->read(offset, bytes.data(), bytes.size());

            if (bytes == sequence) {
                if (LITERAL_COMPARE(occurrenceIndex, occurrenceIndex < occurrences)) {
                    occurrences++;
                    continue;
                }

                return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned64Bit, offset });
            }
        }

        throwEvaluateError("failed to find sequence", 1);
    }

    BUILTIN_FUNCTION(readUnsigned) {
        auto address = params[0]->getValue();
        auto size = params[1]->getValue();

        if (LITERAL_COMPARE(address, address >= this->m_provider->getActualSize()))
            throwEvaluateError("address out of range", 1);

        return std::visit([this](auto &&address, auto &&size) {
            if (size <= 0 || size > 16)
                throwEvaluateError("invalid read size", 1);

            u8 value[(u8)size];
            this->m_provider->read(address, value, size);

            switch ((u8)size) {
                case 1:  return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned8Bit,   hex::changeEndianess(*reinterpret_cast<u8*>(value), 1, this->getCurrentEndian()) });
                case 2:  return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned16Bit,  hex::changeEndianess(*reinterpret_cast<u16*>(value), 2, this->getCurrentEndian()) });
                case 4:  return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned32Bit,  hex::changeEndianess(*reinterpret_cast<u32*>(value), 4, this->getCurrentEndian()) });
                case 8:  return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned64Bit,  hex::changeEndianess(*reinterpret_cast<u64*>(value), 8, this->getCurrentEndian()) });
                case 16: return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned128Bit, hex::changeEndianess(*reinterpret_cast<u128*>(value), 16, this->getCurrentEndian()) });
                default: throwEvaluateError("invalid read size", 1);
            }
        }, address, size);
    }

    BUILTIN_FUNCTION(readSigned) {
        auto address = params[0]->getValue();
        auto size = params[1]->getValue();

        if (LITERAL_COMPARE(address, address >= this->m_provider->getActualSize()))
            throwEvaluateError("address out of range", 1);

        return std::visit([this](auto &&address, auto &&size) {
            if (size <= 0 || size > 16)
                throwEvaluateError("invalid read size", 1);

            u8 value[(u8)size];
            this->m_provider->read(address, value, size);

            switch ((u8)size) {
            case 1:  return new ASTNodeIntegerLiteral({ Token::ValueType::Signed8Bit,   hex::changeEndianess(*reinterpret_cast<s8*>(value), 1, this->getCurrentEndian()) });
            case 2:  return new ASTNodeIntegerLiteral({ Token::ValueType::Signed16Bit,  hex::changeEndianess(*reinterpret_cast<s16*>(value), 2, this->getCurrentEndian()) });
            case 4:  return new ASTNodeIntegerLiteral({ Token::ValueType::Signed32Bit,  hex::changeEndianess(*reinterpret_cast<s32*>(value), 4, this->getCurrentEndian()) });
            case 8:  return new ASTNodeIntegerLiteral({ Token::ValueType::Signed64Bit,  hex::changeEndianess(*reinterpret_cast<s64*>(value), 8, this->getCurrentEndian()) });
            case 16: return new ASTNodeIntegerLiteral({ Token::ValueType::Signed128Bit, hex::changeEndianess(*reinterpret_cast<s128*>(value), 16, this->getCurrentEndian()) });
            default: throwEvaluateError("invalid read size", 1);
        }
        }, address, size);
    }


    #undef BUILTIN_FUNCTION

}