#include "lang/lexer.hpp"

#include <vector>
#include <functional>

namespace hex::lang {

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

    std::pair<Result, std::vector<Token>> Lexer::lex(const std::string& code) {
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
                tokens.push_back({ .type = Token::Type::EndOfExpression, .lineNumber = lineNumber });
                offset += 1;
            } else if (c == '{') {
                tokens.push_back({ .type = Token::Type::ScopeOpen, .lineNumber = lineNumber });
                offset += 1;
            } else if (c == '}') {
                tokens.push_back({ .type = Token::Type::ScopeClose, .lineNumber = lineNumber });
                offset += 1;
            } else if (c == '[') {
                tokens.push_back({ .type = Token::Type::ArrayOpen, .lineNumber = lineNumber });
                offset += 1;
            } else if (c == ']') {
                tokens.push_back({.type = Token::Type::ArrayClose, .lineNumber = lineNumber });
                offset += 1;
            } else if (c == ',') {
                tokens.push_back({ .type = Token::Type::Separator, .lineNumber = lineNumber });
                offset += 1;
            } else if (c == '@') {
              tokens.push_back({ .type = Token::Type::Operator, .operatorToken = { .op = Token::OperatorToken::Operator::AtDeclaration }, .lineNumber = lineNumber });
              offset += 1;
            } else if (c == '=') {
                tokens.push_back({ .type = Token::Type::Operator, .operatorToken = { .op = Token::OperatorToken::Operator::Assignment }, .lineNumber = lineNumber });
                offset += 1;
            } else if (c == ':') {
                tokens.push_back({ .type = Token::Type::Operator, .operatorToken = { .op = Token::OperatorToken::Operator::Inherit }, .lineNumber = lineNumber });
                offset += 1;
            } else if (c == '*') {
                tokens.push_back({ .type = Token::Type::Operator, .operatorToken = { .op = Token::OperatorToken::Operator::Star }, .lineNumber = lineNumber });
                offset += 1;
            } else if (c == '\'') {
                offset += 1;

                if (offset >= code.length()) {
                    this->m_error = { lineNumber, "Invalid character literal" };
                    return { ResultLexicalError, { } };
                }

                char character = code[offset];

                if (character == '\\') {
                    offset += 1;

                    if (offset >= code.length()) {
                        this->m_error = { lineNumber, "Invalid character literal" };
                        return { ResultLexicalError, { } };
                    }

                    if (code[offset] != '\\' && code[offset] != '\'') {
                        this->m_error = { lineNumber, "Invalid escape sequence" };
                        return { ResultLexicalError, { } };
                    }

                    character = code[offset];
                } else {
                    if (code[offset] == '\\' || code[offset] == '\'' || character == '\n' || character == '\r') {
                        this->m_error = { lineNumber, "Invalid character literal" };
                        return { ResultLexicalError, { } };
                    }
                }

                offset += 1;

                if (offset >= code.length() || code[offset] != '\'') {
                    this->m_error = { lineNumber, "Missing terminating ' after character literal" };
                    return { ResultLexicalError, { } };
                }

                tokens.push_back({ .type = Token::Type::Integer, .integerToken = { .integer = character }, .lineNumber = lineNumber });
                offset += 1;

            } else if (std::isalpha(c)) {
                std::string identifier = matchTillInvalid(&code[offset], [](char c) -> bool { return std::isalnum(c) || c == '_'; });

                // Check for reserved keywords

                if (identifier == "struct")
                    tokens.push_back({ .type = Token::Type::Keyword, .keywordToken = { .keyword = Token::KeywordToken::Keyword::Struct }, .lineNumber = lineNumber });
                else if (identifier == "union")
                    tokens.push_back({ .type = Token::Type::Keyword, .keywordToken = { .keyword = Token::KeywordToken::Keyword::Union }, .lineNumber = lineNumber });
                else if (identifier == "using")
                    tokens.push_back({ .type = Token::Type::Keyword, .keywordToken = { .keyword = Token::KeywordToken::Keyword::Using }, .lineNumber = lineNumber });
                else if (identifier == "enum")
                    tokens.push_back({ .type = Token::Type::Keyword, .keywordToken = { .keyword = Token::KeywordToken::Keyword::Enum }, .lineNumber = lineNumber });
                else if (identifier == "bitfield")
                    tokens.push_back({ .type = Token::Type::Keyword, .keywordToken = { .keyword = Token::KeywordToken::Keyword::Bitfield }, .lineNumber = lineNumber });
                else if (identifier == "be")
                    tokens.push_back({ .type = Token::Type::Keyword, .keywordToken = { .keyword = Token::KeywordToken::Keyword::BigEndian }, .lineNumber = lineNumber });
                else if (identifier == "le")
                    tokens.push_back({ .type = Token::Type::Keyword, .keywordToken = { .keyword = Token::KeywordToken::Keyword::LittleEndian }, .lineNumber = lineNumber });

                    // Check for built-in types
                else if (identifier == "u8")
                    tokens.push_back({ .type = Token::Type::Type, .typeToken = { .type = Token::TypeToken::Type::Unsigned8Bit }, .lineNumber = lineNumber });
                else if (identifier == "s8")
                    tokens.push_back({ .type = Token::Type::Type, .typeToken = { .type = Token::TypeToken::Type::Signed8Bit }, .lineNumber = lineNumber });
                else if (identifier == "u16")
                    tokens.push_back({ .type = Token::Type::Type, .typeToken = { .type = Token::TypeToken::Type::Unsigned16Bit }, .lineNumber = lineNumber });
                else if (identifier == "s16")
                    tokens.push_back({ .type = Token::Type::Type, .typeToken = { .type = Token::TypeToken::Type::Signed16Bit }, .lineNumber = lineNumber });
                else if (identifier == "u32")
                    tokens.push_back({ .type = Token::Type::Type, .typeToken = { .type = Token::TypeToken::Type::Unsigned32Bit }, .lineNumber = lineNumber });
                else if (identifier == "s32")
                    tokens.push_back({ .type = Token::Type::Type, .typeToken = { .type = Token::TypeToken::Type::Signed32Bit }, .lineNumber = lineNumber });
                else if (identifier == "u64")
                    tokens.push_back({ .type = Token::Type::Type, .typeToken = { .type = Token::TypeToken::Type::Unsigned64Bit }, .lineNumber = lineNumber });
                else if (identifier == "s64")
                    tokens.push_back({ .type = Token::Type::Type, .typeToken = { .type = Token::TypeToken::Type::Signed64Bit }, .lineNumber = lineNumber });
                else if (identifier == "u128")
                    tokens.push_back({ .type = Token::Type::Type, .typeToken = { .type = Token::TypeToken::Type::Unsigned128Bit }, .lineNumber = lineNumber });
                else if (identifier == "s128")
                    tokens.push_back({ .type = Token::Type::Type, .typeToken = { .type = Token::TypeToken::Type::Signed128Bit }, .lineNumber = lineNumber });
                else if (identifier == "float")
                    tokens.push_back({ .type = Token::Type::Type, .typeToken = { .type = Token::TypeToken::Type::Float }, .lineNumber = lineNumber });
                else if (identifier == "double")
                    tokens.push_back({ .type = Token::Type::Type, .typeToken = { .type = Token::TypeToken::Type::Double }, .lineNumber = lineNumber });
                else if (identifier == "padding")
                    tokens.push_back({ .type = Token::Type::Type, .typeToken = { .type = Token::TypeToken::Type::Padding }, .lineNumber = lineNumber });

                // If it's not a keyword and a builtin type, it has to be an identifier

                else
                    tokens.push_back({.type = Token::Type::Identifier, .identifierToken = { .identifier = identifier }, .lineNumber = lineNumber });

                offset += identifier.length();
            } else if (std::isdigit(c)) {
                char *end = nullptr;
                std::strtoull(&code[offset], &end, 0);

                auto integer = parseInt(std::string_view(&code[offset], end - &code[offset]));

                if (!integer.has_value()) {
                    this->m_error = { lineNumber, "Invalid integer literal" };
                    return { ResultLexicalError, {}};
                }

                tokens.push_back({ .type = Token::Type::Integer, .integerToken = { .integer = integer.value() }, .lineNumber = lineNumber });
                offset += (end - &code[offset]);
            } else {
                this->m_error = { lineNumber, "Unknown token" };
                return { ResultLexicalError, {} };
            }
        }

        tokens.push_back({ .type = Token::Type::EndOfProgram, .lineNumber = lineNumber });

        return { ResultSuccess, tokens };
    }
}