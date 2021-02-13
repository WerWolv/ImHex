#include <hex/plugin.hpp>

#include <hex/helpers/crypto.hpp>

#include <cctype>

namespace hex::plugin::builtin {

    class NodeNullptr : public dp::Node {
    public:
        NodeNullptr() : Node("hex.builtin.nodes.constants.nullptr.header", {
            dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.constants.nullptr.output")
        }) {}

        void process() override {
            this->setBufferOnOutput(0, { });
        }
    };

    class NodeBuffer : public dp::Node {
    public:
        NodeBuffer() : Node("hex.builtin.nodes.constants.buffer.header", {
            dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.constants.buffer.output")
        }) {}

        void drawNode() override {
            constexpr int StepSize = 1, FastStepSize = 10;

            ImGui::PushItemWidth(100);
            ImGui::InputScalar("hex.builtin.nodes.constants.buffer.size"_lang, ImGuiDataType_U32, &this->m_size, &StepSize, &FastStepSize);
            ImGui::PopItemWidth();
        }

        void process() override {
            if (this->m_buffer.size() != this->m_size)
                this->m_buffer.resize(this->m_size, 0x00);

            this->setBufferOnOutput(0, this->m_buffer);
        }

    private:
        u32 m_size = 1;
        std::vector<u8> m_buffer;
    };

    class NodeString : public dp::Node {
    public:
        NodeString() : Node("hex.builtin.nodes.constants.string.header", {
            dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.constants.string.output")
        }) {
            this->m_value.resize(0xFFF, 0x00);
        }

        void drawNode() override {
            ImGui::PushItemWidth(100);
            ImGui::InputText("##string", reinterpret_cast<char*>(this->m_value.data()), this->m_value.size() - 1);
            ImGui::PopItemWidth();
        }

        void process() override {
            std::vector<u8> output(std::strlen(this->m_value.c_str()) + 1, 0x00);
            std::strcpy(reinterpret_cast<char*>(output.data()), this->m_value.c_str());

            output.pop_back();

            this->setBufferOnOutput(0, output);
        }

    private:
        std::string m_value;
    };

    class NodeInteger : public dp::Node {
    public:
        NodeInteger() : Node("hex.builtin.nodes.constants.int.header", {
            dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.constants.int.output")
        }) {}

        void drawNode() override {
            ImGui::TextUnformatted("0x"); ImGui::SameLine(0, 0);
            ImGui::PushItemWidth(100);
            ImGui::InputScalar("##integerValue", ImGuiDataType_U64, &this->m_value, nullptr, nullptr, "%llx", ImGuiInputTextFlags_CharsHexadecimal);
            ImGui::PopItemWidth();
        }

        void process() override {
            std::vector<u8> data(sizeof(this->m_value), 0);

            std::memcpy(data.data(), &this->m_value, sizeof(u64));
            this->setBufferOnOutput(0, data);
        }

    private:
        u64 m_value = 0;
    };

    class NodeFloat : public dp::Node {
    public:
        NodeFloat() : Node("hex.builtin.nodes.constants.float.header", {
            dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Float, "hex.builtin.nodes.constants.float.output")
        }) {}

        void drawNode() override {
            ImGui::PushItemWidth(100);
            ImGui::InputScalar("##floatValue", ImGuiDataType_Float, &this->m_value, nullptr, nullptr, "%f", ImGuiInputTextFlags_CharsDecimal);
            ImGui::PopItemWidth();
        }

        void process() override {
            std::vector<u8> data;
            data.resize(sizeof(this->m_value));

            std::copy(&this->m_value, &this->m_value + 1, data.data());
            this->setBufferOnOutput(0, data);
        }

    private:
        float m_value = 0;
    };

    class NodeRGBA8 : public dp::Node {
    public:
        NodeRGBA8() : Node("hex.builtin.nodes.constants.rgba8.header",
                           { dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.constants.rgba8.output.r"),
                                     dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.constants.rgba8.output.g"),
                                     dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.constants.rgba8.output.b"),
                                     dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.constants.rgba8.output.a")}) {}

        void drawNode() override {
            ImGui::PushItemWidth(200);
            ImGui::ColorPicker4("##colorPicker", &this->m_color.Value.x, ImGuiColorEditFlags_AlphaBar);
            ImGui::PopItemWidth();
        }

        void process() override {
            this->setBufferOnOutput(0, hex::toBytes<u64>(this->m_color.Value.x * 0xFF));
            this->setBufferOnOutput(1, hex::toBytes<u64>(this->m_color.Value.y * 0xFF));
            this->setBufferOnOutput(2, hex::toBytes<u64>(this->m_color.Value.z * 0xFF));
            this->setBufferOnOutput(3, hex::toBytes<u64>(this->m_color.Value.w * 0xFF));

        }

    private:
        ImColor m_color;
    };

    class NodeComment : public dp::Node {
    public:
        NodeComment() : Node("hex.builtin.nodes.constants.comment.header", { }) {
            this->m_comment.resize(0xFFF, 0x00);
        }

        void drawNode() override {
            ImGui::InputTextMultiline("##string", reinterpret_cast<char*>(this->m_comment.data()), this->m_comment.size() - 1, ImVec2(150, 100));
        }

        void process() override {

        }

    private:
        std::string m_comment;
    };


    class NodeDisplayInteger : public dp::Node {
    public:
        NodeDisplayInteger() : Node("hex.builtin.nodes.display.int.header", {
            dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.display.int.input")
        }) {}

        void drawNode() override {
            ImGui::PushItemWidth(150);
            if (this->m_value.has_value())
                ImGui::Text("0x%llx", this->m_value.value());
            else
                ImGui::TextUnformatted("???");
            ImGui::PopItemWidth();
        }

        void process() override {
            this->m_value.reset();
            auto input = this->getIntegerOnInput(0);

            this->m_value = input;
        }

    private:
        std::optional<u64> m_value;
    };

    class NodeDisplayFloat : public dp::Node {
    public:
        NodeDisplayFloat() : Node("hex.builtin.nodes.display.float.header", {
            dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Float, "hex.builtin.nodes.display.float.input")
        }) {}

        void drawNode() override {
            ImGui::PushItemWidth(150);
            if (this->m_value.has_value())
                ImGui::Text("%f", this->m_value.value());
            else
                ImGui::TextUnformatted("???");
            ImGui::PopItemWidth();
        }

        void process() override {
            this->m_value.reset();
            auto input = this->getFloatOnInput(0);

            this->m_value = input;
        }

    private:
        std::optional<float> m_value;
    };


    class NodeBitwiseNOT : public dp::Node {
    public:
        NodeBitwiseNOT() : Node("hex.builtin.nodes.bitwise.not.header", {
            dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.bitwise.not.input"),
            dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.bitwise.not.output") }) {}

        void process() override {
            auto input = this->getBufferOnInput(0);

            std::vector<u8> output = input;
            for (auto &byte : output)
                byte = ~byte;

            this->setBufferOnOutput(1, output);
        }
    };

    class NodeBitwiseAND : public dp::Node {
    public:
        NodeBitwiseAND() : Node("hex.builtin.nodes.bitwise.and.header", {
            dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.bitwise.and.input.a"),
            dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.bitwise.and.input.b"),
            dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.bitwise.and.output") }) {}

        void process() override {
            auto inputA = this->getBufferOnInput(0);
            auto inputB = this->getBufferOnInput(1);

            std::vector<u8> output(std::min(inputA.size(), inputB.size()), 0x00);

            for (u32 i = 0; i < output.size(); i++)
                output[i] = inputA[i] & inputB[i];

            this->setBufferOnOutput(2, output);
        }
    };

    class NodeBitwiseOR : public dp::Node {
    public:
        NodeBitwiseOR() : Node("hex.builtin.nodes.bitwise.or.header", {
            dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.bitwise.or.input.a"),
            dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.bitwise.or.input.b"),
            dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.bitwise.or.output") }) {}

        void process() override {
            auto inputA = this->getBufferOnInput(0);
            auto inputB = this->getBufferOnInput(1);

            std::vector<u8> output(std::min(inputA.size(), inputB.size()), 0x00);

            for (u32 i = 0; i < output.size(); i++)
                output[i] = inputA[i] | inputB[i];

            this->setBufferOnOutput(2, output);
        }
    };

    class NodeBitwiseXOR : public dp::Node {
    public:
        NodeBitwiseXOR() : Node("hex.builtin.nodes.bitwise.xor.header", {
            dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.bitwise.xor.input.a"),
            dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.bitwise.xor.input.b"),
            dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.bitwise.xor.output") }) {}

        void process() override {
            auto inputA = this->getBufferOnInput(0);
            auto inputB = this->getBufferOnInput(1);

            std::vector<u8> output(std::min(inputA.size(), inputB.size()), 0x00);

            for (u32 i = 0; i < output.size(); i++)
                output[i] = inputA[i] ^ inputB[i];

            this->setBufferOnOutput(2, output);
        }
    };

    class NodeReadData : public dp::Node {
    public:
        NodeReadData() : Node("hex.builtin.nodes.data_access.read.header", {
                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.data_access.read.address"),
                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.data_access.read.size"),
                dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.data_access.read.data")
        }) { }

        void process() override {
            auto address = this->getIntegerOnInput(0);
            auto size = this->getIntegerOnInput(1);

            std::vector<u8> data;
            data.resize(size);

            SharedData::currentProvider->readRaw(address, data.data(), size);

            this->setBufferOnOutput(2, data);
        }
    };

    class NodeWriteData : public dp::Node {
    public:
        NodeWriteData() : Node("hex.builtin.nodes.data_access.write.header", {
            dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.data_access.write.address"),
            dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.data_access.write.data") }) {}

        void process() override {
            auto address = this->getIntegerOnInput(0);
            auto data = this->getBufferOnInput(1);

            this->setOverlayData(address, data);
        }
    };

    class NodeCastIntegerToBuffer : public dp::Node {
    public:
        NodeCastIntegerToBuffer() : Node("hex.builtin.nodes.casting.int_to_buffer.header", {
            dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.casting.int_to_buffer.input"),
            dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.casting.int_to_buffer.output") }) {}

        void process() override {
            auto input = this->getIntegerOnInput(0);

            std::vector<u8> output(sizeof(u64), 0x00);
            std::memcpy(output.data(), &input, sizeof(u64));

            this->setBufferOnOutput(1, output);
        }
    };

    class NodeCastBufferToInteger : public dp::Node {
    public:
        NodeCastBufferToInteger() : Node("hex.builtin.nodes.casting.buffer_to_int.header", {
            dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.casting.buffer_to_int.input"),
            dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.casting.buffer_to_int.output") }) {}

        void process() override {
            auto input = this->getBufferOnInput(0);

            u64 output;
            std::memcpy(&output, input.data(), sizeof(u64));

            this->setIntegerOnOutput(1, output);
        }
    };

    class NodeIf : public dp::Node {
    public:
        NodeIf() : Node("ex.builtin.nodes.control_flow.if.header",
                          { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.control_flow.if.condition"),
                                     dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.control_flow.if.true"),
                                     dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.control_flow.if.false"),
                                     dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.control_flow.if.output") }) {}

        void process() override {
            auto cond = this->getIntegerOnInput(0);
            auto trueData = this->getBufferOnInput(1);
            auto falseData = this->getBufferOnInput(2);

            if (cond != 0)
                this->setBufferOnOutput(3, trueData);
            else
                this->setBufferOnOutput(3, falseData);

        }
    };

    class NodeEquals : public dp::Node {
    public:
        NodeEquals() : Node("hex.builtin.nodes.control_flow.equals.header",
                            { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.control_flow.equals.input.a"),
                                      dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.control_flow.equals.input.b"),
                                      dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.control_flow.equals.output") }) {}

        void process() override {
            auto inputA = this->getIntegerOnInput(0);
            auto inputB = this->getIntegerOnInput(1);

            this->setIntegerOnOutput(2, inputA == inputB);
        }
    };

    class NodeNot : public dp::Node {
    public:
        NodeNot() : Node("hex.builtin.nodes.control_flow.not.header",
                           { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.control_flow.not.input"),
                                     dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.control_flow.not.output") }) {}

        void process() override {
            auto input = this->getIntegerOnInput(0);

            this->setIntegerOnOutput(1, !input);
        }
    };

    class NodeGreaterThan : public dp::Node {
    public:
        NodeGreaterThan() : Node("hex.builtin.nodes.control_flow.gt.header",
                                 { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.control_flow.gt.input.a"),
                                           dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.control_flow.gt.input.b"),
                                           dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.control_flow.gt.output") }) {}

        void process() override {
            auto inputA = this->getIntegerOnInput(0);
            auto inputB = this->getIntegerOnInput(1);

            this->setIntegerOnOutput(2, inputA > inputB);
        }
    };

    class NodeLessThan : public dp::Node {
    public:
        NodeLessThan() : Node("hex.builtin.nodes.control_flow.lt.header",
                              { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.control_flow.lt.input.a"),
                                        dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.control_flow.lt.input.b"),
                                        dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.control_flow.lt.output") }) {}

        void process() override {
            auto inputA = this->getIntegerOnInput(0);
            auto inputB = this->getIntegerOnInput(1);

            this->setIntegerOnOutput(2, inputA < inputB);
        }
    };

    class NodeBoolAND : public dp::Node {
    public:
        NodeBoolAND() : Node("hex.builtin.nodes.control_flow.and.header",
                             { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.control_flow.and.input.a"),
                                       dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.control_flow.and.input.b"),
                                       dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.control_flow.and.output") }) {}

        void process() override {
            auto inputA = this->getIntegerOnInput(0);
            auto inputB = this->getIntegerOnInput(1);

            this->setIntegerOnOutput(2, inputA && inputB);
        }
    };

    class NodeBoolOR : public dp::Node {
    public:
        NodeBoolOR() : Node("hex.builtin.nodes.control_flow.or.header",
                            { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.control_flow.or.input.a"),
                                      dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.control_flow.or.input.b"),
                                      dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.control_flow.or.output") }) {}

        void process() override {
            auto inputA = this->getIntegerOnInput(0);
            auto inputB = this->getIntegerOnInput(1);

            this->setIntegerOnOutput(2, inputA || inputB);
        }
    };

    class NodeCryptoAESDecrypt : public dp::Node {
    public:
        NodeCryptoAESDecrypt() : Node("hex.builtin.nodes.crypto.aes.header",
                                      { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.crypto.aes.key"),
                                                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.crypto.aes.iv"),
                                                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.crypto.aes.nonce"),
                                                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.crypto.aes.input"),
                                                dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.crypto.aes.output") }) {}

        void drawNode() override {
            ImGui::PushItemWidth(100);
            ImGui::Combo("hex.builtin.nodes.crypto.aes.mode"_lang, &this->m_mode, "ECB\0CBC\0CFB128\0CTR\0GCM\0CCM\0OFB\0");
            ImGui::Combo("hex.builtin.nodes.crypto.aes.key_length"_lang, &this->m_keyLength, "128 Bits\000192 Bits\000256 Bits\000");
            ImGui::PopItemWidth();
        }

        void process() override {
            auto key = this->getBufferOnInput(0);
            auto iv = this->getBufferOnInput(1);
            auto nonce = this->getBufferOnInput(2);
            auto input = this->getBufferOnInput(3);

            if (key.empty())
                throwNodeError("Key cannot be empty");

            if (input.empty())
                throwNodeError("Input cannot be empty");

            std::array<u8, 8> ivData = { 0 }, nonceData = { 0 };

            std::copy(iv.begin(), iv.end(), ivData.begin());
            std::copy(nonce.begin(), nonce.end(), nonceData.begin());

            auto output = crypt::aesDecrypt(static_cast<crypt::AESMode>(this->m_mode), static_cast<crypt::KeyLength>(this->m_keyLength), key, nonceData, ivData, input);

            this->setBufferOnOutput(4, output);
        }

    private:
        int m_mode = 0;
        int m_keyLength = 0;
    };

    class NodeDecodingBase64 : public dp::Node {
    public:
        NodeDecodingBase64() : Node("hex.builtin.nodes.decoding.base64.header", {
            dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.decoding.base64.input"),
            dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.decoding.base64.output") }) {}

        void process() override {
            auto input = this->getBufferOnInput(0);

            auto output = crypt::decode64(input);

            this->setBufferOnOutput(1, output);
        }
    };

    class NodeDecodingHex : public dp::Node {
    public:
        NodeDecodingHex() : Node("hex.builtin.nodes.decoding.hex.header", {
                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.decoding.hex.input"),
                dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.decoding.hex.output") }) {}

        void process() override {
            auto input = this->getBufferOnInput(0);

            if (input.size() % 2 != 0)
                throwNodeError("Can't decode odd number of hex characters");

            std::vector<u8> output;
            for (u32 i = 0; i < input.size(); i += 2) {
                char c1 = static_cast<char>(std::tolower(input[i]));
                char c2 = static_cast<char>(std::tolower(input[i + 1]));

                if (!std::isxdigit(c1) || !isxdigit(c2))
                    throwNodeError("Can't decode non-hexadecimal character");

                u8 value;
                if (std::isdigit(c1))
                    value = (c1 - '0') << 4;
                else
                    value = ((c1 - 'a') + 0x0A) << 4;

                if (std::isdigit(c2))
                    value |= c2 - '0';
                else
                    value |= (c2 - 'a') + 0x0A;

                output.push_back(value);
            }

            this->setBufferOnOutput(1, output);
        }
    };

    void registerDataProcessorNodes() {
        ContentRegistry::DataProcessorNode::add<NodeInteger>("hex.builtin.nodes.constants", "hex.builtin.nodes.constants.int");
        ContentRegistry::DataProcessorNode::add<NodeFloat>("hex.builtin.nodes.constants", "hex.builtin.nodes.constants.float");
        ContentRegistry::DataProcessorNode::add<NodeNullptr>("hex.builtin.nodes.constants", "hex.builtin.nodes.constants.nullptr");
        ContentRegistry::DataProcessorNode::add<NodeBuffer>("hex.builtin.nodes.constants", "hex.builtin.nodes.constants.buffer");
        ContentRegistry::DataProcessorNode::add<NodeString>("hex.builtin.nodes.constants", "hex.builtin.nodes.constants.string");
        ContentRegistry::DataProcessorNode::add<NodeRGBA8>("hex.builtin.nodes.constants", "hex.builtin.nodes.constants.rgba8");
        ContentRegistry::DataProcessorNode::add<NodeComment>("hex.builtin.nodes.constants", "hex.builtin.nodes.constants.comment");

        ContentRegistry::DataProcessorNode::add<NodeDisplayInteger>("hex.builtin.nodes.display", "hex.builtin.nodes.display.int");
        ContentRegistry::DataProcessorNode::add<NodeDisplayFloat>("hex.builtin.nodes.display", "hex.builtin.nodes.display.float");

        ContentRegistry::DataProcessorNode::add<NodeReadData>("hex.builtin.nodes.data_access", "hex.builtin.nodes.data_access.read");
        ContentRegistry::DataProcessorNode::add<NodeWriteData>("hex.builtin.nodes.data_access", "hex.builtin.nodes.data_access.write");

        ContentRegistry::DataProcessorNode::add<NodeCastIntegerToBuffer>("hex.builtin.nodes.casting", "hex.builtin.nodes.casting.int_to_buffer");
        ContentRegistry::DataProcessorNode::add<NodeCastBufferToInteger>("hex.builtin.nodes.casting", "hex.builtin.nodes.casting.buffer_to_int");

        ContentRegistry::DataProcessorNode::add<NodeIf>("hex.builtin.nodes.control_flow", "hex.builtin.nodes.control_flow.if");
        ContentRegistry::DataProcessorNode::add<NodeEquals>("hex.builtin.nodes.control_flow", "hex.builtin.nodes.control_flow.equals");
        ContentRegistry::DataProcessorNode::add<NodeNot>("hex.builtin.nodes.control_flow", "hex.builtin.nodes.control_flow.not");
        ContentRegistry::DataProcessorNode::add<NodeGreaterThan>("hex.builtin.nodes.control_flow", "hex.builtin.nodes.control_flow.gt");
        ContentRegistry::DataProcessorNode::add<NodeLessThan>("hex.builtin.nodes.control_flow", "hex.builtin.nodes.control_flow.lt");
        ContentRegistry::DataProcessorNode::add<NodeBoolAND>("hex.builtin.nodes.control_flow", "hex.builtin.nodes.control_flow.and");
        ContentRegistry::DataProcessorNode::add<NodeBoolOR>("hex.builtin.nodes.control_flow", "hex.builtin.nodes.control_flow.or");

        ContentRegistry::DataProcessorNode::add<NodeBitwiseAND>("hex.builtin.nodes.bitwise", "hex.builtin.nodes.bitwise.and");
        ContentRegistry::DataProcessorNode::add<NodeBitwiseOR>("hex.builtin.nodes.bitwise", "hex.builtin.nodes.bitwise.or");
        ContentRegistry::DataProcessorNode::add<NodeBitwiseXOR>("hex.builtin.nodes.bitwise", "hex.builtin.nodes.bitwise.xor");
        ContentRegistry::DataProcessorNode::add<NodeBitwiseNOT>("hex.builtin.nodes.bitwise", "hex.builtin.nodes.bitwise.not");

        ContentRegistry::DataProcessorNode::add<NodeDecodingBase64>("hex.builtin.nodes.decoding", "hex.builtin.nodes.decoding.base64");
        ContentRegistry::DataProcessorNode::add<NodeDecodingHex>("hex.builtin.nodes.decoding", "hex.builtin.nodes.decoding.hex");

        ContentRegistry::DataProcessorNode::add<NodeCryptoAESDecrypt>("hex.builtin.nodes.crypto", "hex.builtin.nodes.crypto.aes");
    }

}