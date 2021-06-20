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

                   return new ASTNodeIntegerLiteral(offset);
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
                case 1:  return new ASTNodeIntegerLiteral(*reinterpret_cast<u8*>(value));
                case 2:  return new ASTNodeIntegerLiteral(*reinterpret_cast<u16*>(value));
                case 4:  return new ASTNodeIntegerLiteral(*reinterpret_cast<u32*>(value));
                case 8:  return new ASTNodeIntegerLiteral(*reinterpret_cast<u64*>(value));
                case 16: return new ASTNodeIntegerLiteral(*reinterpret_cast<u128*>(value));
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
                case 1:  return new ASTNodeIntegerLiteral(*reinterpret_cast<s8*>(value));
                case 2:  return new ASTNodeIntegerLiteral(*reinterpret_cast<s16*>(value));
                case 4:  return new ASTNodeIntegerLiteral(*reinterpret_cast<s32*>(value));
                case 8:  return new ASTNodeIntegerLiteral(*reinterpret_cast<s64*>(value));
                case 16: return new ASTNodeIntegerLiteral(*reinterpret_cast<s128*>(value));
                default: ctx.getConsole().abortEvaluation("invalid read size");
            }
            }, address, size);
        });

        /* assert(condition, message) */
        ContentRegistry::PatternLanguageFunctions::add("assert", 2, [](auto &ctx, auto params) {
            auto condition = AS_TYPE(ASTNodeIntegerLiteral, params[0])->getValue();
            auto message = AS_TYPE(ASTNodeStringLiteral, params[1])->getString();

            if (LITERAL_COMPARE(condition, condition == 0))
                ctx.getConsole().abortEvaluation(hex::format("assert failed \"{0}\"", message.data()));

            return nullptr;
        });

        /* warnAssert(condition, message) */
        ContentRegistry::PatternLanguageFunctions::add("warnAssert", 2, [](auto ctx, auto params) {
            auto condition = AS_TYPE(ASTNodeIntegerLiteral, params[0])->getValue();
            auto message = AS_TYPE(ASTNodeStringLiteral, params[1])->getString();

            if (LITERAL_COMPARE(condition, condition == 0))
                ctx.getConsole().log(LogConsole::Level::Warning, hex::format("assert failed \"{0}\"", message.data()));

            return nullptr;
        });

        /* print(values...) */
        ContentRegistry::PatternLanguageFunctions::add("print", ContentRegistry::PatternLanguageFunctions::MoreParametersThan | 0, [](auto &ctx, auto params) {
            std::string message;
            for (auto& param : params) {
                if (auto integerLiteral = dynamic_cast<ASTNodeIntegerLiteral*>(param); integerLiteral != nullptr) {
                    std::visit([&](auto &&value) {
                        using Type = std::remove_cvref_t<decltype(value)>;
                        if constexpr (std::is_same_v<Type, char>)
                            message += (char)value;
                        else if constexpr (std::is_same_v<Type, bool>)
                            message += value == 0 ? "false" : "true";
                        else if constexpr (std::is_unsigned_v<Type>)
                            message += std::to_string(static_cast<u64>(value));
                        else if constexpr (std::is_signed_v<Type>)
                            message += std::to_string(static_cast<s64>(value));
                        else if constexpr (std::is_floating_point_v<Type>)
                            message += std::to_string(value);
                        else
                            message += "< Custom Type >";
                    }, integerLiteral->getValue());
                }
                else if (auto stringLiteral = dynamic_cast<ASTNodeStringLiteral*>(param); stringLiteral != nullptr)
                    message += stringLiteral->getString();
            }

            ctx.getConsole().log(LogConsole::Level::Info, message);

            return nullptr;
        });

        /* alignTo(alignment, value) */
        ContentRegistry::PatternLanguageFunctions::add("alignTo", 2, [](auto &ctx, auto params) -> ASTNode* {
            auto alignment = AS_TYPE(ASTNodeIntegerLiteral, params[0])->getValue();
            auto value = AS_TYPE(ASTNodeIntegerLiteral, params[1])->getValue();

            auto result = std::visit([](auto &&alignment, auto &&value) {
                u64 remainder = u64(value) % u64(alignment);
                return remainder != 0 ? u64(value) + (u64(alignment) - remainder) : u64(value);
            }, alignment, value);

            return new ASTNodeIntegerLiteral(u64(result));
        });

        /* dataSize() */
        ContentRegistry::PatternLanguageFunctions::add("dataSize", ContentRegistry::PatternLanguageFunctions::NoParameters, [](auto &ctx, auto params) -> ASTNode* {
            return new ASTNodeIntegerLiteral(u64(SharedData::currentProvider->getActualSize()));
        });
    }

}