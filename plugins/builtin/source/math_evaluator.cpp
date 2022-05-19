#include "math_evaluator.hpp"

#include <hex/helpers/concepts.hpp>

#include <string>
#include <queue>
#include <stack>
#include <stdexcept>
#include <cmath>
#include <cstdint>
#include <optional>

namespace hex {

    template<typename T>
    i16 MathEvaluator<T>::comparePrecedence(const Operator &a, const Operator &b) {
        return static_cast<i16>((static_cast<i8>(a) & 0x0F0) - (static_cast<i8>(b) & 0x0F0));
    }

    template<typename T>
    bool MathEvaluator<T>::isLeftAssociative(const Operator op) {
        return (static_cast<u32>(op) & 0xF00) == 0;
    }

    template<typename T>
    std::pair<typename MathEvaluator<T>::Operator, size_t> MathEvaluator<T>::toOperator(const std::string &input) {
        if (input.starts_with("##")) return { Operator::Combine, 2 };
        if (input.starts_with("==")) return { Operator::Equals, 2 };
        if (input.starts_with("!=")) return { Operator::NotEquals, 2 };
        if (input.starts_with(">=")) return { Operator::GreaterThanOrEquals, 2 };
        if (input.starts_with("<=")) return { Operator::LessThanOrEquals, 2 };
        if (input.starts_with(">>")) return { Operator::ShiftRight, 2 };
        if (input.starts_with("<<")) return { Operator::ShiftLeft, 2 };
        if (input.starts_with("||")) return { Operator::Or, 2 };
        if (input.starts_with("^^")) return { Operator::Xor, 2 };
        if (input.starts_with("&&")) return { Operator::And, 2 };
        if (input.starts_with("**")) return { Operator::Exponentiation, 2 };
        if (input.starts_with(">")) return { Operator::GreaterThan, 1 };
        if (input.starts_with("<")) return { Operator::LessThan, 1 };
        if (input.starts_with("!")) return { Operator::Not, 1 };
        if (input.starts_with("|")) return { Operator::BitwiseOr, 1 };
        if (input.starts_with("^")) return { Operator::BitwiseXor, 1 };
        if (input.starts_with("&")) return { Operator::BitwiseAnd, 1 };
        if (input.starts_with("~")) return { Operator::BitwiseNot, 1 };
        if (input.starts_with("+")) return { Operator::Addition, 1 };
        if (input.starts_with("-")) return { Operator::Subtraction, 1 };
        if (input.starts_with("*")) return { Operator::Multiplication, 1 };
        if (input.starts_with("/")) return { Operator::Division, 1 };
        if (input.starts_with("%")) return { Operator::Modulus, 1 };
        if (input.starts_with("=")) return { Operator::Assign, 1 };

        return { Operator::Invalid, 0 };
    }

    template<typename T>
    std::optional<std::queue<typename MathEvaluator<T>::Token>> MathEvaluator<T>::toPostfix(std::queue<Token> inputQueue) {
        std::queue<Token> outputQueue;
        std::stack<Token> operatorStack;

        while (!inputQueue.empty()) {
            Token currToken = inputQueue.front();
            inputQueue.pop();

            if (currToken.type == TokenType::Number || currToken.type == TokenType::Variable || currToken.type == TokenType::Function)
                outputQueue.push(currToken);
            else if (currToken.type == TokenType::Operator) {
                while ((!operatorStack.empty()) && ((operatorStack.top().type == TokenType::Operator && currToken.type == TokenType::Operator && (comparePrecedence(operatorStack.top().op, currToken.op) > 0)) || (comparePrecedence(operatorStack.top().op, currToken.op) == 0 && isLeftAssociative(currToken.op))) && operatorStack.top().type != TokenType::Bracket) {
                    outputQueue.push(operatorStack.top());
                    operatorStack.pop();
                }
                operatorStack.push(currToken);
            } else if (currToken.type == TokenType::Bracket) {
                if (currToken.bracketType == BracketType::Left)
                    operatorStack.push(currToken);
                else {
                    if (operatorStack.empty()) {
                        this->setError("Mismatching parenthesis!");
                        return std::nullopt;
                    }

                    while (operatorStack.top().type != TokenType::Bracket || (operatorStack.top().type == TokenType::Bracket && operatorStack.top().bracketType != BracketType::Left)) {
                        if (operatorStack.empty()) {
                            this->setError("Mismatching parenthesis!");
                            return std::nullopt;
                        }

                        outputQueue.push(operatorStack.top());
                        operatorStack.pop();
                    }

                    operatorStack.pop();
                }
            }
        }

        while (!operatorStack.empty()) {
            auto top = operatorStack.top();

            if (top.type == TokenType::Bracket) {
                this->setError("Mismatching parenthesis!");
                return std::nullopt;
            }

            outputQueue.push(top);
            operatorStack.pop();
        }

        return outputQueue;
    }

    template<typename T>
    std::optional<std::queue<typename MathEvaluator<T>::Token>> MathEvaluator<T>::parseInput(std::string input) {
        std::queue<Token> inputQueue;

        char *prevPos = input.data();
        for (char *pos = prevPos; *pos != 0x00;) {
            if (std::isdigit(*pos) || *pos == '.') {
                auto number = [&] {
                   if constexpr (hex::floating_point<T>)
                       return std::strtold(pos, &pos);
                   else if constexpr (hex::signed_integral<T>)
                       return std::strtoll(pos, &pos, 10);
                   else if constexpr (hex::unsigned_integral<T>)
                       return std::strtoull(pos, &pos, 10);
                   else
                       static_assert(hex::always_false<T>::value, "Can't parse literal of this type");
                }();

                if (*pos == 'x') {
                    pos--;
                    number = std::strtoull(pos, &pos, 0);
                }

                inputQueue.push(Token { .type = TokenType::Number, .number = number, .name = "", .arguments = { } });
            } else if (*pos == '(') {
                inputQueue.push(Token { .type = TokenType::Bracket, .bracketType = BracketType::Left, .name = "", .arguments = { } });
                pos++;
            } else if (*pos == ')') {
                inputQueue.push(Token { .type = TokenType::Bracket, .bracketType = BracketType::Right, .name = "", .arguments = { } });
                pos++;
            } else if (std::isspace(*pos)) {
                pos++;
            } else {
                auto [op, width] = toOperator(pos);

                if (op != Operator::Invalid) {
                    inputQueue.push(Token { .type = TokenType::Operator, .op = op, .name = "", .arguments = { } });
                    pos += width;
                } else {
                    Token token;

                    while (std::isalpha(*pos) || *pos == '_') {
                        token.name += *pos;
                        pos++;
                    }

                    if (*pos == '(') {
                        pos++;

                        u32 depth = 1;
                        std::vector<std::string> expressions;
                        expressions.emplace_back();

                        while (*pos != 0x00) {
                            if (*pos == '(') depth++;
                            else if (*pos == ')') depth--;

                            if (depth == 0)
                                break;

                            if (depth == 1 && *pos == ',') {
                                expressions.emplace_back();
                                pos++;
                            }

                            expressions.back() += *pos;

                            pos++;
                        }

                        pos++;

                        for (const auto &expression : expressions) {
                            if (expression.empty() && expressions.size() > 1) {
                                this->setError("Invalid function call syntax!");
                                return std::nullopt;
                            }
                            else if (expression.empty())
                                break;

                            auto newInputQueue = parseInput(expression);
                            if (!newInputQueue.has_value())
                                return std::nullopt;

                            auto postfixTokens = toPostfix(*newInputQueue);
                            if (!postfixTokens.has_value())
                                return std::nullopt;

                            auto result = evaluate(*postfixTokens);
                            if (!result.has_value()) {
                                this->setError("Invalid argument for function!");
                                return std::nullopt;
                            }

                            token.arguments.push_back(result.value());
                        }

                        token.type = TokenType::Function;
                        inputQueue.push(token);

                    } else {
                        token.type = TokenType::Variable;
                        inputQueue.push(token);
                    }
                }
            }

            if (prevPos == pos) {
                this->setError("Invalid syntax!");
                return std::nullopt;
            }

            prevPos = pos;
        }

        return inputQueue;
    }

    template<typename T>
    std::optional<T> MathEvaluator<T>::evaluate(std::queue<Token> postfixTokens) {
        std::stack<T> evaluationStack;

        while (!postfixTokens.empty()) {
            auto front = postfixTokens.front();
            postfixTokens.pop();

            if (front.type == TokenType::Number)
                evaluationStack.push(front.number);
            else if (front.type == TokenType::Operator) {
                T rightOperand, leftOperand;
                if (evaluationStack.size() < 2) {
                    if ((front.op == Operator::Addition || front.op == Operator::Subtraction || front.op == Operator::Not || front.op == Operator::BitwiseNot) && evaluationStack.size() == 1) {
                        rightOperand = evaluationStack.top();
                        evaluationStack.pop();
                        leftOperand = 0;
                    } else {
                        this->setError("Not enough operands for operator!");
                        return std::nullopt;
                    }
                } else {
                    rightOperand = evaluationStack.top();
                    evaluationStack.pop();
                    leftOperand = evaluationStack.top();
                    evaluationStack.pop();
                }

                T result = [] {
                   if constexpr (std::numeric_limits<T>::has_quiet_NaN)
                       return std::numeric_limits<T>::quiet_NaN();
                   else
                       return 0;
                }();
                switch (front.op) {
                    default:
                    case Operator::Invalid:
                        this->setError("Invalid operator!");
                        return std::nullopt;
                    case Operator::And:
                        result = static_cast<i64>(leftOperand) && static_cast<i64>(rightOperand);
                        break;
                    case Operator::Or:
                        result = static_cast<i64>(leftOperand) || static_cast<i64>(rightOperand);
                        break;
                    case Operator::Xor:
                        result = (static_cast<i64>(leftOperand) ^ static_cast<i64>(rightOperand)) > 0;
                        break;
                    case Operator::GreaterThan:
                        result = leftOperand > rightOperand;
                        break;
                    case Operator::LessThan:
                        result = leftOperand < rightOperand;
                        break;
                    case Operator::GreaterThanOrEquals:
                        result = leftOperand >= rightOperand;
                        break;
                    case Operator::LessThanOrEquals:
                        result = leftOperand <= rightOperand;
                        break;
                    case Operator::Equals:
                        result = leftOperand == rightOperand;
                        break;
                    case Operator::NotEquals:
                        result = leftOperand != rightOperand;
                        break;
                    case Operator::Not:
                        result = !static_cast<i64>(rightOperand);
                        break;
                    case Operator::BitwiseOr:
                        result = static_cast<i64>(leftOperand) | static_cast<i64>(rightOperand);
                        break;
                    case Operator::BitwiseXor:
                        result = static_cast<i64>(leftOperand) ^ static_cast<i64>(rightOperand);
                        break;
                    case Operator::BitwiseAnd:
                        result = static_cast<i64>(leftOperand) & static_cast<i64>(rightOperand);
                        break;
                    case Operator::BitwiseNot:
                        result = ~static_cast<i64>(rightOperand);
                        break;
                    case Operator::ShiftLeft:
                        result = static_cast<i64>(leftOperand) << static_cast<i64>(rightOperand);
                        break;
                    case Operator::ShiftRight:
                        result = static_cast<i64>(leftOperand) >> static_cast<i64>(rightOperand);
                        break;
                    case Operator::Addition:
                        result = leftOperand + rightOperand;
                        break;
                    case Operator::Subtraction:
                        result = leftOperand - rightOperand;
                        break;
                    case Operator::Multiplication:
                        result = leftOperand * rightOperand;
                        break;
                    case Operator::Division:
                        result = leftOperand / rightOperand;
                        break;
                    case Operator::Modulus:
                        result = std::fmod(leftOperand, rightOperand);
                        break;
                    case Operator::Exponentiation:
                        result = std::pow(leftOperand, rightOperand);
                        break;
                    case Operator::Combine:
                        result = (static_cast<u64>(leftOperand) << (64 - __builtin_clzll(static_cast<u64>(rightOperand)))) | static_cast<u64>(rightOperand);
                        break;
                }

                evaluationStack.push(result);
            } else if (front.type == TokenType::Variable) {
                if (this->m_variables.contains(front.name))
                    evaluationStack.push(this->m_variables.at(front.name));
                else {
                    this->setError("Unknown variable!");
                    return std::nullopt;
                }
            } else if (front.type == TokenType::Function) {
                if (!this->m_functions[front.name]) {
                    this->setError("Unknown function called!");
                    return std::nullopt;
                }

                auto result = this->m_functions[front.name](front.arguments);

                if (result.has_value())
                    evaluationStack.push(result.value());
            } else {
                this->setError("Parenthesis in postfix expression!");
                return std::nullopt;
            }
        }

        if (evaluationStack.empty()) {
            return std::nullopt;
        }
        else if (evaluationStack.size() > 1) {
            this->setError("Undigested input left!");
            return std::nullopt;
        }
        else {
            return evaluationStack.top();
        }
    }


    template<typename T>
    std::optional<T> MathEvaluator<T>::evaluate(const std::string &input) {
        auto inputQueue = parseInput(input);
        if (!inputQueue.has_value())
            return std::nullopt;

        std::string resultVariable = "ans";

        {
            auto queueCopy = *inputQueue;
            if (queueCopy.front().type == TokenType::Variable) {
                resultVariable = queueCopy.front().name;
                queueCopy.pop();
                if (queueCopy.front().type != TokenType::Operator || queueCopy.front().op != Operator::Assign)
                    resultVariable = "ans";
                else {
                    inputQueue->pop();
                    inputQueue->pop();
                }
            }
        }

        auto postfixTokens = toPostfix(*inputQueue);
        if (!postfixTokens.has_value())
            return std::nullopt;

        auto result = evaluate(*postfixTokens);

        if (result.has_value()) {
            this->setVariable(resultVariable, result.value());
        }

        return result;
    }

    template<typename T>
    void MathEvaluator<T>::setVariable(const std::string &name, T value) {
        this->m_variables[name] = value;
    }

    template<typename T>
    void MathEvaluator<T>::setFunction(const std::string &name, const std::function<std::optional<T>(std::vector<T>)> &function, size_t minNumArgs, size_t maxNumArgs) {
        this->m_functions[name] = [this, minNumArgs, maxNumArgs, function](auto args) -> std::optional<T> {
            if (args.size() < minNumArgs || args.size() > maxNumArgs) {
                this->setError("Invalid number of function arguments!");
                return std::nullopt;
            }

            return function(args);
        };
    }


    template<typename T>
    void MathEvaluator<T>::registerStandardVariables() {
        this->setVariable("ans", 0);
    }

    template<typename T>
    void MathEvaluator<T>::registerStandardFunctions() {
        if constexpr (hex::floating_point<T>) {
            this->setFunction(
                "sin", [](auto args) { return std::sin(args[0]); }, 1, 1);
            this->setFunction(
                "cos", [](auto args) { return std::cos(args[0]); }, 1, 1);
            this->setFunction(
                "tan", [](auto args) { return std::tan(args[0]); }, 1, 1);
            this->setFunction(
                "sqrt", [](auto args) { return std::sqrt(args[0]); }, 1, 1);
            this->setFunction(
                "ceil", [](auto args) { return std::ceil(args[0]); }, 1, 1);
            this->setFunction(
                "floor", [](auto args) { return std::floor(args[0]); }, 1, 1);
            this->setFunction(
                "sign", [](auto args) { return (args[0] > 0) ? 1 : (args[0] == 0) ? 0
                                                                                  : -1; }, 1, 1);
            this->setFunction(
                "abs", [](auto args) { return std::abs(args[0]); }, 1, 1);
            this->setFunction(
                "ln", [](auto args) { return std::log(args[0]); }, 1, 1);
            this->setFunction(
                "lb", [](auto args) { return std::log2(args[0]); }, 1, 1);
            this->setFunction(
                "log", [](auto args) { return args.size() == 1 ? std::log10(args[0]) : std::log(args[1]) / std::log(args[0]); }, 1, 2);
        }
    }

}
