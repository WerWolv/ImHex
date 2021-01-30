#pragma once

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

        Attribute(IOType ioType, Type type, std::string_view name) : m_id(SharedData::dataProcessorNodeIdCounter++), m_ioType(ioType), m_type(type), m_name(name) {

        }

        ~Attribute() {
            for (auto &[linkId, attr] : this->getConnectedAttributes())
                attr->removeConnectedAttribute(linkId);
        }

        [[nodiscard]] u32 getID() const { return this->m_id; }
        [[nodiscard]] IOType getIOType() const { return this->m_ioType; }
        [[nodiscard]] Type getType() const { return this->m_type; }
        [[nodiscard]] std::string_view getName() const { return this->m_name; }

        void addConnectedAttribute(u32 linkId, Attribute *to) { this->m_connectedAttributes.insert({ linkId, to }); }
        void removeConnectedAttribute(u32 linkId) { this->m_connectedAttributes.erase(linkId); }
        [[nodiscard]] std::map<u32, Attribute*>& getConnectedAttributes() { return this->m_connectedAttributes; }

        [[nodiscard]] Node* getParentNode() { return this->m_parentNode; }

        [[nodiscard]] std::vector<u8>& getOutputData() { return this->m_outputData; }
    private:
        u32 m_id;
        IOType m_ioType;
        Type m_type;
        std::string m_name;
        std::map<u32, Attribute*> m_connectedAttributes;
        Node *m_parentNode;

        std::vector<u8> m_outputData;

        friend class Node;
        void setParentNode(Node *node) { this->m_parentNode = node; }
    };

}