#include <hex/pattern_language/evaluator.hpp>
#include <hex/pattern_language/patterns/pattern.hpp>

#include <hex/pattern_language/ast/ast_node.hpp>
#include <hex/pattern_language/ast/ast_node_type_decl.hpp>
#include <hex/pattern_language/ast/ast_node_variable_decl.hpp>
#include <hex/pattern_language/ast/ast_node_function_call.hpp>
#include <hex/pattern_language/ast/ast_node_function_definition.hpp>

#include <hex/pattern_language/patterns/pattern_unsigned.hpp>
#include <hex/pattern_language/patterns/pattern_signed.hpp>
#include <hex/pattern_language/patterns/pattern_float.hpp>
#include <hex/pattern_language/patterns/pattern_boolean.hpp>
#include <hex/pattern_language/patterns/pattern_character.hpp>
#include <hex/pattern_language/patterns/pattern_string.hpp>
#include <hex/pattern_language/patterns/pattern_enum.hpp>

#include <hex/helpers/logger.hpp>

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

        auto startOffset = this->dataOffset();

        std::unique_ptr<Pattern> pattern;

        bool referenceType = false;

        auto typePattern = type->createPatterns(this);

        this->dataOffset() = startOffset;

        if (typePattern.empty()) {
            // Handle auto variables
            if (!value.has_value())
                LogConsole::abortEvaluation("cannot determine type of auto variable", type);

            if (std::get_if<u128>(&value.value()) != nullptr)
                pattern = std::unique_ptr<Pattern>(new PatternUnsigned(this, 0, sizeof(u128)));
            else if (std::get_if<i128>(&value.value()) != nullptr)
                pattern = std::unique_ptr<Pattern>(new PatternSigned(this, 0, sizeof(i128)));
            else if (std::get_if<double>(&value.value()) != nullptr)
                pattern = std::unique_ptr<Pattern>(new PatternFloat(this, 0, sizeof(double)));
            else if (std::get_if<bool>(&value.value()) != nullptr)
                pattern = std::unique_ptr<Pattern>(new PatternBoolean(this, 0));
            else if (std::get_if<char>(&value.value()) != nullptr)
                pattern = std::unique_ptr<Pattern>(new PatternCharacter(this, 0));
            else if (std::get_if<std::string>(&value.value()) != nullptr)
                pattern = std::unique_ptr<Pattern>(new PatternString(this, 0, 1));
            else if (auto patternValue = std::get_if<Pattern *>(&value.value()); patternValue != nullptr) {
                pattern       = (*patternValue)->clone();
                referenceType = true;
            } else
                LogConsole::abortEvaluation("cannot determine type of auto variable", type);
        } else {
            pattern = std::move(typePattern.front());
        }

        pattern->setVariableName(name);

        if (!referenceType) {
            pattern->setOffset(this->getStack().size());
            pattern->setLocal(true);
            this->getStack().emplace_back();
        }

        if (outVariable)
            this->m_outVariables[name] = pattern->getOffset();

        variables.push_back(std::move(pattern));
    }

    void Evaluator::setVariable(const std::string &name, const Token::Literal &value) {
        std::unique_ptr<Pattern> pattern = nullptr;

        {
            auto &variables = *this->getScope(0).scope;
            for (auto &variable : variables) {
                if (variable->getVariableName() == name) {
                    pattern = variable->clone();
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

                    pattern = variable->clone();
                    break;
                }
            }
        }

        if (pattern == nullptr)
            LogConsole::abortEvaluation(hex::format("no variable with name '{}' found", name));

        if (!pattern->isLocal()) return;

        Token::Literal castedLiteral = std::visit(overloaded {
                                                      [&](double &value) -> Token::Literal {
                                                          if (dynamic_cast<PatternUnsigned *>(pattern.get()))
                                                              return u128(value) & bitmask(pattern->getSize() * 8);
                                                          else if (dynamic_cast<PatternSigned *>(pattern.get()))
                                                              return i128(value) & bitmask(pattern->getSize() * 8);
                                                          else if (dynamic_cast<PatternFloat *>(pattern.get()))
                                                              return pattern->getSize() == sizeof(float) ? double(float(value)) : value;
                                                          else
                                                              LogConsole::abortEvaluation(hex::format("cannot cast type 'double' to type '{}'", pattern->getTypeName()));
                                                      },
                                                      [&](const std::string &value) -> Token::Literal {
                                                          if (dynamic_cast<PatternString *>(pattern.get()))
                                                              return value;
                                                          else
                                                              LogConsole::abortEvaluation(hex::format("cannot cast type 'string' to type '{}'", pattern->getTypeName()));
                                                      },
                                                      [&](Pattern *value) -> Token::Literal {
                                                          if (value->getTypeName() == pattern->getTypeName())
                                                              return value;
                                                          else
                                                              LogConsole::abortEvaluation(hex::format("cannot cast type '{}' to type '{}'", value->getTypeName(), pattern->getTypeName()));
                                                      },
                                                      [&](auto &&value) -> Token::Literal {
                                                          if (dynamic_cast<PatternUnsigned *>(pattern.get()) || dynamic_cast<PatternEnum *>(pattern.get()))
                                                              return u128(value) & bitmask(pattern->getSize() * 8);
                                                          else if (dynamic_cast<PatternSigned *>(pattern.get()))
                                                              return i128(value) & bitmask(pattern->getSize() * 8);
                                                          else if (dynamic_cast<PatternCharacter *>(pattern.get()))
                                                              return char(value);
                                                          else if (dynamic_cast<PatternBoolean *>(pattern.get()))
                                                              return bool(value);
                                                          else if (dynamic_cast<PatternFloat *>(pattern.get()))
                                                              return pattern->getSize() == sizeof(float) ? double(float(value)) : value;
                                                          else
                                                              LogConsole::abortEvaluation(hex::format("cannot cast integer literal to type '{}'", pattern->getTypeName()));
                                                      } },
            value);

        this->getStack()[pattern->getOffset()] = castedLiteral;
    }

    std::optional<std::vector<std::shared_ptr<Pattern>>> Evaluator::evaluate(const std::vector<std::shared_ptr<ASTNode>> &ast) {
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

        this->m_customFunctionDefinitions.clear();

        std::vector<std::shared_ptr<Pattern>> patterns;

        try {
            this->setCurrentControlFlowStatement(ControlFlowStatement::None);
            pushScope(nullptr, patterns);
            ON_SCOPE_EXIT {
                popScope();
            };

            for (auto &node : ast) {
                if (dynamic_cast<ASTNodeTypeDecl *>(node.get())) {
                    ;    // Don't create patterns from type declarations
                } else if (dynamic_cast<ASTNodeFunctionCall *>(node.get())) {
                    (void)node->evaluate(this);
                } else if (dynamic_cast<ASTNodeFunctionDefinition *>(node.get())) {
                    this->m_customFunctionDefinitions.push_back(node->evaluate(this));
                } else if (auto varDeclNode = dynamic_cast<ASTNodeVariableDecl *>(node.get())) {
                    for (auto &pattern : node->createPatterns(this)) {
                        if (varDeclNode->getPlacementOffset() == nullptr) {
                            auto type = varDeclNode->getType()->evaluate(this);

                            auto &name = pattern->getVariableName();
                            this->createVariable(name, type.get(), std::nullopt, varDeclNode->isOutVariable());

                            if (varDeclNode->isInVariable() && this->m_inVariables.contains(name))
                                this->setVariable(name, this->m_inVariables[name]);
                        } else {
                            patterns.push_back(std::move(pattern));
                        }
                    }
                } else {
                    auto newPatterns = node->createPatterns(this);
                    std::move(newPatterns.begin(), newPatterns.end(), std::back_inserter(patterns));
                }
            }

            if (this->m_customFunctions.contains("main")) {
                auto mainFunction = this->m_customFunctions["main"];

                if (mainFunction.parameterCount.max > 0)
                    LogConsole::abortEvaluation("main function may not accept any arguments");

                this->m_mainResult = mainFunction.func(this, {});
            }
        } catch (PatternLanguageError &error) {
            if (error.getLineNumber() != 0)
                this->m_console.setHardError(error);

            patterns.clear();

            this->m_currPatternCount = 0;

            return std::nullopt;
        }

        // Remove global local variables
        std::erase_if(patterns, [](const std::shared_ptr<Pattern> &pattern) {
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