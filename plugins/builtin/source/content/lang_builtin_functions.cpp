#include <hex/plugin.hpp>

#include <hex/lang/ast_node.hpp>
#include <hex/lang/log_console.hpp>
#include <hex/lang/evaluator.hpp>

#include <hex/helpers/utils.hpp>

#include <vector>

namespace hex::plugin::builtin {

    #define LITERAL_COMPARE(literal, cond) std::visit([&](auto &&literal) { return (cond) != 0; }, literal)
    #define AS_TYPE(type, value) ctx.template asType<type>(value)

    void registerPatternLanguageFunctions() {
        using namespace hex::lang;

        /* findSequence(occurrenceIndex, byte...) */
        ContentRegistry::PatternLanguageFunctions::add("findSequence", ContentRegistry::PatternLanguageFunctions::MoreParametersThan | 1, [](auto &ctx, auto params) {
           auto& occurrenceIndex = AS_TYPE(ASTNodeIntegerLiteral, params[0])->getValue();
           std::vector<u8> sequence;
           for (u32 i = 1; i < params.size(); i++) {
               sequence.push_back(std::visit([&](auto &&value) -> u8 {
                   if (value <= 0xFF)
                       return value;
                   else
                       ctx.getConsole().abortEvaluation("sequence bytes need to fit into 1 byte");
               }, AS_TYPE(ASTNodeIntegerLiteral, params[i])->getValue()));
           }

           std::vector<u8> bytes(sequence.size(), 0x00);
           u32 occurrences = 0;
           for (u64 offset = 0; offset < SharedData::currentProvider->getSize() - sequence.size(); offset++) {
               SharedData::currentProvider->read(offset, bytes.data(), bytes.size());

               if (bytes == sequence) {
                   if (LITERAL_COMPARE(occurrenceIndex, occurrences < occurrenceIndex)) {
                       occurrences++;
                       continue;
                   }

                   return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned64Bit, offset });
               }
           }

           ctx.getConsole().abortEvaluation("failed to find sequence");
       });

        /* readUnsigned(address, size) */
        ContentRegistry::PatternLanguageFunctions::add("readUnsigned", 2, [](auto &ctx, auto params) {
            auto address = AS_TYPE(ASTNodeIntegerLiteral, params[0])->getValue();
            auto size = AS_TYPE(ASTNodeIntegerLiteral, params[1])->getValue();

            if (LITERAL_COMPARE(address, address >= SharedData::currentProvider->getActualSize()))
                ctx.getConsole().abortEvaluation("address out of range");

            return std::visit([&](auto &&address, auto &&size) {
                if (size <= 0 || size > 16)
                    ctx.getConsole().abortEvaluation("invalid read size");

                u8 value[(u8)size];
                SharedData::currentProvider->read(address, value, size);

                switch ((u8)size) {
                case 1:  return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned8Bit,   *reinterpret_cast<u8*>(value)   });
                case 2:  return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned16Bit,  *reinterpret_cast<u16*>(value)  });
                case 4:  return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned32Bit,  *reinterpret_cast<u32*>(value)  });
                case 8:  return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned64Bit,  *reinterpret_cast<u64*>(value)  });
                case 16: return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned128Bit, *reinterpret_cast<u128*>(value) });
                default: ctx.getConsole().abortEvaluation("invalid read size");
            }
            }, address, size);
        });

        /* readSigned(address, size) */
        ContentRegistry::PatternLanguageFunctions::add("readSigned", 2, [](auto &ctx, auto params) {
            auto address = AS_TYPE(ASTNodeIntegerLiteral, params[0])->getValue();
            auto size = AS_TYPE(ASTNodeIntegerLiteral, params[1])->getValue();

            if (LITERAL_COMPARE(address, address >= SharedData::currentProvider->getActualSize()))
                ctx.getConsole().abortEvaluation("address out of range");

            return std::visit([&](auto &&address, auto &&size) {
                if (size <= 0 || size > 16)
                    ctx.getConsole().abortEvaluation("invalid read size");

                u8 value[(u8)size];
                SharedData::currentProvider->read(address, value, size);

                switch ((u8)size) {
                case 1:  return new ASTNodeIntegerLiteral({ Token::ValueType::Signed8Bit,   *reinterpret_cast<s8*>(value)   });
                case 2:  return new ASTNodeIntegerLiteral({ Token::ValueType::Signed16Bit,  *reinterpret_cast<s16*>(value)  });
                case 4:  return new ASTNodeIntegerLiteral({ Token::ValueType::Signed32Bit,  *reinterpret_cast<s32*>(value)  });
                case 8:  return new ASTNodeIntegerLiteral({ Token::ValueType::Signed64Bit,  *reinterpret_cast<s64*>(value)  });
                case 16: return new ASTNodeIntegerLiteral({ Token::ValueType::Signed128Bit, *reinterpret_cast<s128*>(value) });
                default: ctx.getConsole().abortEvaluation("invalid read size");
            }
            }, address, size);
        });

        /* assert(condition, message) */
        ContentRegistry::PatternLanguageFunctions::add("assert", 2, [](auto &ctx, auto params) {
            auto condition = AS_TYPE(ASTNodeIntegerLiteral, params[0])->getValue();
            auto message = AS_TYPE(ASTNodeStringLiteral, params[1])->getString();

            if (LITERAL_COMPARE(condition, condition == 0))
                ctx.getConsole().abortEvaluation(hex::format("assert failed \"%s\"", message.data()));

            return nullptr;
        });

        /* warnAssert(condition, message) */
        ContentRegistry::PatternLanguageFunctions::add("warnAssert", 2, [](auto ctx, auto params) {
            auto condition = AS_TYPE(ASTNodeIntegerLiteral, params[0])->getValue();
            auto message = AS_TYPE(ASTNodeStringLiteral, params[1])->getString();

            if (LITERAL_COMPARE(condition, condition == 0))
                ctx.getConsole().log(LogConsole::Level::Warning, hex::format("assert failed \"%s\"", message.data()));

            return nullptr;
        });

        /* print(values...) */
        ContentRegistry::PatternLanguageFunctions::add("print", ContentRegistry::PatternLanguageFunctions::MoreParametersThan | 0, [](auto &ctx, auto params) {
            std::string message;
            for (auto& param : params) {
                if (auto integerLiteral = dynamic_cast<ASTNodeIntegerLiteral*>(param); integerLiteral != nullptr) {
                    switch (integerLiteral->getType()) {
                        case Token::ValueType::Character:       message += std::get<s8>(integerLiteral->getValue()); break;
                        case Token::ValueType::Unsigned8Bit:    message += std::to_string(std::get<u8>(integerLiteral->getValue())); break;
                        case Token::ValueType::Signed8Bit:      message += std::to_string(std::get<s8>(integerLiteral->getValue())); break;
                        case Token::ValueType::Unsigned16Bit:   message += std::to_string(std::get<u16>(integerLiteral->getValue())); break;
                        case Token::ValueType::Signed16Bit:     message += std::to_string(std::get<s16>(integerLiteral->getValue())); break;
                        case Token::ValueType::Unsigned32Bit:   message += std::to_string(std::get<u32>(integerLiteral->getValue())); break;
                        case Token::ValueType::Signed32Bit:     message += std::to_string(std::get<s32>(integerLiteral->getValue())); break;
                        case Token::ValueType::Unsigned64Bit:   message += std::to_string(std::get<u64>(integerLiteral->getValue())); break;
                        case Token::ValueType::Signed64Bit:     message += std::to_string(std::get<s64>(integerLiteral->getValue())); break;
                        case Token::ValueType::Unsigned128Bit:  message += hex::to_string(std::get<u128>(integerLiteral->getValue())); break;
                        case Token::ValueType::Signed128Bit:    message += hex::to_string(std::get<s128>(integerLiteral->getValue())); break;
                        case Token::ValueType::Float:           message += std::to_string(std::get<float>(integerLiteral->getValue())); break;
                        case Token::ValueType::Double:          message += std::to_string(std::get<double>(integerLiteral->getValue())); break;
                        case Token::ValueType::Boolean:         message += std::get<s32>(integerLiteral->getValue()) ? "true" : "false"; break;
                        case Token::ValueType::CustomType:      message += "< Custom Type >"; break;
                    }
                }
                else if (auto stringLiteral = dynamic_cast<ASTNodeStringLiteral*>(param); stringLiteral != nullptr)
                    message += stringLiteral->getString();
            }

            ctx.getConsole().log(LogConsole::Level::Info, message);

            return nullptr;
        });

        /* addressof(rValueString) */
        ContentRegistry::PatternLanguageFunctions::add("addressof", 1, [](auto &ctx, auto params) -> ASTNode* {
            auto name = AS_TYPE(ASTNodeStringLiteral, params[0])->getString();

            std::vector<std::string> path = splitString(name, ".");
            auto pattern = ctx.patternFromName(path);

            return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned64Bit, u64(pattern->getOffset()) });
        });

        /* sizeof(rValueString) */
        ContentRegistry::PatternLanguageFunctions::add("sizeof", 1, [](auto &ctx, auto params) -> ASTNode* {
            auto name = AS_TYPE(ASTNodeStringLiteral, params[0])->getString();

            std::vector<std::string> path = splitString(name, ".");
            auto pattern = ctx.patternFromName(path);

            return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned64Bit, u64(pattern->getSize()) });
        });

        /* nextAfter(rValueString) */
        ContentRegistry::PatternLanguageFunctions::add("nextAfter", 1, [](auto &ctx, auto params) -> ASTNode* {
            auto name = AS_TYPE(ASTNodeStringLiteral, params[0])->getString();

            std::vector<std::string> path = splitString(name, ".");
            auto pattern = ctx.patternFromName(path);

            return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned64Bit, u64(pattern->getOffset() + pattern->getSize()) });
        });

        /* alignTo(alignment, value) */
        ContentRegistry::PatternLanguageFunctions::add("alignTo", 2, [](auto &ctx, auto params) -> ASTNode* {
            auto alignment = AS_TYPE(ASTNodeIntegerLiteral, params[0])->getValue();
            auto value = AS_TYPE(ASTNodeIntegerLiteral, params[1])->getValue();

            auto result = std::visit([](auto &&alignment, auto &&value) {
                u64 remainder = u64(value) % u64(alignment);
                return remainder != 0 ? u64(value) + (u64(alignment) - remainder) : u64(value);
            }, alignment, value);

            return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned64Bit, u64(result) });
        });
    }

}