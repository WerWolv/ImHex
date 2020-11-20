#include "lang/evaluator.hpp"

#include <bit>
#include <unordered_map>

namespace hex::lang {

    Evaluator::Evaluator() {

    }

    std::pair<PatternData*, size_t> Evaluator::createStructPattern(ASTNodeVariableDecl *varDeclNode, u64 offset) {
        std::vector<PatternData*> members;

        auto structNode = static_cast<ASTNodeStruct*>(this->m_types[varDeclNode->getCustomVariableTypeName()]);

        if (structNode == nullptr)
            return { nullptr, 0 };

        size_t structSize = 0;
        for (const auto &node : structNode->getNodes()) {
            const auto &member = static_cast<ASTNodeVariableDecl*>(node);

            const auto typeDeclNode = static_cast<ASTNodeTypeDecl*>(this->m_types[member->getCustomVariableTypeName()]);

            if (member->getVariableType() == Token::TypeToken::Type::Signed8Bit && member->getArraySize() > 1) {
                const auto &[pattern, size] = this->createStringPattern(member, offset + structSize);

                if (pattern == nullptr)
                    return { nullptr, 0 };

                members.push_back(pattern);
                structSize += size;
            } else if (member->getVariableType() == Token::TypeToken::Type::CustomType
                && typeDeclNode != nullptr && typeDeclNode->getAssignedType() == Token::TypeToken::Type::Signed8Bit
                && member->getArraySize() > 1) {

                const auto &[pattern, size] = this->createStringPattern(member, offset + structSize);

                if (pattern == nullptr)
                    return { nullptr, 0 };

                members.push_back(pattern);
                structSize += size;
            }
            else if (member->getArraySize() > 1) {
                const auto &[pattern, size] = this->createArrayPattern(member, offset + structSize);

                if (pattern == nullptr)
                    return { nullptr, 0 };

                members.push_back(pattern);
                structSize += size;
            }
            else if (member->getVariableType() != Token::TypeToken::Type::CustomType) {
                const auto &[pattern, size] = this->createBuiltInTypePattern(member, offset + structSize);

                if (pattern == nullptr)
                    return { nullptr, 0 };

                members.push_back(pattern);
                structSize += size;
            }
            else {
                const auto &[pattern, size] = this->createCustomTypePattern(member, offset + structSize);

                if (pattern == nullptr)
                    return { nullptr, 0 };

                members.push_back(pattern);
                structSize += size;
            }
        }

        return { new PatternDataStruct(offset, structSize, varDeclNode->getVariableName(), structNode->getName(), members, 0x00FFFFFF), structSize };
    }

    std::pair<PatternData*, size_t> Evaluator::createUnionPattern(ASTNodeVariableDecl *varDeclNode, u64 offset) {
        std::vector<PatternData*> members;

        auto unionNode = static_cast<ASTNodeUnion*>(this->m_types[varDeclNode->getCustomVariableTypeName()]);

        if (unionNode == nullptr)
            return { nullptr, 0 };

        size_t unionSize = 0;
        for (const auto &node : unionNode->getNodes()) {
            const auto &member = static_cast<ASTNodeVariableDecl*>(node);

            const auto typeDeclNode = static_cast<ASTNodeTypeDecl*>(this->m_types[member->getCustomVariableTypeName()]);

            if (member->getVariableType() == Token::TypeToken::Type::Signed8Bit && member->getArraySize() > 1) {
                const auto &[pattern, size] = this->createStringPattern(member, offset);

                if (pattern == nullptr)
                    return { nullptr, 0 };

                members.push_back(pattern);
                unionSize = std::max(size, unionSize);
            } else if (member->getVariableType() == Token::TypeToken::Type::CustomType
                       && typeDeclNode != nullptr && typeDeclNode->getAssignedType() == Token::TypeToken::Type::Signed8Bit
                       && member->getArraySize() > 1) {

                const auto &[pattern, size] = this->createStringPattern(member, offset);

                if (pattern == nullptr)
                    return { nullptr, 0 };

                members.push_back(pattern);
                unionSize = std::max(size, unionSize);
            }
            else if (member->getArraySize() > 1) {
                const auto &[pattern, size] = this->createArrayPattern(member, offset);

                if (pattern == nullptr)
                    return { nullptr, 0 };

                members.push_back(pattern);
                unionSize = std::max(size, unionSize);
            }
            else if (member->getVariableType() != Token::TypeToken::Type::CustomType) {
                const auto &[pattern, size] = this->createBuiltInTypePattern(member, offset);

                if (pattern == nullptr)
                    return { nullptr, 0 };

                members.push_back(pattern);
                unionSize = std::max(size, unionSize);
            }
            else {
                const auto &[pattern, size] = this->createCustomTypePattern(member, offset);

                if (pattern == nullptr)
                    return { nullptr, 0 };

                members.push_back(pattern);
                unionSize = std::max(size, unionSize);
            }
        }

        return { new PatternDataUnion(offset, unionSize, varDeclNode->getVariableName(), unionNode->getName(), members, 0x00FFFFFF), unionSize };
    }

    std::pair<PatternData*, size_t> Evaluator::createEnumPattern(ASTNodeVariableDecl *varDeclNode, u64 offset) {
        std::vector<std::pair<u64, std::string>> enumValues;

        auto *enumType = static_cast<ASTNodeEnum*>(this->m_types[varDeclNode->getCustomVariableTypeName()]);

        if (enumType == nullptr)
            return { nullptr, 0 };

        size_t size = getTypeSize(enumType->getUnderlyingType());

        return { new PatternDataEnum(offset, size, varDeclNode->getVariableName(), enumType->getName(), enumType->getValues()), size };
    }

    std::pair<PatternData*, size_t> Evaluator::createBitfieldPattern(ASTNodeVariableDecl *varDeclNode, u64 offset) {

        auto *bitfieldType = static_cast<ASTNodeBitField*>(this->m_types[varDeclNode->getCustomVariableTypeName()]);

        if (bitfieldType == nullptr)
            return { nullptr, 0 };

        size_t size = 0;
        for (auto &[fieldName, fieldSize] : bitfieldType->getFields())
            size += fieldSize;

        size = std::bit_ceil(size) / 8;

        return { new PatternDataBitfield(offset, size, varDeclNode->getVariableName(), bitfieldType->getName(), bitfieldType->getFields()), size };
    }

    std::pair<PatternData*, size_t> Evaluator::createArrayPattern(ASTNodeVariableDecl *varDeclNode, u64 offset) {
        std::vector<PatternData*> entries;

        size_t arraySize = 0;
        for (u32 i = 0; i < varDeclNode->getArraySize(); i++) {
            ASTNodeVariableDecl *nonArrayVarDeclNode = new ASTNodeVariableDecl(varDeclNode->getVariableType(), "[" + std::to_string(i) + "]", varDeclNode->getCustomVariableTypeName(), varDeclNode->getOffset(), 1);

            if (varDeclNode->getVariableType() != Token::TypeToken::Type::CustomType) {
                const auto &[pattern, size] = this->createBuiltInTypePattern(nonArrayVarDeclNode, offset + arraySize);

                if (pattern == nullptr)
                    return { nullptr, 0 };

                entries.push_back(pattern);
                arraySize += size;
            } else {
                const auto &[pattern, size] = this->createCustomTypePattern(nonArrayVarDeclNode, offset + arraySize);

                if (pattern == nullptr)
                    return { nullptr, 0 };

                entries.push_back(pattern);
                arraySize += size;
            }

            delete nonArrayVarDeclNode;
        }

        return { new PatternDataArray(offset, arraySize, varDeclNode->getVariableName(), entries, 0x00FFFFFF), arraySize };
    }

    std::pair<PatternData*, size_t> Evaluator::createStringPattern(ASTNodeVariableDecl *varDeclNode, u64 offset) {
        size_t arraySize = varDeclNode->getArraySize();

        return { new PatternDataString(offset, arraySize, varDeclNode->getVariableName()), arraySize };
    }

    std::pair<PatternData*, size_t> Evaluator::createCustomTypePattern(ASTNodeVariableDecl *varDeclNode, u64 offset) {
        auto &currType = this->m_types[varDeclNode->getCustomVariableTypeName()];

        if (currType == nullptr)
            return { nullptr, 0 };

        switch (currType->getType()) {
            case ASTNode::Type::Struct:
                return this->createStructPattern(varDeclNode, offset);
            case ASTNode::Type::Union:
                return this->createUnionPattern(varDeclNode, offset);
            case ASTNode::Type::Enum:
                return this->createEnumPattern(varDeclNode, offset);
            case ASTNode::Type::Bitfield:
                return this->createBitfieldPattern(varDeclNode, offset);
            case ASTNode::Type::TypeDecl:
                return this->createBuiltInTypePattern(varDeclNode, offset);
        }

        return { nullptr, 0 };
    }

    std::pair<PatternData*, size_t> Evaluator::createBuiltInTypePattern(ASTNodeVariableDecl *varDeclNode, u64 offset) {
        auto type = varDeclNode->getVariableType();
        if (type == Token::TypeToken::Type::CustomType) {
            const auto &currType =  static_cast<ASTNodeTypeDecl*>(this->m_types[varDeclNode->getCustomVariableTypeName()]);
            if (currType == nullptr)
                return { nullptr, 0 };

            type = currType->getAssignedType();
        }

        size_t typeSize = getTypeSize(type);
        size_t arraySize = varDeclNode->getArraySize();

        if (isSigned(type)) {
            if (typeSize == 1 && arraySize == 1)
                return { new PatternDataCharacter(offset, typeSize, varDeclNode->getVariableName()), 1 };
            else if (arraySize > 1)
                return createArrayPattern(varDeclNode, offset);
            else
                return { new PatternDataSigned(offset, typeSize, varDeclNode->getVariableName()), typeSize * arraySize };
        } else if (isUnsigned(varDeclNode->getVariableType())) {
            if (arraySize > 1)
                return createArrayPattern(varDeclNode, offset);
            else
                return { new PatternDataUnsigned(offset, typeSize, varDeclNode->getVariableName()), typeSize * arraySize };
        } else if (isFloatingPoint(varDeclNode->getVariableType())) {
            if (arraySize > 1)
                return createArrayPattern(varDeclNode, offset);
            else
                return { new PatternDataFloat(offset, typeSize, varDeclNode->getVariableName()), typeSize * arraySize };
        }

        return { nullptr, 0 };
    }

    std::pair<Result, std::vector<PatternData*>> Evaluator::evaluate(const std::vector<ASTNode *> &ast) {

        // Evaluate types
        for (const auto &node : ast) {

            switch(node->getType()) {
                case ASTNode::Type::Struct:
                {
                    auto *structNode = static_cast<ASTNodeStruct*>(node);
                    this->m_types.emplace(structNode->getName(), structNode);
                }
                    break;
                case ASTNode::Type::Union:
                {
                    auto *unionNode = static_cast<ASTNodeUnion*>(node);
                    this->m_types.emplace(unionNode->getName(), unionNode);
                }
                    break;
                case ASTNode::Type::Enum:
                {
                    auto *enumNode = static_cast<ASTNodeEnum*>(node);
                    this->m_types.emplace(enumNode->getName(), enumNode);
                }
                    break;
                case ASTNode::Type::Bitfield:
                {
                    auto *bitfieldNode = static_cast<ASTNodeBitField*>(node);
                    this->m_types.emplace(bitfieldNode->getName(), bitfieldNode);
                }
                    break;
                case ASTNode::Type::TypeDecl:
                {
                    auto *typeDeclNode = static_cast<ASTNodeTypeDecl*>(node);

                    if (typeDeclNode->getAssignedType() == Token::TypeToken::Type::CustomType)
                        this->m_types.emplace(typeDeclNode->getTypeName(), this->m_types[typeDeclNode->getAssignedCustomTypeName()]);
                    else
                        this->m_types.emplace(typeDeclNode->getTypeName(), typeDeclNode);
                }
                    break;
                case ASTNode::Type::VariableDecl: break;
                case ASTNode::Type::Scope: break;
            }
        }

        // Evaluate variable declarations

        std::vector<PatternData*> variables;
        for (const auto &node : ast) {
            if (node->getType() != ASTNode::Type::VariableDecl)
                continue;

            auto *varDeclNode = static_cast<ASTNodeVariableDecl*>(node);

            if (varDeclNode->getVariableType() == Token::TypeToken::Type::Signed8Bit && varDeclNode->getArraySize() > 1) {
                const auto &[pattern, _] = createStringPattern(varDeclNode, varDeclNode->getOffset().value());
                variables.push_back(pattern);
            }
            else if (varDeclNode->getArraySize() > 1) {
                const auto &[pattern, _] = this->createArrayPattern(varDeclNode, varDeclNode->getOffset().value());
                variables.push_back(pattern);

            } else if (varDeclNode->getVariableType() != Token::TypeToken::Type::CustomType) {
                const auto &[pattern, _] = this->createBuiltInTypePattern(varDeclNode, varDeclNode->getOffset().value());
                variables.push_back(pattern);
            } else {
                const auto &[pattern, _] = this->createCustomTypePattern(varDeclNode, varDeclNode->getOffset().value());
                variables.push_back(pattern);
            }
        }

        for (const auto &var : variables)
            if (var == nullptr)
                return { ResultEvaluatorError, { } };

        return { ResultSuccess, variables };
    }

}