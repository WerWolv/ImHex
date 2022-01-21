#include <hex/data_processor/node.hpp>

#include <hex/helpers/fmt.hpp>

#include <hex/api/localization.hpp>
#include <hex/providers/provider.hpp>

namespace hex::dp {

    u32 Node::s_idCounter = 1;

    Node::Node(std::string unlocalizedTitle, std::vector<Attribute> attributes) : m_id(Node::s_idCounter++), m_unlocalizedTitle(std::move(unlocalizedTitle)), m_attributes(std::move(attributes)) {
        for (auto &attr : this->m_attributes)
            attr.setParentNode(this);
    }

    std::vector<u8> Node::getBufferOnInput(u32 index) {
        auto attribute = this->getConnectedInputAttribute(index);

        if (attribute == nullptr)
            throwNodeError(hex::format("Nothing connected to input '{0}'", LangEntry(this->m_attributes[index].getUnlocalizedName())));

        if (attribute->getType() != Attribute::Type::Buffer)
            throwNodeError("Tried to read buffer from non-buffer attribute");

        markInputProcessed(index);
        attribute->getParentNode()->process();

        auto &outputData = attribute->getOutputData();

        if (!outputData.has_value())
            throw std::runtime_error("No data available at connected attribute");

        return outputData.value();
    }

    u64 Node::getIntegerOnInput(u32 index) {
        if (index >= this->getAttributes().size())
            throw std::runtime_error("Attribute index out of bounds");

        auto &localAttribute = this->getAttributes()[index];
        auto connectedAttribute = this->getConnectedInputAttribute(index);

        if (connectedAttribute == nullptr && !localAttribute.allowsImmediate())
            throwNodeError(hex::format("Nothing connected to input '{0}'", LangEntry(this->m_attributes[index].getUnlocalizedName())));

        auto connectedIsInteger = connectedAttribute != nullptr ?
            connectedAttribute->getType() == Attribute::Type::Integer : false;

        if (!connectedIsInteger && localAttribute.getType() != Attribute::Type::Integer)
            throwNodeError("Tried to read integer from non-integer attribute");

        if (connectedAttribute != nullptr)
        {
            markInputProcessed(index);
            connectedAttribute->getParentNode()->process();

            auto &outputData = connectedAttribute->getOutputData();

            if (!outputData.has_value())
                throw std::runtime_error("No data available at connected attribute");

            if (outputData->size() < sizeof(u64))
                throw std::runtime_error("Not enough data provided for integer");

            return *reinterpret_cast<u64*>(outputData->data());
        }

        return localAttribute.integerImmediate();
    }

    float Node::getFloatOnInput(u32 index) {
        if (index >= this->getAttributes().size())
            throw std::runtime_error("Attribute index out of bounds");

        auto &localAttribute = this->getAttributes()[index];
        auto connectedAttribute = this->getConnectedInputAttribute(index);

        if (connectedAttribute == nullptr && !localAttribute.allowsImmediate())
            throwNodeError(hex::format("Nothing connected to input '{0}'", LangEntry(this->m_attributes[index].getUnlocalizedName())));

        auto connectedIsFloat = connectedAttribute != nullptr ?
            connectedAttribute->getType() == Attribute::Type::Float : false;

        if (!connectedIsFloat && localAttribute.getType() != Attribute::Type::Float)
            throwNodeError("Tried to read float from non-float attribute");

        if (connectedAttribute != nullptr)
        {
            markInputProcessed(index);
            connectedAttribute->getParentNode()->process();

            auto &outputData = connectedAttribute->getOutputData();

            if (!outputData.has_value())
                throw std::runtime_error("No data available at connected attribute");

            if (outputData->size() < sizeof(float))
                throw std::runtime_error("Not enough data provided for float");

            return *reinterpret_cast<float*>(outputData->data());
        }

        return localAttribute.floatImmediate();
    }

    void Node::setBufferOnOutput(u32 index, std::vector<u8> data) {
        if (index >= this->getAttributes().size())
            throw std::runtime_error("Attribute index out of bounds!");

        auto &attribute = this->getAttributes()[index];

        if (attribute.getIOType() != Attribute::IOType::Out)
            throw std::runtime_error("Tried to set output data of an input attribute!");

        attribute.getOutputData() = data;
    }

    void Node::setIntegerOnOutput(u32 index, u64 integer) {
        if (index >= this->getAttributes().size())
            throw std::runtime_error("Attribute index out of bounds!");

        auto &attribute = this->getAttributes()[index];

        if (attribute.getIOType() != Attribute::IOType::Out)
            throw std::runtime_error("Tried to set output data of an input attribute!");

        std::vector<u8> buffer(sizeof(u64), 0);
        std::memcpy(buffer.data(), &integer, sizeof(u64));

        attribute.getOutputData() = buffer;
    }

    void Node::setFloatOnOutput(u32 index, float floatingPoint) {
        if (index >= this->getAttributes().size())
            throw std::runtime_error("Attribute index out of bounds!");

        auto &attribute = this->getAttributes()[index];

        if (attribute.getIOType() != Attribute::IOType::Out)
            throw std::runtime_error("Tried to set output data of an input attribute!");

        std::vector<u8> buffer(sizeof(float), 0);
        std::memcpy(buffer.data(), &floatingPoint, sizeof(float));

        attribute.getOutputData() = buffer;
    }

    void Node::setOverlayData(u64 address, const std::vector<u8> &data) {
        if (this->m_overlay == nullptr)
            throw std::runtime_error("Tried setting overlay data on a node that's not the end of a chain!");

        this->m_overlay->setAddress(address);
        this->m_overlay->getData() = data;
    }

    void Node::allowImmediateOnInput(u32 index, bool allow) {
        if (index >= this->getAttributes().size())
            throw std::runtime_error("Attribute index out of bounds!");

        auto &attribute = this->getAttributes()[index];

        if (attribute.getIOType() != Attribute::IOType::In)
            throw std::runtime_error("Only input attributes can have immediate values!");

        if (attribute.getType() == Attribute::Type::Buffer)
            throw std::runtime_error("Buffer attributes can't have immediate values");

        attribute.m_allowsImmediate = allow;
    }
}

