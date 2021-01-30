#include <hex/plugin.hpp>

#include "math_evaluator.hpp"

namespace hex::plugin::builtin {

    class NodeInteger : public dp::Node {
    public:
        NodeInteger() : Node("Integer", { dp::Attribute(dp::Attribute::Type::Out, "Value") }) {}

        void drawNode() override {
            ImGui::TextUnformatted("0x"); ImGui::SameLine(0, 0);
            ImGui::PushItemWidth(100);
            ImGui::InputScalar("##integerValue", ImGuiDataType_U64, &this->m_value, nullptr, nullptr, "%llx", ImGuiInputTextFlags_CharsHexadecimal);
            ImGui::PopItemWidth();
        }

        [[nodiscard]] std::vector<u8> process(prv::Overlay *dataOverlay) override {
            std::vector<u8> data;
            data.resize(sizeof(this->m_value));

            std::copy(&this->m_value, &this->m_value + 1, data.data());
            return data;
        }

    private:
        u64 m_value = 0;
    };

    class NodeReadData : public dp::Node {
    public:
        NodeReadData() : Node("Read Data", { dp::Attribute(dp::Attribute::Type::In, "Address"),
                                                          dp::Attribute(dp::Attribute::Type::In, "Size"),
                                                          dp::Attribute(dp::Attribute::Type::Out, "Data")
                                                        }) { }

        [[nodiscard]] std::vector<u8> process(prv::Overlay *dataOverlay) override {
            auto connectedInputAddress = this->getConnectedInputNode(0);
            auto connectedInputSize = this->getConnectedInputNode(1);
            if (connectedInputAddress == nullptr || connectedInputSize == nullptr)
                return {};

            auto address = *reinterpret_cast<u64*>(connectedInputAddress->process(dataOverlay).data());
            auto size = *reinterpret_cast<u64*>(connectedInputSize->process(dataOverlay).data());

            std::vector<u8> data;
            data.resize(size);

            SharedData::currentProvider->readRaw(address, data.data(), size);
            return data;
        }

    private:
    };

    class NodeInvert : public dp::Node {
    public:
        NodeInvert() : Node("Bitwise NOT", { dp::Attribute(dp::Attribute::Type::In, "Input"), dp::Attribute(dp::Attribute::Type::Out, "Output") }) {}

        [[nodiscard]] std::vector<u8> process(prv::Overlay *dataOverlay) override {
            auto connectedInput = this->getConnectedInputNode(0);
            if (connectedInput == nullptr)
                return {};

            std::vector<u8> output = connectedInput->process(dataOverlay);

            for (auto &byte : output)
                byte = ~byte;

            return output;
        }
    };

    class NodeWriteData : public dp::Node {
    public:
        NodeWriteData() : Node("Write Data", { dp::Attribute(dp::Attribute::Type::In, "Address"), dp::Attribute(dp::Attribute::Type::In, "Data") }, true) {}

        [[nodiscard]] std::vector<u8> process(prv::Overlay *dataOverlay) override {
            auto connectedInputAddress = this->getConnectedInputNode(0);
            auto connectedInputData = this->getConnectedInputNode(1);
            if (connectedInputAddress == nullptr || connectedInputData == nullptr)
                return {};

            auto address = *reinterpret_cast<u64*>(connectedInputAddress->process(dataOverlay).data());
            auto data = connectedInputData->process(dataOverlay);

            dataOverlay->setAddress(address);
            dataOverlay->getData() = data;

            return data;
        }
    };

    void registerDataProcessorNodes() {
        ContentRegistry::DataProcessorNode::add<NodeInteger>("Constants", "Integer");
        ContentRegistry::DataProcessorNode::add<NodeReadData>("Data Access", "Read");
        ContentRegistry::DataProcessorNode::add<NodeWriteData>("Data Access", "Write");
        ContentRegistry::DataProcessorNode::add<NodeInvert>("Bitwise Operations", "NOT");
    }

}