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

        void setCurrentOverlay(prv::Overlay *overlay) {
            this->m_overlay = overlay;
        }

        virtual void drawNode() { }
        virtual void process() = 0;
    private:
        u32 m_id;
        std::string m_title;
        std::vector<Attribute> m_attributes;
        prv::Overlay *m_overlay = nullptr;

        Attribute* getConnectedInputAttribute(u32 index) {
            if (index >= this->getAttributes().size())
                throw std::runtime_error("Attribute index out of bounds!");

            auto &connectedAttribute = this->getAttributes()[index].getConnectedAttributes();

            if (connectedAttribute.empty())
                return nullptr;

            return connectedAttribute.begin()->second;
        }

    protected:

        std::optional<std::vector<u8>> getBufferOnInput(u32 index) {
            auto attribute = this->getConnectedInputAttribute(index);

            if (attribute == nullptr || attribute->getType() != Attribute::Type::Buffer)
                return { };

            attribute->getParentNode()->process();

            auto &outputData = attribute->getOutputData();

            return outputData;
        }

        std::optional<u64> getIntegerOnInput(u32 index) {
            auto attribute = this->getConnectedInputAttribute(index);

            if (attribute == nullptr || attribute->getType() != Attribute::Type::Integer)
                return { };

            attribute->getParentNode()->process();

            auto &outputData = attribute->getOutputData();

            if (outputData.empty() || outputData.size() < sizeof(u64))
                return { };
            else
                return *reinterpret_cast<u64*>(outputData.data());
        }

        std::optional<float> getFloatOnInput(u32 index) {
            auto attribute = this->getConnectedInputAttribute(index);

            if (attribute == nullptr || attribute->getType() != Attribute::Type::Float)
                return { };

            attribute->getParentNode()->process();

            auto &outputData = attribute->getOutputData();

            if (outputData.empty() || outputData.size() < sizeof(float))
                return { };
            else
                return *reinterpret_cast<float*>(outputData.data());
        }

        void setBufferOnOutput(u32 index, std::vector<u8> data) {
            if (index >= this->getAttributes().size())
                throw std::runtime_error("Attribute index out of bounds!");

            auto &attribute = this->getAttributes()[index];

            if (attribute.getIOType() != Attribute::IOType::Out)
                throw std::runtime_error("Tried to set output data of an input attribute!");

            attribute.getOutputData() = data;
        }

        void setIntegerOnOutput(u32 index, u64 integer) {
            if (index >= this->getAttributes().size())
                throw std::runtime_error("Attribute index out of bounds!");

            auto &attribute = this->getAttributes()[index];

            if (attribute.getIOType() != Attribute::IOType::Out)
                throw std::runtime_error("Tried to set output data of an input attribute!");

            std::vector<u8> buffer(sizeof(u64), 0);
            std::memcpy(buffer.data(), &integer, sizeof(u64));

            attribute.getOutputData() = buffer;
        }

        void setFloatOnOutput(u32 index, float floatingPoint) {
            if (index >= this->getAttributes().size())
                throw std::runtime_error("Attribute index out of bounds!");

            auto &attribute = this->getAttributes()[index];

            if (attribute.getIOType() != Attribute::IOType::Out)
                throw std::runtime_error("Tried to set output data of an input attribute!");

            std::vector<u8> buffer(sizeof(float), 0);
            std::memcpy(buffer.data(), &floatingPoint, sizeof(float));

            attribute.getOutputData() = buffer;
        }

        void setOverlayData(u64 address, const std::vector<u8> &data) {
            if (this->m_overlay == nullptr)
                throw std::runtime_error("Tried setting overlay data on a node that's not the end of a chain!");

            this->m_overlay->setAddress(address);
            this->m_overlay->getData() = data;
        }

    };

}