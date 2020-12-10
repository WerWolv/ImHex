#pragma once

#include "token.hpp"

#include <bit>
#include <optional>
#include <unordered_map>
#include <vector>

namespace hex::lang {

    class ASTNode {
    public:
        constexpr ASTNode() { }
        constexpr virtual ~ASTNode() { }

        [[nodiscard]] constexpr u32 getLineNumber() const { return this->m_lineNumber; }
        constexpr void setLineNumber(u32 lineNumber) { this->m_lineNumber = lineNumber; }

    private:
        u32 m_lineNumber = 0;
    };

    class ASTNodeBuiltinType : public ASTNode {
    public:
        constexpr ASTNodeBuiltinType(Token::ValueType type)
            : ASTNode(), m_type(type) { }

        [[nodiscard]] constexpr const auto& getType() const { return this->m_type; }

    private:
        const Token::ValueType m_type;
    };

    class ASTNodeTypeDecl : public ASTNode {
    public:
        ASTNodeTypeDecl(std::string_view name, ASTNode *type, std::endian endian = std::endian::native)
            : ASTNode(), m_name(name), m_type(type), m_endian(endian) { }


        virtual ~ASTNodeTypeDecl() {
            //TODO: Delete type again and make sure this type can be moved
        }

        [[nodiscard]] std::string_view getName() const { return this->m_name; }
        [[nodiscard]] ASTNode* getType() { return this->m_type; }
        [[nodiscard]] std::endian getEndian() const { return this->m_endian; }

    private:
        std::string m_name;
        ASTNode *m_type;
        std::endian m_endian;
    };

    class ASTNodeVariableDecl : public ASTNode {
    public:
        ASTNodeVariableDecl(std::string_view name, ASTNode *type)
            : ASTNode(), m_name(name), m_type(type) { }

        virtual ~ASTNodeVariableDecl() {}

        [[nodiscard]] std::string_view getName() const { return this->m_name; }
        [[nodiscard]] constexpr ASTNode* getType() const { return this->m_type; }

    private:
        std::string m_name;
        ASTNode *m_type;
    };

}