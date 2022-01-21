#pragma once
#include <hex.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <map>
#include <vector>

namespace hex::dp {

    class Node;

    class Attribute {
    public:
        enum class Type {
            Integer,
            Float,
            Buffer
        };

        enum class IOType {
            In, Out
        };

        Attribute(IOType ioType, Type type, std::string unlocalizedName);
        ~Attribute();

        [[nodiscard]] u32 getId() const { return this->m_id; }
        void setId(u32 id) { this->m_id = id; }

        [[nodiscard]] IOType getIOType() const { return this->m_ioType; }
        [[nodiscard]] Type getType() const { return this->m_type; }
        [[nodiscard]] const std::string &getUnlocalizedName() const { return this->m_unlocalizedName; }
        [[nodiscard]] bool allowsImmediate() const { return this->m_allowsImmediate; }
        [[nodiscard]] u64 integerImmediate() const { return this->m_integerImmediate; }
        [[nodiscard]] float floatImmediate() const { return this->m_floatImmediate; }
        [[nodiscard]] u64* integerImmediatePtr() { return &this->m_integerImmediate; }
        [[nodiscard]] float* floatImmediatePtr() { return &this->m_floatImmediate; }

        void addConnectedAttribute(u32 linkId, Attribute *to) { this->m_connectedAttributes.insert({ linkId, to }); }
        void removeConnectedAttribute(u32 linkId) { this->m_connectedAttributes.erase(linkId); }
        [[nodiscard]] std::map<u32, Attribute *> &getConnectedAttributes() { return this->m_connectedAttributes; }

        [[nodiscard]] Node *getParentNode() { return this->m_parentNode; }

        [[nodiscard]] std::optional<std::vector<u8>> &getOutputData() { return this->m_outputData; }

        static void setIdCounter(u32 id) {
            if (id > Attribute::s_idCounter)
                Attribute::s_idCounter = id;
        }

        void setIntegerImmediate(u64 value) { this->m_integerImmediate = value; }
        void setFloatImmediate(float value) { this->m_floatImmediate = value; }
    private:
        u32 m_id;
        IOType m_ioType;
        Type m_type;
        std::string m_unlocalizedName;
        std::map<u32, Attribute *> m_connectedAttributes;
        Node *m_parentNode = nullptr;

        bool m_allowsImmediate;

        union {
            u64 m_integerImmediate = 0;
            float m_floatImmediate;
        };

        std::optional<std::vector<u8>> m_outputData;

        friend class Node;
        void setParentNode(Node *node) { this->m_parentNode = node; }

        static u32 s_idCounter;
    };

}
