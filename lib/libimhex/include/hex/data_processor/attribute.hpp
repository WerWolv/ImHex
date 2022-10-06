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
            In,
            Out
        };

        Attribute(IOType ioType, Type type, std::string unlocalizedName);
        ~Attribute();

        [[nodiscard]] int getId() const { return this->m_id; }
        void setId(int id) { this->m_id = id; }

        [[nodiscard]] IOType getIOType() const { return this->m_ioType; }
        [[nodiscard]] Type getType() const { return this->m_type; }
        [[nodiscard]] const std::string &getUnlocalizedName() const { return this->m_unlocalizedName; }

        void addConnectedAttribute(int linkId, Attribute *to) { this->m_connectedAttributes.insert({ linkId, to }); }
        void removeConnectedAttribute(int linkId) { this->m_connectedAttributes.erase(linkId); }
        [[nodiscard]] std::map<int, Attribute *> &getConnectedAttributes() { return this->m_connectedAttributes; }

        [[nodiscard]] Node *getParentNode() { return this->m_parentNode; }

        [[nodiscard]] std::optional<std::vector<u8>> &getOutputData() { return this->m_outputData; }

        static void setIdCounter(int id) {
            if (id > Attribute::s_idCounter)
                Attribute::s_idCounter = id;
        }

    private:
        int m_id;
        IOType m_ioType;
        Type m_type;
        std::string m_unlocalizedName;
        std::map<int, Attribute *> m_connectedAttributes;
        Node *m_parentNode = nullptr;

        std::optional<std::vector<u8>> m_outputData;

        friend class Node;
        void setParentNode(Node *node) { this->m_parentNode = node; }

        static int s_idCounter;
    };

}