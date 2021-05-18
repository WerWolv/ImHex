#pragma once

#include <hex/data_processor/attribute.hpp>

#include <set>
#include <vector>

#include <nlohmann/json.hpp>

namespace hex::dp {

    class Node {
    public:
        Node(std::string_view unlocalizedTitle, std::vector<Attribute> attributes) : m_id(SharedData::dataProcessorNodeIdCounter++), m_unlocalizedTitle(unlocalizedTitle), m_attributes(std::move(attributes)) {
            for (auto &attr : this->m_attributes)
                attr.setParentNode(this);
        }

        virtual ~Node() = default;

        [[nodiscard]] u32 getID() const { return this->m_id; }
        void setID(u32 id) { this->m_id = id; }

        [[nodiscard]] std::string_view getUnlocalizedName() const { return this->m_unlocalizedName; }
        void setUnlocalizedName(std::string_view unlocalizedName) { this->m_unlocalizedName = unlocalizedName; }

        [[nodiscard]] std::string_view getUnlocalizedTitle() const { return this->m_unlocalizedTitle; }
        [[nodiscard]] std::vector<Attribute>& getAttributes() { return this->m_attributes; }

        void setCurrentOverlay(prv::Overlay *overlay) {
            this->m_overlay = overlay;
        }

        virtual void drawNode() { }
        virtual void process() = 0;

        virtual nlohmann::json store() { return nullptr; }
        virtual void load(nlohmann::json &j) { }

        using NodeError = std::pair<Node*, std::string>;

        void resetOutputData() {
            for (auto &attribute : this->m_attributes)
                attribute.getOutputData().reset();
        }

        void resetProcessedInputs() {
            this->m_processedInputs.clear();
        }

    private:
        u32 m_id;
        std::string m_unlocalizedTitle, m_unlocalizedName;
        std::vector<Attribute> m_attributes;
        std::set<u32> m_processedInputs;
        prv::Overlay *m_overlay = nullptr;

        Attribute* getConnectedInputAttribute(u32 index) {
            if (index >= this->getAttributes().size())
                throw std::runtime_error("Attribute index out of bounds!");

            auto &connectedAttribute = this->getAttributes()[index].getConnectedAttributes();

            if (connectedAttribute.empty())
                return nullptr;

            return connectedAttribute.begin()->second;
        }

        void markInputProcessed(u32 index) {
            const auto &[iter, inserted] = this->m_processedInputs.insert(index);
            if (!inserted)
                throwNodeError("Recursion detected!");
        }

    protected:

        [[noreturn]] void throwNodeError(std::string_view message) {
            throw NodeError(this, message);
        }

        std::vector<u8> getBufferOnInput(u32 index) {
            auto attribute = this->getConnectedInputAttribute(index);

            if (attribute == nullptr)
                throwNodeError(hex::format("Nothing connected to input '{0}'", static_cast<const char*>(LangEntry(this->m_attributes[index].getUnlocalizedName()))));

            if (attribute->getType() != Attribute::Type::Buffer)
                throwNodeError("Tried to read buffer from non-buffer attribute");

            markInputProcessed(index);
            attribute->getParentNode()->process();

            auto &outputData = attribute->getOutputData();

            if (!outputData.has_value())
                throw std::runtime_error("No data available at connected attribute");

            return outputData.value();
        }

        u64 getIntegerOnInput(u32 index) {
            auto attribute = this->getConnectedInputAttribute(index);

            if (attribute == nullptr)
                throwNodeError(hex::format("Nothing connected to input '{0}'", static_cast<const char*>(LangEntry(this->m_attributes[index].getUnlocalizedName()))));

            if (attribute->getType() != Attribute::Type::Integer)
                throwNodeError("Tried to read integer from non-integer attribute");

            markInputProcessed(index);
            attribute->getParentNode()->process();

            auto &outputData = attribute->getOutputData();

            if (!outputData.has_value())
                throw std::runtime_error("No data available at connected attribute");

            if (outputData->size() < sizeof(u64))
                throw std::runtime_error("Not enough data provided for integer");

            return *reinterpret_cast<u64*>(outputData->data());
        }

        float getFloatOnInput(u32 index) {
            auto attribute = this->getConnectedInputAttribute(index);

            if (attribute == nullptr)
                throwNodeError(hex::format("Nothing connected to input '{0}'", static_cast<const char*>(LangEntry(this->m_attributes[index].getUnlocalizedName()))));

            if (attribute->getType() != Attribute::Type::Float)
                throwNodeError("Tried to read float from non-float attribute");

            markInputProcessed(index);
            attribute->getParentNode()->process();

            auto &outputData = attribute->getOutputData();

            if (!outputData.has_value())
                throw std::runtime_error("No data available at connected attribute");

            if (outputData->size() < sizeof(float))
                throw std::runtime_error("Not enough data provided for float");

            return *reinterpret_cast<float*>(outputData->data());
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