#include <algorithm>
#include <hex/api/content_registry/data_processor.hpp>
#include <hex/data_processor/node.hpp>

#include <ranges>

namespace hex::plugin::builtin {

    class NodeBitwiseNOT : public dp::Node {
    public:
        NodeBitwiseNOT() : Node("hex.builtin.nodes.bitwise.not.header"_unlocalized, { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"_unlocalized), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output"_unlocalized) }) { }

        void process() override {
            const auto &input = this->getBufferOnInput(0);

            std::vector<u8> output = input;
            for (auto &byte : output)
                byte = ~byte;

            this->setBufferOnOutput(1, output);
        }
    };

    class NodeBitwiseShiftLeft : public dp::Node {
    public:
        NodeBitwiseShiftLeft() : Node("hex.builtin.nodes.bitwise.shift_left.header"_unlocalized, {
                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"_unlocalized),
                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.amount"_unlocalized),
                dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output"_unlocalized)
        }) { }

        void process() override {
            const auto &input  = this->getBufferOnInput(0);
            const auto &amount = this->getIntegerOnInput(1);

            std::vector<u8> output = input;

            for (u32 i = 0; i < amount; i += 1) {
                u8 prevByte = 0x00;
                for (auto &byte : output) {
                    auto startValue = byte;

                    byte <<= 1;
                    byte |= (prevByte & 0x80) >> 7;
                    prevByte = startValue;
                }
            }

            this->setBufferOnOutput(2, output);
        }
    };

    class NodeBitwiseShiftRight : public dp::Node {
    public:
        NodeBitwiseShiftRight() : Node("hex.builtin.nodes.bitwise.shift_right.header"_unlocalized, {
                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"_unlocalized),
                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.amount"_unlocalized),
                dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output"_unlocalized)
        }) { }

        void process() override {
            const auto &input  = this->getBufferOnInput(0);
            const auto &amount = this->getIntegerOnInput(1);

            std::vector<u8> output = input;

            for (u32 i = 0; i < amount; i += 1) {
                u8 prevByte = 0x00;
                for (auto &byte : output | std::views::reverse) {
                    auto startValue = byte;
                    byte >>= 1;
                    byte |= (prevByte & 0x01) << 7;
                    prevByte = startValue;
                }
            }

            this->setBufferOnOutput(2, output);
        }
    };

    class NodeBitwiseADD : public dp::Node {
    public:
        NodeBitwiseADD() : Node("hex.builtin.nodes.bitwise.add.header"_unlocalized, { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input.a"_unlocalized), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input.b"_unlocalized), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output"_unlocalized) }) { }

        void process() override {
            const auto &inputA = this->getBufferOnInput(0);
            const auto &inputB = this->getBufferOnInput(1);

            std::vector<u8> output(std::min(inputA.size(), inputB.size()), 0x00);

            for (u32 i = 0; i < output.size(); i++)
                output[i] = inputA[i] + inputB[i];

            this->setBufferOnOutput(2, output);
        }
    };

    class NodeBitwiseAND : public dp::Node {
    public:
        NodeBitwiseAND() : Node("hex.builtin.nodes.bitwise.and.header"_unlocalized, { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input.a"_unlocalized), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input.b"_unlocalized), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output"_unlocalized) }) { }

        void process() override {
            const auto &inputA = this->getBufferOnInput(0);
            const auto &inputB = this->getBufferOnInput(1);

            std::vector<u8> output(std::min(inputA.size(), inputB.size()), 0x00);

            for (u32 i = 0; i < output.size(); i++)
                output[i] = inputA[i] & inputB[i];

            this->setBufferOnOutput(2, output);
        }
    };

    class NodeBitwiseOR : public dp::Node {
    public:
        NodeBitwiseOR() : Node("hex.builtin.nodes.bitwise.or.header"_unlocalized, { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input.a"_unlocalized), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input.b"_unlocalized), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output"_unlocalized) }) { }

        void process() override {
            const auto &inputA = this->getBufferOnInput(0);
            const auto &inputB = this->getBufferOnInput(1);

            std::vector<u8> output(std::min(inputA.size(), inputB.size()), 0x00);

            for (u32 i = 0; i < output.size(); i++)
                output[i] = inputA[i] | inputB[i];

            this->setBufferOnOutput(2, output);
        }
    };

    class NodeBitwiseXOR : public dp::Node {
    public:
        NodeBitwiseXOR() : Node("hex.builtin.nodes.bitwise.xor.header"_unlocalized, { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input.a"_unlocalized), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input.b"_unlocalized), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output"_unlocalized) }) { }

        void process() override {
            const auto &inputA = this->getBufferOnInput(0);
            const auto &inputB = this->getBufferOnInput(1);

            std::vector<u8> output(std::min(inputA.size(), inputB.size()), 0x00);

            for (u32 i = 0; i < output.size(); i++)
                output[i] = inputA[i] ^ inputB[i];

            this->setBufferOnOutput(2, output);
        }
    };

    class NodeBitwiseSwap : public dp::Node {
    public:
        NodeBitwiseSwap() : Node("hex.builtin.nodes.bitwise.swap.header"_unlocalized, {dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"_unlocalized), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output"_unlocalized) }) { }

        void process() override {
            // Table contains reversed nibble entries
            static constexpr std::array<u8, 16> BitFlipLookup = {
                    0x0, 0x8, 0x4, 0xC, 0x2, 0xA, 0x6, 0xE,
                    0x1, 0x9, 0x5, 0xD, 0x3, 0xB, 0x7, 0xF, };
            auto data = this->getBufferOnInput(0);

            for (u8 &b : data)
                b = BitFlipLookup[b & 0xf] << 4 | BitFlipLookup[b >> 4];

            std::ranges::reverse(data);
            this->setBufferOnOutput(1, data);
        }

    };
    
    void registerLogicDataProcessorNodes() {
        ContentRegistry::DataProcessor::add<NodeBitwiseADD>("hex.builtin.nodes.bitwise"_unlocalized, "hex.builtin.nodes.bitwise.add"_unlocalized);
        ContentRegistry::DataProcessor::add<NodeBitwiseAND>("hex.builtin.nodes.bitwise"_unlocalized, "hex.builtin.nodes.bitwise.and"_unlocalized);
        ContentRegistry::DataProcessor::add<NodeBitwiseOR>("hex.builtin.nodes.bitwise"_unlocalized, "hex.builtin.nodes.bitwise.or"_unlocalized);
        ContentRegistry::DataProcessor::add<NodeBitwiseXOR>("hex.builtin.nodes.bitwise"_unlocalized, "hex.builtin.nodes.bitwise.xor"_unlocalized);
        ContentRegistry::DataProcessor::add<NodeBitwiseNOT>("hex.builtin.nodes.bitwise"_unlocalized, "hex.builtin.nodes.bitwise.not"_unlocalized);
        ContentRegistry::DataProcessor::add<NodeBitwiseShiftLeft>("hex.builtin.nodes.bitwise"_unlocalized, "hex.builtin.nodes.bitwise.shift_left"_unlocalized);
        ContentRegistry::DataProcessor::add<NodeBitwiseShiftRight>("hex.builtin.nodes.bitwise"_unlocalized, "hex.builtin.nodes.bitwise.shift_right"_unlocalized);
        ContentRegistry::DataProcessor::add<NodeBitwiseSwap>("hex.builtin.nodes.bitwise"_unlocalized, "hex.builtin.nodes.bitwise.swap"_unlocalized);
    }

}