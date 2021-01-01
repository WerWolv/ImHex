#include "lang/evaluator.hpp"

#include "lang/token.hpp"

#include <bit>
#include <algorithm>
#include <ranges>

#include <unistd.h>

namespace hex::lang {

    Evaluator::Evaluator(prv::Provider* &provider, std::endian defaultDataEndian)
        : m_provider(provider), m_defaultDataEndian(defaultDataEndian) {
    }

    ASTNodeIntegerLiteral* Evaluator::evaluateRValue(ASTNodeRValue *node) {

        const std::vector<PatternData*>* currMembers = this->m_currMembers;

        PatternData *currPattern = nullptr;
        for (const auto &identifier : node->getPath()) {
            if (auto structPattern = dynamic_cast<PatternDataStruct*>(currPattern); structPattern != nullptr)
                currMembers = &structPattern->getMembers();
            else if (auto unionPattern = dynamic_cast<PatternDataUnion*>(currPattern); unionPattern != nullptr)
                currMembers = &unionPattern->getMembers();
            else if (currPattern != nullptr)
                throwEvaluateError("tried to access member of a non-struct/union type", node->getLineNumber());

            auto candidate = std::find_if(currMembers->begin(), currMembers->end(), [&](auto member) {
                return member->getVariableName() == identifier;
            });

            if (candidate != currMembers->end())
                currPattern = *candidate;
            else
                throwEvaluateError(hex::format("could not find identifier '%s'", identifier.c_str()), node->getLineNumber());
        }

        if (auto unsignedPattern = dynamic_cast<PatternDataUnsigned*>(currPattern); unsignedPattern != nullptr) {
            s128 value = 0;
            this->m_provider->read(unsignedPattern->getOffset(), &value, unsignedPattern->getSize());
            return new ASTNodeIntegerLiteral(value, Token::ValueType::Signed128Bit);
        } else if (auto signedPattern = dynamic_cast<PatternDataSigned*>(currPattern); signedPattern != nullptr) {
            s128 value = 0;
            this->m_provider->read(signedPattern->getOffset(), &value, signedPattern->getSize());
            return new ASTNodeIntegerLiteral(signExtend(value, signedPattern->getSize() * 8, 128), Token::ValueType::Signed128Bit);
        } else
            throwEvaluateError("tried to use non-integer value in numeric expression", node->getLineNumber());
    }

    ASTNodeIntegerLiteral* Evaluator::evaluateOperator(ASTNodeIntegerLiteral *left, ASTNodeIntegerLiteral *right, Token::Operator op) {
        return std::visit([&](auto &&leftValue, auto &&rightValue) -> ASTNodeIntegerLiteral* {

            auto newType = [&] {
                #define CHECK_TYPE(type) if (left->getType() == (type) || right->getType() == (type)) return (type)
                #define DEFAULT_TYPE(type) return (type)

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
                DEFAULT_TYPE(Token::ValueType::Signed32Bit);

                #undef CHECK_TYPE
                #undef DEFAULT_TYPE
            }();

            switch (op) {
                case Token::Operator::Plus:
                    return new ASTNodeIntegerLiteral(leftValue + rightValue, newType);
                case Token::Operator::Minus:
                    return new ASTNodeIntegerLiteral(leftValue - rightValue, newType);
                case Token::Operator::Star:
                    return new ASTNodeIntegerLiteral(leftValue * rightValue, newType);
                case Token::Operator::Slash:
                    return new ASTNodeIntegerLiteral(leftValue / rightValue, newType);
                case Token::Operator::ShiftLeft:
                    return new ASTNodeIntegerLiteral(leftValue << rightValue, newType);
                case Token::Operator::ShiftRight:
                    return new ASTNodeIntegerLiteral(leftValue >> rightValue, newType);
                case Token::Operator::BitAnd:
                    return new ASTNodeIntegerLiteral(leftValue & rightValue, newType);
                case Token::Operator::BitXor:
                    return new ASTNodeIntegerLiteral(leftValue ^ rightValue, newType);
                case Token::Operator::BitOr:
                    return new ASTNodeIntegerLiteral(leftValue | rightValue, newType);
                default: throwEvaluateError("invalid operator used in mathematical expression", left->getLineNumber());

            }

        }, left->getValue(), right->getValue());
    }

    ASTNodeIntegerLiteral* Evaluator::evaluateMathematicalExpression(ASTNodeNumericExpression *node) {
        ASTNodeIntegerLiteral *leftInteger, *rightInteger;

        if (auto leftExprLiteral = dynamic_cast<ASTNodeIntegerLiteral*>(node->getLeftOperand()); leftExprLiteral != nullptr)
            leftInteger = leftExprLiteral;
        else if (auto leftExprExpression = dynamic_cast<ASTNodeNumericExpression*>(node->getLeftOperand()); leftExprExpression != nullptr)
            leftInteger = evaluateMathematicalExpression(leftExprExpression);
        else if (auto leftExprRvalue = dynamic_cast<ASTNodeRValue*>(node->getLeftOperand()); leftExprRvalue != nullptr)
            leftInteger = evaluateRValue(leftExprRvalue);
        else
            throwEvaluateError("invalid expression. Expected integer literal", node->getLineNumber());

        if (auto rightExprLiteral = dynamic_cast<ASTNodeIntegerLiteral*>(node->getRightOperand()); rightExprLiteral != nullptr)
            rightInteger = rightExprLiteral;
        else if (auto rightExprExpression = dynamic_cast<ASTNodeNumericExpression*>(node->getRightOperand()); rightExprExpression != nullptr)
            rightInteger = evaluateMathematicalExpression(rightExprExpression);
        else if (auto rightExprRvalue = dynamic_cast<ASTNodeRValue*>(node->getRightOperand()); rightExprRvalue != nullptr)
            rightInteger = evaluateRValue(rightExprRvalue);
        else
            throwEvaluateError("invalid expression. Expected integer literal", node->getLineNumber());

        return evaluateOperator(leftInteger, rightInteger, node->getOperator());
    }

    PatternData* Evaluator::evaluateBuiltinType(ASTNodeBuiltinType *node) {
        auto &type = node->getType();
        auto typeSize = Token::getTypeSize(type);

        PatternData *pattern;

        if (type == Token::ValueType::Character)
            pattern = new PatternDataCharacter(this->m_currOffset);
        else if (Token::isUnsigned(type))
            pattern = new PatternDataUnsigned(this->m_currOffset, typeSize);
        else if (Token::isSigned(type))
            pattern = new PatternDataSigned(this->m_currOffset, typeSize);
        else if (Token::isFloatingPoint(type))
            pattern = new PatternDataFloat(this->m_currOffset, typeSize);
        else
            throwEvaluateError("invalid builtin type", node->getLineNumber());

        this->m_currOffset += typeSize;

        pattern->setTypeName(Token::getTypeName(type));

        return pattern;
    }

    PatternData* Evaluator::evaluateStruct(ASTNodeStruct *node) {
        std::vector<PatternData*> memberPatterns;

        ScopeExit currMemberReset([this] { this->m_currMembers = nullptr; });
        if (this->m_currMembers == nullptr)
            this->m_currMembers = &memberPatterns;
        else
            currMemberReset.release();

        auto startOffset = this->m_currOffset;
        for (auto &member : node->getMembers()) {
            if (auto memberVariableNode = dynamic_cast<ASTNodeVariableDecl*>(member); memberVariableNode != nullptr)
                memberPatterns.emplace_back(this->evaluateVariable(memberVariableNode));
            else if (auto memberArrayNode = dynamic_cast<ASTNodeArrayVariableDecl*>(member); memberArrayNode != nullptr)
                memberPatterns.emplace_back(this->evaluateArray(memberArrayNode));
            else
                throwEvaluateError("invalid struct member", member->getLineNumber());

            this->m_currEndian.reset();
        }

        return new PatternDataStruct(startOffset, this->m_currOffset - startOffset, memberPatterns);
    }

    PatternData* Evaluator::evaluateUnion(ASTNodeUnion *node) {
        std::vector<PatternData*> memberPatterns;

        ScopeExit currMemberReset([this] { this->m_currMembers = nullptr; });
        if (this->m_currMembers == nullptr)
            this->m_currMembers = &memberPatterns;
        else
            currMemberReset.release();

        auto startOffset = this->m_currOffset;
        for (auto &member : node->getMembers()) {
            if (auto memberVariableNode = dynamic_cast<ASTNodeVariableDecl*>(member); memberVariableNode != nullptr)
                memberPatterns.emplace_back(this->evaluateVariable(memberVariableNode));
            else if (auto memberArrayNode = dynamic_cast<ASTNodeArrayVariableDecl*>(member); memberArrayNode != nullptr)
                memberPatterns.emplace_back(this->evaluateArray(memberArrayNode));
            else
                throwEvaluateError("invalid union member", member->getLineNumber());

            this->m_currOffset = startOffset;
            this->m_currEndian.reset();
        }

        return new PatternDataUnion(startOffset, this->m_currOffset - startOffset, memberPatterns);
    }

    PatternData* Evaluator::evaluateEnum(ASTNodeEnum *node) {
        std::vector<std::pair<u64, std::string>> entryPatterns;

        auto startOffset = this->m_currOffset;
        for (auto &[name, value] : node->getEntries()) {
            auto expression = dynamic_cast<ASTNodeNumericExpression*>(value);
            if (expression == nullptr)
                throwEvaluateError("invalid expression in enum value", value->getLineNumber());

            auto valueNode = evaluateMathematicalExpression(expression);
            SCOPE_EXIT( delete valueNode; );

            entryPatterns.emplace_back( std::get<s128>(valueNode->getValue()), name );
        }

        size_t size;
        if (auto underlyingType = dynamic_cast<const ASTNodeBuiltinType*>(node->getUnderlyingType()); underlyingType != nullptr)
            size = Token::getTypeSize(underlyingType->getType());
        else
            throwEvaluateError("invalid enum underlying type", node->getLineNumber());

        return new PatternDataEnum(startOffset, size, entryPatterns);
    }

    PatternData* Evaluator::evaluateBitfield(ASTNodeBitfield *node) {
        std::vector<std::pair<std::string, size_t>> entryPatterns;

        auto startOffset = this->m_currOffset;
        size_t bits = 0;
        for (auto &[name, value] : node->getEntries()) {
            auto expression = dynamic_cast<ASTNodeNumericExpression*>(value);
            if (expression == nullptr)
                throwEvaluateError("invalid expression in bitfield field size", value->getLineNumber());

            auto valueNode = evaluateMathematicalExpression(expression);
            SCOPE_EXIT( delete valueNode; );

            auto fieldBits = std::get<s128>(valueNode->getValue());
            if (fieldBits > 64)
                throwEvaluateError("bitfield entry must at most occupy 64 bits", value->getLineNumber());

            bits += fieldBits;

            entryPatterns.emplace_back(name, fieldBits);
        }

        return new PatternDataBitfield(startOffset, (bits / 8) + 1, entryPatterns);
    }

    PatternData* Evaluator::evaluateType(ASTNodeTypeDecl *node) {
        auto type = node->getType();

        if (!this->m_currEndian.has_value())
            this->m_currEndian = node->getEndian();

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
            throwEvaluateError("type could not be evaluated", node->getLineNumber());

        if (!node->getName().empty())
            pattern->setTypeName(node->getName().data());

        return pattern;
    }

    PatternData* Evaluator::evaluateVariable(ASTNodeVariableDecl *node) {

        if (auto offset = dynamic_cast<ASTNodeNumericExpression*>(node->getPlacementOffset()); offset != nullptr) {
            auto valueNode = evaluateMathematicalExpression(offset);
            SCOPE_EXIT( delete valueNode; );

            this->m_currOffset = std::get<s128>(valueNode->getValue());
        }
        if (this->m_currOffset >= this->m_provider->getActualSize())
            throwEvaluateError("array exceeds size of file", node->getLineNumber());

        PatternData *pattern;
        if (auto typeDecl = dynamic_cast<ASTNodeTypeDecl*>(node->getType()); typeDecl != nullptr)
            pattern = this->evaluateType(typeDecl);
        else if (auto builtinTypeDecl = dynamic_cast<ASTNodeBuiltinType*>(node->getType()); builtinTypeDecl != nullptr)
            pattern = this->evaluateBuiltinType(builtinTypeDecl);
        else
            throwEvaluateError("ASTNodeVariableDecl had an invalid type. This is a bug!", 1);

        pattern->setVariableName(node->getName().data());
        pattern->setEndian(this->getCurrentEndian());
        this->m_currEndian.reset();

        return pattern;
    }

    PatternData* Evaluator::evaluateArray(ASTNodeArrayVariableDecl *node) {

        if (auto offset = dynamic_cast<ASTNodeNumericExpression*>(node->getPlacementOffset()); offset != nullptr) {
            auto valueNode = evaluateMathematicalExpression(offset);
            SCOPE_EXIT( delete valueNode; );

            this->m_currOffset = std::get<s128>(valueNode->getValue());
        }

        auto startOffset = this->m_currOffset;

        auto sizeNode = dynamic_cast<ASTNodeNumericExpression*>(node->getSize());
        if (sizeNode == nullptr)
            throwEvaluateError("array size not a numeric expression", node->getLineNumber());

        auto valueNode = evaluateMathematicalExpression(sizeNode);
        SCOPE_EXIT( delete valueNode; );

        auto arraySize = std::get<s128>(valueNode->getValue());

        if (auto typeDecl = dynamic_cast<ASTNodeTypeDecl*>(node->getType()); typeDecl != nullptr) {
            if (auto builtinType = dynamic_cast<ASTNodeBuiltinType*>(typeDecl->getType()); builtinType != nullptr) {
                if (builtinType->getType() == Token::ValueType::Padding) {
                    this->m_currOffset += arraySize;
                    return new PatternDataPadding(startOffset, arraySize);
                }
            }
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
                throwEvaluateError("ASTNodeVariableDecl had an invalid type. This is a bug!", 1);

            entry->setVariableName(hex::format("[%llu]", (u64)i));
            entry->setEndian(this->getCurrentEndian());

            if (!color.has_value())
                color = entry->getColor();
            entry->setColor(color.value_or(0));

            entries.push_back(entry);

            if (this->m_currOffset >= this->m_provider->getActualSize())
                throwEvaluateError("array exceeds size of file", node->getLineNumber());
        }

        this->m_currEndian.reset();

        if (entries.empty())
            throwEvaluateError("array size must be greater than zero", node->getLineNumber());


        PatternData *pattern;
        if (dynamic_cast<PatternDataCharacter*>(entries[0]))
            pattern = new PatternDataString(startOffset, (this->m_currOffset - startOffset), color.value_or(0));
        else
            pattern = new PatternDataArray(startOffset, (this->m_currOffset - startOffset), entries, color.value_or(0));

        pattern->setVariableName(node->getName().data());

        return pattern;
    }

    std::optional<std::vector<PatternData*>> Evaluator::evaluate(const std::vector<ASTNode *> &ast) {

        std::vector<PatternData*> patterns;

        try {
            for (const auto& node : ast) {
                this->m_currEndian.reset();

                if (auto variableDeclNode = dynamic_cast<ASTNodeVariableDecl*>(node); variableDeclNode != nullptr) {
                    patterns.push_back(this->evaluateVariable(variableDeclNode));
                } else if (auto arrayDeclNode = dynamic_cast<ASTNodeArrayVariableDecl*>(node); arrayDeclNode != nullptr) {
                    patterns.push_back(this->evaluateArray(arrayDeclNode));
                }

            }
        } catch (EvaluateError &e) {
            this->m_error = e;
            return { };
        }


        return patterns;
    }

}