#pragma once

#include <hex/pattern_language/ast/ast_node.hpp>

namespace hex::pl {

    class ASTNodeLiteral : public ASTNode {
    public:
        explicit ASTNodeLiteral(Token::Literal literal) : ASTNode(), m_literal(std::move(literal)) { }

        ASTNodeLiteral(const ASTNodeLiteral &) = default;

        [[nodiscard]] std::unique_ptr<ASTNode> clone() const override {
            return std::unique_ptr<ASTNode>(new ASTNodeLiteral(*this));
        }

        [[nodiscard]] const auto &getValue() const {
            return this->m_literal;
        }

    private:
        Token::Literal m_literal;
    };

}