#include "lang/validator.hpp"

#include <unordered_set>
#include <string>

#include "helpers/utils.hpp"

namespace hex::lang {

    Validator::Validator() {

    }

    bool Validator::validate(const std::vector<ASTNode*>& ast) {

        std::unordered_set<std::string> typeNames;

        for (const auto &node : ast) {
            if (node == nullptr) {
                this->m_error = { 1, "Empty AST" };
                return false;
            }

            switch (node->getType()) {
                case ASTNode::Type::VariableDecl:
                {
                    // Check for duplicate variable names
                    auto varDeclNode = static_cast<ASTNodeVariableDecl*>(node);
                    if (!typeNames.insert(varDeclNode->getVariableName()).second) {
                        this->m_error = { varDeclNode->getLineNumber(), hex::format("Redefinition of variable '%s'", varDeclNode->getVariableName().c_str()) };
                        return false;
                    }

                    if (varDeclNode->getArraySize() == 0 && !varDeclNode->getArraySizeVariable().has_value() ||
                        varDeclNode->getArraySize() != 0 && varDeclNode->getArraySizeVariable().has_value()) {

                        this->m_error = { varDeclNode->getLineNumber(), "Invalid array size" };
                        return false;
                    }
                }
                    break;
                case ASTNode::Type::TypeDecl:
                {
                    // Check for duplicate type names
                    auto typeDeclNode = static_cast<ASTNodeTypeDecl*>(node);
                    if (!typeNames.insert(typeDeclNode->getTypeName()).second) {
                        this->m_error = { typeDeclNode->getLineNumber(), hex::format("Redefinition of type '%s'", typeDeclNode->getTypeName().c_str()) };
                        return false;
                    }

                    if (typeDeclNode->getAssignedType() == Token::TypeToken::Type::CustomType && !typeNames.contains(typeDeclNode->getAssignedCustomTypeName())) {
                        this->m_error = { typeDeclNode->getLineNumber(), "Type declaration without a name" };
                        return false;
                    }
                }
                    break;
                case ASTNode::Type::Struct:
                {
                    // Check for duplicate type name
                    auto structNode = static_cast<ASTNodeStruct*>(node);
                    if (!typeNames.insert(structNode->getName()).second) {
                        this->m_error = { structNode->getLineNumber(), hex::format("Redeclaration of type '%s'", structNode->getName().c_str()) };
                        return false;
                    }

                    // Check for duplicate member names
                    std::unordered_set<std::string> memberNames;
                    for (const auto &member : structNode->getNodes())
                        if (!memberNames.insert(static_cast<ASTNodeVariableDecl*>(member)->getVariableName()).second) {
                            this->m_error = { member->getLineNumber(), hex::format("Redeclaration of member '%s'", static_cast<ASTNodeVariableDecl*>(member)->getVariableName().c_str()) };
                            return false;
                        }
                }
                    break;
                case ASTNode::Type::Enum:
                {
                    // Check for duplicate type name
                    auto enumNode = static_cast<ASTNodeEnum*>(node);
                    if (!typeNames.insert(enumNode->getName()).second) {
                        this->m_error = { enumNode->getLineNumber(), hex::format("Redeclaration of type '%s'", enumNode->getName().c_str()) };
                        return false;
                    }

                    // Check for duplicate constant names
                    std::unordered_set<std::string> constantNames;
                    for (const auto &[value, name] : enumNode->getValues())
                        if (!constantNames.insert(name).second) {
                            this->m_error = { enumNode->getLineNumber(), hex::format("Redeclaration of enum constant '%s'", name.c_str()) };
                            return false;
                        }
                }
                    break;
                case ASTNode::Type::Bitfield:
                {
                    // Check for duplicate type name
                    auto bitfieldNode = static_cast<ASTNodeBitField*>(node);
                    if (!typeNames.insert(bitfieldNode->getName()).second) {
                        this->m_error = { bitfieldNode->getLineNumber(), hex::format("Redeclaration of type '%s'", bitfieldNode->getName().c_str()) };
                        return false;
                    }

                    size_t bitfieldSize = 0;

                    // Check for duplicate constant names
                    std::unordered_set<std::string> flagNames;
                    for (const auto &[name, size] : bitfieldNode->getFields()) {
                        if (!flagNames.insert(name).second) {
                            this->m_error = { bitfieldNode->getLineNumber(), hex::format("Redeclaration of member '%s'", name.c_str()) };
                            return false;
                        }

                        bitfieldSize += size;
                    }

                    if (bitfieldSize > 64) {
                        this->m_error = { bitfieldNode->getLineNumber(), "Bitfield exceeds maximum size of 64 bits" };
                        return false;
                    }
                }
                    break;
            }
        }

        return true;
    }

}