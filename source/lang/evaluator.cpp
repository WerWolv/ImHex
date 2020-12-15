#include "lang/evaluator.hpp"

#include "lang/token.hpp"

#include <bit>

namespace hex::lang {

    Evaluator::Evaluator(prv::Provider* &provider, std::endian defaultDataEndian)
        : m_provider(provider), m_defaultDataEndian(defaultDataEndian) {

    }

    PatternData* Evaluator::evaluateBuiltinType(ASTNodeBuiltinType *node) {
        auto &type = node->getType();
        auto typeSize = Token::getTypeSize(type);

        PatternData *pattern;

        if (Token::isUnsigned(type))
            pattern = new PatternDataUnsigned(this->m_currOffset, typeSize);
        else if (Token::isSigned(type) && typeSize == 1)
            pattern = new PatternDataCharacter(this->m_currOffset);
        else if (Token::isSigned(type))
            pattern = new PatternDataSigned(this->m_currOffset, typeSize);
        else if (Token::isFloatingPoint(type))
            pattern = new PatternDataFloat(this->m_currOffset, typeSize);
        else
            throwEvaluateError("Invalid builtin type", node->getLineNumber());

        this->m_currOffset += typeSize;

        pattern->setTypeName(Token::getTypeName(type));

        return pattern;
    }

    PatternData* Evaluator::evaluateStruct(ASTNodeStruct *node) {
        std::vector<PatternData*> memberPatterns;

        auto startOffset = this->m_currOffset;
        for (auto &member : node->getMembers()) {
            if (auto memberVariableNode = dynamic_cast<ASTNodeVariableDecl*>(member); memberVariableNode != nullptr)
                memberPatterns.emplace_back(this->evaluateVariable(memberVariableNode));
            else if (auto memberArrayNode = dynamic_cast<ASTNodeArrayVariableDecl*>(member); memberArrayNode != nullptr)
                memberPatterns.emplace_back(this->evaluateArray(memberArrayNode));
            else
                throwEvaluateError("Invalid struct member", member->getLineNumber());
        }

        return new PatternDataStruct(startOffset, this->m_currOffset - startOffset, memberPatterns);
    }

    PatternData* Evaluator::evaluateUnion(ASTNodeUnion *node) {
        std::vector<PatternData*> memberPatterns;

        auto startOffset = this->m_currOffset;
        for (auto &member : node->getMembers()) {
            if (auto memberVariableNode = dynamic_cast<ASTNodeVariableDecl*>(member); memberVariableNode != nullptr)
                memberPatterns.emplace_back(this->evaluateVariable(memberVariableNode));
            else if (auto memberArrayNode = dynamic_cast<ASTNodeArrayVariableDecl*>(member); memberArrayNode != nullptr)
                memberPatterns.emplace_back(this->evaluateArray(memberArrayNode));
            else
                throwEvaluateError("Invalid union member", member->getLineNumber());
        }

        return new PatternDataUnion(startOffset, this->m_currOffset - startOffset, memberPatterns);
    }

    PatternData* Evaluator::evaluateEnum(ASTNodeEnum *node) {
        std::vector<std::pair<u64, std::string>> entryPatterns;

        auto startOffset = this->m_currOffset;
        for (auto &[name, value] : node->getEntries()) {
            auto expression = dynamic_cast<ASTNodeNumericExpression*>(value);
            if (expression == nullptr)
                throwEvaluateError("Invalid expression in enum value", value->getLineNumber());

            entryPatterns.emplace_back( std::get<s128>(expression->evaluate()->getValue()), name );
        }

        size_t size;
        if (auto underlyingType = dynamic_cast<const ASTNodeBuiltinType*>(node->getUnderlyingType()); underlyingType != nullptr)
            size = Token::getTypeSize(underlyingType->getType());
        else
            throwEvaluateError("Invalid enum underlying type", node->getLineNumber());

        return new PatternDataEnum(startOffset, size, entryPatterns);
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
        else
            throwEvaluateError("Type could not be evaluated", node->getLineNumber());

        if (!node->getName().empty())
            pattern->setTypeName(node->getName().data());

        return pattern;
    }

    PatternData* Evaluator::evaluateVariable(ASTNodeVariableDecl *node) {
        const auto& type = dynamic_cast<ASTNodeTypeDecl*>(node->getType());

        if (type == nullptr)
            throwEvaluateError("ASTNodeVariableDecl had an invalid type. This is a bug!", 1);

        if (node->getPlacementOffset().has_value())
            this->m_currOffset = node->getPlacementOffset().value();

        auto pattern = this->evaluateType(type);
        pattern->setVariableName(node->getName().data());
        pattern->setEndian(this->getCurrentEndian());

        return pattern;
    }

    PatternData* Evaluator::evaluateArray(ASTNodeArrayVariableDecl *node) {
        const auto& type = dynamic_cast<ASTNodeTypeDecl*>(node->getType());

        if (type == nullptr)
            throwEvaluateError("ASTNodeVariableDecl had an invalid type. This is a bug!", 1);

        if (node->getPlacementOffset().has_value())
            this->m_currOffset = node->getPlacementOffset().value();

        auto startOffset = this->m_currOffset;

        auto sizeNode = dynamic_cast<ASTNodeNumericExpression*>(node->getSize());
        if (sizeNode == nullptr)
            throwEvaluateError("Array size not a numeric expression", node->getLineNumber());

        auto arraySize = std::get<s128>(sizeNode->evaluate()->getValue());

        std::vector<PatternData*> entries;
        for (s128 i = 0; i < arraySize; i++) {
            auto entry = this->evaluateType(type);
            entry->setVariableName(node->getName().data());
            entry->setTypeName(node->getName().data());
            entry->setEndian(this->getCurrentEndian());

            entries.push_back(entry);
        }

        return new PatternDataArray(startOffset, (this->m_currOffset - startOffset) * arraySize, entries);
    }

    std::pair<Result, std::vector<PatternData*>> Evaluator::evaluate(const std::vector<ASTNode *> &ast) {

        std::vector<PatternData*> patterns;

        try {
            for (const auto& node : ast) {

                if (auto variableDeclNode = dynamic_cast<ASTNodeVariableDecl*>(node); variableDeclNode != nullptr) {
                    patterns.push_back(this->evaluateVariable(variableDeclNode));
                } else if (auto arrayDeclNode = dynamic_cast<ASTNodeArrayVariableDecl*>(node); arrayDeclNode != nullptr) {
                    patterns.push_back(this->evaluateArray(arrayDeclNode));
                }

            }
        } catch (EvaluateError &e) {
            this->m_error = e;
            return { ResultEvaluatorError, { } };
        }


        return { ResultSuccess, patterns };
    }

}