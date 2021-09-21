#pragma once

#include <hex.hpp>

#include <utility>
#include <string>
#include <variant>

#include <hex/helpers/utils.hpp>

namespace hex::pl {

    class PatternData;

    class Token {
    public:
        enum class Type : u64 {
            Keyword,
            ValueType,
            Operator,
            Integer,
            String,
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
            BigEndian,
            If,
            Else,
            Parent,
            This,
            While,
            Function,
            Return,
            Namespace
        };

        enum class Operator {
            AtDeclaration,
            Assignment,
            Inherit,
            Plus,
            Minus,
            Star,
            Slash,
            Percent,
            ShiftLeft,
            ShiftRight,
            BitOr,
            BitAnd,
            BitXor,
            BitNot,
            BoolEquals,
            BoolNotEquals,
            BoolGreaterThan,
            BoolLessThan,
            BoolGreaterThanOrEquals,
            BoolLessThanOrEquals,
            BoolAnd,
            BoolOr,
            BoolXor,
            BoolNot,
            TernaryConditional,
            Dollar,
            AddressOf,
            SizeOf,
            ScopeResolution
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
            Character16         = 0x23,
            Boolean             = 0x14,
            Float               = 0x42,
            Double              = 0x82,
            String              = 0x15,
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
            Dot,
            EndOfExpression,
            EndOfProgram
        };

        struct Identifier {
            explicit Identifier(std::string identifier) : m_identifier(std::move(identifier)) { }

            [[nodiscard]]
            const std::string &get() const { return this->m_identifier; }

            auto operator<=>(const Identifier&) const = default;
            bool operator==(const Identifier&) const = default;

        private:
            std::string m_identifier;
        };

        using Literal = std::variant<char, bool, u128, s128, double, std::string, PatternData*>;
        using ValueTypes = std::variant<Keyword, Identifier, Operator, Literal, ValueType, Separator>;

        Token(Type type, auto value, u32 lineNumber) : type(type), value(value), lineNumber(lineNumber) {

        }

        [[nodiscard]] constexpr static inline bool isUnsigned(const ValueType type) {
            return (static_cast<u32>(type) & 0x0F) == 0x00;
        }

        [[nodiscard]] constexpr static inline bool isSigned(const ValueType type) {
            return (static_cast<u32>(type) & 0x0F) == 0x01;
        }

        [[nodiscard]] constexpr static inline bool isFloatingPoint(const ValueType type) {
            return (static_cast<u32>(type) & 0x0F) == 0x02;
        }

        [[nodiscard]] constexpr static inline u32 getTypeSize(const ValueType type) {
            return static_cast<u32>(type) >> 4;
        }

        static u128 literalToUnsigned(const pl::Token::Literal &literal) {
            return std::visit(overloaded {
                                      [](std::string) -> u128 { throw std::string("expected integral type, got string"); },
                                      [](PatternData*) -> u128 { throw std::string("expected integral type, got custom type"); },
                                      [](auto &&value) -> u128 { return value; }
                              },
                              literal);
        }

        static s128 literalToSigned(const pl::Token::Literal &literal) {
            return std::visit(overloaded {
                                      [](std::string) -> s128 { throw std::string("expected integral type, got string"); },
                                      [](PatternData*) -> s128 { throw std::string("expected integral type, got custom type"); },
                                      [](auto &&value) -> s128 { return value; }
                              },
                              literal);
        }

        static double literalToFloatingPoint(const pl::Token::Literal &literal) {
            return std::visit(overloaded {
                                      [](std::string) -> double { throw std::string("expected integral type, got string"); },
                                      [](PatternData*) -> double { throw std::string("expected integral type, got custom type"); },
                                      [](auto &&value) -> double { return value; }
                              },
                              literal);
        }

        static bool literalToBoolean(const pl::Token::Literal &literal) {
            return std::visit(overloaded {
                                      [](std::string) -> bool { throw std::string("expected integral type, got string"); },
                                      [](PatternData*) -> bool { throw std::string("expected integral type, got custom type"); },
                                      [](auto &&value) -> bool { return value != 0; }
                              },
                              literal);
        }

        static std::string literalToString(const pl::Token::Literal &literal, bool cast) {
            if (!cast && std::get_if<std::string>(&literal) == nullptr)
                throw std::string("expected string type, got integral");

            return std::visit(overloaded {
                                      [](std::string value) -> std::string { return value; },
                                      [](u128 value)   -> std::string { return std::to_string(u64(value)); },
                                      [](s128 value)   -> std::string { return std::to_string(s64(value)); },
                                      [](bool value)   -> std::string { return value ? "true" : "false"; },
                                      [](char value)   -> std::string { return std::string() + value; },
                                      [](PatternData*) -> std::string { throw std::string("expected integral type, got custom type"); },
                                      [](auto &&value) -> std::string { return std::to_string(value); }
                              },
                              literal);
        }

        [[nodiscard]] constexpr static auto getTypeName(const pl::Token::ValueType type) {
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
                case ValueType::Character16:    return "char16";
                case ValueType::Padding:        return "padding";
                default:                        return "< ??? >";
            }
        }

        bool operator==(const ValueTypes &other) const {
            if (this->type == Type::Integer || this->type == Type::Identifier || this->type == Type::String)
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

    };

}

#define COMPONENT(type, value) hex::pl::Token::Type::type, hex::pl::Token::type::value

#define KEYWORD_STRUCT                      COMPONENT(Keyword, Struct)
#define KEYWORD_UNION                       COMPONENT(Keyword, Union)
#define KEYWORD_USING                       COMPONENT(Keyword, Using)
#define KEYWORD_ENUM                        COMPONENT(Keyword, Enum)
#define KEYWORD_BITFIELD                    COMPONENT(Keyword, Bitfield)
#define KEYWORD_LE                          COMPONENT(Keyword, LittleEndian)
#define KEYWORD_BE                          COMPONENT(Keyword, BigEndian)
#define KEYWORD_IF                          COMPONENT(Keyword, If)
#define KEYWORD_ELSE                        COMPONENT(Keyword, Else)
#define KEYWORD_PARENT                      COMPONENT(Keyword, Parent)
#define KEYWORD_THIS                        COMPONENT(Keyword, This)
#define KEYWORD_WHILE                       COMPONENT(Keyword, While)
#define KEYWORD_FUNCTION                    COMPONENT(Keyword, Function)
#define KEYWORD_RETURN                      COMPONENT(Keyword, Return)
#define KEYWORD_NAMESPACE                   COMPONENT(Keyword, Namespace)

#define INTEGER                             hex::pl::Token::Type::Integer, hex::pl::Token::Literal(u128(0))
#define IDENTIFIER                          hex::pl::Token::Type::Identifier, ""
#define STRING                              hex::pl::Token::Type::String, hex::pl::Token::Literal("")

#define OPERATOR_AT                         COMPONENT(Operator, AtDeclaration)
#define OPERATOR_ASSIGNMENT                 COMPONENT(Operator, Assignment)
#define OPERATOR_INHERIT                    COMPONENT(Operator, Inherit)
#define OPERATOR_PLUS                       COMPONENT(Operator, Plus)
#define OPERATOR_MINUS                      COMPONENT(Operator, Minus)
#define OPERATOR_STAR                       COMPONENT(Operator, Star)
#define OPERATOR_SLASH                      COMPONENT(Operator, Slash)
#define OPERATOR_PERCENT                    COMPONENT(Operator, Percent)
#define OPERATOR_SHIFTLEFT                  COMPONENT(Operator, ShiftLeft)
#define OPERATOR_SHIFTRIGHT                 COMPONENT(Operator, ShiftRight)
#define OPERATOR_BITOR                      COMPONENT(Operator, BitOr)
#define OPERATOR_BITAND                     COMPONENT(Operator, BitAnd)
#define OPERATOR_BITXOR                     COMPONENT(Operator, BitXor)
#define OPERATOR_BITNOT                     COMPONENT(Operator, BitNot)
#define OPERATOR_BOOLEQUALS                 COMPONENT(Operator, BoolEquals)
#define OPERATOR_BOOLNOTEQUALS              COMPONENT(Operator, BoolNotEquals)
#define OPERATOR_BOOLGREATERTHAN            COMPONENT(Operator, BoolGreaterThan)
#define OPERATOR_BOOLLESSTHAN               COMPONENT(Operator, BoolLessThan)
#define OPERATOR_BOOLGREATERTHANOREQUALS    COMPONENT(Operator, BoolGreaterThanOrEquals)
#define OPERATOR_BOOLLESSTHANOREQUALS       COMPONENT(Operator, BoolLessThanOrEquals)
#define OPERATOR_BOOLAND                    COMPONENT(Operator, BoolAnd)
#define OPERATOR_BOOLOR                     COMPONENT(Operator, BoolOr)
#define OPERATOR_BOOLXOR                    COMPONENT(Operator, BoolXor)
#define OPERATOR_BOOLNOT                    COMPONENT(Operator, BoolNot)
#define OPERATOR_TERNARYCONDITIONAL         COMPONENT(Operator, TernaryConditional)
#define OPERATOR_DOLLAR                     COMPONENT(Operator, Dollar)
#define OPERATOR_ADDRESSOF                  COMPONENT(Operator, AddressOf)
#define OPERATOR_SIZEOF                     COMPONENT(Operator, SizeOf)
#define OPERATOR_SCOPERESOLUTION            COMPONENT(Operator, ScopeResolution)

#define VALUETYPE_CUSTOMTYPE                COMPONENT(ValueType, CustomType)
#define VALUETYPE_PADDING                   COMPONENT(ValueType, Padding)
#define VALUETYPE_UNSIGNED                  COMPONENT(ValueType, Unsigned)
#define VALUETYPE_SIGNED                    COMPONENT(ValueType, Signed)
#define VALUETYPE_FLOATINGPOINT             COMPONENT(ValueType, FloatingPoint)
#define VALUETYPE_INTEGER                   COMPONENT(ValueType, Integer)
#define VALUETYPE_ANY                       COMPONENT(ValueType, Any)

#define SEPARATOR_ROUNDBRACKETOPEN          COMPONENT(Separator, RoundBracketOpen)
#define SEPARATOR_ROUNDBRACKETCLOSE         COMPONENT(Separator, RoundBracketClose)
#define SEPARATOR_CURLYBRACKETOPEN          COMPONENT(Separator, CurlyBracketOpen)
#define SEPARATOR_CURLYBRACKETCLOSE         COMPONENT(Separator, CurlyBracketClose)
#define SEPARATOR_SQUAREBRACKETOPEN         COMPONENT(Separator, SquareBracketOpen)
#define SEPARATOR_SQUAREBRACKETCLOSE        COMPONENT(Separator, SquareBracketClose)
#define SEPARATOR_COMMA                     COMPONENT(Separator, Comma)
#define SEPARATOR_DOT                       COMPONENT(Separator, Dot)
#define SEPARATOR_ENDOFEXPRESSION           COMPONENT(Separator, EndOfExpression)
#define SEPARATOR_ENDOFPROGRAM              COMPONENT(Separator, EndOfProgram)