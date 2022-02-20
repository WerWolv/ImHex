#include <hex/pattern_language/validator.hpp>

#include <hex/pattern_language/ast_node.hpp>

#include <hex/helpers/fmt.hpp>

#include <unordered_set>
#include <string>

namespace hex::pl {

    bool Validator::validate(const std::vector<ASTNode *> &ast) {
        std::unordered_set<std::string> identifiers;
        std::unordered_set<std::string> types;

        try {

            for (const auto &node : ast) {
                if (node == nullptr)
                    throwValidatorError("nullptr in AST. This is a bug!", 1);

                if (auto variableDeclNode = dynamic_cast<ASTNodeVariableDecl *>(node); variableDeclNode != nullptr) {
                    if (!identifiers.insert(variableDeclNode->getName().data()).second)
                        throwValidatorError(hex::format("redefinition of identifier '{0}'", variableDeclNode->getName().data()), variableDeclNode->getLineNumber());

                    this->validate({ variableDeclNode->getType() });
                } else if (auto typeDeclNode = dynamic_cast<ASTNodeTypeDecl *>(node); typeDeclNode != nullptr) {
                    if (!types.insert(typeDeclNode->getName().data()).second)
                        throwValidatorError(hex::format("redefinition of type '{0}'", typeDeclNode->getName().data()), typeDeclNode->getLineNumber());

                    this->validate({ typeDeclNode->getType() });
                } else if (auto structNode = dynamic_cast<ASTNodeStruct *>(node); structNode != nullptr) {
                    this->validate(structNode->getMembers());
                } else if (auto unionNode = dynamic_cast<ASTNodeUnion *>(node); unionNode != nullptr) {
                    this->validate(unionNode->getMembers());
                } else if (auto enumNode = dynamic_cast<ASTNodeEnum *>(node); enumNode != nullptr) {
                    std::unordered_set<std::string> enumIdentifiers;
                    for (auto &[name, value] : enumNode->getEntries()) {
                        if (!enumIdentifiers.insert(name).second)
                            throwValidatorError(hex::format("redefinition of enum constant '{0}'", name.c_str()), value->getLineNumber());
                    }
                }
            }

        } catch (PatternLanguageError &e) {
            this->m_error = e;
            return false;
        }

        return true;
    }
}