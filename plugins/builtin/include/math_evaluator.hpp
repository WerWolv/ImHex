#pragma once

#include <hex.hpp>

#include <string>
#include <vector>
#include <queue>
#include <unordered_map>
#include <functional>
#include <optional>

namespace hex {

    enum class TokenType {
        Number,
        Variable,
        Function,
        Operator,
        Bracket
    };

    enum class Operator : u16 {
        Invalid                 = 0x000,
        Assign                  = 0x010,
        Or                      = 0x020,
        Xor                     = 0x030,
        And                     = 0x040,
        BitwiseOr               = 0x050,
        BitwiseXor              = 0x060,
        BitwiseAnd              = 0x070,
        Equals                  = 0x080,
        NotEquals               = 0x081,
        GreaterThan             = 0x090,
        LessThan                = 0x091,
        GreaterThanOrEquals     = 0x092,
        LessThanOrEquals        = 0x093,
        ShiftLeft               = 0x0A0,
        ShiftRight              = 0x0A1,
        Addition                = 0x0B0,
        Subtraction             = 0x0B1,
        Multiplication          = 0x0C0,
        Division                = 0x0C1,
        Modulus                 = 0x0C2,
        Exponentiation          = 0x1D0,
        Combine                 = 0x0E0,
        BitwiseNot              = 0x0F0,
        Not                     = 0x0F1
    };

    enum class BracketType : std::uint8_t {
        Left,
        Right
    };

    struct Token {
        TokenType type;

        long double number;
        Operator op;
        BracketType bracketType;
        std::string name;
        std::vector<long double> arguments;
    };

    class MathEvaluator {
    public:
        MathEvaluator() = default;

        std::optional<long double> evaluate(std::string input);

        void registerStandardVariables();
        void registerStandardFunctions();

        void setVariable(std::string name, long double value);
        void setFunction(std::string name, std::function<std::optional<long double>(std::vector<long double>)> function, size_t minNumArgs, size_t maxNumArgs);

        std::unordered_map<std::string, long double>& getVariables() { return this->m_variables; }

    private:
        std::queue<Token> parseInput(const char *input);
        std::queue<Token> toPostfix(std::queue<Token> inputQueue);
        std::optional<long double> evaluate(std::queue<Token> postfixTokens);

        std::unordered_map<std::string, long double> m_variables;
        std::unordered_map<std::string, std::function<std::optional<long double>(std::vector<long double>)>> m_functions;
    };

}