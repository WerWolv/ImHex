#pragma once

#include <hex/data_processor/attribute.hpp>

namespace hex::dp {

    class Node {
    public:
        Node(std::string_view title, std::vector<Attribute> attributes) : m_id(SharedData::dataProcessorNodeIdCounter++), m_title(title), m_attributes(std::move(attributes)) {
            for (auto &attr : this->m_attributes)
                attr.setParentNode(this);
        }

        virtual ~Node() = default;

        [[nodiscard]] u32 getID() const { return this->m_id; }
        [[nodiscard]] std::string_view getTitle() const { return this->m_title; }
        [[nodiscard]] std::vector<Attribute>& getAttributes() { return this->m_attributes; }

        virtual void drawNode() { }
        virtual void process(prv::Overlay *dataOverlay) = 0;
    private:
        u32 m_id;
        std::string m_title;
        std::vector<Attribute> m_attributes;

    protected:
        Attribute* getConnectedInputAttribute(u32 attributeId) {
            auto &connectedAttribute = this->getAttributes()[attributeId].getConnectedAttributes();

            if (connectedAttribute.empty())
                return nullptr;

            return connectedAttribute.begin()->second;
        }
    };

}