#include <hex/plugin.hpp>

#include "math_evaluator.hpp"

namespace hex::plugin::builtin {

    class NodeInteger : public dp::Node {
    public:
        NodeInteger() : Node("Integer", { dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "Value") }) {}

        void drawNode() override {
            ImGui::TextUnformatted("0x"); ImGui::SameLine(0, 0);
            ImGui::PushItemWidth(100);
            ImGui::InputScalar("##integerValue", ImGuiDataType_U64, &this->m_value, nullptr, nullptr, "%llx", ImGuiInputTextFlags_CharsHexadecimal);
            ImGui::PopItemWidth();
        }

        void process(prv::Overlay *dataOverlay) override {
            std::vector<u8> data(sizeof(this->m_value), 0);

            std::memcpy(data.data(), &this->m_value, sizeof(u64));
            this->getAttributes()[0].getOutputData() = data;
        }

    private:
        u64 m_value = 0;
    };

    class NodeFloat : public dp::Node {
    public:
        NodeFloat() : Node("Float", { dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Float, "Value") }) {}

        void drawNode() override {
            ImGui::PushItemWidth(100);
            ImGui::InputScalar("##floatValue", ImGuiDataType_Float, &this->m_value, nullptr, nullptr, "%f", ImGuiInputTextFlags_CharsDecimal);
            ImGui::PopItemWidth();
        }

        void process(prv::Overlay *dataOverlay) override {
            std::vector<u8> data;
            data.resize(sizeof(this->m_value));

            std::copy(&this->m_value, &this->m_value + 1, data.data());
            this->getAttributes()[0].getOutputData() = data;
        }

    private:
        float m_value = 0;
    };

    class NodeRGBA8 : public dp::Node {
    public:
        NodeRGBA8() : Node("RGBA8 Color", { dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "Red"),
                                                         dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "Green"),
                                                         dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "Blue"),
                                                         dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "Alpha")}) {}

        void drawNode() override {
            ImGui::PushItemWidth(200);
            ImGui::ColorPicker4("##colorPicker", &this->m_color.Value.x, ImGuiColorEditFlags_AlphaBar);
            ImGui::PopItemWidth();
        }

        void process(prv::Overlay *dataOverlay) override {
            this->getAttributes()[0].getOutputData() = hex::toBytes<u64>(this->m_color.Value.x * 0xFF);
            this->getAttributes()[1].getOutputData() = hex::toBytes<u64>(this->m_color.Value.y * 0xFF);
            this->getAttributes()[2].getOutputData() = hex::toBytes<u64>(this->m_color.Value.z * 0xFF);
            this->getAttributes()[3].getOutputData() = hex::toBytes<u64>(this->m_color.Value.w * 0xFF);
        }

    private:
        ImColor m_color;
    };


    class NodeDisplayInteger : public dp::Node {
    public:
        NodeDisplayInteger() : Node("Display Integer", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Value") }) {}

        void drawNode() override {
            ImGui::PushItemWidth(150);
            if (this->m_value.has_value())
                ImGui::Text("0x%llx", this->m_value.value());
            else
                ImGui::TextUnformatted("???");
            ImGui::PopItemWidth();
        }

        void process(prv::Overlay *dataOverlay) override {
            auto connectedInput = this->getConnectedInputAttribute(0);
            if (connectedInput == nullptr) {
                this->m_value.reset();
                return;
            }

            connectedInput->getParentNode()->process(dataOverlay);

            if (connectedInput->getOutputData().size() < sizeof(u64))
                return;

            this->m_value = *reinterpret_cast<u64*>(connectedInput->getOutputData().data());
        }

    private:
        std::optional<u64> m_value = 0;
    };

    class NodeDisplayFloat : public dp::Node {
    public:
        NodeDisplayFloat() : Node("Display Float", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Float, "Value") }) {}

        void drawNode() override {
            ImGui::PushItemWidth(150);
            if (this->m_value.has_value())
                ImGui::Text("%f", this->m_value.value());
            else
                ImGui::TextUnformatted("???");
            ImGui::PopItemWidth();
        }

        void process(prv::Overlay *dataOverlay) override {
            auto connectedInput = this->getConnectedInputAttribute(0);
            if (connectedInput == nullptr) {
                this->m_value.reset();
                return;
            }

            connectedInput->getParentNode()->process(dataOverlay);

            if (connectedInput->getOutputData().size() < sizeof(float))
                return;

            this->m_value = *reinterpret_cast<float*>(connectedInput->getOutputData().data());
        }

    private:
        std::optional<float> m_value = 0;
    };


    class NodeBitwiseNOT : public dp::Node {
    public:
        NodeBitwiseNOT() : Node("Bitwise NOT", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "Input"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "Output") }) {}

        void process(prv::Overlay *dataOverlay) override {
            auto connectedInput = this->getConnectedInputAttribute(0);
            if (connectedInput == nullptr)
                return;

            connectedInput->getParentNode()->process(dataOverlay);

            std::vector<u8> output = connectedInput->getOutputData();

            for (auto &byte : output)
                byte = ~byte;

            this->getAttributes()[1].getOutputData() = output;
        }
    };

    class NodeBitwiseAND : public dp::Node {
    public:
        NodeBitwiseAND() : Node("Bitwise AND", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "Input A"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "Input B"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "Output") }) {}

        void process(prv::Overlay *dataOverlay) override {
            auto connectedInputA = this->getConnectedInputAttribute(0);
            auto connectedInputB = this->getConnectedInputAttribute(1);
            if (connectedInputA == nullptr || connectedInputB == nullptr)
                return;

            connectedInputA->getParentNode()->process(dataOverlay);
            connectedInputB->getParentNode()->process(dataOverlay);

            std::vector<u8> inputA = connectedInputA->getOutputData();
            std::vector<u8> inputB = connectedInputB->getOutputData();

            std::vector<u8> output(std::min(inputA.size(), inputB.size()), 0x00);

            for (u32 i = 0; i < output.size(); i++)
                output[i] = inputA[i] & inputB[i];

            this->getAttributes()[2].getOutputData() = output;
        }
    };

    class NodeBitwiseOR : public dp::Node {
    public:
        NodeBitwiseOR() : Node("Bitwise OR", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "Input A"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "Input B"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "Output") }) {}

        void process(prv::Overlay *dataOverlay) override {
            auto connectedInputA = this->getConnectedInputAttribute(0);
            auto connectedInputB = this->getConnectedInputAttribute(1);
            if (connectedInputA == nullptr || connectedInputB == nullptr)
                return;

            connectedInputA->getParentNode()->process(dataOverlay);
            connectedInputB->getParentNode()->process(dataOverlay);

            std::vector<u8> inputA = connectedInputA->getOutputData();
            std::vector<u8> inputB = connectedInputB->getOutputData();

            std::vector<u8> output(std::min(inputA.size(), inputB.size()), 0x00);

            for (u32 i = 0; i < output.size(); i++)
                output[i] = inputA[i] | inputB[i];

            this->getAttributes()[2].getOutputData() = output;
        }
    };

    class NodeBitwiseXOR : public dp::Node {
    public:
        NodeBitwiseXOR() : Node("Bitwise XOR", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "Input A"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "Input B"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "Output") }) {}

        void process(prv::Overlay *dataOverlay) override {
            auto connectedInputA = this->getConnectedInputAttribute(0);
            auto connectedInputB = this->getConnectedInputAttribute(1);
            if (connectedInputA == nullptr || connectedInputB == nullptr)
                return;

            connectedInputA->getParentNode()->process(dataOverlay);
            connectedInputB->getParentNode()->process(dataOverlay);

            std::vector<u8> inputA = connectedInputA->getOutputData();
            std::vector<u8> inputB = connectedInputB->getOutputData();

            std::vector<u8> output(std::min(inputA.size(), inputB.size()), 0x00);

            for (u32 i = 0; i < output.size(); i++)
                output[i] = inputA[i] ^ inputB[i];

            this->getAttributes()[2].getOutputData() = output;
        }
    };

    class NodeReadData : public dp::Node {
    public:
        NodeReadData() : Node("Read Data", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Address"),
                                             dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Size"),
                                             dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "Data")
        }) { }

        void process(prv::Overlay *dataOverlay) override {
            auto connectedInputAddress = this->getConnectedInputAttribute(0);
            auto connectedInputSize = this->getConnectedInputAttribute(1);
            if (connectedInputAddress == nullptr || connectedInputSize == nullptr)
                return;

            connectedInputAddress->getParentNode()->process(dataOverlay);
            connectedInputSize->getParentNode()->process(dataOverlay);

            auto address = *reinterpret_cast<u64*>(connectedInputAddress->getOutputData().data());
            auto size = *reinterpret_cast<u64*>(connectedInputSize->getOutputData().data());

            std::vector<u8> data;
            data.resize(size);

            SharedData::currentProvider->readRaw(address, data.data(), size);

            this->getAttributes()[2].getOutputData() = data;
        }
    };

    class NodeWriteData : public dp::Node {
    public:
        NodeWriteData() : Node("Write Data", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Address"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "Data") }) {}

        void process(prv::Overlay *dataOverlay) override {
            auto connectedInputAddress = this->getConnectedInputAttribute(0);
            auto connectedInputData = this->getConnectedInputAttribute(1);
            if (connectedInputAddress == nullptr || connectedInputData == nullptr)
                return;

            connectedInputAddress->getParentNode()->process(dataOverlay);
            connectedInputData->getParentNode()->process(dataOverlay);

            auto address = *reinterpret_cast<u64*>(connectedInputAddress->getOutputData().data());
            auto data = connectedInputData->getOutputData();

            dataOverlay->setAddress(address);
            dataOverlay->getData() = data;
        }
    };

    class NodeCastIntegerToBuffer : public dp::Node {
    public:
        NodeCastIntegerToBuffer() : Node("Integer to Buffer", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "In"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "Out") }) {}

        void process(prv::Overlay *dataOverlay) override {
            auto connectedInputInteger = this->getConnectedInputAttribute(0);
            if (connectedInputInteger == nullptr)
                return;

            connectedInputInteger->getParentNode()->process(dataOverlay);

            std::vector<u8> output(sizeof(u64), 0);

            auto &inputData = connectedInputInteger->getOutputData();

            if (!inputData.empty())
                std::copy(inputData.begin(), inputData.end(), output.begin());

            this->getAttributes()[1].getOutputData() = output;
        }
    };

    class NodeCastBufferToInteger : public dp::Node {
    public:
        NodeCastBufferToInteger() : Node("Buffer to Integer", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "In"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "Out") }) {}

        void process(prv::Overlay *dataOverlay) override {
            auto connectedInputBuffer = this->getConnectedInputAttribute(0);
            if (connectedInputBuffer == nullptr)
                return;

            connectedInputBuffer->getParentNode()->process(dataOverlay);

            std::vector<u8> output(sizeof(u64), 0);

            auto &inputData = connectedInputBuffer->getOutputData();

            if (!inputData.empty())
                std::copy(inputData.begin(), inputData.end(), output.begin());

            this->getAttributes()[1].getOutputData() = output;
        }
    };

    class NodeIf : public dp::Node {
    public:
        NodeIf() : Node("If", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Condition"),
                                             dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "True"),
                                             dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "False"),
                                             dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "Output") }) {}

        void process(prv::Overlay *dataOverlay) override {
            auto connectedInputCondition = this->getConnectedInputAttribute(0);
            auto connectedInputTrue = this->getConnectedInputAttribute(1);
            auto connectedInputFalse = this->getConnectedInputAttribute(2);
            if (connectedInputCondition == nullptr || connectedInputTrue == nullptr || connectedInputFalse == nullptr)
                return;

            connectedInputCondition->getParentNode()->process(dataOverlay);
            connectedInputTrue->getParentNode()->process(dataOverlay);
            connectedInputFalse->getParentNode()->process(dataOverlay);

            if (*reinterpret_cast<u64*>(connectedInputCondition->getOutputData().data()) != 0)
                this->getAttributes()[3].getOutputData() = connectedInputTrue->getOutputData();
            else
                this->getAttributes()[3].getOutputData() = connectedInputFalse->getOutputData();

        }
    };

    class NodeEquals : public dp::Node {
    public:
        NodeEquals() : Node("Equals", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Input A"),
                                                     dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Input B"),
                                                     dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "Output") }) {}

        void process(prv::Overlay *dataOverlay) override {
            auto connectedInputInputA = this->getConnectedInputAttribute(0);
            auto connectedInputInputB = this->getConnectedInputAttribute(1);
            if (connectedInputInputA == nullptr || connectedInputInputB == nullptr)
                return;

            connectedInputInputA->getParentNode()->process(dataOverlay);
            connectedInputInputB->getParentNode()->process(dataOverlay);

            auto &inputA = connectedInputInputA->getOutputData();
            auto &inputB = connectedInputInputB->getOutputData();

            if (inputA.empty() || inputB.empty())
                return;

            if (*reinterpret_cast<u64*>(inputA.data()) == *reinterpret_cast<u64*>(inputB.data()))
                this->getAttributes()[2].getOutputData() = hex::toBytes<u64>(1);
            else
                this->getAttributes()[2].getOutputData() = hex::toBytes<u64>(0);

        }
    };

    class NodeNot : public dp::Node {
    public:
        NodeNot() : Node("Not", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Input"),
                                     dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "Output") }) {}

        void process(prv::Overlay *dataOverlay) override {
            auto connectedInputInput = this->getConnectedInputAttribute(0);
            if (connectedInputInput == nullptr)
                return;

            connectedInputInput->getParentNode()->process(dataOverlay);


            auto &input = connectedInputInput->getOutputData();
            if (input.empty())
                return;

            if (*reinterpret_cast<u64*>(connectedInputInput->getOutputData().data()) == 0)
                this->getAttributes()[1].getOutputData() = hex::toBytes<u64>(1);
            else
                this->getAttributes()[1].getOutputData() = hex::toBytes<u64>(0);

        }
    };

    class NodeGreaterThan : public dp::Node {
    public:
        NodeGreaterThan() : Node("Greater Than", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Input A"),
                                                                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Input B"),
                                                                dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "Output") }) {}

        void process(prv::Overlay *dataOverlay) override {
            auto connectedInputInputA = this->getConnectedInputAttribute(0);
            auto connectedInputInputB = this->getConnectedInputAttribute(1);
            if (connectedInputInputA == nullptr || connectedInputInputB == nullptr)
                return;

            connectedInputInputA->getParentNode()->process(dataOverlay);
            connectedInputInputB->getParentNode()->process(dataOverlay);

            auto &inputA = connectedInputInputA->getOutputData();
            auto &inputB = connectedInputInputB->getOutputData();

            if (inputA.empty() || inputB.empty())
                return;

            if (*reinterpret_cast<u64*>(inputA.data()) > *reinterpret_cast<u64*>(inputB.data()))
                this->getAttributes()[2].getOutputData() = hex::toBytes<u64>(1);
            else
                this->getAttributes()[2].getOutputData() = hex::toBytes<u64>(0);

        }
    };

    class NodeLessThan : public dp::Node {
    public:
        NodeLessThan() : Node("Less Than", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Input A"),
                                                          dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Input B"),
                                                          dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "Output") }) {}

        void process(prv::Overlay *dataOverlay) override {
            auto connectedInputInputA = this->getConnectedInputAttribute(0);
            auto connectedInputInputB = this->getConnectedInputAttribute(1);
            if (connectedInputInputA == nullptr || connectedInputInputB == nullptr)
                return;

            auto &inputA = connectedInputInputA->getOutputData();
            auto &inputB = connectedInputInputB->getOutputData();

            if (inputA.empty() || inputB.empty())
                return;

            if (*reinterpret_cast<u64*>(inputA.data()) < *reinterpret_cast<u64*>(inputB.data()))
                this->getAttributes()[2].getOutputData() = hex::toBytes<u64>(1);
            else
                this->getAttributes()[2].getOutputData() = hex::toBytes<u64>(0);

        }
    };

    class NodeBoolAND : public dp::Node {
    public:
        NodeBoolAND() : Node("Boolean AND", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Input A"),
                                                           dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Input B"),
                                                           dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "Output") }) {}

        void process(prv::Overlay *dataOverlay) override {
            auto connectedInputInputA = this->getConnectedInputAttribute(0);
            auto connectedInputInputB = this->getConnectedInputAttribute(1);
            if (connectedInputInputA == nullptr || connectedInputInputB == nullptr)
                return;

            connectedInputInputA->getParentNode()->process(dataOverlay);
            connectedInputInputB->getParentNode()->process(dataOverlay);

            auto &inputA = connectedInputInputA->getOutputData();
            auto &inputB = connectedInputInputB->getOutputData();

            if (inputA.empty() || inputB.empty())
                return;

            if (*reinterpret_cast<u64*>(inputA.data()) && *reinterpret_cast<u64*>(inputB.data()))
                this->getAttributes()[2].getOutputData() = hex::toBytes<u64>(1);
            else
                this->getAttributes()[2].getOutputData() = hex::toBytes<u64>(0);

        }
    };

    class NodeBoolOR : public dp::Node {
    public:
        NodeBoolOR() : Node("Boolean OR", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Input A"),
                                                         dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Input B"),
                                                         dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "Output") }) {}

        void process(prv::Overlay *dataOverlay) override {
            auto connectedInputInputA = this->getConnectedInputAttribute(0);
            auto connectedInputInputB = this->getConnectedInputAttribute(1);
            if (connectedInputInputA == nullptr || connectedInputInputB == nullptr)
                return;

            connectedInputInputA->getParentNode()->process(dataOverlay);
            connectedInputInputB->getParentNode()->process(dataOverlay);

            auto &inputA = connectedInputInputA->getOutputData();
            auto &inputB = connectedInputInputB->getOutputData();

            if (inputA.empty() || inputB.empty())
                return;

            if (*reinterpret_cast<u64*>(inputA.data()) || *reinterpret_cast<u64*>(inputB.data()))
                this->getAttributes()[2].getOutputData() = hex::toBytes<u64>(1);
            else
                this->getAttributes()[2].getOutputData() = hex::toBytes<u64>(0);

        }
    };

    void registerDataProcessorNodes() {
        ContentRegistry::DataProcessorNode::add<NodeInteger>("Constants", "Integer");
        ContentRegistry::DataProcessorNode::add<NodeFloat>("Constants", "Float");
        ContentRegistry::DataProcessorNode::add<NodeRGBA8>("Constants", "RGBA8 Color");

        ContentRegistry::DataProcessorNode::add<NodeDisplayInteger>("Display", "Integer");
        ContentRegistry::DataProcessorNode::add<NodeDisplayFloat>("Display", "Float");

        ContentRegistry::DataProcessorNode::add<NodeReadData>("Data Access", "Read");
        ContentRegistry::DataProcessorNode::add<NodeWriteData>("Data Access", "Write");

        ContentRegistry::DataProcessorNode::add<NodeCastIntegerToBuffer>("Casting", "Integer to Buffer");
        ContentRegistry::DataProcessorNode::add<NodeCastBufferToInteger>("Casting", "Buffer to Integer");

        ContentRegistry::DataProcessorNode::add<NodeIf>("Control Flow", "If");
        ContentRegistry::DataProcessorNode::add<NodeEquals>("Control Flow", "Equals");
        ContentRegistry::DataProcessorNode::add<NodeNot>("Control Flow", "Not");
        ContentRegistry::DataProcessorNode::add<NodeGreaterThan>("Control Flow", "Greater Than");
        ContentRegistry::DataProcessorNode::add<NodeLessThan>("Control Flow", "Less Than");
        ContentRegistry::DataProcessorNode::add<NodeBoolAND>("Control Flow", "AND");
        ContentRegistry::DataProcessorNode::add<NodeBoolOR>("Control Flow", "OR");

        ContentRegistry::DataProcessorNode::add<NodeBitwiseAND>("Bitwise Operations", "AND");
        ContentRegistry::DataProcessorNode::add<NodeBitwiseOR>("Bitwise Operations", "OR");
        ContentRegistry::DataProcessorNode::add<NodeBitwiseXOR>("Bitwise Operations", "XOR");
        ContentRegistry::DataProcessorNode::add<NodeBitwiseNOT>("Bitwise Operations", "NOT");
    }

}