#include <hex/pattern_language/evaluator.hpp>
#include <hex/pattern_language/ast_node.hpp>

namespace hex::pl {

    void Evaluator::createVariable(const std::string &name, ASTNode *type) {
        auto &variables = *this->getScope(0).scope;
        for (auto &variable : variables) {
            if (variable->getVariableName() == name) {
                LogConsole::abortEvaluation(hex::format("variable with name '{}' already exists", name));
            }
        }

        auto pattern = type->createPatterns(this).front();

        pattern->setVariableName(name);
        pattern->setOffset(this->getStack().size());
        pattern->setLocal(true);

        this->getStack().resize(this->getStack().size() + pattern->getSize());

        variables.push_back(pattern);
    }

    void Evaluator::setVariable(const std::string &name, const Token::Literal& value) {
        PatternData *pattern = nullptr;

        auto &variables = *this->getScope(0).scope;
        for (auto &variable : variables) {
            if (variable->getVariableName() == name) {
                pattern = variable;
                break;
            }
        }

        if (pattern == nullptr)
            LogConsole::abortEvaluation(hex::format("no variable with name '{}' found", name));

        Token::Literal castedLiteral = std::visit(overloaded {
                [&](double &value) -> Token::Literal {
                    if (dynamic_cast<PatternDataUnsigned*>(pattern))
                        return u128(value);
                    else if (dynamic_cast<PatternDataSigned*>(pattern))
                        return s128(value);
                    else if (dynamic_cast<PatternDataFloat*>(pattern))
                        return value;
                    else
                        LogConsole::abortEvaluation(hex::format("cannot cast type 'double' to type '{}'", pattern->getTypeName()));
                },
                [&](const std::string &value) -> Token::Literal {
                    if (dynamic_cast<PatternDataString*>(pattern))
                        return value;
                    else
                        LogConsole::abortEvaluation(hex::format("cannot cast type 'string' to type '{}'", pattern->getTypeName()));
                },
                [&](PatternData * const &value) -> Token::Literal {
                    if (value->getTypeName() == pattern->getTypeName())
                        return value;
                    else
                        LogConsole::abortEvaluation(hex::format("cannot cast type '{}' to type '{}'", value->getTypeName(), pattern->getTypeName()));
                },
                [&](auto &&value) -> Token::Literal {
                    if (dynamic_cast<PatternDataUnsigned*>(pattern))
                        return u128(value);
                    else if (dynamic_cast<PatternDataSigned*>(pattern))
                        return s128(value);
                    else if (dynamic_cast<PatternDataCharacter*>(pattern))
                        return char(value);
                    else if (dynamic_cast<PatternDataBoolean*>(pattern))
                        return bool(value);
                    else if (dynamic_cast<PatternDataFloat*>(pattern))
                        return double(value);
                    else
                        LogConsole::abortEvaluation(hex::format("cannot cast type 'string' to type '{}'", pattern->getTypeName()));
                    }
            }, value);

        std::visit(overloaded {
                [&](PatternData *value) {
                    pattern->setOffset(value->getOffset());
                    pattern->setSize(value->getSize());
                },
                [&](std::string value) {
                    auto size = value.length();

                    pattern->setSize(size);
                    this->getStack().resize(this->getStack().size() + size);
                    std::memcpy(&this->getStack()[pattern->getOffset()], value.data(), size);
                },
                [&](double &value) {
                    auto size = std::min(sizeof(value), pattern->getSize());

                    if (pattern->getSize() == sizeof(float)) {
                        auto floatValue = static_cast<float>(value);

                        this->getStack().resize(this->getStack().size() + size);
                        std::memcpy(&this->getStack()[pattern->getOffset()], &floatValue, size);
                    } else if (pattern->getSize() == sizeof(double)) {
                        this->getStack().resize(this->getStack().size() + size);
                        std::memcpy(&this->getStack()[pattern->getOffset()], &value, size);
                    }
                },
                [&](auto &&value) {
                    auto size = std::min(sizeof(value), pattern->getSize());

                    this->getStack().resize(this->getStack().size() + size);
                    std::memcpy(&this->getStack()[pattern->getOffset()], &value, size);
                }
        }, castedLiteral);
    }

    std::optional<std::vector<PatternData*>> Evaluator::evaluate(const std::vector<ASTNode*> &ast) {
        this->m_stack.clear();
        this->m_customFunctions.clear();
        this->m_scopes.clear();

        for (auto &func : this->m_customFunctionDefinitions)
            delete func;
        this->m_customFunctionDefinitions.clear();

        std::vector<PatternData*> patterns;

        try {
            pushScope(nullptr, patterns);
            for (auto node : ast) {
                if (dynamic_cast<ASTNodeTypeDecl*>(node)) {
                    ;// Don't create patterns from type declarations
                } else if (dynamic_cast<ASTNodeFunctionCall*>(node)) {
                    delete node->evaluate(this);
                } else if (dynamic_cast<ASTNodeFunctionDefinition*>(node)) {
                    this->m_customFunctionDefinitions.push_back(node->evaluate(this));
                } else {
                    auto newPatterns = node->createPatterns(this);
                    patterns.insert(patterns.end(), newPatterns.begin(), newPatterns.end());
                }

            }
            popScope();
        } catch (const LogConsole::EvaluateError &error) {
            this->m_console.log(LogConsole::Level::Error, error.second);

            if (error.first != 0)
                this->m_console.setHardError(error);

            for (auto &pattern : patterns)
                delete pattern;
            patterns.clear();

            return std::nullopt;
        }

        return patterns;
    }

}