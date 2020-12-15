#pragma once

#include <hex.hpp>

#include "helpers/utils.hpp"

#include <string>
#include <variant>

namespace hex::lang {

    class Token {
    public:
        enum class Type : u64 {
            Keyword,
            ValueType,
            Operator,
            Integer,
            Identifier,
            Separator
        };

        enum class Keyword {
            Struct,
            Union,
            Using,
            Enum,
            Bitfield,
            LittleEndian,
            BigEndian
        };

        enum class Operator {
            AtDeclaration,
            Assignment,
            Inherit,
            Plus,
            Minus,
            Star,
            Slash,
            ShiftLeft,
            ShiftRight,
            BitOr,
            BitAnd,
            BitXor
        };

        enum class ValueType {
            Unsigned8Bit        = 0x10,
            Signed8Bit          = 0x11,
            Unsigned16Bit       = 0x20,
            Signed16Bit         = 0x21,
            Unsigned32Bit       = 0x40,
            Signed32Bit         = 0x41,
            Unsigned64Bit       = 0x80,
            Signed64Bit         = 0x81,
            Unsigned128Bit      = 0x100,
            Signed128Bit        = 0x101,
            Character           = 0x13,
            Float               = 0x42,
            Double              = 0x82,
            CustomType          = 0x00,
            Padding             = 0x1F,

            Unsigned            = 0xFF00,
            Signed              = 0xFF01,
            FloatingPoint       = 0xFF02,
            Integer             = 0xFF03,
            Any                 = 0xFFFF
        };

        enum class Separator {
            RoundBracketOpen,
            RoundBracketClose,
            CurlyBracketOpen,
            CurlyBracketClose,
            SquareBracketOpen,
            SquareBracketClose,
            Comma,
            EndOfExpression,
            EndOfProgram
        };

        using ValueTypes = std::variant<Keyword, std::string, Operator, s128, ValueType, Separator>;

        Token(Type type, auto value, u32 lineNumber) : type(type), value(value), lineNumber(lineNumber) {

        }

        [[nodiscard]] constexpr static inline bool isUnsigned(const lang::Token::ValueType type) {
            return (static_cast<u32>(type) & 0x0F) == 0x00;
        }

        [[nodiscard]] constexpr static inline bool isSigned(const lang::Token::ValueType type) {
            return (static_cast<u32>(type) & 0x0F) == 0x01;
        }

        [[nodiscard]] constexpr static inline bool isFloatingPoint(const lang::Token::ValueType type) {
            return (static_cast<u32>(type) & 0x0F) == 0x02;
        }

        [[nodiscard]] constexpr static inline u32 getTypeSize(const lang::Token::ValueType type) {
            return static_cast<u32>(type) >> 4;
        }

        [[nodiscard]] constexpr static auto getTypeName(const lang::Token::ValueType type) {
            switch (type) {
                case ValueType::Signed8Bit:     return "s8";
                case ValueType::Signed16Bit:    return "s16";
                case ValueType::Signed32Bit:    return "s32";
                case ValueType::Signed64Bit:    return "s64";
                case ValueType::Signed128Bit:   return "s128";
                case ValueType::Unsigned8Bit:   return "u8";
                case ValueType::Unsigned16Bit:  return "u16";
                case ValueType::Unsigned32Bit:  return "u32";
                case ValueType::Unsigned64Bit:  return "u64";
                case ValueType::Unsigned128Bit: return "u128";
                case ValueType::Float:          return "float";
                case ValueType::Double:         return "double";
                case ValueType::Character:      return "char";
                default:                        return "< ??? >";
            }
        }

        bool operator==(const ValueTypes &other) const {
            if (this->type == Type::Integer || this->type == Type::Identifier)
                return true;
            else if (this->type == Type::ValueType) {
                auto otherValueType =   std::get_if<ValueType>(&other);
                auto valueType =        std::get_if<ValueType>(&this->value);

                if (otherValueType == nullptr) return false;
                if (valueType == nullptr) return false;

                if (*otherValueType == *valueType)
                    return true;
                else if (*otherValueType == ValueType::Any)
                    return *valueType != ValueType::CustomType && *valueType != ValueType::Padding;
                else if (*otherValueType == ValueType::Unsigned)
                    return isUnsigned(*valueType);
                else if (*otherValueType == ValueType::Signed)
                    return isSigned(*valueType);
                else if (*otherValueType == ValueType::FloatingPoint)
                    return isFloatingPoint(*valueType);
                else if (*otherValueType == ValueType::Integer)
                    return isUnsigned(*valueType) || isSigned(*valueType);
            }
            else
                return other == this->value;

            return false;
        }

        bool operator!=(const ValueTypes &other) const {
            return !operator==(other);
        }

        Type type;
        ValueTypes value;
        u32 lineNumber;

        [[nodiscard]] constexpr static inline bool isUnsigned(const TypeToken::Type type) {
            return (static_cast<u32>(type) & 0x0F) == 0x00;
        }

        [[nodiscard]] constexpr static inline bool isSigned(const TypeToken::Type type) {
            return (static_cast<u32>(type) & 0x0F) == 0x01;
        }

        [[nodiscard]] constexpr static inline bool isFloatingPoint(const TypeToken::Type type) {
            return (static_cast<u32>(type) & 0x0F) == 0x02;
        }

        [[nodiscard]] constexpr static inline u32 getTypeSize(const TypeToken::Type type) {
            return static_cast<u32>(type) >> 4;
        }
    };

}

#define COMPONENT(type, value) hex::lang::Token::Type::type, hex::lang::Token::type::value

#define KEYWORD_STRUCT                  COMPONENT(Keyword, Struct)
#define KEYWORD_UNION                   COMPONENT(Keyword, Union)
#define KEYWORD_USING                   COMPONENT(Keyword, Using)
#define KEYWORD_ENUM                    COMPONENT(Keyword, Enum)
#define KEYWORD_BITFIELD                COMPONENT(Keyword, Bitfield)
#define KEYWORD_LE                      COMPONENT(Keyword, LittleEndian)
#define KEYWORD_BE                      COMPONENT(Keyword, BigEndian)

#define INTEGER                         hex::lang::Token::Type::Integer, 0xFFFF'FFFF'FFFF'FFFF
#define IDENTIFIER                      hex::lang::Token::Type::Identifier, ""

#define OPERATOR_AT                     COMPONENT(Operator, AtDeclaration)
#define OPERATOR_ASSIGNMENT             COMPONENT(Operator, Assignment)
#define OPERATOR_INHERIT                COMPONENT(Operator, Inherit)
#define OPERATOR_PLUS                   COMPONENT(Operator, Plus)
#define OPERATOR_MINUS                  COMPONENT(Operator, Minus)
#define OPERATOR_STAR                   COMPONENT(Operator, Star)
#define OPERATOR_SLASH                  COMPONENT(Operator, Slash)
#define OPERATOR_SHIFTLEFT              COMPONENT(Operator, ShiftLeft)
#define OPERATOR_SHIFTRIGHT             COMPONENT(Operator, ShiftRight)
#define OPERATOR_BITOR                  COMPONENT(Operator, BitOr)
#define OPERATOR_BITAND                 COMPONENT(Operator, BitAnd)
#define OPERATOR_BITXOR                 COMPONENT(Operator, BitXor)

#define VALUETYPE_CUSTOMTYPE            COMPONENT(ValueType, CustomType)
#define VALUETYPE_PADDING               COMPONENT(ValueType, Padding)
#define VALUETYPE_UNSIGNED              COMPONENT(ValueType, Unsigned)
#define VALUETYPE_SIGNED                COMPONENT(ValueType, Signed)
#define VALUETYPE_FLOATINGPOINT         COMPONENT(ValueType, FloatingPoint)
#define VALUETYPE_INTEGER               COMPONENT(ValueType, Integer)
#define VALUETYPE_ANY                   COMPONENT(ValueType, Any)

#define SEPARATOR_ROUNDBRACKETOPEN      COMPONENT(Separator, RoundBracketOpen)
#define SEPARATOR_ROUNDBRACKETCLOSE     COMPONENT(Separator, RoundBracketClose)
#define SEPARATOR_CURLYBRACKETOPEN      COMPONENT(Separator, CurlyBracketOpen)
#define SEPARATOR_CURLYBRACKETCLOSE     COMPONENT(Separator, CurlyBracketClose)
#define SEPARATOR_SQUAREBRACKETOPEN     COMPONENT(Separator, SquareBracketOpen)
#define SEPARATOR_SQUAREBRACKETCLOSE    COMPONENT(Separator, SquareBracketClose)
#define SEPARATOR_COMMA                 COMPONENT(Separator, Comma)
#define SEPARATOR_ENDOFEXPRESSION       COMPONENT(Separator, EndOfExpression)
#define SEPARATOR_ENDOFPROGRAM          COMPONENT(Separator, EndOfProgram)