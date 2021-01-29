#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/views/view.hpp>

#include <array>
#include <string>

namespace hex {

    class Node;

    class Attribute {
    public:
        enum class Type {
            In, Out
        };

        Attribute(Type type, std::string_view name) : m_id(Attribute::s_idCounter++), m_type(type), m_name(name) {

        }

        ~Attribute() {
            for (auto &[linkId, attr] : this->getConnectedAttributes())
                attr->removeConnectedAttribute(linkId);
        }

        [[nodiscard]] u32 getID() const { return this->m_id; }
        [[nodiscard]] Type getType() const { return this->m_type; }
        [[nodiscard]] std::string_view getName() const { return this->m_name; }

        void addConnectedAttribute(u32 linkId, Attribute *to) { this->m_connectedAttributes.insert({ linkId, to }); }
        void removeConnectedAttribute(u32 linkId) { this->m_connectedAttributes.erase(linkId); }
        [[nodiscard]] std::map<u32, Attribute*>& getConnectedAttributes() { return this->m_connectedAttributes; }

        [[nodiscard]] Node* getParentNode() { return this->m_parentNode; }
    private:
        static inline u32 s_idCounter = 1;
        u32 m_id;
        Type m_type;
        std::string m_name;
        std::map<u32, Attribute*> m_connectedAttributes;
        Node *m_parentNode;

        friend class Node;
        void setParentNode(Node *node) { this->m_parentNode = node; }
    };

    class Link {
    public:
        Link(u32 from, u32 to) : m_id(Link::s_idCounter++), m_from(from), m_to(to) { }

        [[nodiscard]] u32 getID()     const { return this->m_id;   }
        [[nodiscard]] u32 getFromID() const { return this->m_from; }
        [[nodiscard]] u32 getToID()   const { return this->m_to;   }

    private:
        static inline u32 s_idCounter = 1;

        u32 m_id;
        u32 m_from, m_to;
    };

    class Node {
    public:
        Node(std::string_view title, std::vector<Attribute> attributes, bool endNode = false) : m_id(Node::s_idCounter++), m_title(title), m_attributes(std::move(attributes)), m_endNode(endNode) {
            for (auto &attr : this->m_attributes)
                attr.setParentNode(this);
        }

        virtual ~Node() = default;

        [[nodiscard]] u32 getID() const { return this->m_id; }
        [[nodiscard]] std::string_view getTitle() const { return this->m_title; }
        [[nodiscard]] std::vector<Attribute>& getAttributes() { return this->m_attributes; }
        [[nodiscard]] bool isEndNode() const { return this->m_endNode; }

        virtual void drawNode() { }
        [[nodiscard]] virtual std::vector<u8> process() = 0;
    private:
        static inline u32 s_idCounter = 1;

        u32 m_id;
        std::string m_title;
        std::vector<Attribute> m_attributes;
        bool m_endNode;

    protected:
        Node* getConnectedInputNode(u32 attributeId) {
            auto &connectedAttribute = this->getAttributes()[attributeId].getConnectedAttributes();

            if (connectedAttribute.empty())
                return nullptr;

            return connectedAttribute.begin()->second->getParentNode();
        }
    };

    class NodeInteger : public Node {
    public:
        NodeInteger() : Node("Integer", { Attribute(Attribute::Type::Out, "Value") }) {}

        void drawNode() override {
            if (ImGui::BeginChild(this->getID(), ImVec2(100, ImGui::GetTextLineHeight()))) {
                ImGui::InputScalar("##integerValue", ImGuiDataType_U64, &this->m_value, nullptr, nullptr, "%llx", ImGuiInputTextFlags_CharsHexadecimal);
            }
            ImGui::EndChild();
        }

        [[nodiscard]] std::vector<u8> process() override {
            std::vector<u8> data;
            data.resize(sizeof(this->m_value));

            std::copy(&this->m_value, &this->m_value + 1, data.data());
            return data;
        }

    private:
        u64 m_value = 0;
    };

    class NodeReadData : public Node {
    public:
        NodeReadData() : Node("Read Data", { Attribute(Attribute::Type::In, "Address"), Attribute(Attribute::Type::In, "Size"), Attribute(Attribute::Type::Out, "Data") }) {}

        [[nodiscard]] std::vector<u8> process() override {
            auto connectedInputAddress = this->getConnectedInputNode(0);
            auto connectedInputSize = this->getConnectedInputNode(1);
            if (connectedInputAddress == nullptr || connectedInputSize == nullptr)
                return {};

            auto address = *reinterpret_cast<u64*>(connectedInputAddress->process().data());
            auto size = *reinterpret_cast<u64*>(connectedInputSize->process().data());

            std::vector<u8> data;
            data.resize(size);

            SharedData::currentProvider->readRaw(address, data.data(), size);
            return data;
        }

    private:
    };

    class NodeInvert : public Node {
    public:
        NodeInvert() : Node("Invert", { Attribute(Attribute::Type::In, "Input"), Attribute(Attribute::Type::Out, "Output") }) {}

        [[nodiscard]] std::vector<u8> process() override {
            auto connectedInput = this->getConnectedInputNode(0);
            if (connectedInput == nullptr)
                return {};

            std::vector<u8> output = connectedInput->process();

            for (auto &byte : output)
                byte = ~byte;

            return output;
        }
    };

    class NodeWriteData : public Node {
    public:
        NodeWriteData() : Node("Write Data", { Attribute(Attribute::Type::In, "Address"), Attribute(Attribute::Type::In, "Data") }, true) {}

        [[nodiscard]] std::vector<u8> process() override {
            auto connectedInputAddress = this->getConnectedInputNode(0);
            auto connectedInputData = this->getConnectedInputNode(1);
            if (connectedInputAddress == nullptr || connectedInputData == nullptr)
                return {};

            auto address = *reinterpret_cast<u64*>(connectedInputAddress->process().data());
            auto data = connectedInputData->process();

            SharedData::currentProvider->write(address, data.data(), data.size());
            return data;
        }
    };

    namespace prv { class Provider; }

    class ViewDataProcessor : public View {
    public:
        ViewDataProcessor();
        ~ViewDataProcessor() override;

        void drawContent() override;
        void drawMenu() override;

    private:
        std::list<Node*> m_endNodes;
        std::list<Node*> m_nodes;
        std::list<Link>  m_links;

        auto eraseLink(u32 id);
        void processNodes();
    };

}