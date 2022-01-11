#include <hex/pattern_language/evaluator.hpp>
#include <hex/pattern_language/ast_node.hpp>

namespace hex::pl {

    void Evaluator::createVariable(const std::string &name, ASTNode *type, const std::optional<Token::Literal> &value, bool outVariable) {
        auto &variables = *this->getScope(0).scope;
        for (auto &variable : variables) {
            if (variable->getVariableName() == name) {
                LogConsole::abortEvaluation(hex::format("variable with name '{}' already exists", name));
            }
        }

        auto startOffset = this->dataOffset();
        auto pattern = type->createPatterns(this).front();
        this->dataOffset() = startOffset;

        if (pattern == nullptr) {
            // Handle auto variables
            if (!value.has_value())
                LogConsole::abortEvaluation("cannot determine type of auto variable", type);

            if (std::get_if<u128>(&value.value()) != nullptr)
                pattern = new PatternDataUnsigned(0, sizeof(u128), this);
            else if (std::get_if<s128>(&value.value()) != nullptr)
                pattern = new PatternDataSigned(0, sizeof(s128), this);
            else if (std::get_if<double>(&value.value()) != nullptr)
                pattern = new PatternDataFloat(0, sizeof(double), this);
            else if (std::get_if<bool>(&value.value()) != nullptr)
                pattern = new PatternDataBoolean(0, this);
            else if (std::get_if<char>(&value.value()) != nullptr)
                pattern = new PatternDataCharacter(0, this);
            else if (std::get_if<PatternData*>(&value.value()) != nullptr)
                pattern = std::get<PatternData*>(value.value())->clone();
            else if (std::get_if<std::string>(&value.value()) != nullptr)
                pattern = new PatternDataString(0, 1, this);
            else
                __builtin_unreachable();
        }

        pattern->setVariableName(name);
        pattern->setLocal(true);
        pattern->setOffset(this->getStack().size());

        this->getStack().emplace_back();
        variables.push_back(pattern);

        if (outVariable)
            this->m_outVariables[name] = pattern->getOffset();
    }

    void Evaluator::setVariable(const std::string &name, const Token::Literal& value) {
        PatternData *pattern = nullptr;

        {
            auto &variables = *this->getScope(0).scope;
            for (auto &variable : variables) {
                if (variable->getVariableName() == name) {
                    pattern = variable;
                    break;
                }
            }
        }

        if (pattern == nullptr) {
            auto &variables = *this->getGlobalScope().scope;
            for (auto &variable : variables) {
                if (variable->getVariableName() == name) {
                    if (!variable->isLocal())
                        LogConsole::abortEvaluation(hex::format("cannot modify global variable '{}' which has been placed in memory", name));

                    pattern = variable;
                    break;
                }
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
                    if (dynamic_cast<PatternDataUnsigned*>(pattern) || dynamic_cast<PatternDataEnum*>(pattern))
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
                        LogConsole::abortEvaluation(hex::format("cannot cast integer literal to type '{}'", pattern->getTypeName()));
                    }
            }, value);

        this->getStack()[pattern->getOffset()] = castedLiteral;

    }

    std::optional<std::vector<PatternData*>> Evaluator::evaluate(const std::vector<ASTNode*> &ast) {
        this->m_stack.clear();
        this->m_customFunctions.clear();
        this->m_scopes.clear();
        this->m_aborted = false;

        if (this->m_allowDangerousFunctions == DangerousFunctionPermission::Deny)
            this->m_allowDangerousFunctions = DangerousFunctionPermission::Ask;

        this->m_dangerousFunctionCalled = false;

        ON_SCOPE_EXIT {
            this->m_envVariables.clear();
        };

        this->dataOffset() = 0x00;
        this->m_currPatternCount = 0;

        for (auto &func : this->m_customFunctionDefinitions)
            delete func;
        this->m_customFunctionDefinitions.clear();

        std::vector<PatternData*> patterns;

        try {
            this->setCurrentControlFlowStatement(ControlFlowStatement::None);
            pushScope(nullptr, patterns);

            for (auto node : ast) {
                if (dynamic_cast<ASTNodeTypeDecl*>(node)) {
                    ;// Don't create patterns from type declarations
                } else if (dynamic_cast<ASTNodeFunctionCall*>(node)) {
                    delete node->evaluate(this);
                } else if (dynamic_cast<ASTNodeFunctionDefinition*>(node)) {
                    this->m_customFunctionDefinitions.push_back(node->evaluate(this));
                } else if (auto varDeclNode = dynamic_cast<ASTNodeVariableDecl*>(node)) {
                    auto pattern = node->createPatterns(this).front();

                    if (varDeclNode->getPlacementOffset() == nullptr) {
                        auto type = varDeclNode->getType()->evaluate(this);
                        ON_SCOPE_EXIT { delete type; };

                        auto &name = pattern->getVariableName();
                        this->createVariable(name, type, std::nullopt, varDeclNode->isOutVariable());

                        if (varDeclNode->isInVariable() && this->m_inVariables.contains(name))
                            this->setVariable(name, this->m_inVariables[name]);

                        delete pattern;
                  } else {
                    patterns.push_back(pattern);
                  }
                } else {
                    auto newPatterns = node->createPatterns(this);
                    patterns.insert(patterns.end(), newPatterns.begin(), newPatterns.end());
                }
            }

            if (this->m_customFunctions.contains("main")) {
                auto mainFunction = this->m_customFunctions["main"];

                if (mainFunction.parameterCount > 0)
                    LogConsole::abortEvaluation("main function may not accept any arguments");

                auto result = mainFunction.func(this, { });
                if (result) {
                    auto returnCode = Token::literalToSigned(*result);

                    if (returnCode != 0)
                        LogConsole::abortEvaluation(hex::format("non-success value returned from main: {}", returnCode));
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

            this->m_currPatternCount = 0;

            return std::nullopt;
        }

        // Remove global local variables
        std::erase_if(patterns, [](PatternData *pattern) {
            return pattern->isLocal();
        });

        return patterns;
    }

    void Evaluator::patternCreated() {
        if (this->m_currPatternCount > this->m_patternLimit)
            LogConsole::abortEvaluation(hex::format("exceeded maximum number of patterns: {}", this->m_patternLimit));
        this->m_currPatternCount++;
    }

    void Evaluator::patternDestroyed() {
        this->m_currPatternCount--;
    }

}