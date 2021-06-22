#include <hex/lang/evaluator.hpp>

#include <hex/lang/token.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/api/content_registry.hpp>

#include <bit>
#include <algorithm>

#include <unistd.h>

namespace hex::lang {

    ASTNodeIntegerLiteral* Evaluator::evaluateScopeResolution(ASTNodeScopeResolution *node) {
        ASTNode *currScope = nullptr;
        for (const auto &identifier : node->getPath()) {
            if (currScope == nullptr) {
                if (!this->m_types.contains(identifier))
                    break;

                currScope = this->m_types[identifier.data()];
            } else if (auto enumNode = dynamic_cast<ASTNodeEnum*>(currScope); enumNode != nullptr) {
                if (!enumNode->getEntries().contains(identifier))
                    break;
                else
                    return evaluateMathematicalExpression(static_cast<ASTNodeNumericExpression*>(enumNode->getEntries().at(identifier)));
            }
        }

        this->getConsole().abortEvaluation("failed to find identifier");
    }

    PatternData* Evaluator::findPattern(std::vector<PatternData*> currMembers, const ASTNodeRValue::Path &path) {
        PatternData *currPattern = nullptr;
        for (const auto &part : path) {
            if (auto stringPart = std::get_if<std::string>(&part); stringPart != nullptr) {
                if (*stringPart == "parent") {
                    if (currPattern == nullptr) {
                        if (!currMembers.empty())
                            currPattern = this->m_currMemberScope.back();

                        if (currPattern == nullptr)
                            this->getConsole().abortEvaluation("attempted to get parent of global namespace");
                    }

                    auto parent = currPattern->getParent();

                    if (parent == nullptr) {
                        this->getConsole().abortEvaluation("no parent available for identifier");
                    } else {
                        currPattern = parent;
                    }
                } else {
                    if (currPattern != nullptr) {
                        if (auto structPattern = dynamic_cast<PatternDataStruct*>(currPattern); structPattern != nullptr)
                            currMembers = structPattern->getMembers();
                        else if (auto unionPattern = dynamic_cast<PatternDataUnion*>(currPattern); unionPattern != nullptr)
                            currMembers = unionPattern->getMembers();
                        else if (auto arrayPattern = dynamic_cast<PatternDataArray*>(currPattern); arrayPattern != nullptr) {
                            currMembers = arrayPattern->getEntries();
                            continue;
                        }
                        else
                            this->getConsole().abortEvaluation("tried to access member of a non-struct/union type");
                    }

                    auto candidate = std::find_if(currMembers.begin(), currMembers.end(), [&](auto member) {
                        return member->getVariableName() == *stringPart;
                    });

                    if (candidate != currMembers.end())
                        currPattern = *candidate;
                    else
                        this->getConsole().abortEvaluation(hex::format("no member found with identifier '{0}'", *stringPart));
                }
            } else if (auto nodePart = std::get_if<ASTNode*>(&part); nodePart != nullptr) {
                if (auto numericalExpressionNode = dynamic_cast<ASTNodeNumericExpression*>(*nodePart)) {
                    auto arrayIndexNode = evaluateMathematicalExpression(numericalExpressionNode);
                    ON_SCOPE_EXIT { delete arrayIndexNode; };

                    if (currPattern != nullptr) {
                        if (auto arrayPattern = dynamic_cast<PatternDataArray*>(currPattern); arrayPattern != nullptr) {
                            std::visit([this](auto &&arrayIndex) {
                                if (std::is_floating_point_v<decltype(arrayIndex)>)
                                    this->getConsole().abortEvaluation("cannot use float to index into array");
                            }, arrayIndexNode->getValue());

                            std::visit([&](auto &&arrayIndex){
                                if (arrayIndex >= 0 && arrayIndex < arrayPattern->getEntries().size())
                                    currPattern = arrayPattern->getEntries()[arrayIndex];
                                else
                                    this->getConsole().abortEvaluation(hex::format("tried to access out of bounds index {} of '{}'", arrayIndex, currPattern->getVariableName()));
                            }, arrayIndexNode->getValue());

                        }
                        else
                            this->getConsole().abortEvaluation("tried to index into non-array type");
                    }
                } else {
                    this->getConsole().abortEvaluation(hex::format("invalid node in rvalue path. This is a bug!'"));
                }
            }

            if (auto pointerPattern = dynamic_cast<PatternDataPointer*>(currPattern); pointerPattern != nullptr)
                currPattern = pointerPattern->getPointedAtPattern();
        }

        return currPattern;
    }

    PatternData* Evaluator::patternFromName(const ASTNodeRValue::Path &path) {

        PatternData *currPattern = nullptr;

        // Local variable access
        if (!this->m_localVariables.empty())
            currPattern = this->findPattern(*this->m_localVariables.back(), path);

        // If no local variable was found try local structure members
        if (this->m_currMembers.size() > 1) {
            currPattern = this->findPattern(*this->m_currMembers[this->m_currMembers.size() - 2], path);
        }

        // If no local member was found, try globally
        if (currPattern == nullptr) {
            currPattern = this->findPattern(this->m_globalMembers, path);
        }

        // If still no pattern was found, the path is invalid
        if (currPattern == nullptr) {
            std::string identifier;
            for (const auto& part : path) {
                if (part.index() == 0) {
                    // Path part is a identifier
                    identifier += std::get<std::string>(part);
                } else if (part.index() == 1) {
                    // Path part is a array index
                    identifier += "[..]";
                }

                identifier += ".";
            }
            identifier.pop_back();
            this->getConsole().abortEvaluation(hex::format("no identifier with name '{}' was found", identifier));
        }

        return currPattern;
    }

    ASTNodeIntegerLiteral* Evaluator::evaluateRValue(ASTNodeRValue *node) {
        if (node->getPath().size() == 1) {
            if (auto part = std::get_if<std::string>(&node->getPath()[0]); part != nullptr && *part == "$")
                return new ASTNodeIntegerLiteral(this->m_currOffset);
        }

        auto currPattern = this->patternFromName(node->getPath());

        if (auto unsignedPattern = dynamic_cast<PatternDataUnsigned*>(currPattern); unsignedPattern != nullptr) {

            u8 value[unsignedPattern->getSize()];
            if (currPattern->isLocal())
                std::memcpy(value, this->m_localStack.data() + unsignedPattern->getOffset(), unsignedPattern->getSize());
            else
                this->m_provider->read(unsignedPattern->getOffset(), value, unsignedPattern->getSize());

            switch (unsignedPattern->getSize()) {
                case 1:  return new ASTNodeIntegerLiteral(hex::changeEndianess(*reinterpret_cast<u8*>(value),   1,  unsignedPattern->getEndian()));
                case 2:  return new ASTNodeIntegerLiteral(hex::changeEndianess(*reinterpret_cast<u16*>(value),  2,  unsignedPattern->getEndian()));
                case 4:  return new ASTNodeIntegerLiteral(hex::changeEndianess(*reinterpret_cast<u32*>(value),  4,  unsignedPattern->getEndian()));
                case 8:  return new ASTNodeIntegerLiteral(hex::changeEndianess(*reinterpret_cast<u64*>(value),  8,  unsignedPattern->getEndian()));
                case 16: return new ASTNodeIntegerLiteral(hex::changeEndianess(*reinterpret_cast<u128*>(value), 16, unsignedPattern->getEndian()));
                default: this->getConsole().abortEvaluation("invalid rvalue size");
            }
        } else if (auto signedPattern = dynamic_cast<PatternDataSigned*>(currPattern); signedPattern != nullptr) {
            u8 value[signedPattern->getSize()];
            if (currPattern->isLocal())
                std::memcpy(value, this->m_localStack.data() + signedPattern->getOffset(), signedPattern->getSize());
            else
                this->m_provider->read(signedPattern->getOffset(), value, signedPattern->getSize());

            switch (signedPattern->getSize()) {
                case 1:  return new ASTNodeIntegerLiteral(hex::changeEndianess(*reinterpret_cast<s8*>(value),   1,  signedPattern->getEndian()));
                case 2:  return new ASTNodeIntegerLiteral(hex::changeEndianess(*reinterpret_cast<s16*>(value),  2,  signedPattern->getEndian()));
                case 4:  return new ASTNodeIntegerLiteral(hex::changeEndianess(*reinterpret_cast<s32*>(value),  4,  signedPattern->getEndian()));
                case 8:  return new ASTNodeIntegerLiteral(hex::changeEndianess(*reinterpret_cast<s64*>(value),  8,  signedPattern->getEndian()));
                case 16: return new ASTNodeIntegerLiteral(hex::changeEndianess(*reinterpret_cast<s128*>(value), 16, signedPattern->getEndian()));
                default: this->getConsole().abortEvaluation("invalid rvalue size");
            }
        } else if (auto enumPattern = dynamic_cast<PatternDataEnum*>(currPattern); enumPattern != nullptr) {
            u8 value[enumPattern->getSize()];
            if (currPattern->isLocal())
                std::memcpy(value, this->m_localStack.data() + enumPattern->getOffset(), enumPattern->getSize());
            else
                this->m_provider->read(enumPattern->getOffset(), value, enumPattern->getSize());

            switch (enumPattern->getSize()) {
                case 1:  return new ASTNodeIntegerLiteral(hex::changeEndianess(*reinterpret_cast<u8*>(value),   1,  enumPattern->getEndian()));
                case 2:  return new ASTNodeIntegerLiteral(hex::changeEndianess(*reinterpret_cast<u16*>(value),  2,  enumPattern->getEndian()));
                case 4:  return new ASTNodeIntegerLiteral(hex::changeEndianess(*reinterpret_cast<u32*>(value),  4,  enumPattern->getEndian()));
                case 8:  return new ASTNodeIntegerLiteral(hex::changeEndianess(*reinterpret_cast<u64*>(value),  8,  enumPattern->getEndian()));
                case 16: return new ASTNodeIntegerLiteral(hex::changeEndianess(*reinterpret_cast<u128*>(value), 16, enumPattern->getEndian()));
                default: this->getConsole().abortEvaluation("invalid rvalue size");
            }
        } else
            this->getConsole().abortEvaluation("tried to use non-integer value in numeric expression");
    }

    ASTNode* Evaluator::evaluateFunctionCall(ASTNodeFunctionCall *node) {
        std::vector<ASTNode*> evaluatedParams;
        ON_SCOPE_EXIT {
           for (auto &param : evaluatedParams)
               delete param;
        };

        for (auto &param : node->getParams()) {
            if (auto numericExpression = dynamic_cast<ASTNodeNumericExpression*>(param); numericExpression != nullptr)
                evaluatedParams.push_back(this->evaluateMathematicalExpression(numericExpression));
            else if (auto typeOperatorExpression = dynamic_cast<ASTNodeTypeOperator*>(param); typeOperatorExpression != nullptr)
                evaluatedParams.push_back(this->evaluateTypeOperator(typeOperatorExpression));
            else if (auto stringLiteral = dynamic_cast<ASTNodeStringLiteral*>(param); stringLiteral != nullptr)
                evaluatedParams.push_back(stringLiteral->clone());
        }

        ContentRegistry::PatternLanguageFunctions::Function *function;
        if (this->m_definedFunctions.contains(node->getFunctionName().data()))
            function = &this->m_definedFunctions[node->getFunctionName().data()];
        else if (ContentRegistry::PatternLanguageFunctions::getEntries().contains(node->getFunctionName().data()))
            function = &ContentRegistry::PatternLanguageFunctions::getEntries()[node->getFunctionName().data()];
        else
            this->getConsole().abortEvaluation(hex::format("no function named '{0}' found", node->getFunctionName().data()));

        if (function->parameterCount == ContentRegistry::PatternLanguageFunctions::UnlimitedParameters) {
            ; // Don't check parameter count
        }
        else if (function->parameterCount & ContentRegistry::PatternLanguageFunctions::LessParametersThan) {
            if (evaluatedParams.size() >= (function->parameterCount & ~ContentRegistry::PatternLanguageFunctions::LessParametersThan))
                this->getConsole().abortEvaluation(hex::format("too many parameters for function '{0}'. Expected {1}", node->getFunctionName().data(), function->parameterCount & ~ContentRegistry::PatternLanguageFunctions::LessParametersThan));
        } else if (function->parameterCount & ContentRegistry::PatternLanguageFunctions::MoreParametersThan) {
            if (evaluatedParams.size() <= (function->parameterCount & ~ContentRegistry::PatternLanguageFunctions::MoreParametersThan))
                this->getConsole().abortEvaluation(hex::format("too few parameters for function '{0}'. Expected {1}", node->getFunctionName().data(), function->parameterCount & ~ContentRegistry::PatternLanguageFunctions::MoreParametersThan));
        } else if (function->parameterCount != evaluatedParams.size()) {
            this->getConsole().abortEvaluation(hex::format("invalid number of parameters for function '{0}'. Expected {1}", node->getFunctionName().data(), function->parameterCount));
        }

        return function->func(*this, evaluatedParams);
    }

    ASTNodeIntegerLiteral* Evaluator::evaluateTypeOperator(ASTNodeTypeOperator *typeOperatorNode) {
        if (auto rvalue = dynamic_cast<ASTNodeRValue*>(typeOperatorNode->getExpression()); rvalue != nullptr) {
            auto pattern = this->patternFromName(rvalue->getPath());

            switch (typeOperatorNode->getOperator()) {
                case Token::Operator::AddressOf:
                    return new ASTNodeIntegerLiteral(static_cast<u64>(pattern->getOffset()));
                case Token::Operator::SizeOf:
                    return new ASTNodeIntegerLiteral(static_cast<u64>(pattern->getSize()));
                default:
                    this->getConsole().abortEvaluation("invalid type operator used. This is a bug!");
            }
        } else {
            this->getConsole().abortEvaluation("non-rvalue used in type operator");
        }
    }

#define FLOAT_BIT_OPERATION(name) \
    auto name(hex::floating_point auto left, auto right) { throw std::runtime_error(""); return 0; } \
    auto name(auto left, hex::floating_point auto right) { throw std::runtime_error(""); return 0; } \
    auto name(hex::floating_point auto left, hex::floating_point auto right) { throw std::runtime_error(""); return 0; } \
    auto name(hex::integral auto left, hex::integral auto right)

    namespace {

        FLOAT_BIT_OPERATION(shiftLeft) {
            return left << right;
        }

        FLOAT_BIT_OPERATION(shiftRight) {
            return left >> right;
        }

        FLOAT_BIT_OPERATION(bitAnd) {
            return left & right;
        }

        FLOAT_BIT_OPERATION(bitOr) {
            return left | right;
        }

        FLOAT_BIT_OPERATION(bitXor) {
            return left ^ right;
        }

        FLOAT_BIT_OPERATION(bitNot) {
            return ~right;
        }

        FLOAT_BIT_OPERATION(modulus) {
            return left % right;
        }

    }

    ASTNodeIntegerLiteral* Evaluator::evaluateOperator(ASTNodeIntegerLiteral *left, ASTNodeIntegerLiteral *right, Token::Operator op) {
        try {
            return std::visit([&](auto &&leftValue, auto &&rightValue) -> ASTNodeIntegerLiteral * {
                switch (op) {
                    case Token::Operator::Plus:
                        return new ASTNodeIntegerLiteral(leftValue + rightValue);
                    case Token::Operator::Minus:
                        return new ASTNodeIntegerLiteral(leftValue - rightValue);
                    case Token::Operator::Star:
                        return new ASTNodeIntegerLiteral(leftValue * rightValue);
                    case Token::Operator::Slash:
                        if (rightValue == 0)
                            this->getConsole().abortEvaluation("Division by zero");
                        return new ASTNodeIntegerLiteral(leftValue / rightValue);
                    case Token::Operator::Percent:
                        if (rightValue == 0)
                            this->getConsole().abortEvaluation("Division by zero");
                        return new ASTNodeIntegerLiteral(modulus(leftValue, rightValue));
                    case Token::Operator::ShiftLeft:
                        return new ASTNodeIntegerLiteral(shiftLeft(leftValue, rightValue));
                    case Token::Operator::ShiftRight:
                        return new ASTNodeIntegerLiteral(shiftRight(leftValue, rightValue));
                    case Token::Operator::BitAnd:
                        return new ASTNodeIntegerLiteral(bitAnd(leftValue, rightValue));
                    case Token::Operator::BitXor:
                        return new ASTNodeIntegerLiteral(bitXor(leftValue, rightValue));
                    case Token::Operator::BitOr:
                        return new ASTNodeIntegerLiteral(bitOr(leftValue, rightValue));
                    case Token::Operator::BitNot:
                        return new ASTNodeIntegerLiteral(bitNot(leftValue, rightValue));
                    case Token::Operator::BoolEquals:
                        return new ASTNodeIntegerLiteral(leftValue == rightValue);
                    case Token::Operator::BoolNotEquals:
                        return new ASTNodeIntegerLiteral(leftValue != rightValue);
                    case Token::Operator::BoolGreaterThan:
                        return new ASTNodeIntegerLiteral(leftValue > rightValue);
                    case Token::Operator::BoolLessThan:
                        return new ASTNodeIntegerLiteral(leftValue < rightValue);
                    case Token::Operator::BoolGreaterThanOrEquals:
                        return new ASTNodeIntegerLiteral(leftValue >= rightValue);
                    case Token::Operator::BoolLessThanOrEquals:
                        return new ASTNodeIntegerLiteral(leftValue <= rightValue);
                    case Token::Operator::BoolAnd:
                        return new ASTNodeIntegerLiteral(leftValue && rightValue);
                    case Token::Operator::BoolXor:
                        return new ASTNodeIntegerLiteral(leftValue && !rightValue || !leftValue && rightValue);
                    case Token::Operator::BoolOr:
                        return new ASTNodeIntegerLiteral(leftValue || rightValue);
                    case Token::Operator::BoolNot:
                        return new ASTNodeIntegerLiteral(!rightValue);
                    default:
                        this->getConsole().abortEvaluation("invalid operator used in mathematical expression");
                }

            }, left->getValue(), right->getValue());
        } catch (std::runtime_error &e) {
            this->getConsole().abortEvaluation("bitwise operations on floating point numbers are forbidden");
        }
    }

    ASTNodeIntegerLiteral* Evaluator::evaluateOperand(ASTNode *node) {
        if (auto exprLiteral = dynamic_cast<ASTNodeIntegerLiteral*>(node); exprLiteral != nullptr)
            return exprLiteral;
        else if (auto exprExpression = dynamic_cast<ASTNodeNumericExpression*>(node); exprExpression != nullptr)
            return evaluateMathematicalExpression(exprExpression);
        else if (auto exprRvalue = dynamic_cast<ASTNodeRValue*>(node); exprRvalue != nullptr)
            return evaluateRValue(exprRvalue);
        else if (auto exprScopeResolution = dynamic_cast<ASTNodeScopeResolution*>(node); exprScopeResolution != nullptr)
            return evaluateScopeResolution(exprScopeResolution);
        else if (auto exprTernary = dynamic_cast<ASTNodeTernaryExpression*>(node); exprTernary != nullptr)
            return evaluateTernaryExpression(exprTernary);
        else if (auto exprFunctionCall = dynamic_cast<ASTNodeFunctionCall*>(node); exprFunctionCall != nullptr) {
            auto returnValue = evaluateFunctionCall(exprFunctionCall);

            if (returnValue == nullptr)
                this->getConsole().abortEvaluation("function returning void used in expression");
            else if (auto integerNode = dynamic_cast<ASTNodeIntegerLiteral*>(returnValue); integerNode != nullptr)
                return integerNode;
            else
                this->getConsole().abortEvaluation("function not returning a numeric value used in expression");
        } else if (auto typeOperator = dynamic_cast<ASTNodeTypeOperator*>(node); typeOperator != nullptr)
            return evaluateTypeOperator(typeOperator);
        else
            this->getConsole().abortEvaluation("invalid operand");
    }

    ASTNodeIntegerLiteral* Evaluator::evaluateTernaryExpression(ASTNodeTernaryExpression *node) {
        switch (node->getOperator()) {
            case Token::Operator::TernaryConditional: {
                auto condition = this->evaluateOperand(node->getFirstOperand());
                ON_SCOPE_EXIT { delete condition; };

                if (std::visit([](auto &&value){ return value != 0; }, condition->getValue()))
                    return this->evaluateOperand(node->getSecondOperand());
                else
                    return this->evaluateOperand(node->getThirdOperand());
            }
            default:
                this->getConsole().abortEvaluation("invalid operator used in ternary expression");
        }
    }

    ASTNodeIntegerLiteral* Evaluator::evaluateMathematicalExpression(ASTNodeNumericExpression *node) {
        auto leftInteger  = this->evaluateOperand(node->getLeftOperand());
        auto rightInteger = this->evaluateOperand(node->getRightOperand());

        return evaluateOperator(leftInteger, rightInteger, node->getOperator());
    }

    void Evaluator::createLocalVariable(std::string_view varName, PatternData *pattern) {
        auto startOffset = this->m_currOffset;
        ON_SCOPE_EXIT { this->m_currOffset = startOffset; };

        auto endOfStack = this->m_localStack.size();

        for (auto &variable : *this->m_localVariables.back()) {
            if (variable->getVariableName() == varName)
                this->getConsole().abortEvaluation(hex::format("redefinition of variable {}", varName));
        }

        this->m_localStack.resize(endOfStack + pattern->getSize());

        pattern->setVariableName(std::string(varName));
        pattern->setOffset(endOfStack);
        pattern->setLocal(true);
        this->m_localVariables.back()->push_back(pattern);
        std::memset(this->m_localStack.data() + pattern->getOffset(), 0x00, pattern->getSize());

    }

    void Evaluator::setLocalVariableValue(std::string_view varName, const void *value, size_t size) {
        PatternData *varPattern = nullptr;
        for (auto &var : *this->m_localVariables.back()) {
            if (var->getVariableName() == varName)
                varPattern = var;
        }

        std::memset(this->m_localStack.data() + varPattern->getOffset(), 0x00, varPattern->getSize());
        std::memcpy(this->m_localStack.data() + varPattern->getOffset(), value, std::min(varPattern->getSize(), size));
    }

    void Evaluator::evaluateFunctionDefinition(ASTNodeFunctionDefinition *node) {
        ContentRegistry::PatternLanguageFunctions::Function function = {
                (u32)node->getParams().size(),
                [paramNames = node->getParams(), body = node->getBody()](Evaluator& evaluator, std::vector<ASTNode*> &params) -> ASTNode* {
                    // Create local variables from parameters
                    std::vector<PatternData*> localVariables;
                    evaluator.m_localVariables.push_back(&localVariables);

                    ON_SCOPE_EXIT {
                        u32 stackSizeToDrop = 0;
                        for (auto &localVar : *evaluator.m_localVariables.back()) {
                            stackSizeToDrop += localVar->getSize();
                            delete localVar;
                        }
                        evaluator.m_localVariables.pop_back();
                        evaluator.m_localStack.resize(evaluator.m_localStack.size() - stackSizeToDrop);
                    };

                    auto startOffset = evaluator.m_currOffset;
                    for (u32 i = 0; i < params.size(); i++) {
                        if (auto integerLiteralNode = dynamic_cast<ASTNodeIntegerLiteral*>(params[i]); integerLiteralNode != nullptr) {
                            std::visit([&](auto &&value) {
                                using Type = std::remove_cvref_t<decltype(value)>;

                                PatternData *pattern;
                                if constexpr (std::is_unsigned_v<Type>)
                                    pattern = new PatternDataUnsigned(0, sizeof(value));
                                else if constexpr (std::is_signed_v<Type>)
                                    pattern = new PatternDataSigned(0, sizeof(value));
                                else if constexpr (std::is_floating_point_v<Type>)
                                    pattern = new PatternDataFloat(0, sizeof(value));
                                else return;

                                evaluator.createLocalVariable(paramNames[i], pattern);
                                evaluator.setLocalVariableValue(paramNames[i], &value, sizeof(value));
                            }, integerLiteralNode->getValue());
                        }
                    }
                    evaluator.m_currOffset = startOffset;

                    return evaluator.evaluateFunctionBody(body).value_or(nullptr);
                }
        };

        if (this->m_definedFunctions.contains(std::string(node->getName())))
            this->getConsole().abortEvaluation(hex::format("redefinition of function {}", node->getName()));

        this->m_definedFunctions.insert({ std::string(node->getName()), function });
    }

    std::optional<ASTNode*> Evaluator::evaluateFunctionBody(const std::vector<ASTNode*> &body) {
        std::optional<ASTNode*> returnResult;
        auto startOffset = this->m_currOffset;

        for (auto &statement : body) {
            ON_SCOPE_EXIT { this->m_currOffset = startOffset; };

            if (auto functionCallNode = dynamic_cast<ASTNodeFunctionCall*>(statement); functionCallNode != nullptr) {
                auto result = this->evaluateFunctionCall(functionCallNode);
                delete result;
            } else if (auto varDeclNode = dynamic_cast<ASTNodeVariableDecl*>(statement); varDeclNode != nullptr) {
                auto pattern = this->evaluateVariable(varDeclNode);
                this->createLocalVariable(varDeclNode->getName(), pattern);
            } else if (auto assignmentNode = dynamic_cast<ASTNodeAssignment*>(statement); assignmentNode != nullptr) {
                if (auto numericExpressionNode = dynamic_cast<ASTNodeNumericExpression*>(assignmentNode->getRValue()); numericExpressionNode != nullptr) {
                    auto value = this->evaluateMathematicalExpression(numericExpressionNode);
                    ON_SCOPE_EXIT { delete value; };

                    std::visit([&](auto &&value) {
                        this->setLocalVariableValue(assignmentNode->getLValueName(), &value, sizeof(value));
                    }, value->getValue());
                } else {
                    this->getConsole().abortEvaluation("invalid rvalue used in assignment");
                }
            } else if (auto returnNode = dynamic_cast<ASTNodeReturnStatement*>(statement); returnNode != nullptr) {
                if (returnNode->getRValue() == nullptr) {
                    returnResult = nullptr;
                } else if (auto numericExpressionNode = dynamic_cast<ASTNodeNumericExpression*>(returnNode->getRValue()); numericExpressionNode != nullptr) {
                    returnResult = this->evaluateMathematicalExpression(numericExpressionNode);
                } else {
                    this->getConsole().abortEvaluation("invalid rvalue used in return statement");
                }
            } else if (auto conditionalNode = dynamic_cast<ASTNodeConditionalStatement*>(statement); conditionalNode != nullptr) {
                if (auto numericExpressionNode = dynamic_cast<ASTNodeNumericExpression*>(conditionalNode->getCondition()); numericExpressionNode != nullptr) {
                    auto condition = this->evaluateMathematicalExpression(numericExpressionNode);

                    u32 localVariableStartCount = this->m_localVariables.back()->size();
                    u32 localVariableStackStartSize = this->m_localStack.size();

                    if (std::visit([](auto &&value) { return value != 0; }, condition->getValue()))
                        returnResult = this->evaluateFunctionBody(conditionalNode->getTrueBody());
                    else
                        returnResult = this->evaluateFunctionBody(conditionalNode->getFalseBody());

                    for (u32 i = localVariableStartCount; i < this->m_localVariables.back()->size(); i++)
                        delete (*this->m_localVariables.back())[i];
                    this->m_localVariables.back()->resize(localVariableStartCount);
                    this->m_localStack.resize(localVariableStackStartSize);

                } else {
                    this->getConsole().abortEvaluation("invalid rvalue used in return statement");
                }
            } else if (auto whileLoopNode = dynamic_cast<ASTNodeWhileStatement*>(statement); whileLoopNode != nullptr) {
                if (auto numericExpressionNode = dynamic_cast<ASTNodeNumericExpression*>(whileLoopNode->getCondition()); numericExpressionNode != nullptr) {
                    auto condition = this->evaluateMathematicalExpression(numericExpressionNode);

                    while (std::visit([](auto &&value) { return value != 0; }, condition->getValue())) {
                        u32 localVariableStartCount = this->m_localVariables.back()->size();
                        u32 localVariableStackStartSize = this->m_localStack.size();

                        returnResult = this->evaluateFunctionBody(whileLoopNode->getBody());
                        if (returnResult.has_value())
                            break;

                        for (u32 i = localVariableStartCount; i < this->m_localVariables.back()->size(); i++)
                            delete (*this->m_localVariables.back())[i];
                        this->m_localVariables.back()->resize(localVariableStartCount);
                        this->m_localStack.resize(localVariableStackStartSize);

                        condition = this->evaluateMathematicalExpression(numericExpressionNode);
                    }

                } else {
                    this->getConsole().abortEvaluation("invalid rvalue used in return statement");
                }
            }

            if (returnResult.has_value())
                return returnResult.value();
        }


        return { };
    }

    PatternData* Evaluator::evaluateAttributes(ASTNode *currNode, PatternData *currPattern) {
        auto attributableNode = dynamic_cast<Attributable*>(currNode);
        if (attributableNode == nullptr)
            this->getConsole().abortEvaluation("attributes applied to invalid expression");

        auto handleVariableAttributes = [this, &currPattern](auto attribute, auto value) {

            if (attribute == "color" && value.has_value())
                currPattern->setColor(hex::changeEndianess(u32(strtoul(value->data(), nullptr, 16)) << 8, std::endian::big));
            else if (attribute == "name" && value.has_value())
                currPattern->setVariableName(value->data());
            else if (attribute == "comment" && value.has_value())
                currPattern->setComment(value->data());
            else if (attribute == "hidden" && value.has_value())
                currPattern->setHidden(true);
            else
                this->getConsole().abortEvaluation("unknown or invalid attribute");

        };

        auto &attributes = attributableNode->getAttributes();

        if (attributes.empty())
            return currPattern;

        if (auto variableDeclNode = dynamic_cast<ASTNodeVariableDecl*>(currNode); variableDeclNode != nullptr) {
            for (auto &attribute : attributes)
                handleVariableAttributes(attribute->getAttribute(), attribute->getValue());
        } else if (auto arrayDeclNode = dynamic_cast<ASTNodeArrayVariableDecl*>(currNode); arrayDeclNode != nullptr) {
            for (auto &attribute : attributes)
                handleVariableAttributes(attribute->getAttribute(), attribute->getValue());
        } else if (auto pointerDeclNode = dynamic_cast<ASTNodePointerVariableDecl*>(currNode); pointerDeclNode != nullptr) {
            for (auto &attribute : attributes)
                handleVariableAttributes(attribute->getAttribute(), attribute->getValue());
        } else if (auto structNode = dynamic_cast<ASTNodeStruct*>(currNode); structNode != nullptr) {
            this->getConsole().abortEvaluation("unknown or invalid attribute");
        } else if (auto unionNode = dynamic_cast<ASTNodeUnion*>(currNode); unionNode != nullptr) {
            this->getConsole().abortEvaluation("unknown or invalid attribute");
        } else if (auto enumNode = dynamic_cast<ASTNodeEnum*>(currNode); enumNode != nullptr) {
            this->getConsole().abortEvaluation("unknown or invalid attribute");
        } else if (auto bitfieldNode = dynamic_cast<ASTNodeBitfield*>(currNode); bitfieldNode != nullptr) {
            this->getConsole().abortEvaluation("unknown or invalid attribute");
        } else
            this->getConsole().abortEvaluation("attributes applied to invalid expression");

        return currPattern;
    }

    PatternData* Evaluator::evaluateBuiltinType(ASTNodeBuiltinType *node) {
        auto &type = node->getType();
        auto typeSize = Token::getTypeSize(type);

        PatternData *pattern;

        if (type == Token::ValueType::Character)
            pattern = new PatternDataCharacter(this->m_currOffset);
        else if (type == Token::ValueType::Character16)
            pattern = new PatternDataCharacter16(this->m_currOffset);
        else if (type == Token::ValueType::Boolean)
            pattern = new PatternDataBoolean(this->m_currOffset);
        else if (Token::isUnsigned(type))
            pattern = new PatternDataUnsigned(this->m_currOffset, typeSize);
        else if (Token::isSigned(type))
            pattern = new PatternDataSigned(this->m_currOffset, typeSize);
        else if (Token::isFloatingPoint(type))
            pattern = new PatternDataFloat(this->m_currOffset, typeSize);
        else if (type == Token::ValueType::Padding)
            pattern = new PatternDataPadding(this->m_currOffset, 1);
        else
            this->getConsole().abortEvaluation("invalid builtin type");

        this->m_currOffset += typeSize;

        pattern->setTypeName(Token::getTypeName(type));
        pattern->setEndian(this->getCurrentEndian());

        return pattern;
    }

    void Evaluator::evaluateMember(ASTNode *node, std::vector<PatternData*> &currMembers, bool increaseOffset) {
        auto startOffset = this->m_currOffset;

        if (auto memberVariableNode = dynamic_cast<ASTNodeVariableDecl*>(node); memberVariableNode != nullptr)
            currMembers.push_back(this->evaluateVariable(memberVariableNode));
        else if (auto memberArrayNode = dynamic_cast<ASTNodeArrayVariableDecl*>(node); memberArrayNode != nullptr)
            currMembers.push_back(this->evaluateArray(memberArrayNode));
        else if (auto memberPointerNode = dynamic_cast<ASTNodePointerVariableDecl*>(node); memberPointerNode != nullptr)
            currMembers.push_back(this->evaluatePointer(memberPointerNode));
        else if (auto conditionalNode = dynamic_cast<ASTNodeConditionalStatement*>(node); conditionalNode != nullptr) {
            auto condition = this->evaluateMathematicalExpression(static_cast<ASTNodeNumericExpression*>(conditionalNode->getCondition()));

            if (std::visit([](auto &&value) { return value != 0; }, condition->getValue())) {
                for (auto &statement : conditionalNode->getTrueBody()) {
                    this->evaluateMember(statement, currMembers, increaseOffset);
                }
            } else {
                for (auto &statement : conditionalNode->getFalseBody()) {
                    this->evaluateMember(statement, currMembers, increaseOffset);
                }
            }

            delete condition;
        }
        else
            this->getConsole().abortEvaluation("invalid struct member");

        if (!increaseOffset)
            this->m_currOffset = startOffset;
    }

    PatternData* Evaluator::evaluateStruct(ASTNodeStruct *node) {
        std::vector<PatternData*> memberPatterns;

        auto structPattern = new PatternDataStruct(this->m_currOffset, 0);
        structPattern->setParent(this->m_currMemberScope.back());

        this->m_currMembers.push_back(&memberPatterns);
        this->m_currMemberScope.push_back(structPattern);
        ON_SCOPE_EXIT {
            this->m_currMembers.pop_back();
            this->m_currMemberScope.pop_back();
        };

        this->m_currRecursionDepth++;
        if (this->m_currRecursionDepth > this->m_recursionLimit)
            this->getConsole().abortEvaluation(hex::format("evaluation depth exceeds maximum of {0}. Use #pragma eval_depth <depth> to increase the maximum", this->m_recursionLimit));

        auto startOffset = this->m_currOffset;
        for (auto &member : node->getMembers()) {
            this->evaluateMember(member, memberPatterns, true);
            structPattern->setMembers(memberPatterns);
        }
        structPattern->setSize(this->m_currOffset - startOffset);

        this->m_currRecursionDepth--;

        return this->evaluateAttributes(node, structPattern);
    }

    PatternData* Evaluator::evaluateUnion(ASTNodeUnion *node) {
        std::vector<PatternData*> memberPatterns;

        auto unionPattern = new PatternDataUnion(this->m_currOffset, 0);
        unionPattern->setParent(this->m_currMemberScope.back());

        this->m_currMembers.push_back(&memberPatterns);
        this->m_currMemberScope.push_back(unionPattern);
        ON_SCOPE_EXIT {
            this->m_currMembers.pop_back();
            this->m_currMemberScope.pop_back();
        };

        auto startOffset = this->m_currOffset;

        this->m_currRecursionDepth++;
        if (this->m_currRecursionDepth > this->m_recursionLimit)
            this->getConsole().abortEvaluation(hex::format("evaluation depth exceeds maximum of {0}. Use #pragma eval_depth <depth> to increase the maximum", this->m_recursionLimit));

        for (auto &member : node->getMembers()) {
            this->evaluateMember(member, memberPatterns, false);
            unionPattern->setMembers(memberPatterns);
        }

        this->m_currRecursionDepth--;

        size_t size = 0;
        for (const auto &pattern : memberPatterns)
            size = std::max(size, pattern->getSize());
        unionPattern->setSize(size);

        this->m_currOffset += size;

        return this->evaluateAttributes(node, unionPattern);
    }

    PatternData* Evaluator::evaluateEnum(ASTNodeEnum *node) {
        std::vector<std::pair<Token::IntegerLiteral, std::string>> entryPatterns;

        auto underlyingType = dynamic_cast<ASTNodeTypeDecl*>(node->getUnderlyingType());
        if (underlyingType == nullptr)
            this->getConsole().abortEvaluation("enum underlying type was not ASTNodeTypeDecl. This is a bug");

        size_t size;
        auto builtinUnderlyingType = dynamic_cast<ASTNodeBuiltinType*>(underlyingType->getType());
        if (builtinUnderlyingType != nullptr)
            size = Token::getTypeSize(builtinUnderlyingType->getType());
        else
            this->getConsole().abortEvaluation("invalid enum underlying type");

        auto startOffset = this->m_currOffset;
        for (auto &[name, value] : node->getEntries()) {
            auto expression = dynamic_cast<ASTNodeNumericExpression*>(value);
            if (expression == nullptr)
                this->getConsole().abortEvaluation("invalid expression in enum value");

            auto valueNode = evaluateMathematicalExpression(expression);
            ON_SCOPE_EXIT { delete valueNode; };

            entryPatterns.emplace_back(valueNode->getValue(), name);
        }

        this->m_currOffset += size;

        auto enumPattern = new PatternDataEnum(startOffset, size);
        enumPattern->setSize(size);
        enumPattern->setEnumValues(entryPatterns);

        return this->evaluateAttributes(node, enumPattern);
    }

    PatternData* Evaluator::evaluateBitfield(ASTNodeBitfield *node) {
        std::vector<std::pair<std::string, size_t>> entryPatterns;

        auto startOffset = this->m_currOffset;
        size_t bits = 0;
        for (auto &[name, value] : node->getEntries()) {
            auto expression = dynamic_cast<ASTNodeNumericExpression*>(value);
            if (expression == nullptr)
                this->getConsole().abortEvaluation("invalid expression in bitfield field size");

            auto valueNode = evaluateMathematicalExpression(expression);
            ON_SCOPE_EXIT { delete valueNode; };

            auto fieldBits = std::visit([this] (auto &&value) {
                using Type = std::remove_cvref_t<decltype(value)>;
                if constexpr (std::is_floating_point_v<Type>)
                    this->getConsole().abortEvaluation("bitfield entry size must be an integer value");
                return static_cast<s128>(value);
            }, valueNode->getValue());

            if (fieldBits > 64 || fieldBits <= 0)
                this->getConsole().abortEvaluation("bitfield entry must occupy between 1 and 64 bits");

            bits += fieldBits;

            entryPatterns.emplace_back(name, fieldBits);
        }

        size_t size = (bits + 7) / 8;
        this->m_currOffset += size;

        auto bitfieldPattern = new PatternDataBitfield(startOffset, size);
        bitfieldPattern->setFields(entryPatterns);

        return this->evaluateAttributes(node, bitfieldPattern);
    }

    PatternData* Evaluator::evaluateType(ASTNodeTypeDecl *node) {
        auto type = node->getType();

        if (type == nullptr)
            type = this->m_types[node->getName().data()];

        this->m_endianStack.push_back(node->getEndian().value_or(this->m_defaultDataEndian));

        PatternData *pattern;

        if (auto builtinTypeNode = dynamic_cast<ASTNodeBuiltinType*>(type); builtinTypeNode != nullptr)
            return this->evaluateBuiltinType(builtinTypeNode);
        else if (auto typeDeclNode = dynamic_cast<ASTNodeTypeDecl*>(type); typeDeclNode != nullptr)
            pattern = this->evaluateType(typeDeclNode);
        else if (auto structNode = dynamic_cast<ASTNodeStruct*>(type); structNode != nullptr)
            pattern = this->evaluateStruct(structNode);
        else if (auto unionNode = dynamic_cast<ASTNodeUnion*>(type); unionNode != nullptr)
            pattern = this->evaluateUnion(unionNode);
        else if (auto enumNode = dynamic_cast<ASTNodeEnum*>(type); enumNode != nullptr)
            pattern = this->evaluateEnum(enumNode);
        else if (auto bitfieldNode = dynamic_cast<ASTNodeBitfield*>(type); bitfieldNode != nullptr)
            pattern = this->evaluateBitfield(bitfieldNode);
        else
            this->getConsole().abortEvaluation("type could not be evaluated");

        if (!node->getName().empty())
            pattern->setTypeName(node->getName().data());

        pattern->setEndian(this->getCurrentEndian());

        this->m_endianStack.pop_back();

        return pattern;
    }

    PatternData* Evaluator::evaluateVariable(ASTNodeVariableDecl *node) {

        if (auto offset = dynamic_cast<ASTNodeNumericExpression*>(node->getPlacementOffset()); offset != nullptr) {
            auto valueNode = evaluateMathematicalExpression(offset);
            ON_SCOPE_EXIT { delete valueNode; };

            this->m_currOffset = std::visit([this] (auto &&value) {
                using Type = std::remove_cvref_t<decltype(value)>;
                if constexpr (std::is_floating_point_v<Type>)
                    this->getConsole().abortEvaluation("bitfield entry size must be an integer value");
                return static_cast<u64>(value);
            }, valueNode->getValue());
        }

        if (this->m_currOffset < this->m_provider->getBaseAddress() || this->m_currOffset >= this->m_provider->getActualSize() + this->m_provider->getBaseAddress()) {
            if (node->getPlacementOffset() != nullptr)
                this->getConsole().abortEvaluation("variable placed out of range");
            else
                return nullptr;
        }

        PatternData *pattern;
        if (auto typeDecl = dynamic_cast<ASTNodeTypeDecl*>(node->getType()); typeDecl != nullptr)
            pattern = this->evaluateType(typeDecl);
        else if (auto builtinTypeDecl = dynamic_cast<ASTNodeBuiltinType*>(node->getType()); builtinTypeDecl != nullptr)
            pattern = this->evaluateBuiltinType(builtinTypeDecl);
        else
            this->getConsole().abortEvaluation("ASTNodeVariableDecl had an invalid type. This is a bug!");

        pattern->setVariableName(node->getName().data());

        return this->evaluateAttributes(node, pattern);
    }

    PatternData* Evaluator::evaluateArray(ASTNodeArrayVariableDecl *node) {

        // Evaluate placement of array
        if (auto offset = dynamic_cast<ASTNodeNumericExpression*>(node->getPlacementOffset()); offset != nullptr) {
            auto valueNode = evaluateMathematicalExpression(offset);
            ON_SCOPE_EXIT { delete valueNode; };

            this->m_currOffset = std::visit([this] (auto &&value) {
                using Type = std::remove_cvref_t<decltype(value)>;
                if constexpr (std::is_floating_point_v<Type>)
                    this->getConsole().abortEvaluation("bitfield entry size must be an integer value");
                return static_cast<u64>(value);
            }, valueNode->getValue());
        }

        // Check if placed in range of the data
        if (this->m_currOffset < this->m_provider->getBaseAddress() || this->m_currOffset >= this->m_provider->getActualSize() + this->m_provider->getBaseAddress()) {
            if (node->getPlacementOffset() != nullptr)
                this->getConsole().abortEvaluation("variable placed out of range");
            else
                return nullptr;
        }

        auto startOffset = this->m_currOffset;

        std::vector<PatternData*> entries;
        std::optional<u32> color;

        auto addEntry = [this, node, &entries, &color](u64 index) {
            PatternData *entry;
            if (auto typeDecl = dynamic_cast<ASTNodeTypeDecl*>(node->getType()); typeDecl != nullptr)
                entry = this->evaluateType(typeDecl);
            else if (auto builtinTypeDecl = dynamic_cast<ASTNodeBuiltinType*>(node->getType()); builtinTypeDecl != nullptr)
                entry = this->evaluateBuiltinType(builtinTypeDecl);
            else
                this->getConsole().abortEvaluation("ASTNodeVariableDecl had an invalid type. This is a bug!");

            entry->setVariableName(hex::format("[{0}]", index));
            entry->setEndian(this->getCurrentEndian());

            if (!color.has_value())
                color = entry->getColor();
            entry->setColor(color.value_or(0));

            if (this->m_currOffset > this->m_provider->getActualSize() + this->m_provider->getBaseAddress()) {
                delete entry;
                return;
            }

            entries.push_back(entry);
        };

        auto sizeNode = node->getSize();
        if (auto numericExpression = dynamic_cast<ASTNodeNumericExpression*>(sizeNode); numericExpression != nullptr) {
            // Parse explicit size of array
            auto valueNode = this->evaluateMathematicalExpression(numericExpression);
            ON_SCOPE_EXIT { delete valueNode; };

            auto arraySize = std::visit([this] (auto &&value) {
                using Type = std::remove_cvref_t<decltype(value)>;
                if constexpr (std::is_floating_point_v<Type>)
                    this->getConsole().abortEvaluation("bitfield entry size must be an integer value");
                return static_cast<u64>(value);
            }, valueNode->getValue());

            for (u64 i = 0; i < arraySize; i++) {
                addEntry(i);
            }

        } else if (auto whileLoopExpression = dynamic_cast<ASTNodeWhileStatement*>(sizeNode); whileLoopExpression != nullptr) {
            // Parse while loop based size of array
            auto conditionNode = this->evaluateMathematicalExpression(static_cast<ASTNodeNumericExpression*>(whileLoopExpression->getCondition()));
            ON_SCOPE_EXIT { delete conditionNode; };

            u64 index = 0;
            while (std::visit([](auto &&value) { return value != 0; }, conditionNode->getValue())) {

                addEntry(index);
                index++;

                delete conditionNode;
                conditionNode = this->evaluateMathematicalExpression(static_cast<ASTNodeNumericExpression*>(whileLoopExpression->getCondition()));
            }
        } else {
            // Parse unsized array

            if (auto typeDecl = dynamic_cast<ASTNodeTypeDecl*>(node->getType()); typeDecl != nullptr) {
                if (auto builtinType = dynamic_cast<ASTNodeBuiltinType*>(typeDecl->getType()); builtinType != nullptr) {
                    std::vector<u8> bytes(Token::getTypeSize(builtinType->getType()), 0x00);
                    u64 offset = startOffset;

                    u64 index = 0;
                    do {
                        this->m_provider->read(offset, bytes.data(), bytes.size());
                        offset += bytes.size();
                        addEntry(index);
                        index++;
                    } while (!std::all_of(bytes.begin(), bytes.end(), [](u8 byte){ return byte == 0x00; }) && offset < this->m_provider->getSize());
                }
            }
        }

        auto deleteEntries = SCOPE_GUARD {
            for (auto &entry : entries)
                delete entry;
        };

        if (auto typeDecl = dynamic_cast<ASTNodeTypeDecl*>(node->getType()); typeDecl != nullptr) {
            if (auto builtinType = dynamic_cast<ASTNodeBuiltinType *>(typeDecl->getType()); builtinType != nullptr) {
                if (builtinType->getType() == Token::ValueType::Padding)
                    return new PatternDataPadding(startOffset, this->m_currOffset - startOffset);
            }
        }

        PatternData *pattern;
        if (entries.empty())
            pattern = new PatternDataPadding(startOffset, 0);
        else if (dynamic_cast<PatternDataCharacter*>(entries[0]) != nullptr)
            pattern = new PatternDataString(startOffset, (this->m_currOffset - startOffset), color.value_or(0));
        else if (dynamic_cast<PatternDataCharacter16*>(entries[0]) != nullptr)
            pattern = new PatternDataString16(startOffset, (this->m_currOffset - startOffset), color.value_or(0));
        else {
            if (node->getSize() == nullptr)
                this->getConsole().abortEvaluation("no bounds provided for array");
            auto arrayPattern = new PatternDataArray(startOffset, (this->m_currOffset - startOffset), color.value_or(0));

            arrayPattern->setEntries(entries);
            deleteEntries.release();

            pattern = arrayPattern;
        }

        pattern->setVariableName(node->getName().data());

        return this->evaluateAttributes(node, pattern);
    }

    PatternData* Evaluator::evaluatePointer(ASTNodePointerVariableDecl *node) {
        s128 pointerOffset;
        if (auto offset = dynamic_cast<ASTNodeNumericExpression*>(node->getPlacementOffset()); offset != nullptr) {
            auto valueNode = evaluateMathematicalExpression(offset);
            ON_SCOPE_EXIT { delete valueNode; };

            pointerOffset = std::visit([this] (auto &&value) {
                using Type = std::remove_cvref_t<decltype(value)>;
                if constexpr (std::is_floating_point_v<Type>)
                    this->getConsole().abortEvaluation("bitfield entry size must be an integer value");
                return static_cast<s128>(value);
            }, valueNode->getValue());
            this->m_currOffset = pointerOffset;
        } else {
            pointerOffset = this->m_currOffset;
        }

        if (this->m_currOffset < this->m_provider->getBaseAddress() || this->m_currOffset >= this->m_provider->getActualSize() + this->m_provider->getBaseAddress()) {
            if (node->getPlacementOffset() != nullptr)
                this->getConsole().abortEvaluation("variable placed out of range");
            else
                return nullptr;
        }

        PatternData *sizeType;

        auto underlyingType = dynamic_cast<ASTNodeTypeDecl*>(node->getSizeType());
        if (underlyingType == nullptr)
            this->getConsole().abortEvaluation("underlying type is not ASTNodeTypeDecl. This is a bug");

        if (auto builtinTypeNode = dynamic_cast<ASTNodeBuiltinType*>(underlyingType->getType()); builtinTypeNode != nullptr) {
            sizeType = evaluateBuiltinType(builtinTypeNode);
        } else
            this->getConsole().abortEvaluation("pointer size is not a builtin type");

        size_t pointerSize = sizeType->getSize();

        u128 pointedAtOffset = 0;
        this->m_provider->read(pointerOffset, &pointedAtOffset, pointerSize);
        this->m_currOffset = hex::changeEndianess(pointedAtOffset, pointerSize, underlyingType->getEndian().value_or(this->m_defaultDataEndian));

        delete sizeType;


        if (this->m_currOffset > this->m_provider->getActualSize() + this->m_provider->getBaseAddress())
            this->getConsole().abortEvaluation("pointer points past the end of the data");

        PatternData *pointedAt;
        if (auto typeDecl = dynamic_cast<ASTNodeTypeDecl*>(node->getType()); typeDecl != nullptr)
            pointedAt = this->evaluateType(typeDecl);
        else if (auto builtinTypeDecl = dynamic_cast<ASTNodeBuiltinType*>(node->getType()); builtinTypeDecl != nullptr)
            pointedAt = this->evaluateBuiltinType(builtinTypeDecl);
        else
            this->getConsole().abortEvaluation("ASTNodeVariableDecl had an invalid type. This is a bug!");

        this->m_currOffset = pointerOffset + pointerSize;

        auto pattern = new PatternDataPointer(pointerOffset, pointerSize);

        pattern->setVariableName(node->getName().data());
        pattern->setEndian(this->getCurrentEndian());
        pattern->setPointedAtPattern(pointedAt);

        return this->evaluateAttributes(node, pattern);
    }

    std::optional<std::vector<PatternData*>> Evaluator::evaluate(const std::vector<ASTNode *> &ast) {

        this->m_globalMembers.clear();
        this->m_types.clear();
        this->m_endianStack.clear();
        this->m_definedFunctions.clear();
        this->m_currOffset = 0;

        try {
            for (const auto& node : ast) {
                if (auto typeDeclNode = dynamic_cast<ASTNodeTypeDecl*>(node); typeDeclNode != nullptr) {
                    if (this->m_types[typeDeclNode->getName().data()] == nullptr)
                        this->m_types[typeDeclNode->getName().data()] = typeDeclNode->getType();
                }
            }

            for (const auto& [name, node] : this->m_types) {
                if (auto typeDeclNode = static_cast<ASTNodeTypeDecl*>(node); typeDeclNode->getType() == nullptr)
                    this->getConsole().abortEvaluation(hex::format("unresolved type '{}'", name));
            }

            for (const auto& node : ast) {
                this->m_currMembers.clear();
                this->m_currMemberScope.clear();
                this->m_currMemberScope.push_back(nullptr);

                this->m_endianStack.push_back(this->m_defaultDataEndian);
                this->m_currRecursionDepth = 0;

                PatternData *pattern = nullptr;

                if (auto variableDeclNode = dynamic_cast<ASTNodeVariableDecl*>(node); variableDeclNode != nullptr) {
                    pattern = this->evaluateVariable(variableDeclNode);
                } else if (auto arrayDeclNode = dynamic_cast<ASTNodeArrayVariableDecl*>(node); arrayDeclNode != nullptr) {
                    pattern = this->evaluateArray(arrayDeclNode);
                } else if (auto pointerDeclNode = dynamic_cast<ASTNodePointerVariableDecl*>(node); pointerDeclNode != nullptr) {
                    pattern = this->evaluatePointer(pointerDeclNode);
                } else if (auto typeDeclNode = dynamic_cast<ASTNodeTypeDecl*>(node); typeDeclNode != nullptr) {
                    // Handled above
                } else if (auto functionCallNode = dynamic_cast<ASTNodeFunctionCall*>(node); functionCallNode != nullptr) {
                    auto result = this->evaluateFunctionCall(functionCallNode);
                    delete result;
                } else if (auto functionDefNode = dynamic_cast<ASTNodeFunctionDefinition*>(node); functionDefNode != nullptr) {
                    this->evaluateFunctionDefinition(functionDefNode);
                }

                if (pattern != nullptr)
                    this->m_globalMembers.push_back(pattern);

                this->m_endianStack.clear();
            }
        } catch (LogConsole::EvaluateError &e) {
            this->getConsole().log(LogConsole::Level::Error, e);

            return { };
        }

        return this->m_globalMembers;
    }

}