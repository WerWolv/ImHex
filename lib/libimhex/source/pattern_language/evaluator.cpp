#include <hex/pattern_language/evaluator.hpp>
#include <hex/pattern_language/ast_node.hpp>
#include <hex/pattern_language/pattern_data.hpp>

namespace hex::pl {

    void Evaluator::createParameterPack(const std::string &name, const std::vector<Token::Literal> &values) {
        this->getScope(0).parameterPack = ParameterPack {
            name,
            values
        };
    }

    void Evaluator::createVariable(const std::string &name, ASTNode *type, const std::optional<Token::Literal> &value, bool outVariable) {
        auto &variables = *this->getScope(0).scope;
        for (auto &variable : variables) {
            if (variable->getVariableName() == name) {
                LogConsole::abortEvaluation(hex::format("variable with name '{}' already exists", name));
            }
        }

        auto startOffset   = this->dataOffset();
        auto pattern       = type == nullptr ? nullptr : type->createPatterns(this).front();
        this->dataOffset() = startOffset;

        bool referenceType = false;

        if (pattern == nullptr) {
            // Handle auto variables
            if (!value.has_value())
                LogConsole::abortEvaluation("cannot determine type of auto variable", type);

            if (std::get_if<u128>(&value.value()) != nullptr)
                pattern = new PatternDataUnsigned(this, 0, sizeof(u128));
            else if (std::get_if<i128>(&value.value()) != nullptr)
                pattern = new PatternDataSigned(this, 0, sizeof(i128));
            else if (std::get_if<double>(&value.value()) != nullptr)
                pattern = new PatternDataFloat(this, 0, sizeof(double));
            else if (std::get_if<bool>(&value.value()) != nullptr)
                pattern = new PatternDataBoolean(this, 0);
            else if (std::get_if<char>(&value.value()) != nullptr)
                pattern = new PatternDataCharacter(this, 0);
            else if (std::get_if<std::string>(&value.value()) != nullptr)
                pattern = new PatternDataString(this, 0, 1);
            else if (std::get_if<PatternData *>(&value.value()) != nullptr) {
                pattern       = std::get<PatternData *>(value.value())->clone();
                referenceType = true;
            } else
                LogConsole::abortEvaluation("cannot determine type of auto variable", type);
        }

        pattern->setVariableName(name);

        if (!referenceType) {
            pattern->setOffset(this->getStack().size());
            pattern->setLocal(true);
            this->getStack().emplace_back();
        }

        variables.push_back(pattern);

        if (outVariable)
            this->m_outVariables[name] = pattern->getOffset();
    }

    void Evaluator::setVariable(const std::string &name, const Token::Literal &value) {
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

        if (!pattern->isLocal()) return;

        Token::Literal castedLiteral = std::visit(overloaded {
                                                      [&](double &value) -> Token::Literal {
                                                          if (dynamic_cast<PatternDataUnsigned *>(pattern))
                                                              return u128(value) & bitmask(pattern->getSize() * 8);
                                                          else if (dynamic_cast<PatternDataSigned *>(pattern))
                                                              return i128(value) & bitmask(pattern->getSize() * 8);
                                                          else if (dynamic_cast<PatternDataFloat *>(pattern))
                                                              return pattern->getSize() == sizeof(float) ? double(float(value)) : value;
                                                          else
                                                              LogConsole::abortEvaluation(hex::format("cannot cast type 'double' to type '{}'", pattern->getTypeName()));
                                                      },
                                                      [&](const std::string &value) -> Token::Literal {
                                                          if (dynamic_cast<PatternDataString *>(pattern))
                                                              return value;
                                                          else
                                                              LogConsole::abortEvaluation(hex::format("cannot cast type 'string' to type '{}'", pattern->getTypeName()));
                                                      },
                                                      [&](PatternData *const &value) -> Token::Literal {
                                                          if (value->getTypeName() == pattern->getTypeName())
                                                              return value;
                                                          else
                                                              LogConsole::abortEvaluation(hex::format("cannot cast type '{}' to type '{}'", value->getTypeName(), pattern->getTypeName()));
                                                      },
                                                      [&](auto &&value) -> Token::Literal {
                                                          if (dynamic_cast<PatternDataUnsigned *>(pattern) || dynamic_cast<PatternDataEnum *>(pattern))
                                                              return u128(value) & bitmask(pattern->getSize() * 8);
                                                          else if (dynamic_cast<PatternDataSigned *>(pattern))
                                                              return i128(value) & bitmask(pattern->getSize() * 8);
                                                          else if (dynamic_cast<PatternDataCharacter *>(pattern))
                                                              return char(value);
                                                          else if (dynamic_cast<PatternDataBoolean *>(pattern))
                                                              return bool(value);
                                                          else if (dynamic_cast<PatternDataFloat *>(pattern))
                                                              return pattern->getSize() == sizeof(float) ? double(float(value)) : value;
                                                          else
                                                              LogConsole::abortEvaluation(hex::format("cannot cast integer literal to type '{}'", pattern->getTypeName()));
                                                      } },
            value);

        this->getStack()[pattern->getOffset()] = castedLiteral;
    }

    std::optional<std::vector<PatternData *>> Evaluator::evaluate(const std::vector<ASTNode *> &ast) {
        this->m_stack.clear();
        this->m_customFunctions.clear();
        this->m_scopes.clear();
        this->m_mainResult.reset();
        this->m_aborted = false;

        if (this->m_allowDangerousFunctions == DangerousFunctionPermission::Deny)
            this->m_allowDangerousFunctions = DangerousFunctionPermission::Ask;

        this->m_dangerousFunctionCalled = false;

        ON_SCOPE_EXIT {
            this->m_envVariables.clear();
        };

        this->dataOffset()       = 0x00;
        this->m_currPatternCount = 0;

        for (auto &func : this->m_customFunctionDefinitions)
            delete func;
        this->m_customFunctionDefinitions.clear();

        std::vector<PatternData *> patterns;

        try {
            this->setCurrentControlFlowStatement(ControlFlowStatement::None);
            pushScope(nullptr, patterns);
            ON_SCOPE_EXIT {
                popScope();
            };

            for (auto node : ast) {
                if (dynamic_cast<ASTNodeTypeDecl *>(node)) {
                    ;    // Don't create patterns from type declarations
                } else if (dynamic_cast<ASTNodeFunctionCall *>(node)) {
                    delete node->evaluate(this);
                } else if (dynamic_cast<ASTNodeFunctionDefinition *>(node)) {
                    this->m_customFunctionDefinitions.push_back(node->evaluate(this));
                } else if (auto varDeclNode = dynamic_cast<ASTNodeVariableDecl *>(node)) {
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
                    std::copy(newPatterns.begin(), newPatterns.end(), std::back_inserter(patterns));
                }
            }

            if (this->m_customFunctions.contains("main")) {
                auto mainFunction = this->m_customFunctions["main"];

                if (mainFunction.parameterCount > 0)
                    LogConsole::abortEvaluation("main function may not accept any arguments");

                this->m_mainResult = mainFunction.func(this, {});
            }
        } catch (PatternLanguageError &error) {
            this->m_console.log(LogConsole::Level::Error, error.what());

            if (error.getLineNumber() != 0)
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