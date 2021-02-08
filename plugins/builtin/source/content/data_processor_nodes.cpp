#include <hex/plugin.hpp>

#include <hex/helpers/crypto.hpp>

#include <cctype>

namespace hex::plugin::builtin {

    class NodeNullptr : public dp::Node {
    public:
        NodeNullptr() : Node("Nullptr", { dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "") }) {}

        void process() override {
            this->setBufferOnOutput(0, { });
        }
    };

    class NodeBuffer : public dp::Node {
    public:
        NodeBuffer() : Node("Buffer", { dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "") }) {}

        void drawNode() override {
            constexpr int StepSize = 1, FastStepSize = 10;

            ImGui::PushItemWidth(100);
            ImGui::InputScalar("Size", ImGuiDataType_U32, &this->m_size, &StepSize, &FastStepSize);
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
        NodeString() : Node("String", { dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "") }) {
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
        NodeInteger() : Node("Integer", { dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "") }) {}

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
        NodeFloat() : Node("Float", { dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Float, "") }) {}

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
        NodeRGBA8() : Node("RGBA8 Color", { dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "Red"),
                                                         dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "Green"),
                                                         dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "Blue"),
                                                         dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "Alpha")}) {}

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
        NodeComment() : Node("Comment", { }) {
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
        NodeDisplayInteger() : Node("Display Integer", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Value") }) {}

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
        NodeDisplayFloat() : Node("Display Float", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Float, "Value") }) {}

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
        NodeBitwiseNOT() : Node("Bitwise NOT", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "Input"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "Output") }) {}

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
        NodeBitwiseAND() : Node("Bitwise AND", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "Input A"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "Input B"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "Output") }) {}

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
        NodeBitwiseOR() : Node("Bitwise OR", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "Input A"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "Input B"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "Output") }) {}

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
        NodeBitwiseXOR() : Node("Bitwise XOR", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "Input A"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "Input B"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "Output") }) {}

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
        NodeReadData() : Node("Read Data", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Address"),
                                             dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Size"),
                                             dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "Data")
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
        NodeWriteData() : Node("Write Data", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Address"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "Data") }) {}

        void process() override {
            auto address = this->getIntegerOnInput(0);
            auto data = this->getBufferOnInput(1);

            this->setOverlayData(address, data);
        }
    };

    class NodeCastIntegerToBuffer : public dp::Node {
    public:
        NodeCastIntegerToBuffer() : Node("Integer to Buffer", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "In"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "Out") }) {}

        void process() override {
            auto input = this->getIntegerOnInput(0);

            std::vector<u8> output(sizeof(u64), 0x00);
            std::memcpy(output.data(), &input, sizeof(u64));

            this->setBufferOnOutput(1, output);
        }
    };

    class NodeCastBufferToInteger : public dp::Node {
    public:
        NodeCastBufferToInteger() : Node("Buffer to Integer", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "In"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "Out") }) {}

        void process() override {
            auto input = this->getBufferOnInput(0);

            u64 output;
            std::memcpy(&output, input.data(), sizeof(u64));

            this->setIntegerOnOutput(1, output);
        }
    };

    class NodeIf : public dp::Node {
    public:
        NodeIf() : Node("If", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Condition"),
                                             dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "True"),
                                             dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "False"),
                                             dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "Output") }) {}

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
        NodeEquals() : Node("Equals", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Input A"),
                                                     dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Input B"),
                                                     dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "Output") }) {}

        void process() override {
            auto inputA = this->getIntegerOnInput(0);
            auto inputB = this->getIntegerOnInput(1);

            this->setIntegerOnOutput(2, inputA == inputB);
        }
    };

    class NodeNot : public dp::Node {
    public:
        NodeNot() : Node("Not", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Input"),
                                     dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "Output") }) {}

        void process() override {
            auto input = this->getIntegerOnInput(0);

            this->setIntegerOnOutput(1, !input);
        }
    };

    class NodeGreaterThan : public dp::Node {
    public:
        NodeGreaterThan() : Node("Greater Than", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Input A"),
                                                                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Input B"),
                                                                dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "Output") }) {}

        void process() override {
            auto inputA = this->getIntegerOnInput(0);
            auto inputB = this->getIntegerOnInput(1);

            this->setIntegerOnOutput(2, inputA > inputB);
        }
    };

    class NodeLessThan : public dp::Node {
    public:
        NodeLessThan() : Node("Less Than", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Input A"),
                                                          dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Input B"),
                                                          dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "Output") }) {}

        void process() override {
            auto inputA = this->getIntegerOnInput(0);
            auto inputB = this->getIntegerOnInput(1);

            this->setIntegerOnOutput(2, inputA < inputB);
        }
    };

    class NodeBoolAND : public dp::Node {
    public:
        NodeBoolAND() : Node("Boolean AND", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Input A"),
                                                           dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Input B"),
                                                           dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "Output") }) {}

        void process() override {
            auto inputA = this->getIntegerOnInput(0);
            auto inputB = this->getIntegerOnInput(1);

            this->setIntegerOnOutput(2, inputA && inputB);
        }
    };

    class NodeBoolOR : public dp::Node {
    public:
        NodeBoolOR() : Node("Boolean OR", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Input A"),
                                                         dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "Input B"),
                                                         dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "Output") }) {}

        void process() override {
            auto inputA = this->getIntegerOnInput(0);
            auto inputB = this->getIntegerOnInput(1);

            this->setIntegerOnOutput(2, inputA || inputB);
        }
    };

    class NodeCryptoAESDecrypt : public dp::Node {
    public:
        NodeCryptoAESDecrypt() : Node("Decrypt AES", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "Key"),
                                                                    dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "IV"),
                                                                    dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "Nonce"),
                                                                    dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "Input"),
                                                                    dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "Output") }) {}

        void drawNode() override {
            ImGui::PushItemWidth(100);
            ImGui::Combo("Mode", &this->m_mode, "ECB\0CBC\0CFB128\0CTR\0GCM\0CCM\0OFB\0");
            ImGui::Combo("Key Length", &this->m_keyLength, "128 Bits\000192 Bits\000256 Bits\000");
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
        NodeDecodingBase64() : Node("Base64 Decoder", {
            dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "In"),
            dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "Out") }) {}

        void process() override {
            auto input = this->getBufferOnInput(0);

            auto output = crypt::decode64(input);

            this->setBufferOnOutput(1, output);
        }
    };

    class NodeDecodingHex : public dp::Node {
    public:
        NodeDecodingHex() : Node("Hexadecimal Decoder", {
                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "In"),
                dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "Out") }) {}

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
        ContentRegistry::DataProcessorNode::add<NodeInteger>("Constants", "Integer");
        ContentRegistry::DataProcessorNode::add<NodeFloat>("Constants", "Float");
        ContentRegistry::DataProcessorNode::add<NodeNullptr>("Constants", "Nullptr");
        ContentRegistry::DataProcessorNode::add<NodeBuffer>("Constants", "Buffer");
        ContentRegistry::DataProcessorNode::add<NodeString>("Constants", "String");
        ContentRegistry::DataProcessorNode::add<NodeRGBA8>("Constants", "RGBA8 Color");
        ContentRegistry::DataProcessorNode::add<NodeComment>("Constants", "Comment");

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

        ContentRegistry::DataProcessorNode::add<NodeDecodingBase64>("Decoding", "Base64");
        ContentRegistry::DataProcessorNode::add<NodeDecodingHex>("Decoding", "Hexadecimal");

        ContentRegistry::DataProcessorNode::add<NodeCryptoAESDecrypt>("Crypto", "AES");
    }

}