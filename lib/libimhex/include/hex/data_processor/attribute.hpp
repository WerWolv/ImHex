#pragma once

#include <hex.hpp>
#include <hex/api/localization_manager.hpp>

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

        Attribute(IOType ioType, Type type, UnlocalizedString unlocalizedName);
        ~Attribute();

        [[nodiscard]] int getId() const { return m_id; }
        void setId(int id) { m_id = id; }

        [[nodiscard]] IOType getIOType() const { return m_ioType; }
        [[nodiscard]] Type getType() const { return m_type; }
        [[nodiscard]] const UnlocalizedString &getUnlocalizedName() const { return m_unlocalizedName; }

        void addConnectedAttribute(int linkId, Attribute *to) { m_connectedAttributes.insert({ linkId, to }); }
        void removeConnectedAttribute(int linkId) { m_connectedAttributes.erase(linkId); }
        [[nodiscard]] std::map<int, Attribute *> &getConnectedAttributes() { return m_connectedAttributes; }

        [[nodiscard]] Node *getParentNode() const { return m_parentNode; }

        [[nodiscard]] std::vector<u8>& getOutputData() {
            if (!m_outputData.empty())
                return m_outputData;
            else
                return m_defaultData;
        }

        void clearOutputData() { m_outputData.clear(); }

        [[nodiscard]] std::vector<u8>& getDefaultData() { return m_defaultData; }

        static void setIdCounter(int id);

    private:
        int m_id;
        IOType m_ioType;
        Type m_type;
        UnlocalizedString m_unlocalizedName;
        std::map<int, Attribute *> m_connectedAttributes;
        Node *m_parentNode = nullptr;

        std::vector<u8> m_outputData;
        std::vector<u8> m_defaultData;

        friend class Node;
        void setParentNode(Node *node) { m_parentNode = node; }

        static int s_idCounter;
    };

}