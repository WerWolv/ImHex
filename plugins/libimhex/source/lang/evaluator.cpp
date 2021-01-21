#include <hex/lang/evaluator.hpp>

#include <hex/lang/token.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/api/content_registry.hpp>

#include <bit>
#include <algorithm>

#include <unistd.h>

namespace hex::lang {

    Evaluator::Evaluator(prv::Provider* &provider, std::endian defaultDataEndian)
        : m_provider(provider), m_defaultDataEndian(defaultDataEndian) {

        this->registerBuiltinFunctions();
    }

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

    PatternData* Evaluator::patternFromName(const std::vector<std::string> &path) {
        std::vector<PatternData*> currMembers;

        if (!this->m_currMembers.empty())
            std::copy(this->m_currMembers.back()->begin(), this->m_currMembers.back()->end(), std::back_inserter(currMembers));
        if (!this->m_globalMembers.empty())
            std::copy(this->m_globalMembers.begin(), this->m_globalMembers.end(), std::back_inserter(currMembers));

        PatternData *currPattern = nullptr;
        for (u32 i = 0; i < path.size(); i++) {
            const auto &identifier = path[i];

            if (auto structPattern = dynamic_cast<PatternDataStruct*>(currPattern); structPattern != nullptr)
                currMembers = structPattern->getMembers();
            else if (auto unionPattern = dynamic_cast<PatternDataUnion*>(currPattern); unionPattern != nullptr)
                currMembers = unionPattern->getMembers();
            else if (auto pointerPattern = dynamic_cast<PatternDataPointer*>(currPattern); pointerPattern != nullptr) {
                currPattern = pointerPattern->getPointedAtPattern();
                i--;
                continue;
            }
            else if (currPattern != nullptr)
                this->getConsole().abortEvaluation("tried to access member of a non-struct/union type");

            auto candidate = std::find_if(currMembers.begin(), currMembers.end(), [&](auto member) {
                return member->getVariableName() == identifier;
            });

            if (candidate != currMembers.end())
                currPattern = *candidate;
            else
                this->getConsole().abortEvaluation(hex::format("could not find identifier '%s'", identifier.c_str()));
        }

        if (auto pointerPattern = dynamic_cast<PatternDataPointer*>(currPattern); pointerPattern != nullptr)
            currPattern = pointerPattern->getPointedAtPattern();

        return currPattern;
    }

    ASTNodeIntegerLiteral* Evaluator::evaluateRValue(ASTNodeRValue *node) {
        if (this->m_currMembers.empty() && this->m_globalMembers.empty())
            this->getConsole().abortEvaluation("no variables available");

        if (node->getPath().size() == 1 && node->getPath()[0] == "$")
            return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned64Bit, this->m_currOffset });

        auto currPattern = this->patternFromName(node->getPath());

        if (auto unsignedPattern = dynamic_cast<PatternDataUnsigned*>(currPattern); unsignedPattern != nullptr) {
            u8 value[unsignedPattern->getSize()];
            this->m_provider->read(unsignedPattern->getOffset(), value, unsignedPattern->getSize());

            switch (unsignedPattern->getSize()) {
                case 1:  return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned8Bit,   hex::changeEndianess(*reinterpret_cast<u8*>(value),   1,  unsignedPattern->getEndian()) });
                case 2:  return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned16Bit,  hex::changeEndianess(*reinterpret_cast<u16*>(value),  2,  unsignedPattern->getEndian()) });
                case 4:  return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned32Bit,  hex::changeEndianess(*reinterpret_cast<u32*>(value),  4,  unsignedPattern->getEndian()) });
                case 8:  return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned64Bit,  hex::changeEndianess(*reinterpret_cast<u64*>(value),  8,  unsignedPattern->getEndian()) });
                case 16: return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned128Bit, hex::changeEndianess(*reinterpret_cast<u128*>(value), 16, unsignedPattern->getEndian()) });
                default: this->getConsole().abortEvaluation("invalid rvalue size");
            }
        } else if (auto signedPattern = dynamic_cast<PatternDataSigned*>(currPattern); signedPattern != nullptr) {
            u8 value[unsignedPattern->getSize()];
            this->m_provider->read(signedPattern->getOffset(), value, signedPattern->getSize());

            switch (unsignedPattern->getSize()) {
                case 1:  return new ASTNodeIntegerLiteral({ Token::ValueType::Signed8Bit,   hex::changeEndianess(*reinterpret_cast<s8*>(value),   1,  signedPattern->getEndian()) });
                case 2:  return new ASTNodeIntegerLiteral({ Token::ValueType::Signed16Bit,  hex::changeEndianess(*reinterpret_cast<s16*>(value),  2,  signedPattern->getEndian()) });
                case 4:  return new ASTNodeIntegerLiteral({ Token::ValueType::Signed32Bit,  hex::changeEndianess(*reinterpret_cast<s32*>(value),  4,  signedPattern->getEndian()) });
                case 8:  return new ASTNodeIntegerLiteral({ Token::ValueType::Signed64Bit,  hex::changeEndianess(*reinterpret_cast<s64*>(value),  8,  signedPattern->getEndian()) });
                case 16: return new ASTNodeIntegerLiteral({ Token::ValueType::Signed128Bit, hex::changeEndianess(*reinterpret_cast<s128*>(value), 16, signedPattern->getEndian()) });
                default: this->getConsole().abortEvaluation("invalid rvalue size");
            }
        } else if (auto enumPattern = dynamic_cast<PatternDataEnum*>(currPattern); enumPattern != nullptr) {
            u8 value[enumPattern->getSize()];
            this->m_provider->read(enumPattern->getOffset(), value, enumPattern->getSize());

            switch (enumPattern->getSize()) {
                case 1:  return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned8Bit,   hex::changeEndianess(*reinterpret_cast<u8*>(value),   1,  enumPattern->getEndian()) });
                case 2:  return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned16Bit,  hex::changeEndianess(*reinterpret_cast<u16*>(value),  2,  enumPattern->getEndian()) });
                case 4:  return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned32Bit,  hex::changeEndianess(*reinterpret_cast<u32*>(value),  4,  enumPattern->getEndian()) });
                case 8:  return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned64Bit,  hex::changeEndianess(*reinterpret_cast<u64*>(value),  8,  enumPattern->getEndian()) });
                case 16: return new ASTNodeIntegerLiteral({ Token::ValueType::Unsigned128Bit, hex::changeEndianess(*reinterpret_cast<u128*>(value), 16, enumPattern->getEndian()) });
                default: this->getConsole().abortEvaluation("invalid rvalue size");
            }
        } else
            this->getConsole().abortEvaluation("tried to use non-integer value in numeric expression");
    }

    ASTNode* Evaluator::evaluateFunctionCall(ASTNodeFunctionCall *node) {
        std::vector<ASTNode*> evaluatedParams;
        ScopeExit paramCleanup([&] {
           for (auto &param : evaluatedParams)
               delete param;
        });

        for (auto &param : node->getParams()) {
            if (auto numericExpression = dynamic_cast<ASTNodeNumericExpression*>(param); numericExpression != nullptr)
                evaluatedParams.push_back(this->evaluateMathematicalExpression(numericExpression));
            else if (auto stringLiteral = dynamic_cast<ASTNodeStringLiteral*>(param); stringLiteral != nullptr)
                evaluatedParams.push_back(stringLiteral->clone());
        }

        if (!ContentRegistry::PatternLanguageFunctions::getEntries().contains(node->getFunctionName().data()))
            this->getConsole().abortEvaluation(hex::format("no function named '%s' found", node->getFunctionName().data()));

        auto &function = ContentRegistry::PatternLanguageFunctions::getEntries()[node->getFunctionName().data()];

        if (function.parameterCount == ContentRegistry::PatternLanguageFunctions::UnlimitedParameters) {
            ; // Don't check parameter count
        }
        else if (function.parameterCount & ContentRegistry::PatternLanguageFunctions::LessParametersThan) {
            if (evaluatedParams.size() >= (function.parameterCount & ~ContentRegistry::PatternLanguageFunctions::LessParametersThan))
                this->getConsole().abortEvaluation(hex::format("too many parameters for function '%s'. Expected %d", node->getFunctionName().data(), function.parameterCount & ~ContentRegistry::PatternLanguageFunctions::LessParametersThan));
        } else if (function.parameterCount & ContentRegistry::PatternLanguageFunctions::MoreParametersThan) {
            if (evaluatedParams.size() <= (function.parameterCount & ~ContentRegistry::PatternLanguageFunctions::MoreParametersThan))
                this->getConsole().abortEvaluation(hex::format("too few parameters for function '%s'. Expected %d", node->getFunctionName().data(), function.parameterCount & ~ContentRegistry::PatternLanguageFunctions::MoreParametersThan));
        } else if (function.parameterCount != evaluatedParams.size()) {
            this->getConsole().abortEvaluation(hex::format("invalid number of parameters for function '%s'. Expected %d", node->getFunctionName().data(), function.parameterCount));
        }

        return function.func(this->getConsole(), evaluatedParams);
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
        auto newType = [&] {
            #define CHECK_TYPE(type) if (left->getType() == (type) || right->getType() == (type)) return (type)
            #define DEFAULT_TYPE(type) return (type)

            if (left->getType() == Token::ValueType::Any && right->getType() != Token::ValueType::Any)
                return right->getType();
            if (left->getType() != Token::ValueType::Any && right->getType() == Token::ValueType::Any)
                return left->getType();

            CHECK_TYPE(Token::ValueType::Double);
            CHECK_TYPE(Token::ValueType::Float);
            CHECK_TYPE(Token::ValueType::Unsigned128Bit);
            CHECK_TYPE(Token::ValueType::Signed128Bit);
            CHECK_TYPE(Token::ValueType::Unsigned64Bit);
            CHECK_TYPE(Token::ValueType::Signed64Bit);
            CHECK_TYPE(Token::ValueType::Unsigned32Bit);
            CHECK_TYPE(Token::ValueType::Signed32Bit);
            CHECK_TYPE(Token::ValueType::Unsigned16Bit);
            CHECK_TYPE(Token::ValueType::Signed16Bit);
            CHECK_TYPE(Token::ValueType::Unsigned8Bit);
            CHECK_TYPE(Token::ValueType::Signed8Bit);
            CHECK_TYPE(Token::ValueType::Character);
            CHECK_TYPE(Token::ValueType::Boolean);
            DEFAULT_TYPE(Token::ValueType::Signed32Bit);

            #undef CHECK_TYPE
            #undef DEFAULT_TYPE
        }();

        try {
            return std::visit([&](auto &&leftValue, auto &&rightValue) -> ASTNodeIntegerLiteral * {
                switch (op) {
                    case Token::Operator::Plus:
                        return new ASTNodeIntegerLiteral({ newType, leftValue + rightValue });
                    case Token::Operator::Minus:
                        return new ASTNodeIntegerLiteral({ newType, leftValue - rightValue });
                    case Token::Operator::Star:
                        return new ASTNodeIntegerLiteral({ newType, leftValue * rightValue });
                    case Token::Operator::Slash:
                        return new ASTNodeIntegerLiteral({ newType, leftValue / rightValue });
                    case Token::Operator::Percent:
                        return new ASTNodeIntegerLiteral({ newType, modulus(leftValue, rightValue) });
                    case Token::Operator::ShiftLeft:
                        return new ASTNodeIntegerLiteral({ newType, shiftLeft(leftValue, rightValue) });
                    case Token::Operator::ShiftRight:
                        return new ASTNodeIntegerLiteral({ newType, shiftRight(leftValue, rightValue) });
                    case Token::Operator::BitAnd:
                        return new ASTNodeIntegerLiteral({ newType, bitAnd(leftValue, rightValue) });
                    case Token::Operator::BitXor:
                        return new ASTNodeIntegerLiteral({ newType, bitXor(leftValue, rightValue) });
                    case Token::Operator::BitOr:
                        return new ASTNodeIntegerLiteral({ newType, bitOr(leftValue, rightValue) });
                    case Token::Operator::BitNot:
                        return new ASTNodeIntegerLiteral({ newType, bitNot(leftValue, rightValue) });
                    case Token::Operator::BoolEquals:
                        return new ASTNodeIntegerLiteral({ newType, leftValue == rightValue });
                    case Token::Operator::BoolNotEquals:
                        return new ASTNodeIntegerLiteral({ newType, leftValue != rightValue });
                    case Token::Operator::BoolGreaterThan:
                        return new ASTNodeIntegerLiteral({ newType, leftValue > rightValue });
                    case Token::Operator::BoolLessThan:
                        return new ASTNodeIntegerLiteral({ newType, leftValue < rightValue });
                    case Token::Operator::BoolGreaterThanOrEquals:
                        return new ASTNodeIntegerLiteral({ newType, leftValue >= rightValue });
                    case Token::Operator::BoolLessThanOrEquals:
                        return new ASTNodeIntegerLiteral({ newType, leftValue <= rightValue });
                    case Token::Operator::BoolAnd:
                        return new ASTNodeIntegerLiteral({ newType, leftValue && rightValue });
                    case Token::Operator::BoolXor:
                        return new ASTNodeIntegerLiteral({ newType, leftValue && !rightValue || !leftValue && rightValue });
                    case Token::Operator::BoolOr:
                        return new ASTNodeIntegerLiteral({ newType, leftValue || rightValue });
                    case Token::Operator::BoolNot:
                        return new ASTNodeIntegerLiteral({ newType, !rightValue });
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
        }
        else
            this->getConsole().abortEvaluation("invalid operand");
    }

    ASTNodeIntegerLiteral* Evaluator::evaluateTernaryExpression(ASTNodeTernaryExpression *node) {
        switch (node->getOperator()) {
            case Token::Operator::TernaryConditional: {
                auto condition = this->evaluateOperand(node->getFirstOperand());
                SCOPE_EXIT( delete condition; );

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

    PatternData* Evaluator::evaluateBuiltinType(ASTNodeBuiltinType *node) {
        auto &type = node->getType();
        auto typeSize = Token::getTypeSize(type);

        PatternData *pattern;

        if (type == Token::ValueType::Character)
            pattern = new PatternDataCharacter(this->m_currOffset);
        else if (type == Token::ValueType::Boolean)
            pattern = new PatternDataBoolean(this->m_currOffset);
        else if (Token::isUnsigned(type))
            pattern = new PatternDataUnsigned(this->m_currOffset, typeSize);
        else if (Token::isSigned(type))
            pattern = new PatternDataSigned(this->m_currOffset, typeSize);
        else if (Token::isFloatingPoint(type))
            pattern = new PatternDataFloat(this->m_currOffset, typeSize);
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

        this->m_currMembers.push_back(&memberPatterns);
        SCOPE_EXIT( this->m_currMembers.pop_back(); );

        auto startOffset = this->m_currOffset;
        for (auto &member : node->getMembers()) {
            this->evaluateMember(member, memberPatterns, true);
        }

        return new PatternDataStruct(startOffset, this->m_currOffset - startOffset, memberPatterns);
    }

    PatternData* Evaluator::evaluateUnion(ASTNodeUnion *node) {
        std::vector<PatternData*> memberPatterns;

        this->m_currMembers.push_back(&memberPatterns);
        SCOPE_EXIT( this->m_currMembers.pop_back(); );

        auto startOffset = this->m_currOffset;

        for (auto &member : node->getMembers()) {
            this->evaluateMember(member, memberPatterns, false);
        }

        size_t size = 0;
        for (const auto &pattern : memberPatterns)
            size = std::max(size, pattern->getSize());

        this->m_currOffset += size;

        return new PatternDataUnion(startOffset, size, memberPatterns);
    }

    PatternData* Evaluator::evaluateEnum(ASTNodeEnum *node) {
        std::vector<std::pair<Token::IntegerLiteral, std::string>> entryPatterns;

        auto startOffset = this->m_currOffset;
        for (auto &[name, value] : node->getEntries()) {
            auto expression = dynamic_cast<ASTNodeNumericExpression*>(value);
            if (expression == nullptr)
                this->getConsole().abortEvaluation("invalid expression in enum value");

            auto valueNode = evaluateMathematicalExpression(expression);
            SCOPE_EXIT( delete valueNode; );

            entryPatterns.push_back({{ valueNode->getType(), valueNode->getValue() }, name });
        }

        auto underlyingType = dynamic_cast<ASTNodeTypeDecl*>(node->getUnderlyingType());
        if (underlyingType == nullptr)
            this->getConsole().abortEvaluation("enum underlying type was not ASTNodeTypeDecl. This is a bug");

        size_t size;
        if (auto builtinType = dynamic_cast<ASTNodeBuiltinType*>(underlyingType->getType()); builtinType != nullptr)
            size = Token::getTypeSize(builtinType->getType());
        else
            this->getConsole().abortEvaluation("invalid enum underlying type");

        this->m_currOffset += size;

        return new PatternDataEnum(startOffset, size, entryPatterns);;
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
            SCOPE_EXIT( delete valueNode; );

            auto fieldBits = std::visit([this, node, type = valueNode->getType()] (auto &&value) {
                if (Token::isFloatingPoint(type))
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

        return new PatternDataBitfield(startOffset, size, entryPatterns);
    }

    PatternData* Evaluator::evaluateType(ASTNodeTypeDecl *node) {
        auto type = node->getType();

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
            SCOPE_EXIT( delete valueNode; );

            this->m_currOffset = std::visit([this, node, type = valueNode->getType()] (auto &&value) {
                if (Token::isFloatingPoint(type))
                    this->getConsole().abortEvaluation("placement offset must be an integer value");
                return static_cast<u64>(value);
            }, valueNode->getValue());
        }
        if (this->m_currOffset >= this->m_provider->getActualSize())
            this->getConsole().abortEvaluation("variable placed out of range");

        PatternData *pattern;
        if (auto typeDecl = dynamic_cast<ASTNodeTypeDecl*>(node->getType()); typeDecl != nullptr)
            pattern = this->evaluateType(typeDecl);
        else if (auto builtinTypeDecl = dynamic_cast<ASTNodeBuiltinType*>(node->getType()); builtinTypeDecl != nullptr)
            pattern = this->evaluateBuiltinType(builtinTypeDecl);
        else
            this->getConsole().abortEvaluation("ASTNodeVariableDecl had an invalid type. This is a bug!");

        pattern->setVariableName(node->getName().data());

        return pattern;
    }

    PatternData* Evaluator::evaluateArray(ASTNodeArrayVariableDecl *node) {

        if (auto offset = dynamic_cast<ASTNodeNumericExpression*>(node->getPlacementOffset()); offset != nullptr) {
            auto valueNode = evaluateMathematicalExpression(offset);
            SCOPE_EXIT( delete valueNode; );

            this->m_currOffset = std::visit([this, node, type = valueNode->getType()] (auto &&value) {
                if (Token::isFloatingPoint(type))
                    this->getConsole().abortEvaluation("placement offset must be an integer value");
                return static_cast<u64>(value);
            }, valueNode->getValue());
        }

        auto startOffset = this->m_currOffset;

        ASTNodeIntegerLiteral *valueNode;
        u64 arraySize = 0;

        if (node->getSize() != nullptr) {
            if (auto sizeNumericExpression = dynamic_cast<ASTNodeNumericExpression*>(node->getSize()); sizeNumericExpression != nullptr)
                valueNode = evaluateMathematicalExpression(sizeNumericExpression);
            else
                this->getConsole().abortEvaluation("array size not a numeric expression");

            SCOPE_EXIT( delete valueNode; );

            arraySize = std::visit([this, node, type = valueNode->getType()] (auto &&value) {
                if (Token::isFloatingPoint(type))
                    this->getConsole().abortEvaluation("array size must be an integer value");
                return static_cast<u64>(value);
            }, valueNode->getValue());

            if (auto typeDecl = dynamic_cast<ASTNodeTypeDecl*>(node->getType()); typeDecl != nullptr) {
                if (auto builtinType = dynamic_cast<ASTNodeBuiltinType*>(typeDecl->getType()); builtinType != nullptr) {
                    if (builtinType->getType() == Token::ValueType::Padding) {
                        this->m_currOffset += arraySize;
                        return new PatternDataPadding(startOffset, arraySize);
                    }
                }
            }
        } else {
            u8 currByte = 0x00;
            u64 offset = startOffset;

            do {
                this->m_provider->read(offset, &currByte, sizeof(u8));
                offset += sizeof(u8);
                arraySize += sizeof(u8);
            } while (currByte != 0x00 && offset < this->m_provider->getSize());
        }

        std::vector<PatternData*> entries;
        std::optional<u32> color;
        for (s128 i = 0; i < arraySize; i++) {
            PatternData *entry;
            if (auto typeDecl = dynamic_cast<ASTNodeTypeDecl*>(node->getType()); typeDecl != nullptr)
                entry = this->evaluateType(typeDecl);
            else if (auto builtinTypeDecl = dynamic_cast<ASTNodeBuiltinType*>(node->getType()); builtinTypeDecl != nullptr) {
                entry = this->evaluateBuiltinType(builtinTypeDecl);
            }
            else
                this->getConsole().abortEvaluation("ASTNodeVariableDecl had an invalid type. This is a bug!");

            entry->setVariableName(hex::format("[%llu]", (u64)i));
            entry->setEndian(this->getCurrentEndian());

            if (!color.has_value())
                color = entry->getColor();
            entry->setColor(color.value_or(0));

            entries.push_back(entry);

            if (this->m_currOffset > this->m_provider->getActualSize())
                this->getConsole().abortEvaluation("array exceeds size of file");
        }

        PatternData *pattern;
        if (entries.empty()) {
            pattern = new PatternDataPadding(startOffset, 0);
        }
        else if (dynamic_cast<PatternDataCharacter*>(entries[0]))
            pattern = new PatternDataString(startOffset, (this->m_currOffset - startOffset), color.value_or(0));
        else {
            if (node->getSize() == nullptr)
                this->getConsole().abortEvaluation("no bounds provided for array");
            pattern = new PatternDataArray(startOffset, (this->m_currOffset - startOffset), entries, color.value_or(0));
        }

        pattern->setVariableName(node->getName().data());

        return pattern;
    }

    PatternData* Evaluator::evaluatePointer(ASTNodePointerVariableDecl *node) {
        s128 pointerOffset;
        if (auto offset = dynamic_cast<ASTNodeNumericExpression*>(node->getPlacementOffset()); offset != nullptr) {
            auto valueNode = evaluateMathematicalExpression(offset);
            SCOPE_EXIT( delete valueNode; );

            pointerOffset = std::visit([this, node, type = valueNode->getType()] (auto &&value) {
                if (Token::isFloatingPoint(type))
                    this->getConsole().abortEvaluation("pointer offset must be an integer value");
                return static_cast<s128>(value);
            }, valueNode->getValue());
            this->m_currOffset = pointerOffset;
        } else {
            pointerOffset = this->m_currOffset;
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


        if (this->m_currOffset > this->m_provider->getActualSize())
            this->getConsole().abortEvaluation("pointer points past the end of the data");

        PatternData *pointedAt;
        if (auto typeDecl = dynamic_cast<ASTNodeTypeDecl*>(node->getType()); typeDecl != nullptr)
            pointedAt = this->evaluateType(typeDecl);
        else if (auto builtinTypeDecl = dynamic_cast<ASTNodeBuiltinType*>(node->getType()); builtinTypeDecl != nullptr)
            pointedAt = this->evaluateBuiltinType(builtinTypeDecl);
        else
            this->getConsole().abortEvaluation("ASTNodeVariableDecl had an invalid type. This is a bug!");

        this->m_currOffset = pointerOffset + pointerSize;

        auto pattern = new PatternDataPointer(pointerOffset, pointerSize, pointedAt);

        pattern->setVariableName(node->getName().data());
        pattern->setEndian(this->getCurrentEndian());

        return pattern;
    }

    std::optional<std::vector<PatternData*>> Evaluator::evaluate(const std::vector<ASTNode *> &ast) {

        try {
            for (const auto& node : ast) {
                this->m_endianStack.push_back(this->m_defaultDataEndian);

                if (auto variableDeclNode = dynamic_cast<ASTNodeVariableDecl*>(node); variableDeclNode != nullptr) {
                    this->m_globalMembers.push_back(this->evaluateVariable(variableDeclNode));
                } else if (auto arrayDeclNode = dynamic_cast<ASTNodeArrayVariableDecl*>(node); arrayDeclNode != nullptr) {
                    this->m_globalMembers.push_back(this->evaluateArray(arrayDeclNode));
                } else if (auto pointerDeclNode = dynamic_cast<ASTNodePointerVariableDecl*>(node); pointerDeclNode != nullptr) {
                    this->m_globalMembers.push_back(this->evaluatePointer(pointerDeclNode));
                } else if (auto typeDeclNode = dynamic_cast<ASTNodeTypeDecl*>(node); typeDeclNode != nullptr) {
                    this->m_types[typeDeclNode->getName().data()] = typeDeclNode->getType();
                } else if (auto functionCallNode = dynamic_cast<ASTNodeFunctionCall*>(node); functionCallNode != nullptr) {
                    auto result = this->evaluateFunctionCall(functionCallNode);
                    delete result;
                }

                this->m_endianStack.clear();
            }
        } catch (LogConsole::EvaluateError &e) {
            this->getConsole().log(LogConsole::Level::Error, e);
            this->m_endianStack.clear();

            return { };
        }

        this->m_endianStack.clear();

        return this->m_globalMembers;
    }

}