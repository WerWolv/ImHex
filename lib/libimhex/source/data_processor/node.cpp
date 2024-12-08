#include <hex/data_processor/node.hpp>

#include <hex/helpers/fmt.hpp>

#include <hex/api/localization_manager.hpp>
#include <hex/providers/provider.hpp>

namespace hex::dp {

    int Node::s_idCounter = 1;
    static std::atomic_bool s_interrupted;

    Node::Node(UnlocalizedString unlocalizedTitle, std::vector<Attribute> attributes) : m_id(s_idCounter++), m_unlocalizedTitle(std::move(unlocalizedTitle)), m_attributes(std::move(attributes)) {
        for (auto &attr : m_attributes)
            attr.setParentNode(this);
    }

    void Node::draw() {
        this->drawNode();
    }


    const std::vector<u8>& Node::getBufferOnInput(u32 index) {
        auto attribute = this->getConnectedInputAttribute(index);

        if (attribute == nullptr)
            throwNodeError(hex::format("Nothing connected to input '{0}'", Lang(m_attributes[index].getUnlocalizedName())));

        if (attribute->getType() != Attribute::Type::Buffer)
            throwNodeError("Tried to read buffer from non-buffer attribute");

        markInputProcessed(index);
        attribute->getParentNode()->process();
        unmarkInputProcessed(index);

        auto &outputData = attribute->getOutputData();

        return outputData;
    }

    const i128& Node::getIntegerOnInput(u32 index) {
        auto attribute = this->getConnectedInputAttribute(index);

        auto &outputData = [&] -> std::vector<u8>& {
            if (attribute != nullptr) {
                if (attribute->getType() != Attribute::Type::Integer)
                    throwNodeError("Tried to read integer from non-integer attribute");

                markInputProcessed(index);
                attribute->getParentNode()->process();
                unmarkInputProcessed(index);

                return attribute->getOutputData();
            } else {
                return this->getAttribute(index).getOutputData();
            }
        }();

        if (outputData.empty())
            throwNodeError("No data available at connected attribute");

        if (outputData.size() < sizeof(i128))
            throwNodeError("Not enough data provided for integer");

        return *reinterpret_cast<i128 *>(outputData.data());
    }

    const double& Node::getFloatOnInput(u32 index) {
        auto attribute = this->getConnectedInputAttribute(index);

        auto &outputData = [&] -> std::vector<u8>& {
            if (attribute != nullptr) {
                if (attribute->getType() != Attribute::Type::Float)
                    throwNodeError("Tried to read integer from non-float attribute");

                markInputProcessed(index);
                attribute->getParentNode()->process();
                unmarkInputProcessed(index);

                return attribute->getOutputData();
            } else {
                return this->getAttribute(index).getOutputData();
            }
        }();

        if (outputData.empty())
            throwNodeError("No data available at connected attribute");

        if (outputData.size() < sizeof(double))
            throwNodeError("Not enough data provided for float");

        return *reinterpret_cast<double *>(outputData.data());
    }

    void Node::setBufferOnOutput(u32 index, std::span<const u8> data) {
        if (index >= this->getAttributes().size())
            throwNodeError("Attribute index out of bounds!");

        auto &attribute = this->getAttributes()[index];

        if (attribute.getIOType() != Attribute::IOType::Out)
            throwNodeError("Tried to set output data of an input attribute!");

        if (attribute.getType() != Attribute::Type::Buffer)
            throwNodeError("Tried to set buffer on non-buffer attribute!");

        attribute.getOutputData() = { data.begin(), data.end() };
    }

    void Node::setIntegerOnOutput(u32 index, i128 integer) {
        if (index >= this->getAttributes().size())
            throwNodeError("Attribute index out of bounds!");

        auto &attribute = this->getAttributes()[index];

        if (attribute.getIOType() != Attribute::IOType::Out)
            throwNodeError("Tried to set output data of an input attribute!");

        if (attribute.getType() != Attribute::Type::Integer)
            throwNodeError("Tried to set integer on non-integer attribute!");

        std::vector<u8> buffer(sizeof(integer), 0);
        std::memcpy(buffer.data(), &integer, sizeof(integer));

        attribute.getOutputData() = buffer;
    }

    void Node::setFloatOnOutput(u32 index, double floatingPoint) {
        if (index >= this->getAttributes().size())
            throwNodeError("Attribute index out of bounds!");

        auto &attribute = this->getAttributes()[index];

        if (attribute.getIOType() != Attribute::IOType::Out)
            throwNodeError("Tried to set output data of an input attribute!");

        if (attribute.getType() != Attribute::Type::Float)
            throwNodeError("Tried to set float on non-float attribute!");

        std::vector<u8> buffer(sizeof(floatingPoint), 0);
        std::memcpy(buffer.data(), &floatingPoint, sizeof(floatingPoint));

        attribute.getOutputData() = buffer;
    }

    void Node::setOverlayData(u64 address, const std::vector<u8> &data) {
        if (m_overlay == nullptr)
            throwNodeError("Tried setting overlay data on a node that's not the end of a chain!");

        m_overlay->setAddress(address);
        m_overlay->getData() = data;
    }

    [[noreturn]] void Node::throwNodeError(const std::string &message) {
        throw NodeError { this, message };
    }

    void Node::setAttributes(std::vector<Attribute> attributes) {
        m_attributes = std::move(attributes);

        for (auto &attr : m_attributes)
            attr.setParentNode(this);
    }

    void Node::setIdCounter(int id) {
        if (id > s_idCounter)
            s_idCounter = id;
    }

    Attribute& Node::getAttribute(u32 index) {
        if (index >= this->getAttributes().size())
            throw std::runtime_error("Attribute index out of bounds!");

        return this->getAttributes()[index];
    }

    Attribute *Node::getConnectedInputAttribute(u32 index) {
        const auto &connectedAttribute = this->getAttribute(index).getConnectedAttributes();

        if (connectedAttribute.empty())
            return nullptr;

        return connectedAttribute.begin()->second;
    }

    void Node::markInputProcessed(u32 index) {
        const auto &[iter, inserted] = m_processedInputs.insert(index);
        if (!inserted)
            throwNodeError("Recursion detected!");

        if (s_interrupted) {
            s_interrupted = false;
            throwNodeError("Execution interrupted!");
        }
    }

    void Node::unmarkInputProcessed(u32 index) {
        m_processedInputs.erase(index);
    }

    void Node::interrupt() {
        s_interrupted = true;
    }


}