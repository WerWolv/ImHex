#include "lang/lexer.hpp"

#include <vector>
#include <functional>
#include <optional>

namespace hex::lang {

#define TOKEN(type, value) Token(Token::Type::type, Token::type::value, lineNumber)
#define VALUE_TOKEN(type, value) Token(Token::Type::type, value, lineNumber)

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

    std::optional<u64> parseInt(std::string_view string) {
        u64 integer = 0;
        u8 base;

        std::string_view numberData;

        if (string.starts_with("0x")) {
            numberData = string.substr(2);
            base = 16;

            if (numberData.find_first_not_of("0123456789ABCDEFabcdef") != std::string_view::npos)
                return { };
        } else if (string.starts_with("0b")) {
            numberData = string.substr(2);
            base = 2;

            if (numberData.find_first_not_of("01") != std::string_view::npos)
                return { };
        } else if (isdigit(string[0])) {
            numberData = string;
            base = 10;

            if (numberData.find_first_not_of("0123456789") != std::string_view::npos)
                return { };
        } else return { };

        if (numberData.length() == 0)
            return { };

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

        return integer;
    }

    std::optional<std::vector<Token>> Lexer::lex(const std::string& code) {
        std::vector<Token> tokens;
        u32 offset = 0;

        u32 lineNumber = 1;

        while (offset < code.length()) {

            // Handle comments
            if (code[offset] == '/') {
                offset++;

                if (offset < code.length() && code[offset] == '/') {
                    offset++;
                    while (offset < code.length()) {
                        if (code[offset] == '\n' || code[offset] == '\r')
                            break;
                        offset++;
                    }
                } else if (offset < code.length() && code[offset] == '*') {
                    offset++;
                    while (offset < (code.length() - 1)) {
                        if (code[offset] == '\n') lineNumber++;

                        if (code[offset] == '*' && code[offset + 1] == '/')
                            break;
                        offset++;
                    }

                    offset += 2;
                } else offset--;
            }

            const char& c = code[offset];

            if (c == 0x00)
                break;

            if (std::isblank(c) || std::isspace(c)) {
                if (code[offset] == '\n') lineNumber++;
                offset += 1;
            } else if (c == ';') {
                tokens.push_back(TOKEN(Separator, EndOfExpression));
                offset += 1;
            } else if (c == '(') {
                tokens.push_back(TOKEN(Separator, RoundBracketOpen));
                offset += 1;
            } else if (c == ')') {
                tokens.push_back(TOKEN(Separator, RoundBracketClose));
                offset += 1;
            } else if (c == '{') {
                tokens.push_back(TOKEN(Separator, CurlyBracketOpen));
                offset += 1;
            } else if (c == '}') {
                tokens.push_back(TOKEN(Separator, CurlyBracketClose));
                offset += 1;
            } else if (c == '[') {
                tokens.push_back(TOKEN(Separator, SquareBracketOpen));
                offset += 1;
            } else if (c == ']') {
                tokens.push_back(TOKEN(Separator, SquareBracketClose));
                offset += 1;
            } else if (c == ',') {
                tokens.push_back(TOKEN(Separator, Comma));
                offset += 1;
            } else if (c == '@') {
              tokens.push_back(TOKEN(Operator, AtDeclaration));
              offset += 1;
            } else if (c == '=') {
                tokens.push_back(TOKEN(Operator, Assignment));
                offset += 1;
            } else if (c == ':') {
                tokens.push_back(TOKEN(Operator, Inherit));
                offset += 1;
            } else if (c == '*') {
                tokens.push_back(TOKEN(Operator, Star));
                offset += 1;
            } else if (c == '\'') {
                offset += 1;

                if (offset >= code.length()) {
                    this->m_error = { lineNumber, "Invalid character literal" };
                    return { };
                }

                char character = code[offset];

                if (character == '\\') {
                    offset += 1;

                    if (offset >= code.length()) {
                        this->m_error = { lineNumber, "Invalid character literal" };
                        return { };
                    }

                    if (code[offset] != '\\' && code[offset] != '\'') {
                        this->m_error = { lineNumber, "Invalid escape sequence" };
                        return { };
                    }

                    character = code[offset];
                } else {
                    if (code[offset] == '\\' || code[offset] == '\'' || character == '\n' || character == '\r') {
                        this->m_error = { lineNumber, "Invalid character literal" };
                        return { };
                    }
                }

                offset += 1;

                if (offset >= code.length() || code[offset] != '\'') {
                    this->m_error = { lineNumber, "Missing terminating ' after character literal" };
                    return { };
                }

                tokens.push_back(VALUE_TOKEN(Integer, character));
                offset += 1;

            } else if (std::isalpha(c)) {
                std::string identifier = matchTillInvalid(&code[offset], [](char c) -> bool { return std::isalnum(c) || c == '_'; });

                // Check for reserved keywords

                if (identifier == "struct")
                    tokens.push_back(TOKEN(Keyword, Struct));
                else if (identifier == "union")
                    tokens.push_back(TOKEN(Keyword, Union));
                else if (identifier == "using")
                    tokens.push_back(TOKEN(Keyword, Using));
                else if (identifier == "enum")
                    tokens.push_back(TOKEN(Keyword, Enum));
                else if (identifier == "bitfield")
                    tokens.push_back(TOKEN(Keyword, Bitfield));
                else if (identifier == "be")
                    tokens.push_back(TOKEN(Keyword, BigEndian));
                else if (identifier == "le")
                    tokens.push_back(TOKEN(Keyword, LittleEndian));

                    // Check for built-in types
                else if (identifier == "u8")
                    tokens.push_back(TOKEN(ValueType, Unsigned8Bit));
                else if (identifier == "s8")
                    tokens.push_back(TOKEN(ValueType, Signed8Bit));
                else if (identifier == "u16")
                    tokens.push_back(TOKEN(ValueType, Unsigned16Bit));
                else if (identifier == "s16")
                    tokens.push_back(TOKEN(ValueType, Signed16Bit));
                else if (identifier == "u32")
                    tokens.push_back(TOKEN(ValueType, Unsigned32Bit));
                else if (identifier == "s32")
                    tokens.push_back(TOKEN(ValueType, Signed32Bit));
                else if (identifier == "u64")
                    tokens.push_back(TOKEN(ValueType, Unsigned64Bit));
                else if (identifier == "s64")
                    tokens.push_back(TOKEN(ValueType, Signed64Bit));
                else if (identifier == "u128")
                    tokens.push_back(TOKEN(ValueType, Unsigned128Bit));
                else if (identifier == "s128")
                    tokens.push_back(TOKEN(ValueType, Signed128Bit));
                else if (identifier == "float")
                    tokens.push_back(TOKEN(ValueType, Float));
                else if (identifier == "double")
                    tokens.push_back(TOKEN(ValueType, Double));
                else if (identifier == "padding")
                    tokens.push_back(TOKEN(ValueType, Padding));

                // If it's not a keyword and a builtin type, it has to be an identifier

                else
                    tokens.push_back(VALUE_TOKEN(Identifier, identifier));

                offset += identifier.length();
            } else if (std::isdigit(c)) {
                char *end = nullptr;
                std::strtoull(&code[offset], &end, 0);

                auto integer = parseInt(std::string_view(&code[offset], end - &code[offset]));

                if (!integer.has_value()) {
                    this->m_error = { lineNumber, "Invalid integer literal" };
                    return { };
                }

                tokens.push_back(VALUE_TOKEN(Integer, integer.value()));
                offset += (end - &code[offset]);
            } else {
                this->m_error = { lineNumber, "Unknown token" };
                return { };
            }
        }

        tokens.push_back(TOKEN(Separator, EndOfProgram));

        return tokens;
    }
}