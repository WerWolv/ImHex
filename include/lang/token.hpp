#pragma once

#include <hex.hpp>

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
            Float               = 0x42,
            Double              = 0x82,
            CustomType          = 0x00,
            Padding             = 0x1F,

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

        Token(Type type, auto value, u32 lineNumber) : type(type), value(value), lineNumber(lineNumber) {

        }

    private:
        using ValueTypes = std::variant<Keyword, std::string, Operator, s128, ValueType, Separator>;

    public:
        bool operator==(const ValueTypes &value) const {
            if (this->type == Type::Integer || this->type == Type::Identifier || (this->type == Type::ValueType && *std::get_if<ValueType>(&value) == ValueType::Any))
                return true;
            else
                return value == this->value;
        }

        bool operator!=(const ValueTypes &value) const {
            return !operator==(value);
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

#define INTEGER                         hex::lang::Token::Type::Integer, 0xFFFF'FFFF'FFFF'FFFF'FFFF'FFFF'FFFF'FFFF
#define IDENTIFIER                      hex::lang::Token::Type::Identifier, ""

#define OPERATOR_AT                     COMPONENT(Operator, AtDeclaration)
#define OPERATOR_ASSIGNMENT             COMPONENT(Operator, Assignment)
#define OPERATOR_INHERIT                COMPONENT(Operator, Inherit)
#define OPERATOR_PLUS                   COMPONENT(Operator, Plus)
#define OPERATOR_MINUS                  COMPONENT(Operator, Minus)
#define OPERATOR_STAR                   COMPONENT(Operator, Star)
#define OPERATOR_SLASH                  COMPONENT(Operator, Slash)

#define VALUETYPE_CUSTOMTYPE            COMPONENT(ValueType, CustomType)
#define VALUETYPE_PADDING               COMPONENT(ValueType, Padding)
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