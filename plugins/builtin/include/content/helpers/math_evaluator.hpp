#pragma once

#include <hex.hpp>

#include <string>
#include <vector>
#include <queue>
#include <unordered_map>
#include <functional>
#include <optional>

namespace hex {

    template<typename T>
    class MathEvaluator {
    public:
        MathEvaluator() = default;

        std::optional<T> evaluate(const std::string &input);

        void registerStandardVariables();
        void registerStandardFunctions();

        void setVariable(const std::string &name, T value);
        void setFunction(const std::string &name, const std::function<std::optional<T>(std::vector<T>)> &function, size_t minNumArgs, size_t maxNumArgs);

        std::unordered_map<std::string, T> &getVariables() { return this->m_variables; }

        [[nodiscard]] bool hasError() const {
            return this->m_lastError.has_value();
        }

        [[nodiscard]] std::optional<std::string> getLastError() const {
            return this->m_lastError;
        }

    private:
        void setError(const std::string &error) {
            this->m_lastError = error;
        }

    private:
        enum class TokenType
        {
            Number,
            Variable,
            Function,
            Operator,
            Bracket
        };

        enum class Operator : u16
        {
            Invalid             = 0x000,
            Assign              = 0x010,
            Or                  = 0x020,
            Xor                 = 0x030,
            And                 = 0x040,
            BitwiseOr           = 0x050,
            BitwiseXor          = 0x060,
            BitwiseAnd          = 0x070,
            Equals              = 0x080,
            NotEquals           = 0x081,
            GreaterThan         = 0x090,
            LessThan            = 0x091,
            GreaterThanOrEquals = 0x092,
            LessThanOrEquals    = 0x093,
            ShiftLeft           = 0x0A0,
            ShiftRight          = 0x0A1,
            Addition            = 0x0B0,
            Subtraction         = 0x0B1,
            Multiplication      = 0x0C0,
            Division            = 0x0C1,
            Modulus             = 0x0C2,
            Exponentiation      = 0x1D0,
            Combine             = 0x0E0,
            BitwiseNot          = 0x0F0,
            Not                 = 0x0F1
        };

        enum class BracketType : std::uint8_t
        {
            Left,
            Right
        };

        struct Token {
            TokenType type;

            union {
                T number;
                Operator op;
                BracketType bracketType;
            };

            std::string name;
            std::vector<T> arguments;
        };

        static i16 comparePrecedence(const Operator &a, const Operator &b);
        static bool isLeftAssociative(const Operator &op);
        static std::pair<Operator, size_t> toOperator(const std::string &input);

    private:
        std::optional<std::queue<Token>> parseInput(std::string input);
        std::optional<std::queue<Token>> toPostfix(std::queue<Token> inputQueue);
        std::optional<T> evaluate(std::queue<Token> postfixTokens);

        std::unordered_map<std::string, T> m_variables;
        std::unordered_map<std::string, std::function<std::optional<T>(std::vector<T>)>> m_functions;

        std::optional<std::string> m_lastError;
    };

    extern template class MathEvaluator<long double>;
    extern template class MathEvaluator<i128>;

}