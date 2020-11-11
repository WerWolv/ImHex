#include "parser/lexer.hpp"

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

        while (offset < code.length()) {
            const char& c = code[offset];

            if (std::isblank(c) || std::isspace(c)) {
                offset += 1;
            } else if (c == ';') {
                tokens.push_back({.type = Token::Type::EndOfExpression});
                offset += 1;
            } else if (c == '{') {
                tokens.push_back({.type = Token::Type::ScopeOpen});
                offset += 1;
            } else if (c == '}') {
                tokens.push_back({.type = Token::Type::ScopeClose});
                offset += 1;
            } else if (c == ',') {
                tokens.push_back({.type = Token::Type::Separator});
                offset += 1;
            } else if (c == '@') {
              tokens.push_back({.type = Token::Type::Operator, .operatorToken = { .op = Token::OperatorToken::Operator::AtDeclaration}});
              offset += 1;
            } else if (c == '=') {
                tokens.push_back({.type = Token::Type::Operator, .operatorToken = { .op = Token::OperatorToken::Operator::Assignment}});
                offset += 1;
            } else if (std::isalpha(c)) {
                std::string identifier = matchTillInvalid(&code[offset], [](char c) -> bool { return std::isalnum(c) || c == '_'; });

                // Check for reserved keywords

                if (identifier == "struct")
                    tokens.push_back({.type = Token::Type::Keyword, .keywordToken = { .keyword = Token::KeywordToken::Keyword::Struct}});
                else if (identifier == "using")
                    tokens.push_back({.type = Token::Type::Keyword, .keywordToken = { .keyword = Token::KeywordToken::Keyword::Using}});

                // Check for built-in types
                else if (identifier == "u8")
                    tokens.push_back({.type = Token::Type::Type, .typeToken = { .type = Token::TypeToken::Type::Unsigned8Bit }});
                else if (identifier == "s8")
                    tokens.push_back({.type = Token::Type::Type, .typeToken = { .type = Token::TypeToken::Type::Signed8Bit }});
                else if (identifier == "u16")
                    tokens.push_back({.type = Token::Type::Type, .typeToken = { .type = Token::TypeToken::Type::Unsigned16Bit }});
                else if (identifier == "s16")
                    tokens.push_back({.type = Token::Type::Type, .typeToken = { .type = Token::TypeToken::Type::Signed16Bit }});
                else if (identifier == "u32")
                    tokens.push_back({.type = Token::Type::Type, .typeToken = { .type = Token::TypeToken::Type::Unsigned32Bit }});
                else if (identifier == "s32")
                    tokens.push_back({.type = Token::Type::Type, .typeToken = { .type = Token::TypeToken::Type::Signed32Bit }});
                else if (identifier == "u64")
                    tokens.push_back({.type = Token::Type::Type, .typeToken = { .type = Token::TypeToken::Type::Unsigned64Bit }});
                else if (identifier == "s64")
                    tokens.push_back({.type = Token::Type::Type, .typeToken = { .type = Token::TypeToken::Type::Signed64Bit }});
                else if (identifier == "u128")
                    tokens.push_back({.type = Token::Type::Type, .typeToken = { .type = Token::TypeToken::Type::Unsigned128Bit }});
                else if (identifier == "s128")
                    tokens.push_back({.type = Token::Type::Type, .typeToken = { .type = Token::TypeToken::Type::Signed128Bit }});
                else if (identifier == "float")
                    tokens.push_back({.type = Token::Type::Type, .typeToken = { .type = Token::TypeToken::Type::Float }});
                else if (identifier == "double")
                    tokens.push_back({.type = Token::Type::Type, .typeToken = { .type = Token::TypeToken::Type::Double }});

                // If it's not a keyword and a builtin type, it has to be an identifier

                else
                    tokens.push_back({.type = Token::Type::Identifier, .identifierToken = { .identifier = identifier}});

                offset += identifier.length();
            } else if (std::isdigit(c)) {
                char *end = nullptr;
                std::strtoull(&code[offset], &end, 0);

                auto integer = parseInt(std::string_view(&code[offset], end));

                if (!integer.has_value())
                    return { ResultLexicalError, {}};

                tokens.push_back({.type = Token::Type::Integer, .integerToken = { .integer = integer.value() }});
                offset += (end - &code[offset]);
            } else return { ResultLexicalError, {}};
        }

        tokens.push_back({.type = Token::Type::EndOfProgram});

        return { ResultSuccess, tokens };
    }
}