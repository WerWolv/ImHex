#include <hex/api/content_registry.hpp>
#include <hex/data_processor/node.hpp>

#include <hex/api/localization.hpp>
#include <hex/helpers/crypto.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/providers/provider.hpp>

#include <content/views/view_data_processor.hpp>

#include <content/helpers/provider_extra_data.hpp>
#include <content/helpers/diagrams.hpp>

#include <cctype>
#include <random>

#include <nlohmann/json.hpp>

#include <imgui.h>
#include <implot.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <wolv/utils/core.hpp>
#include <wolv/utils/guards.hpp>

namespace hex::plugin::builtin {

    class NodeNullptr : public dp::Node {
    public:
        NodeNullptr() : Node("hex.builtin.nodes.constants.nullptr.header", { dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "") }) { }

        void process() override {
            this->setBufferOnOutput(0, {});
        }
    };

    class NodeBuffer : public dp::Node {
    public:
        NodeBuffer() : Node("hex.builtin.nodes.constants.buffer.header", { dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "") }) { }

        void drawNode() override {
            constexpr static int StepSize = 1, FastStepSize = 10;

            ImGui::PushItemWidth(100_scaled);
            ImGui::InputScalar("hex.builtin.nodes.constants.buffer.size"_lang, ImGuiDataType_U32, &this->m_size, &StepSize, &FastStepSize);
            ImGui::PopItemWidth();
        }

        void process() override {
            if (this->m_buffer.size() != this->m_size)
                this->m_buffer.resize(this->m_size, 0x00);

            this->setBufferOnOutput(0, this->m_buffer);
        }

        void store(nlohmann::json &j) const override {
            j = nlohmann::json::object();

            j["size"] = this->m_size;
            j["data"] = this->m_buffer;
        }

        void load(const nlohmann::json &j) override {
            this->m_size   = j["size"];
            this->m_buffer = j["data"].get<std::vector<u8>>();
        }

    private:
        u32 m_size = 1;
        std::vector<u8> m_buffer;
    };

    class NodeString : public dp::Node {
    public:
        NodeString() : Node("hex.builtin.nodes.constants.string.header", { dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "") }) {

        }

        void drawNode() override {
            ImGui::InputTextMultiline("##string", this->m_value, ImVec2(150_scaled, 0), ImGuiInputTextFlags_AllowTabInput);
        }

        void process() override {
            this->setBufferOnOutput(0, hex::decodeByteString(this->m_value));
        }

        void store(nlohmann::json &j) const override {
            j = nlohmann::json::object();

            j["data"] = this->m_value;
        }

        void load(const nlohmann::json &j) override {
            this->m_value = j["data"].get<std::string>();
        }

    private:
        std::string m_value;
    };

    class NodeInteger : public dp::Node {
    public:
        NodeInteger() : Node("hex.builtin.nodes.constants.int.header", { dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "") }) { }

        void drawNode() override {
            ImGui::PushItemWidth(100_scaled);
            ImGui::InputHexadecimal("##integer_value", &this->m_value);
            ImGui::PopItemWidth();
        }

        void process() override {
            this->setIntegerOnOutput(0, this->m_value);
        }

        void store(nlohmann::json &j) const override {
            j = nlohmann::json::object();

            j["data"] = this->m_value;
        }

        void load(const nlohmann::json &j) override {
            this->m_value = j["data"];
        }

    private:
        u64 m_value = 0;
    };

    class NodeFloat : public dp::Node {
    public:
        NodeFloat() : Node("hex.builtin.nodes.constants.float.header", { dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Float, "") }) { }

        void drawNode() override {
            ImGui::PushItemWidth(100_scaled);
            ImGui::InputScalar("##floatValue", ImGuiDataType_Float, &this->m_value, nullptr, nullptr, "%f", ImGuiInputTextFlags_CharsDecimal);
            ImGui::PopItemWidth();
        }

        void process() override {
            this->setFloatOnOutput(0, this->m_value);
        }

        void store(nlohmann::json &j) const override {
            j = nlohmann::json::object();

            j["data"] = this->m_value;
        }

        void load(const nlohmann::json &j) override {
            this->m_value = j["data"];
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
                              dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.constants.rgba8.output.a") }) { }

        void drawNode() override {
            ImGui::PushItemWidth(200_scaled);
            ImGui::ColorPicker4("##colorPicker", &this->m_color.Value.x, ImGuiColorEditFlags_AlphaBar);
            ImGui::PopItemWidth();
        }

        void process() override {
            this->setBufferOnOutput(0, wolv::util::toBytes<u8>(u8(this->m_color.Value.x * 0xFF)));
            this->setBufferOnOutput(1, wolv::util::toBytes<u8>(u8(this->m_color.Value.y * 0xFF)));
            this->setBufferOnOutput(2, wolv::util::toBytes<u8>(u8(this->m_color.Value.z * 0xFF)));
            this->setBufferOnOutput(3, wolv::util::toBytes<u8>(u8(this->m_color.Value.w * 0xFF)));
        }

        void store(nlohmann::json &j) const override {
            j = nlohmann::json::object();

            j["data"]      = nlohmann::json::object();
            j["data"]["r"] = this->m_color.Value.x;
            j["data"]["g"] = this->m_color.Value.y;
            j["data"]["b"] = this->m_color.Value.z;
            j["data"]["a"] = this->m_color.Value.w;
        }

        void load(const nlohmann::json &j) override {
            this->m_color = ImVec4(j["data"]["r"], j["data"]["g"], j["data"]["b"], j["data"]["a"]);
        }

    private:
        ImColor m_color;
    };

    class NodeComment : public dp::Node {
    public:
        NodeComment() : Node("hex.builtin.nodes.constants.comment.header", {}) {

        }

        void drawNode() override {
            ImGui::InputTextMultiline("##string", this->m_comment, scaled(ImVec2(150, 100)));
        }

        void process() override {
        }

        void store(nlohmann::json &j) const override {
            j = nlohmann::json::object();

            j["comment"] = this->m_comment;
        }

        void load(const nlohmann::json &j) override {
            this->m_comment = j["comment"].get<std::string>();
        }

    private:
        std::string m_comment;
    };


    class NodeDisplayInteger : public dp::Node {
    public:
        NodeDisplayInteger() : Node("hex.builtin.nodes.display.int.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input") }) { }

        void drawNode() override {
            ImGui::PushItemWidth(150_scaled);
            if (this->m_value.has_value())
                ImGui::TextFormatted("0x{0:X}", this->m_value.value());
            else
                ImGui::TextUnformatted("???");
            ImGui::PopItemWidth();
        }

        void process() override {
            this->m_value.reset();
            const auto &input = this->getIntegerOnInput(0);

            this->m_value = input;
        }

    private:
        std::optional<u64> m_value;
    };

    class NodeDisplayFloat : public dp::Node {
    public:
        NodeDisplayFloat() : Node("hex.builtin.nodes.display.float.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Float, "hex.builtin.nodes.common.input") }) { }

        void drawNode() override {
            ImGui::PushItemWidth(150_scaled);
            if (this->m_value.has_value())
                ImGui::TextFormatted("{0}", this->m_value.value());
            else
                ImGui::TextUnformatted("???");
            ImGui::PopItemWidth();
        }

        void process() override {
            this->m_value.reset();
            const auto &input = this->getFloatOnInput(0);

            this->m_value = input;
        }

    private:
        std::optional<float> m_value;
    };

    class NodeDisplayBuffer : public dp::Node {
    public:
        NodeDisplayBuffer() : Node("hex.builtin.nodes.display.buffer.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input") }) { }

        void drawNode() override {
            static const std::string Header = " Address    00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F                       ";

            if (ImGui::BeginChild("##hex_view", scaled(ImVec2(ImGui::CalcTextSize(Header.c_str()).x, 200)), true)) {
                ImGui::TextUnformatted(Header.c_str());

                auto size = this->m_buffer.size();
                ImGuiListClipper clipper;

                clipper.Begin((size + 0x0F) / 0x10);

                while (clipper.Step())
                    for (auto y = clipper.DisplayStart; y < clipper.DisplayEnd; y++) {
                        auto lineSize = ((size - y * 0x10) < 0x10) ? size % 0x10 : 0x10;

                        std::string line = hex::format(" {:08X}:  ", y * 0x10);
                        for (u32 x = 0; x < 0x10; x++) {
                            if (x < lineSize)
                                line += hex::format("{:02X} ", this->m_buffer[y * 0x10 + x]);
                            else
                                line += "   ";

                            if (x == 7) line += " ";
                        }

                        line += "   ";

                        for (u32 x = 0; x < lineSize; x++) {
                            auto c = char(this->m_buffer[y * 0x10 + x]);
                            if (std::isprint(c))
                                line += c;
                            else
                                line += ".";
                        }

                        ImGui::TextUnformatted(line.c_str());
                    }
                clipper.End();
            }
            ImGui::EndChild();
        }

        void process() override {
            this->m_buffer = this->getBufferOnInput(0);
        }

    private:
        std::vector<u8> m_buffer;
    };

    class NodeDisplayString : public dp::Node {
    public:
        NodeDisplayString() : Node("hex.builtin.nodes.display.string.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input") }) { }

        void drawNode() override {
            constexpr static auto LineLength = 50;
            if (ImGui::BeginChild("##string_view", scaled(ImVec2(ImGui::CalcTextSize(" ").x * (LineLength + 4), 150)), true)) {
                std::string_view string = this->m_value;

                ImGuiListClipper clipper;
                clipper.Begin((string.length() + (LineLength - 1)) / LineLength);

                while (clipper.Step())
                    for (auto i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        auto line = string.substr(i * LineLength, LineLength);
                        ImGui::TextUnformatted("");
                        ImGui::SameLine();
                        ImGui::TextUnformatted(line.data(), line.data() + line.length());
                    }

                clipper.End();
            }
            ImGui::EndChild();
        }

        void process() override {
            const auto &input = this->getBufferOnInput(0);

            this->m_value = hex::encodeByteString(input);
        }

    private:
        std::string m_value;
    };


    class NodeBitwiseNOT : public dp::Node {
    public:
        NodeBitwiseNOT() : Node("hex.builtin.nodes.bitwise.not.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &input = this->getBufferOnInput(0);

            std::vector<u8> output = input;
            for (auto &byte : output)
                byte = ~byte;

            this->setBufferOnOutput(1, output);
        }
    };

    class NodeBitwiseADD : public dp::Node {
    public:
        NodeBitwiseADD() : Node("hex.builtin.nodes.bitwise.add.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input.a"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input.b"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output") }) { }

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
        NodeBitwiseAND() : Node("hex.builtin.nodes.bitwise.and.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input.a"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input.b"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output") }) { }

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
        NodeBitwiseOR() : Node("hex.builtin.nodes.bitwise.or.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input.a"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input.b"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output") }) { }

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
        NodeBitwiseXOR() : Node("hex.builtin.nodes.bitwise.xor.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input.a"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input.b"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &inputA = this->getBufferOnInput(0);
            const auto &inputB = this->getBufferOnInput(1);

            std::vector<u8> output(std::min(inputA.size(), inputB.size()), 0x00);

            for (u32 i = 0; i < output.size(); i++)
                output[i] = inputA[i] ^ inputB[i];

            this->setBufferOnOutput(2, output);
        }
    };

    class NodeReadData : public dp::Node {
    public:
        NodeReadData() : Node("hex.builtin.nodes.data_access.read.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.data_access.read.address"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.data_access.read.size"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.data_access.read.data") }) { }

        void process() override {
            const auto &address = this->getIntegerOnInput(0);
            const auto &size    = this->getIntegerOnInput(1);

            std::vector<u8> data;
            data.resize(size);

            ImHexApi::Provider::get()->readRaw(address, data.data(), size);

            this->setBufferOnOutput(2, data);
        }
    };

    class NodeWriteData : public dp::Node {
    public:
        NodeWriteData() : Node("hex.builtin.nodes.data_access.write.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.data_access.write.address"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.data_access.write.data") }) { }

        void process() override {
            const auto &address = this->getIntegerOnInput(0);
            const auto &data    = this->getBufferOnInput(1);

            this->setOverlayData(address, data);
        }
    };

    class NodeDataSize : public dp::Node {
    public:
        NodeDataSize() : Node("hex.builtin.nodes.data_access.size.header", { dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.data_access.size.size") }) { }

        void process() override {
            auto size = ImHexApi::Provider::get()->getActualSize();

            this->setIntegerOnOutput(0, size);
        }
    };

    class NodeDataSelection : public dp::Node {
    public:
        NodeDataSelection() : Node("hex.builtin.nodes.data_access.selection.header", { dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.data_access.selection.address"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.data_access.selection.size") }) {
            EventManager::subscribe<EventRegionSelected>(this, [this](const auto &region) {
                this->m_address = region.address;
                this->m_size    = region.size;
            });
        }

        ~NodeDataSelection() override {
            EventManager::unsubscribe<EventRegionSelected>(this);
        }

        void process() override {
            this->setIntegerOnOutput(0, this->m_address);
            this->setIntegerOnOutput(1, this->m_size);
        }

    private:
        u64 m_address = 0;
        size_t m_size = 0;
    };

    class NodeCastIntegerToBuffer : public dp::Node {
    public:
        NodeCastIntegerToBuffer() : Node("hex.builtin.nodes.casting.int_to_buffer.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &input = this->getIntegerOnInput(0);

            std::vector<u8> output(sizeof(input), 0x00);
            std::memcpy(output.data(), &input, sizeof(input));

            this->setBufferOnOutput(1, output);
        }
    };

    class NodeCastBufferToInteger : public dp::Node {
    public:
        NodeCastBufferToInteger() : Node("hex.builtin.nodes.casting.buffer_to_int.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &input = this->getBufferOnInput(0);

            i64 output = 0;
            if (input.empty() || input.size() > sizeof(output))
                throwNodeError("Buffer is empty or bigger than 64 bits");

            std::memcpy(&output, input.data(), input.size());

            this->setIntegerOnOutput(1, output);
        }
    };

    class NodeCastFloatToBuffer : public dp::Node {
    public:
        NodeCastFloatToBuffer() : Node("hex.builtin.nodes.casting.float_to_buffer.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Float, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &input = this->getFloatOnInput(0);

            std::vector<u8> output(sizeof(input), 0x00);
            std::memcpy(output.data(), &input, sizeof(input));

            this->setBufferOnOutput(1, output);
        }
    };

    class NodeCastBufferToFloat : public dp::Node {
    public:
        NodeCastBufferToFloat() : Node("hex.builtin.nodes.casting.buffer_to_float.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Float, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &input = this->getBufferOnInput(0);

            float output = 0;
            if (input.empty() || input.size() != sizeof(output))
                throwNodeError("Buffer is empty or not the right size to fit a float");

            std::memcpy(&output, input.data(), input.size());

            this->setFloatOnOutput(1, output);
        }
    };

    class NodeArithmeticAdd : public dp::Node {
    public:
        NodeArithmeticAdd() : Node("hex.builtin.nodes.arithmetic.add.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.a"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.b"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &inputA = this->getIntegerOnInput(0);
            const auto &inputB = this->getIntegerOnInput(1);

            auto output = inputA + inputB;

            this->setIntegerOnOutput(2, output);
        }
    };

    class NodeArithmeticSubtract : public dp::Node {
    public:
        NodeArithmeticSubtract() : Node("hex.builtin.nodes.arithmetic.sub.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.a"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.b"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &inputA = this->getIntegerOnInput(0);
            const auto &inputB = this->getIntegerOnInput(1);

            auto output = inputA - inputB;

            this->setIntegerOnOutput(2, output);
        }
    };

    class NodeArithmeticMultiply : public dp::Node {
    public:
        NodeArithmeticMultiply() : Node("hex.builtin.nodes.arithmetic.mul.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.a"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.b"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &inputA = this->getIntegerOnInput(0);
            const auto &inputB = this->getIntegerOnInput(1);

            auto output = inputA * inputB;

            this->setIntegerOnOutput(2, output);
        }
    };

    class NodeArithmeticDivide : public dp::Node {
    public:
        NodeArithmeticDivide() : Node("hex.builtin.nodes.arithmetic.div.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.a"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.b"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &inputA = this->getIntegerOnInput(0);
            const auto &inputB = this->getIntegerOnInput(1);

            if (inputB == 0)
                throwNodeError("Division by zero");

            auto output = inputA / inputB;

            this->setIntegerOnOutput(2, output);
        }
    };

    class NodeArithmeticModulus : public dp::Node {
    public:
        NodeArithmeticModulus() : Node("hex.builtin.nodes.arithmetic.mod.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.a"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.b"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &inputA = this->getIntegerOnInput(0);
            const auto &inputB = this->getIntegerOnInput(1);

            if (inputB == 0)
                throwNodeError("Division by zero");

            auto output = inputA % inputB;

            this->setIntegerOnOutput(2, output);
        }
    };

    class NodeArithmeticAverage : public dp::Node {
    public:
        NodeArithmeticAverage() : Node("hex.builtin.nodes.arithmetic.average.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &input = this->getBufferOnInput(0);

            double output = std::reduce(input.begin(), input.end(), double(0)) / double(input.size());

            this->setFloatOnOutput(1, output);
        }
    };

    class NodeArithmeticMedian : public dp::Node {
    public:
        NodeArithmeticMedian() : Node("hex.builtin.nodes.arithmetic.median.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            auto input = this->getBufferOnInput(0);

            u64 medianIndex = input.size() / 2;
            std::nth_element(input.begin(), input.begin() + medianIndex, input.end());
            i128 median = 0;

            if (input.size() % 2 == 0) {
                std::nth_element(input.begin(), input.begin() + medianIndex - 1, input.end());
                median = (input[medianIndex] + input[medianIndex - 1]) / 2;
            } else {
                median = input[medianIndex];
            }

            this->setIntegerOnOutput(1, median);
        }
    };

    class NodeArithmeticCeil : public dp::Node {
    public:
        NodeArithmeticCeil() : Node("hex.builtin.nodes.arithmetic.ceil.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Float, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Float, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &input = this->getFloatOnInput(0);

            this->setIntegerOnOutput(1, std::ceil(input));
        }
    };

    class NodeArithmeticFloor : public dp::Node {
    public:
        NodeArithmeticFloor() : Node("hex.builtin.nodes.arithmetic.floor.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Float, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Float, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &input = this->getFloatOnInput(0);

            this->setIntegerOnOutput(1, std::floor(input));
        }
    };

    class NodeArithmeticRound : public dp::Node {
    public:
        NodeArithmeticRound() : Node("hex.builtin.nodes.arithmetic.round.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Float, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Float, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &input = this->getFloatOnInput(0);

            this->setIntegerOnOutput(1, std::round(input));
        }
    };

    class NodeBufferCombine : public dp::Node {
    public:
        NodeBufferCombine() : Node("hex.builtin.nodes.buffer.combine.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input.a"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input.b"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &inputA = this->getBufferOnInput(0);
            const auto &inputB = this->getBufferOnInput(1);

            auto output = inputA;
            std::copy(inputB.begin(), inputB.end(), std::back_inserter(output));

            this->setBufferOnOutput(2, output);
        }
    };

    class NodeBufferSlice : public dp::Node {
    public:
        NodeBufferSlice() : Node("hex.builtin.nodes.buffer.slice.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.buffer.slice.input.buffer"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.buffer.slice.input.from"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.buffer.slice.input.to"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &input = this->getBufferOnInput(0);
            const auto &from  = this->getIntegerOnInput(1);
            const auto &to    = this->getIntegerOnInput(2);

            if (from < 0 || static_cast<u128>(from) >= input.size())
                throwNodeError("'from' input out of range");
            if (to < 0 || static_cast<u128>(to) >= input.size())
                throwNodeError("'to' input out of range");
            if (to <= from)
                throwNodeError("'to' input needs to be greater than 'from' input");

            this->setBufferOnOutput(3, std::vector(input.begin() + u64(from), input.begin() + u64(to)));
        }
    };

    class NodeBufferRepeat : public dp::Node {
    public:
        NodeBufferRepeat() : Node("hex.builtin.nodes.buffer.repeat.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.buffer.repeat.input.buffer"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.buffer.repeat.input.count"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &buffer = this->getBufferOnInput(0);
            const auto &count  = this->getIntegerOnInput(1);

            std::vector<u8> output;
            output.resize(buffer.size() * count);

            for (u32 i = 0; i < count; i++)
                std::copy(buffer.begin(), buffer.end(), output.begin() + buffer.size() * i);

            this->setBufferOnOutput(2, output);
        }
    };

    class NodeBufferPatch : public dp::Node {
    public:
        NodeBufferPatch() : Node("hex.builtin.nodes.buffer.patch.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.buffer.patch.input.patch"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.common.address"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            auto buffer     = this->getBufferOnInput(0);
            const auto &patch      = this->getBufferOnInput(1);
            const auto &address    = this->getIntegerOnInput(2);

            if (address < 0 || static_cast<u128>(address) >= buffer.size())
                throwNodeError("Address out of range");

            if (address + patch.size() > buffer.size())
                buffer.resize(address + patch.size());

            std::copy(patch.begin(), patch.end(), buffer.begin() + address);

            this->setBufferOnOutput(3, buffer);
        }
    };

    class NodeBufferSize : public dp::Node {
    public:
        NodeBufferSize() : Node("hex.builtin.nodes.buffer.size.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.buffer.size.output") }) { }

        void process() override {
            const auto &buffer = this->getBufferOnInput(0);

            this->setIntegerOnOutput(1, buffer.size());
        }
    };

    class NodeIf : public dp::Node {
    public:
        NodeIf() : Node("hex.builtin.nodes.control_flow.if.header",
                       { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.control_flow.if.condition"),
                           dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.control_flow.if.true"),
                           dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.control_flow.if.false"),
                           dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &cond      = this->getIntegerOnInput(0);
            const auto &trueData  = this->getBufferOnInput(1);
            const auto &falseData = this->getBufferOnInput(2);

            if (cond != 0)
                this->setBufferOnOutput(3, trueData);
            else
                this->setBufferOnOutput(3, falseData);
        }
    };

    class NodeEquals : public dp::Node {
    public:
        NodeEquals() : Node("hex.builtin.nodes.control_flow.equals.header",
                           { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.a"),
                               dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.b"),
                               dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &inputA = this->getIntegerOnInput(0);
            const auto &inputB = this->getIntegerOnInput(1);

            this->setIntegerOnOutput(2, inputA == inputB);
        }
    };

    class NodeNot : public dp::Node {
    public:
        NodeNot() : Node("hex.builtin.nodes.control_flow.not.header",
                        { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input"),
                            dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &input = this->getIntegerOnInput(0);

            this->setIntegerOnOutput(1, !input);
        }
    };

    class NodeGreaterThan : public dp::Node {
    public:
        NodeGreaterThan() : Node("hex.builtin.nodes.control_flow.gt.header",
                                { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.a"),
                                    dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.b"),
                                    dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &inputA = this->getIntegerOnInput(0);
            const auto &inputB = this->getIntegerOnInput(1);

            this->setIntegerOnOutput(2, inputA > inputB);
        }
    };

    class NodeLessThan : public dp::Node {
    public:
        NodeLessThan() : Node("hex.builtin.nodes.control_flow.lt.header",
                             { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.a"),
                                 dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.b"),
                                 dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &inputA = this->getIntegerOnInput(0);
            const auto &inputB = this->getIntegerOnInput(1);

            this->setIntegerOnOutput(2, inputA < inputB);
        }
    };

    class NodeBoolAND : public dp::Node {
    public:
        NodeBoolAND() : Node("hex.builtin.nodes.control_flow.and.header",
                            { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.a"),
                                dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.b"),
                                dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &inputA = this->getIntegerOnInput(0);
            const auto &inputB = this->getIntegerOnInput(1);

            this->setIntegerOnOutput(2, inputA && inputB);
        }
    };

    class NodeBoolOR : public dp::Node {
    public:
        NodeBoolOR() : Node("hex.builtin.nodes.control_flow.or.header",
                           { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.a"),
                               dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input.b"),
                               dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &inputA = this->getIntegerOnInput(0);
            const auto &inputB = this->getIntegerOnInput(1);

            this->setIntegerOnOutput(2, inputA || inputB);
        }
    };

    class NodeCryptoAESDecrypt : public dp::Node {
    public:
        NodeCryptoAESDecrypt() : Node("hex.builtin.nodes.crypto.aes.header",
                                     { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.crypto.aes.key"),
                                         dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.crypto.aes.iv"),
                                         dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.crypto.aes.nonce"),
                                         dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"),
                                         dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output") }) { }

        void drawNode() override {
            ImGui::PushItemWidth(100_scaled);
            ImGui::Combo("hex.builtin.nodes.crypto.aes.mode"_lang, &this->m_mode, "ECB\0CBC\0CFB128\0CTR\0GCM\0CCM\0OFB\0");
            ImGui::Combo("hex.builtin.nodes.crypto.aes.key_length"_lang, &this->m_keyLength, "128 Bits\000192 Bits\000256 Bits\000");
            ImGui::PopItemWidth();
        }

        void process() override {
            const auto &key   = this->getBufferOnInput(0);
            const auto &iv    = this->getBufferOnInput(1);
            const auto &nonce = this->getBufferOnInput(2);
            const auto &input = this->getBufferOnInput(3);

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

        void store(nlohmann::json &j) const override {
            j = nlohmann::json::object();

            j["data"]               = nlohmann::json::object();
            j["data"]["mode"]       = this->m_mode;
            j["data"]["key_length"] = this->m_keyLength;
        }

        void load(const nlohmann::json &j) override {
            this->m_mode      = j["data"]["mode"];
            this->m_keyLength = j["data"]["key_length"];
        }

    private:
        int m_mode      = 0;
        int m_keyLength = 0;
    };

    class NodeDecodingBase64 : public dp::Node {
    public:
        NodeDecodingBase64() : Node("hex.builtin.nodes.decoding.base64.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &input = this->getBufferOnInput(0);

            auto output = crypt::decode64(input);

            this->setBufferOnOutput(1, output);
        }
    };

    class NodeDecodingHex : public dp::Node {
    public:
        NodeDecodingHex() : Node("hex.builtin.nodes.decoding.hex.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            auto input = this->getBufferOnInput(0);

            input.erase(std::remove_if(input.begin(), input.end(), [](u8 c) { return std::isspace(c); }), input.end());

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

    class NodeVisualizerDigram : public dp::Node {
    public:
        NodeVisualizerDigram() : Node("hex.builtin.nodes.visualizer.digram.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input") }) { }

        void drawNode() override {
            this->m_digram.draw(scaled({ 200, 200 }));

            if (ImGui::IsItemHovered() && ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
                ImGui::BeginTooltip();
                this->m_digram.draw(scaled({ 600, 600 }));
                ImGui::EndTooltip();
            }
        }

        void process() override {
            this->m_digram.process(this->getBufferOnInput(0));
        }

    private:
        DiagramDigram m_digram;
    };

    class NodeVisualizerLayeredDistribution : public dp::Node {
    public:
        NodeVisualizerLayeredDistribution() : Node("hex.builtin.nodes.visualizer.layered_dist.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input") }) { }

        void drawNode() override {
            this->m_layeredDistribution.draw(scaled({ 200, 200 }));
            if (ImGui::IsItemHovered() && ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
                ImGui::BeginTooltip();
                this->m_layeredDistribution.draw(scaled({ 600, 600 }));
                ImGui::EndTooltip();
            }
        }

        void process() override {
            this->m_layeredDistribution.process(this->getBufferOnInput(0));
        }

    private:
        DiagramLayeredDistribution m_layeredDistribution;
    };

    class NodeVisualizerImage : public dp::Node {
    public:
        NodeVisualizerImage() : Node("hex.builtin.nodes.visualizer.image.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input") }) { }

        void drawNode() override {
            ImGui::Image(this->m_texture, scaled(ImVec2(this->m_texture.getAspectRatio() * 200, 200)));
            if (ImGui::IsItemHovered() && ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
                ImGui::BeginTooltip();
                ImGui::Image(this->m_texture, scaled(ImVec2(this->m_texture.getAspectRatio() * 600, 600)));
                ImGui::EndTooltip();
            }
        }

        void process() override {
            const auto &rawData = this->getBufferOnInput(0);

            this->m_texture = ImGui::Texture(rawData.data(), rawData.size());
        }

    private:
        ImGui::Texture m_texture;
    };

    class NodeVisualizerImageRGBA : public dp::Node {
    public:
        NodeVisualizerImageRGBA() : Node("hex.builtin.nodes.visualizer.image_rgba.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.width"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.height") }) { }

        void drawNode() override {
            ImGui::Image(this->m_texture, scaled(ImVec2(this->m_texture.getAspectRatio() * 200, 200)));
            if (ImGui::IsItemHovered() && ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
                ImGui::BeginTooltip();
                ImGui::Image(this->m_texture, scaled(ImVec2(this->m_texture.getAspectRatio() * 600, 600)));
                ImGui::EndTooltip();
            }
        }

        void process() override {
            this->m_texture = { };

            const auto &rawData = this->getBufferOnInput(0);
            const auto &width = this->getIntegerOnInput(1);
            const auto &height = this->getIntegerOnInput(2);

            const size_t requiredBytes = width * height * 4;
            if (requiredBytes > rawData.size())
                throwNodeError(hex::format("Image requires at least {} bytes of data, but only {} bytes are available", requiredBytes, rawData.size()));

            this->m_texture = ImGui::Texture(rawData.data(), rawData.size(), width, height);
        }

    private:
        ImGui::Texture m_texture;
    };

    class NodeVisualizerByteDistribution : public dp::Node {
    public:
        NodeVisualizerByteDistribution() : Node("hex.builtin.nodes.visualizer.byte_distribution.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input") }) { }

        void drawNode() override {
            drawPlot(scaled({ 400, 300 }));

            if (ImGui::IsItemHovered() && ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
                ImGui::BeginTooltip();
                drawPlot(scaled({ 700, 550 }));
                ImGui::EndTooltip();
            }
        }

        void drawPlot(const ImVec2 &viewSize) {
            if (ImPlot::BeginPlot("##distribution", viewSize, ImPlotFlags_NoLegend | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect)) {
                ImPlot::SetupAxes("Address", "Count", ImPlotAxisFlags_Lock, ImPlotAxisFlags_Lock | ImPlotAxisFlags_LogScale);
                ImPlot::SetupAxesLimits(0, 256, 1, double(*std::max_element(this->m_counts.begin(), this->m_counts.end())) * 1.1F, ImGuiCond_Always);

                static auto x = [] {
                    std::array<ImU64, 256> result { 0 };
                    std::iota(result.begin(), result.end(), 0);
                    return result;
                }();


                ImPlot::PlotBars<ImU64>("##bytes", x.data(), this->m_counts.data(), x.size(), 1);

                ImPlot::EndPlot();
            }
        }

        void process() override {
            const auto &buffer = this->getBufferOnInput(0);

            this->m_counts.fill(0x00);
            for (const auto &byte : buffer) {
                this->m_counts[byte]++;
            }
        }

    private:
        std::array<ImU64, 256> m_counts = { 0 };
    };


    class NodePatternLanguageOutVariable : public dp::Node {
    public:
        NodePatternLanguageOutVariable() : Node("hex.builtin.nodes.pattern_language.out_var.header", { dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output") }) { }

        void drawNode() override {
            ImGui::PushItemWidth(100_scaled);
            ImGui::InputText("##name", this->m_name);
            ImGui::PopItemWidth();
        }

        void process() override {
            auto &pl = ProviderExtraData::getCurrent().patternLanguage;

            std::scoped_lock lock(pl.runtimeMutex);
            const auto &outVars = pl.runtime->getOutVariables();

            if (outVars.contains(this->m_name)) {
                std::visit(wolv::util::overloaded {
                    [](const std::string &) {},
                    [](pl::ptrn::Pattern *) {},
                    [this](auto &&value) {
                        std::vector<u8> buffer(std::min<size_t>(sizeof(value), 8));
                        std::memcpy(buffer.data(), &value, buffer.size());

                        this->setBufferOnOutput(0, buffer);
                    }
                }, outVars.at(this->m_name));
            } else {
                throwNodeError(hex::format("Out variable '{}' has not been defined!", this->m_name));
            }
        }

        void store(nlohmann::json &j) const override {
            j = nlohmann::json::object();

            j["name"] = this->m_name;
        }

        void load(const nlohmann::json &j) override {
            this->m_name = j["name"].get<std::string>();
        }

    private:
        std::string m_name;
    };

    class NodeCustomInput : public dp::Node {
    public:
        NodeCustomInput() : Node("hex.builtin.nodes.custom.input.header", { dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input") }) { }
        ~NodeCustomInput() override = default;

        void drawNode() override {
            ImGui::PushItemWidth(100_scaled);
            if (ImGui::Combo("##type", &this->m_type, "Integer\0Float\0Buffer\0")) {
                this->setAttributes({
                    { dp::Attribute(dp::Attribute::IOType::Out, this->getType(), "hex.builtin.nodes.common.input") }
                });
            }

            if (ImGui::InputText("##name", this->m_name)) {
                this->setUnlocalizedTitle(this->m_name);
            }

            ImGui::PopItemWidth();
        }

        void setValue(auto value) { this->m_value = std::move(value); }

        const std::string &getName() const { return this->m_name; }
        dp::Attribute::Type getType() const {
            switch (this->m_type) {
                default:
                case 0: return dp::Attribute::Type::Integer;
                case 1: return dp::Attribute::Type::Float;
                case 2: return dp::Attribute::Type::Buffer;
            }
        }

        void process() override {
            std::visit(wolv::util::overloaded {
                [this](i128 value) { this->setIntegerOnOutput(0, value); },
                [this](long double value) { this->setFloatOnOutput(0, value); },
                [this](const std::vector<u8> &value) { this->setBufferOnOutput(0, value); }
            }, this->m_value);
        }

        void store(nlohmann::json &j) const override {
            j = nlohmann::json::object();

            j["name"] = this->m_name;
            j["type"] = this->m_type;
        }

        void load(const nlohmann::json &j) override {
            this->m_name = j["name"].get<std::string>();
            this->m_type = j["type"];

            this->setUnlocalizedTitle(this->m_name);
            this->setAttributes({
                { dp::Attribute(dp::Attribute::IOType::Out, this->getType(), "hex.builtin.nodes.common.input") }
            });
        }

    private:
        std::string m_name = LangEntry(this->getUnlocalizedName());
        int m_type = 0;

        std::variant<i128, long double, std::vector<u8>> m_value;
    };

    class NodeCustomOutput : public dp::Node {
    public:
        NodeCustomOutput() : Node("hex.builtin.nodes.custom.output.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }
        ~NodeCustomOutput() override = default;

        void drawNode() override {
            ImGui::PushItemWidth(100_scaled);
            if (ImGui::Combo("##type", &this->m_type, "Integer\0Float\0Buffer\0")) {
                this->setAttributes({
                    { dp::Attribute(dp::Attribute::IOType::In, this->getType(), "hex.builtin.nodes.common.output") }
                });
            }

            if (ImGui::InputText("##name", this->m_name)) {
                this->setUnlocalizedTitle(this->m_name);
            }

            ImGui::PopItemWidth();
        }

        const std::string &getName() const { return this->m_name; }
        dp::Attribute::Type getType() const {
            switch (this->m_type) {
                case 0: return dp::Attribute::Type::Integer;
                case 1: return dp::Attribute::Type::Float;
                case 2: return dp::Attribute::Type::Buffer;
                default: return dp::Attribute::Type::Integer;
            }
        }

        void process() override {
            switch (this->getType()) {
                case dp::Attribute::Type::Integer: this->m_value = this->getIntegerOnInput(0); break;
                case dp::Attribute::Type::Float:   this->m_value = this->getFloatOnInput(0); break;
                case dp::Attribute::Type::Buffer:  this->m_value = this->getBufferOnInput(0); break;
            }
        }

        const auto& getValue() const { return this->m_value; }

        void store(nlohmann::json &j) const override {
            j = nlohmann::json::object();

            j["name"] = this->m_name;
            j["type"] = this->m_type;
        }

        void load(const nlohmann::json &j) override {
            this->m_name = j["name"].get<std::string>();
            this->m_type = j["type"];

            this->setUnlocalizedTitle(this->m_name);
            this->setAttributes({
                { dp::Attribute(dp::Attribute::IOType::In, this->getType(), "hex.builtin.nodes.common.output") }
            });
        }

    private:
        std::string m_name = LangEntry(this->getUnlocalizedName());
        int m_type = 0;

        std::variant<i128, long double, std::vector<u8>> m_value;
    };

    class NodeCustom : public dp::Node {
    public:
        NodeCustom() : Node("hex.builtin.nodes.custom.custom.header", {}) { }
        ~NodeCustom() override = default;

        void drawNode() override {
            if (this->m_requiresAttributeUpdate) {
                this->m_requiresAttributeUpdate = false;
                this->setAttributes(this->findAttributes());
            }

            ImGui::PushItemWidth(200_scaled);

            bool editing = false;
            if (this->m_editable) {
                ImGui::InputTextIcon("##name", ICON_VS_SYMBOL_KEY, this->m_name);
                editing = ImGui::IsItemActive();

                if (ImGui::Button("hex.builtin.nodes.custom.custom.edit"_lang, ImVec2(200_scaled, ImGui::GetTextLineHeightWithSpacing()))) {
                    auto &data = ProviderExtraData::getCurrent().dataProcessor;
                    data.workspaceStack.push_back(&this->m_workspace);

                    this->m_requiresAttributeUpdate = true;
                }
            } else {
                this->setUnlocalizedTitle(this->m_name);

                if (this->getAttributes().empty()) {
                    ImGui::TextUnformatted("hex.builtin.nodes.custom.custom.edit_hint"_lang);
                }
            }

            this->m_editable = ImGui::GetIO().KeyShift || editing;

            ImGui::PopItemWidth();
        }

        void process() override {
            auto indexFromId = [this](u32 id) -> std::optional<u32> {
                const auto &attributes = this->getAttributes();
                for (u32 i = 0; i < attributes.size(); i++)
                    if (u32(attributes[i].getId()) == id)
                        return i;
                return std::nullopt;
            };

            auto prevContext = ImNodes::GetCurrentContext();
            ImNodes::SetCurrentContext(this->m_workspace.context.get());
            ON_SCOPE_EXIT { ImNodes::SetCurrentContext(prevContext); };

            // Forward inputs to input nodes values
            for (auto &attribute : this->getAttributes()) {
                auto index = indexFromId(attribute.getId());
                if (!index.has_value())
                    continue;

                if (auto input = this->findInput(attribute.getUnlocalizedName()); input != nullptr) {
                    switch (attribute.getType()) {
                        case dp::Attribute::Type::Integer: {
                            const auto &value = this->getIntegerOnInput(*index);
                            input->setValue(value);
                            break;
                        }
                        case dp::Attribute::Type::Float: {
                            const auto &value = this->getFloatOnInput(*index);
                            input->setValue(value);
                            break;
                        }
                        case dp::Attribute::Type::Buffer: {
                            const auto &value = this->getBufferOnInput(*index);
                            input->setValue(value);
                            break;
                        }
                    }
                }
            }

            // Process all nodes in our workspace
            for (auto &endNode : this->m_workspace.endNodes) {
                endNode->resetOutputData();

                for (auto &node : this->m_workspace.nodes)
                    node->resetProcessedInputs();

                endNode->process();
            }

            // Forward output node values to outputs
            for (auto &attribute : this->getAttributes()) {
                auto index = indexFromId(attribute.getId());
                if (!index.has_value())
                    continue;

                if (auto output = this->findOutput(attribute.getUnlocalizedName()); output != nullptr) {
                    switch (attribute.getType()) {
                        case dp::Attribute::Type::Integer: {
                            auto value = std::get<i128>(output->getValue());
                            this->setIntegerOnOutput(*index, value);
                            break;
                        }
                        case dp::Attribute::Type::Float: {
                            auto value = std::get<long double>(output->getValue());
                            this->setFloatOnOutput(*index, value);
                            break;
                        }
                        case dp::Attribute::Type::Buffer: {
                            auto value = std::get<std::vector<u8>>(output->getValue());
                            this->setBufferOnOutput(*index, value);
                            break;
                        }
                    }
                }
            }
        }

        void store(nlohmann::json &j) const override {
            j = nlohmann::json::object();

            j["nodes"] = ViewDataProcessor::saveNodes(this->m_workspace);
        }

        void load(const nlohmann::json &j) override {
            ViewDataProcessor::loadNodes(this->m_workspace, j["nodes"]);

            this->m_name = LangEntry(this->getUnlocalizedTitle()).get();
            this->m_requiresAttributeUpdate = true;
        }

    private:
        std::vector<dp::Attribute> findAttributes() {
            std::vector<dp::Attribute> result;

            for (auto &node : this->m_workspace.nodes) {
                if (auto *inputNode = dynamic_cast<NodeCustomInput*>(node.get()); inputNode != nullptr)
                    result.emplace_back(dp::Attribute::IOType::In, inputNode->getType(), inputNode->getName());
                else if (auto *outputNode = dynamic_cast<NodeCustomOutput*>(node.get()); outputNode != nullptr)
                    result.emplace_back(dp::Attribute::IOType::Out, outputNode->getType(), outputNode->getName());
            }

            return result;
        }

        NodeCustomInput* findInput(const std::string &name) {
            for (auto &node : this->m_workspace.nodes) {
                if (auto *inputNode = dynamic_cast<NodeCustomInput*>(node.get()); inputNode != nullptr && inputNode->getName() == name)
                    return inputNode;
            }

            return nullptr;
        }

        NodeCustomOutput* findOutput(const std::string &name) {
            for (auto &node : this->m_workspace.nodes) {
                if (auto *outputNode = dynamic_cast<NodeCustomOutput*>(node.get()); outputNode != nullptr && outputNode->getName() == name)
                    return outputNode;
            }

            return nullptr;
        }

    private:
        std::string m_name = "hex.builtin.nodes.custom.custom.header"_lang;

        bool m_editable = false;

        bool m_requiresAttributeUpdate = false;
        ProviderExtraData::Data::DataProcessor::Workspace m_workspace;
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
        ContentRegistry::DataProcessorNode::add<NodeDisplayBuffer>("hex.builtin.nodes.display", "hex.builtin.nodes.display.buffer");
        ContentRegistry::DataProcessorNode::add<NodeDisplayString>("hex.builtin.nodes.display", "hex.builtin.nodes.display.string");

        ContentRegistry::DataProcessorNode::add<NodeReadData>("hex.builtin.nodes.data_access", "hex.builtin.nodes.data_access.read");
        ContentRegistry::DataProcessorNode::add<NodeWriteData>("hex.builtin.nodes.data_access", "hex.builtin.nodes.data_access.write");
        ContentRegistry::DataProcessorNode::add<NodeDataSize>("hex.builtin.nodes.data_access", "hex.builtin.nodes.data_access.size");
        ContentRegistry::DataProcessorNode::add<NodeDataSelection>("hex.builtin.nodes.data_access", "hex.builtin.nodes.data_access.selection");

        ContentRegistry::DataProcessorNode::add<NodeCastIntegerToBuffer>("hex.builtin.nodes.casting", "hex.builtin.nodes.casting.int_to_buffer");
        ContentRegistry::DataProcessorNode::add<NodeCastBufferToInteger>("hex.builtin.nodes.casting", "hex.builtin.nodes.casting.buffer_to_int");
        ContentRegistry::DataProcessorNode::add<NodeCastFloatToBuffer>("hex.builtin.nodes.casting", "hex.builtin.nodes.casting.float_to_buffer");
        ContentRegistry::DataProcessorNode::add<NodeCastBufferToFloat>("hex.builtin.nodes.casting", "hex.builtin.nodes.casting.buffer_to_float");

        ContentRegistry::DataProcessorNode::add<NodeArithmeticAdd>("hex.builtin.nodes.arithmetic", "hex.builtin.nodes.arithmetic.add");
        ContentRegistry::DataProcessorNode::add<NodeArithmeticSubtract>("hex.builtin.nodes.arithmetic", "hex.builtin.nodes.arithmetic.sub");
        ContentRegistry::DataProcessorNode::add<NodeArithmeticMultiply>("hex.builtin.nodes.arithmetic", "hex.builtin.nodes.arithmetic.mul");
        ContentRegistry::DataProcessorNode::add<NodeArithmeticDivide>("hex.builtin.nodes.arithmetic", "hex.builtin.nodes.arithmetic.div");
        ContentRegistry::DataProcessorNode::add<NodeArithmeticModulus>("hex.builtin.nodes.arithmetic", "hex.builtin.nodes.arithmetic.mod");
        ContentRegistry::DataProcessorNode::add<NodeArithmeticAverage>("hex.builtin.nodes.arithmetic", "hex.builtin.nodes.arithmetic.average");
        ContentRegistry::DataProcessorNode::add<NodeArithmeticMedian>("hex.builtin.nodes.arithmetic", "hex.builtin.nodes.arithmetic.median");
        ContentRegistry::DataProcessorNode::add<NodeArithmeticCeil>("hex.builtin.nodes.arithmetic", "hex.builtin.nodes.arithmetic.ceil");
        ContentRegistry::DataProcessorNode::add<NodeArithmeticFloor>("hex.builtin.nodes.arithmetic", "hex.builtin.nodes.arithmetic.floor");
        ContentRegistry::DataProcessorNode::add<NodeArithmeticRound>("hex.builtin.nodes.arithmetic", "hex.builtin.nodes.arithmetic.round");

        ContentRegistry::DataProcessorNode::add<NodeBufferCombine>("hex.builtin.nodes.buffer", "hex.builtin.nodes.buffer.combine");
        ContentRegistry::DataProcessorNode::add<NodeBufferSlice>("hex.builtin.nodes.buffer", "hex.builtin.nodes.buffer.slice");
        ContentRegistry::DataProcessorNode::add<NodeBufferRepeat>("hex.builtin.nodes.buffer", "hex.builtin.nodes.buffer.repeat");
        ContentRegistry::DataProcessorNode::add<NodeBufferPatch>("hex.builtin.nodes.buffer", "hex.builtin.nodes.buffer.patch");
        ContentRegistry::DataProcessorNode::add<NodeBufferSize>("hex.builtin.nodes.buffer", "hex.builtin.nodes.buffer.size");

        ContentRegistry::DataProcessorNode::add<NodeIf>("hex.builtin.nodes.control_flow", "hex.builtin.nodes.control_flow.if");
        ContentRegistry::DataProcessorNode::add<NodeEquals>("hex.builtin.nodes.control_flow", "hex.builtin.nodes.control_flow.equals");
        ContentRegistry::DataProcessorNode::add<NodeNot>("hex.builtin.nodes.control_flow", "hex.builtin.nodes.control_flow.not");
        ContentRegistry::DataProcessorNode::add<NodeGreaterThan>("hex.builtin.nodes.control_flow", "hex.builtin.nodes.control_flow.gt");
        ContentRegistry::DataProcessorNode::add<NodeLessThan>("hex.builtin.nodes.control_flow", "hex.builtin.nodes.control_flow.lt");
        ContentRegistry::DataProcessorNode::add<NodeBoolAND>("hex.builtin.nodes.control_flow", "hex.builtin.nodes.control_flow.and");
        ContentRegistry::DataProcessorNode::add<NodeBoolOR>("hex.builtin.nodes.control_flow", "hex.builtin.nodes.control_flow.or");

        ContentRegistry::DataProcessorNode::add<NodeBitwiseADD>("hex.builtin.nodes.bitwise", "hex.builtin.nodes.bitwise.add");
        ContentRegistry::DataProcessorNode::add<NodeBitwiseAND>("hex.builtin.nodes.bitwise", "hex.builtin.nodes.bitwise.and");
        ContentRegistry::DataProcessorNode::add<NodeBitwiseOR>("hex.builtin.nodes.bitwise", "hex.builtin.nodes.bitwise.or");
        ContentRegistry::DataProcessorNode::add<NodeBitwiseXOR>("hex.builtin.nodes.bitwise", "hex.builtin.nodes.bitwise.xor");
        ContentRegistry::DataProcessorNode::add<NodeBitwiseNOT>("hex.builtin.nodes.bitwise", "hex.builtin.nodes.bitwise.not");

        ContentRegistry::DataProcessorNode::add<NodeDecodingBase64>("hex.builtin.nodes.decoding", "hex.builtin.nodes.decoding.base64");
        ContentRegistry::DataProcessorNode::add<NodeDecodingHex>("hex.builtin.nodes.decoding", "hex.builtin.nodes.decoding.hex");

        ContentRegistry::DataProcessorNode::add<NodeCryptoAESDecrypt>("hex.builtin.nodes.crypto", "hex.builtin.nodes.crypto.aes");

        ContentRegistry::DataProcessorNode::add<NodeVisualizerDigram>("hex.builtin.nodes.visualizer", "hex.builtin.nodes.visualizer.digram");
        ContentRegistry::DataProcessorNode::add<NodeVisualizerLayeredDistribution>("hex.builtin.nodes.visualizer", "hex.builtin.nodes.visualizer.layered_dist");
        ContentRegistry::DataProcessorNode::add<NodeVisualizerImage>("hex.builtin.nodes.visualizer", "hex.builtin.nodes.visualizer.image");
        ContentRegistry::DataProcessorNode::add<NodeVisualizerImageRGBA>("hex.builtin.nodes.visualizer", "hex.builtin.nodes.visualizer.image_rgba");
        ContentRegistry::DataProcessorNode::add<NodeVisualizerByteDistribution>("hex.builtin.nodes.visualizer", "hex.builtin.nodes.visualizer.byte_distribution");

        ContentRegistry::DataProcessorNode::add<NodePatternLanguageOutVariable>("hex.builtin.nodes.pattern_language", "hex.builtin.nodes.pattern_language.out_var");

        ContentRegistry::DataProcessorNode::add<NodeCustom>("hex.builtin.nodes.custom", "hex.builtin.nodes.custom.custom");
        ContentRegistry::DataProcessorNode::add<NodeCustomInput>("hex.builtin.nodes.custom", "hex.builtin.nodes.custom.input");
        ContentRegistry::DataProcessorNode::add<NodeCustomOutput>("hex.builtin.nodes.custom", "hex.builtin.nodes.custom.output");
    }

}