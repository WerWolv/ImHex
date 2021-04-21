#include <hex/lang/validator.hpp>

#include <unordered_set>
#include <string>

#include <hex/helpers/utils.hpp>

namespace hex::lang {

    Validator::Validator() {

    }

    bool Validator::validate(const std::vector<ASTNode*>& ast) {
        std::unordered_set<std::string> identifiers;

        try {

            for (const auto &node : ast) {
                if (node == nullptr)
                    throwValidateError("nullptr in AST. This is a bug!", 1);

                if (auto variableDeclNode = dynamic_cast<ASTNodeVariableDecl*>(node); variableDeclNode != nullptr) {
                    if (!identifiers.insert(variableDeclNode->getName().data()).second)
                        throwValidateError(hex::format("redefinition of identifier '{0}'", variableDeclNode->getName().data()), variableDeclNode->getLineNumber());

                    this->validate({ variableDeclNode->getType() });
                } else if (auto typeDeclNode = dynamic_cast<ASTNodeTypeDecl*>(node); typeDeclNode != nullptr) {
                    if (!identifiers.insert(typeDeclNode->getName().data()).second)
                        throwValidateError(hex::format("redefinition of identifier '{0}'", typeDeclNode->getName().data()), typeDeclNode->getLineNumber());

                    this->validate({ typeDeclNode->getType() });
                } else if (auto structNode = dynamic_cast<ASTNodeStruct*>(node); structNode != nullptr) {
                    this->validate(structNode->getMembers());
                } else if (auto unionNode = dynamic_cast<ASTNodeUnion*>(node); unionNode != nullptr) {
                    this->validate(unionNode->getMembers());
                } else if (auto enumNode = dynamic_cast<ASTNodeEnum*>(node); enumNode != nullptr) {
                    std::unordered_set<std::string> enumIdentifiers;
                    for (auto &[name, value] : enumNode->getEntries()) {
                        if (!enumIdentifiers.insert(name).second)
                            throwValidateError(hex::format("redefinition of enum constant '{0}'", name.c_str()), value->getLineNumber());
                    }
                }
            }

        } catch (ValidatorError &e) {
            this->m_error = e;
            return false;
        }

        return true;
    }
}