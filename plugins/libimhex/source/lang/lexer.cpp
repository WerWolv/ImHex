#include <hex/lang/lexer.hpp>

#include <algorithm>
#include <functional>
#include <optional>
#include <vector>

namespace hex::lang {

#define TOKEN(type, value) Token::Type::type, Token::type::value, lineNumber
#define VALUE_TOKEN(type, value) Token::Type::type, value, lineNumber

    Lexer::Lexer() { }

    std::string matchTillInvalid(const char* characters, std::function<bool(char)> predicate) {
        std::string ret;

        while (*characters != 0x00) {
            ret += *characters;
            characters++;

            if (!predicate(*characters))
                break;
        }

        return ret;
    }

    size_t getIntegerLiteralLength(std::string_view string) {
        return string.find_first_not_of("0123456789ABCDEFabcdef.xUL");
    }

    std::optional<Token::IntegerLiteral> parseIntegerLiteral(std::string_view string) {
        Token::ValueType type = Token::ValueType::Any;
        Token::IntegerLiteral result;

        u8 base;

        auto endPos = getIntegerLiteralLength(string);
        std::string_view numberData = string.substr(0, endPos);

        if (numberData.ends_with('U')) {
            type = Token::ValueType::Unsigned32Bit;
            numberData.remove_suffix(1);
        } else if (numberData.ends_with("UL")) {
            type = Token::ValueType::Unsigned64Bit;
            numberData.remove_suffix(2);
        } else if (numberData.ends_with("ULL")) {
            type = Token::ValueType::Unsigned128Bit;
            numberData.remove_suffix(3);
        } else if (numberData.ends_with("L")) {
            type = Token::ValueType::Signed64Bit;
            numberData.remove_suffix(1);
        } else if (numberData.ends_with("LL")) {
            type = Token::ValueType::Signed128Bit;
            numberData.remove_suffix(2);
        } else if (!numberData.starts_with("0x") && !numberData.starts_with("0b")) {
            if (numberData.ends_with('F')) {
                type = Token::ValueType::Float;
                numberData.remove_suffix(1);
            } else if (numberData.ends_with('D')) {
                type = Token::ValueType::Double;
                numberData.remove_suffix(1);
            }
        }

        if (numberData.starts_with("0x")) {
            numberData = numberData.substr(2);
            base = 16;

            if (Token::isFloatingPoint(type))
                return { };

            if (numberData.find_first_not_of("0123456789ABCDEFabcdef") != std::string_view::npos)
                return { };
        } else if (numberData.starts_with("0b")) {
            numberData = numberData.substr(2);
            base = 2;

            if (Token::isFloatingPoint(type))
                return { };

            if (numberData.find_first_not_of("01") != std::string_view::npos)
                return { };
        } else if (numberData.find('.') != std::string_view::npos || Token::isFloatingPoint(type)) {
            base = 10;
            if (type == Token::ValueType::Any)
                type = Token::ValueType::Double;

            if (std::count(numberData.begin(), numberData.end(), '.') > 1 || numberData.find_first_not_of("0123456789.") != std::string_view::npos)
                return { };

            if (numberData.ends_with('.'))
                return { };
        } else if (isdigit(numberData[0])) {
            base = 10;

            if (numberData.find_first_not_of("0123456789") != std::string_view::npos)
                return { };
        } else return { };

        if (type == Token::ValueType::Any)
            type = Token::ValueType::Signed32Bit;


        if (numberData.length() == 0)
            return { };

        if (Token::isUnsigned(type) || Token::isSigned(type)) {
            u128 integer = 0;
            for (const char& c : numberData) {
                integer *= base;

                if (isdigit(c))
                    integer += (c - '0');
                else if (c >= 'A' && c <= 'F')
                    integer += 10 + (c - 'A');
                else if (c >= 'a' && c <= 'f')
                    integer += 10 + (c - 'a');
                else return { };
            }

            switch (type) {
                case Token::ValueType::Unsigned32Bit:  return { u32(integer) };
                case Token::ValueType::Signed32Bit:    return { s32(integer) };
                case Token::ValueType::Unsigned64Bit:  return { u64(integer) };
                case Token::ValueType::Signed64Bit:    return { s64(integer) };
                case Token::ValueType::Unsigned128Bit: return { u128(integer) };
                case Token::ValueType::Signed128Bit:   return { s128(integer) };
                default: return { };
            }
        } else if (Token::isFloatingPoint(type)) {
            double floatingPoint = strtod(numberData.data(), nullptr);

            switch (type) {
                case Token::ValueType::Float:  return { float(floatingPoint) };
                case Token::ValueType::Double: return { double(floatingPoint) };
                default: return { };
            }
        }


        return { };
    }

    std::optional<std::pair<char, size_t>> getCharacter(std::string_view string) {

        if (string.length() < 1)
            return { };

        // Escape sequences
        if (string[0] == '\\') {

            if (string.length() < 2)
                return { };

            // Handle simple escape sequences
            switch (string[1]) {
                case 'a':  return {{ '\a', 2 }};
                case 'b':  return {{ '\b', 2 }};
                case 'f':  return {{ '\f', 2 }};
                case 'n':  return {{ '\n', 2 }};
                case 'r':  return {{ '\r', 2 }};
                case 't':  return {{ '\t', 2 }};
                case 'v':  return {{ '\v', 2 }};
                case '\\': return {{ '\\', 2 }};
                case '\'': return {{ '\'', 2 }};
                case '\"': return {{ '\"', 2 }};
            }

            // Hexadecimal number
            if (string[1] == 'x') {
                if (string.length() != 4)
                    return { };

                if (!isxdigit(string[2]) || !isxdigit(string[3]))
                    return { };

                return {{ std::strtoul(&string[2], nullptr, 16), 4 }};
            }

            // Octal number
            if (string[1] == 'o') {
                if (string.length() != 5)
                    return { };

                if (string[2] < '0' || string[2] > '7' || string[3] < '0' || string[3] > '7' || string[4] < '0' || string[4] > '7')
                    return { };

                return {{ std::strtoul(&string[2], nullptr, 8), 5 }};
            }

            return { };
        } else return {{ string[0], 1 }};
    }

    std::optional<std::pair<std::string, size_t>> getStringLiteral(std::string_view string) {
        if (!string.starts_with('\"'))
            return { };

        size_t size = 1;

        std::string result;
        while (string[size] != '\"') {
            auto character = getCharacter(string.substr(size));

            if (!character.has_value())
                return { };

            auto &[c, charSize] = character.value();

            result += c;
            size += charSize;

            if (size >= string.length())
                return { };
        }

        return {{ result, size + 1 }};
    }

    std::optional<std::pair<char, size_t>> getCharacterLiteral(std::string_view string) {
        if (string.empty())
            return { };

        if (string[0] != '\'')
            return { };


        auto character = getCharacter(string.substr(1));

        if (!character.has_value())
            return { };

        auto &[c, charSize] = character.value();

        if (string.length() >= charSize + 2 && string[charSize + 1] != '\'')
            return { };

        return {{ c, charSize + 2 }};
    }

    std::optional<std::vector<Token>> Lexer::lex(const std::string& code) {
        std::vector<Token> tokens;
        u32 offset = 0;

        u32 lineNumber = 1;

        try {

            while (offset < code.length()) {
                const char& c = code[offset];

                if (c == 0x00)
                    break;

                if (std::isblank(c) || std::isspace(c)) {
                    if (code[offset] == '\n') lineNumber++;
                    offset += 1;
                } else if (c == ';') {
                    tokens.emplace_back(TOKEN(Separator, EndOfExpression));
                    offset += 1;
                } else if (c == '(') {
                    tokens.emplace_back(TOKEN(Separator, RoundBracketOpen));
                    offset += 1;
                } else if (c == ')') {
                    tokens.emplace_back(TOKEN(Separator, RoundBracketClose));
                    offset += 1;
                } else if (c == '{') {
                    tokens.emplace_back(TOKEN(Separator, CurlyBracketOpen));
                    offset += 1;
                } else if (c == '}') {
                    tokens.emplace_back(TOKEN(Separator, CurlyBracketClose));
                    offset += 1;
                } else if (c == '[') {
                    tokens.emplace_back(TOKEN(Separator, SquareBracketOpen));
                    offset += 1;
                } else if (c == ']') {
                    tokens.emplace_back(TOKEN(Separator, SquareBracketClose));
                    offset += 1;
                } else if (c == ',') {
                    tokens.emplace_back(TOKEN(Separator, Comma));
                    offset += 1;
                } else if (c == '.') {
                    tokens.emplace_back(TOKEN(Separator, Dot));
                    offset += 1;
                } else if (c == '@') {
                    tokens.emplace_back(TOKEN(Operator, AtDeclaration));
                    offset += 1;
                } else if (code.substr(offset, 2) == "==") {
                    tokens.emplace_back(TOKEN(Operator, BoolEquals));
                    offset += 2;
                } else if (code.substr(offset, 2) == "!=") {
                    tokens.emplace_back(TOKEN(Operator, BoolNotEquals));
                    offset += 2;
                } else if (code.substr(offset, 2) == ">=") {
                    tokens.emplace_back(TOKEN(Operator, BoolGreaterThanOrEquals));
                    offset += 2;
                } else if (code.substr(offset, 2) == "<=") {
                    tokens.emplace_back(TOKEN(Operator, BoolLessThanOrEquals));
                    offset += 2;
                } else if (code.substr(offset, 2) == "&&") {
                    tokens.emplace_back(TOKEN(Operator, BoolAnd));
                    offset += 2;
                } else if (code.substr(offset, 2) == "||") {
                    tokens.emplace_back(TOKEN(Operator, BoolOr));
                    offset += 2;
                } else if (code.substr(offset, 2) == "^^") {
                    tokens.emplace_back(TOKEN(Operator, BoolXor));
                    offset += 2;
                } else if (c == '=') {
                    tokens.emplace_back(TOKEN(Operator, Assignment));
                    offset += 1;
                } else if (code.substr(offset, 2) == "::") {
                    tokens.emplace_back(TOKEN(Separator, ScopeResolution));
                    offset += 2;
                } else if (c == ':') {
                    tokens.emplace_back(TOKEN(Operator, Inherit));
                    offset += 1;
                } else if (c == '+') {
                    tokens.emplace_back(TOKEN(Operator, Plus));
                    offset += 1;
                } else if (c == '-') {
                    tokens.emplace_back(TOKEN(Operator, Minus));
                    offset += 1;
                } else if (c == '*') {
                    tokens.emplace_back(TOKEN(Operator, Star));
                    offset += 1;
                } else if (c == '/') {
                    tokens.emplace_back(TOKEN(Operator, Slash));
                    offset += 1;
                } else if (c == '%') {
                    tokens.emplace_back(TOKEN(Operator, Percent));
                    offset += 1;
                } else if (code.substr(offset, 2) == "<<") {
                    tokens.emplace_back(TOKEN(Operator, ShiftLeft));
                    offset += 2;
                } else if (code.substr(offset, 2) == ">>") {
                    tokens.emplace_back(TOKEN(Operator, ShiftRight));
                    offset += 2;
                } else if (c == '>') {
                    tokens.emplace_back(TOKEN(Operator, BoolGreaterThan));
                    offset += 1;
                } else if (c == '<') {
                    tokens.emplace_back(TOKEN(Operator, BoolLessThan));
                    offset += 1;
                } else if (c == '!') {
                    tokens.emplace_back(TOKEN(Operator, BoolNot));
                    offset += 1;
                } else if (c == '|') {
                    tokens.emplace_back(TOKEN(Operator, BitOr));
                    offset += 1;
                } else if (c == '&') {
                    tokens.emplace_back(TOKEN(Operator, BitAnd));
                    offset += 1;
                } else if (c == '^') {
                    tokens.emplace_back(TOKEN(Operator, BitXor));
                    offset += 1;
                } else if (c == '~') {
                    tokens.emplace_back(TOKEN(Operator, BitNot));
                    offset += 1;
                } else if (c == '?') {
                    tokens.emplace_back(TOKEN(Operator, TernaryConditional));
                    offset += 1;
                } else if (c == '$') {
                    tokens.emplace_back(TOKEN(Operator, Dollar));
                    offset += 1;
                } else if (code.substr(offset, 9) == "addressof") {
                    tokens.emplace_back(TOKEN(Operator, AddressOf));
                    offset += 9;
                }
                else if (code.substr(offset, 6) == "sizeof") {
                    tokens.emplace_back(TOKEN(Operator, SizeOf));
                    offset += 6;
                }
                else if (c == '\'') {
                    auto character = getCharacterLiteral(code.substr(offset));

                    if (!character.has_value())
                        throwLexerError("invalid character literal", lineNumber);

                    auto [c, charSize] = character.value();

                    tokens.emplace_back(VALUE_TOKEN(Integer, c));
                    offset += charSize;
                } else if (c == '\"') {
                    auto string = getStringLiteral(code.substr(offset));

                    if (!string.has_value())
                        throwLexerError("invalid string literal", lineNumber);

                    auto [s, stringSize] = string.value();

                    tokens.emplace_back(VALUE_TOKEN(String, s));
                    offset += stringSize;
                } else if (std::isalpha(c)) {
                    std::string identifier = matchTillInvalid(&code[offset], [](char c) -> bool { return std::isalnum(c) || c == '_'; });

                    // Check for reserved keywords

                    if (identifier == "struct")
                        tokens.emplace_back(TOKEN(Keyword, Struct));
                    else if (identifier == "union")
                        tokens.emplace_back(TOKEN(Keyword, Union));
                    else if (identifier == "using")
                        tokens.emplace_back(TOKEN(Keyword, Using));
                    else if (identifier == "enum")
                        tokens.emplace_back(TOKEN(Keyword, Enum));
                    else if (identifier == "bitfield")
                        tokens.emplace_back(TOKEN(Keyword, Bitfield));
                    else if (identifier == "be")
                        tokens.emplace_back(TOKEN(Keyword, BigEndian));
                    else if (identifier == "le")
                        tokens.emplace_back(TOKEN(Keyword, LittleEndian));
                    else if (identifier == "if")
                        tokens.emplace_back(TOKEN(Keyword, If));
                    else if (identifier == "else")
                        tokens.emplace_back(TOKEN(Keyword, Else));
                    else if (identifier == "false")
                        tokens.emplace_back(VALUE_TOKEN(Integer, bool(0)));
                    else if (identifier == "true")
                        tokens.emplace_back(VALUE_TOKEN(Integer, bool(1)));
                    else if (identifier == "parent")
                        tokens.emplace_back(TOKEN(Keyword, Parent));
                    else if (identifier == "while")
                        tokens.emplace_back(TOKEN(Keyword, While));
                    else if (identifier == "fn")
                        tokens.emplace_back(TOKEN(Keyword, Function));
                    else if (identifier == "return")
                        tokens.emplace_back(TOKEN(Keyword, Return));

                        // Check for built-in types
                    else if (identifier == "u8")
                        tokens.emplace_back(TOKEN(ValueType, Unsigned8Bit));
                    else if (identifier == "s8")
                        tokens.emplace_back(TOKEN(ValueType, Signed8Bit));
                    else if (identifier == "u16")
                        tokens.emplace_back(TOKEN(ValueType, Unsigned16Bit));
                    else if (identifier == "s16")
                        tokens.emplace_back(TOKEN(ValueType, Signed16Bit));
                    else if (identifier == "u32")
                        tokens.emplace_back(TOKEN(ValueType, Unsigned32Bit));
                    else if (identifier == "s32")
                        tokens.emplace_back(TOKEN(ValueType, Signed32Bit));
                    else if (identifier == "u64")
                        tokens.emplace_back(TOKEN(ValueType, Unsigned64Bit));
                    else if (identifier == "s64")
                        tokens.emplace_back(TOKEN(ValueType, Signed64Bit));
                    else if (identifier == "u128")
                        tokens.emplace_back(TOKEN(ValueType, Unsigned128Bit));
                    else if (identifier == "s128")
                        tokens.emplace_back(TOKEN(ValueType, Signed128Bit));
                    else if (identifier == "float")
                        tokens.emplace_back(TOKEN(ValueType, Float));
                    else if (identifier == "double")
                        tokens.emplace_back(TOKEN(ValueType, Double));
                    else if (identifier == "char")
                        tokens.emplace_back(TOKEN(ValueType, Character));
                    else if (identifier == "char16")
                        tokens.emplace_back(TOKEN(ValueType, Character16));
                    else if (identifier == "bool")
                        tokens.emplace_back(TOKEN(ValueType, Boolean));
                    else if (identifier == "padding")
                        tokens.emplace_back(TOKEN(ValueType, Padding));

                    // If it's not a keyword and a builtin type, it has to be an identifier

                    else
                        tokens.emplace_back(VALUE_TOKEN(Identifier, identifier));

                    offset += identifier.length();
                } else if (std::isdigit(c)) {
                    auto integer = parseIntegerLiteral(&code[offset]);

                    if (!integer.has_value())
                        throwLexerError("invalid integer literal", lineNumber);


                    tokens.emplace_back(VALUE_TOKEN(Integer, integer.value()));
                    offset += getIntegerLiteralLength(&code[offset]);
                } else
                    throwLexerError("unknown token", lineNumber);

            }

            tokens.emplace_back(TOKEN(Separator, EndOfProgram));
        } catch (LexerError &e) {
            this->m_error = e;
            return { };
        }


        return tokens;
    }
}