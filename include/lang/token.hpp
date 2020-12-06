#pragma once

#include <hex.hpp>

#include <string>

namespace hex::lang {

    struct Token {
        enum class Type : u64 {
            Keyword,
            Type,
            Operator,
            Integer,
            Identifier,
            EndOfExpression,
            ScopeOpen,
            ScopeClose,
            ArrayOpen,
            ArrayClose,
            Separator,
            EndOfProgram
        } type;

        struct KeywordToken {
            enum class Keyword {
                Struct,
                Union,
                Using,
                Enum,
                Bitfield,
                LittleEndian,
                BigEndian
            } keyword;
        } keywordToken;
        struct IdentifierToken {
            std::string identifier;
        } identifierToken;
        struct OperatorToken {
            enum class Operator {
                AtDeclaration,
                Assignment,
                Inherit,
                Star
            } op;
        } operatorToken;
        struct IntegerToken {
            s128 integer;
        } integerToken;
        struct TypeToken {
            enum class Type {
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
                Padding             = 0x1F
            } type;
        } typeToken;

        u32 lineNumber;
    };
}