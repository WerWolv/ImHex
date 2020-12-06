#pragma once

#include "token.hpp"

#include <bit>
#include <optional>
#include <unordered_map>
#include <vector>

namespace hex::lang {

    class ASTNode {
    public:
        enum class Type {
            VariableDecl,
            TypeDecl,
            Struct,
            Union,
            Enum,
            Bitfield,
            Scope,
        };

        explicit ASTNode(Type type, u32 lineNumber) : m_type(type), m_lineNumber(lineNumber) {}
        virtual ~ASTNode() = default;

        Type getType() { return this->m_type; }
        u32 getLineNumber() { return this->m_lineNumber; }

    private:
        Type m_type;
        u32 m_lineNumber;
    };

    class ASTNodeVariableDecl : public ASTNode {
    public:
        explicit ASTNodeVariableDecl(u32 lineNumber, const Token::TypeToken::Type &type, const std::string &name, const std::string& customTypeName = "", std::optional<u64> offset = { }, size_t arraySize = 1, std::optional<std::string> arraySizeVariable = { }, std::optional<u8> pointerSize = { }, std::optional<std::endian> endianess = { })
            : ASTNode(Type::VariableDecl, lineNumber), m_type(type), m_name(name), m_customTypeName(customTypeName), m_offset(offset), m_arraySize(arraySize), m_arraySizeVariable(arraySizeVariable), m_pointerSize(pointerSize), m_endianess(endianess) { }

        const Token::TypeToken::Type& getVariableType() const { return this->m_type; }
        const std::string& getCustomVariableTypeName() const { return this->m_customTypeName; }
        const std::string& getVariableName() const { return this->m_name; };
        std::optional<u64> getOffset() const { return this->m_offset; }
        size_t getArraySize() const { return this->m_arraySize; }
        std::optional<std::string> getArraySizeVariable() const { return this->m_arraySizeVariable; }
        std::optional<u8> getPointerSize() const { return this->m_pointerSize; }
        std::optional<std::endian> getEndianess() const { return this->m_endianess; }

    private:
        Token::TypeToken::Type m_type;
        std::string m_name, m_customTypeName;
        std::optional<u64> m_offset;
        size_t m_arraySize;
        std::optional<std::string> m_arraySizeVariable;
        std::optional<u8> m_pointerSize;
        std::optional<std::endian> m_endianess = { };
    };

    class ASTNodeScope : public ASTNode {
    public:
        explicit ASTNodeScope(u32 lineNumber, std::vector<ASTNode*> nodes) : ASTNode(Type::Scope, lineNumber), m_nodes(nodes) { }

        std::vector<ASTNode*> &getNodes() { return this->m_nodes; }
    private:
        std::vector<ASTNode*> m_nodes;
    };

    class ASTNodeStruct : public ASTNode {
    public:
        explicit ASTNodeStruct(u32 lineNumber, std::string name, std::vector<ASTNode*> nodes)
            : ASTNode(Type::Struct, lineNumber), m_name(name), m_nodes(nodes) { }

        const std::string& getName() const { return this->m_name; }
        std::vector<ASTNode*> &getNodes() { return this->m_nodes; }
    private:
        std::string m_name;
        std::vector<ASTNode*> m_nodes;
    };

    class ASTNodeUnion : public ASTNode {
    public:
        explicit ASTNodeUnion(u32 lineNumber, std::string name, std::vector<ASTNode*> nodes)
                : ASTNode(Type::Union, lineNumber), m_name(name), m_nodes(nodes) { }

        const std::string& getName() const { return this->m_name; }
        std::vector<ASTNode*> &getNodes() { return this->m_nodes; }
    private:
        std::string m_name;
        std::vector<ASTNode*> m_nodes;
    };

    class ASTNodeBitField : public ASTNode {
    public:
        explicit ASTNodeBitField(u32 lineNumber, std::string name, std::vector<std::pair<std::string, size_t>> fields)
        : ASTNode(Type::Bitfield, lineNumber), m_name(name), m_fields(fields) { }

        const std::string& getName() const { return this->m_name; }
        std::vector<std::pair<std::string, size_t>> &getFields() { return this->m_fields; }
    private:
        std::string m_name;
        std::vector<std::pair<std::string, size_t>> m_fields;
    };

    class ASTNodeTypeDecl : public ASTNode {
    public:
        explicit ASTNodeTypeDecl(u32 lineNumber, const Token::TypeToken::Type &type, const std::string &name, const std::string& customTypeName = "")
                : ASTNode(Type::TypeDecl, lineNumber), m_type(type), m_name(name), m_customTypeName(customTypeName) { }

        const std::string& getTypeName() const { return this->m_name; };

        const Token::TypeToken::Type& getAssignedType() const { return this->m_type; }
        const std::string& getAssignedCustomTypeName() const { return this->m_customTypeName; }
    private:
        Token::TypeToken::Type m_type;
        std::string m_name, m_customTypeName;
    };

    class ASTNodeEnum : public ASTNode {
    public:
        explicit ASTNodeEnum(u32 lineNumber, const Token::TypeToken::Type &type, const std::string &name)
                : ASTNode(Type::Enum, lineNumber), m_type(type), m_name(name) { }

        const std::string& getName() const { return this->m_name; };

        const Token::TypeToken::Type& getUnderlyingType() const { return this->m_type; }
        std::vector<std::pair<u64, std::string>>& getValues() { return this->m_values; }
    private:
        Token::TypeToken::Type m_type;
        std::string m_name;
        std::vector<std::pair<u64, std::string>> m_values;
    };

}