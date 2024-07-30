#include <imgui_internal.h>
#include <hex/api/content_registry.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/data_processor/node.hpp>
#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::plugin::builtin {

    class NodeDisplayInteger : public dp::Node {
    public:
        NodeDisplayInteger() : Node("hex.builtin.nodes.display.int.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input") }) { }

        void drawNode() override {
            ImGui::PushItemWidth(150_scaled);
            if (m_value.has_value()) {
                ImGuiExt::TextFormattedSelectable("{0:d}",   m_value.value());
                ImGuiExt::TextFormattedSelectable("0x{0:02X}", m_value.value());
                ImGuiExt::TextFormattedSelectable("0o{0:03o}", m_value.value());
                ImGuiExt::TextFormattedSelectable("0b{0:08b}", m_value.value());
            } else {
                ImGui::TextUnformatted("???");
                ImGui::TextUnformatted("???");
                ImGui::TextUnformatted("???");
                ImGui::TextUnformatted("???");
            }

            ImGui::PopItemWidth();
        }

        void process() override {
            m_value = this->getIntegerOnInput(0);
        }

    private:
        std::optional<u64> m_value;
    };

    class NodeDisplayFloat : public dp::Node {
    public:
        NodeDisplayFloat() : Node("hex.builtin.nodes.display.float.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Float, "hex.builtin.nodes.common.input") }) { }

        void drawNode() override {
            ImGui::PushItemWidth(150_scaled);
            if (m_value.has_value())
                ImGuiExt::TextFormattedSelectable("{0}", m_value.value());
            else
                ImGui::TextUnformatted("???");
            ImGui::PopItemWidth();
        }

        void process() override {
            m_value.reset();
            const auto &input = this->getFloatOnInput(0);

            m_value = input;
        }

    private:
        std::optional<float> m_value;
    };

    class NodeDisplayBuffer : public dp::Node {
    public:
        NodeDisplayBuffer() : Node("hex.builtin.nodes.display.buffer.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input") }) { }

        void drawNode() override {
            static const std::string Header = " Address    00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F                       ";

            if (ImGui::BeginChild("##hex_view", ImVec2(ImGui::CalcTextSize(Header.c_str()).x, 200_scaled), true)) {
                ImGui::TextUnformatted(Header.c_str());

                auto size = m_buffer.size();
                ImGuiListClipper clipper;

                clipper.Begin((size + 0x0F) / 0x10);

                while (clipper.Step()) {
                    for (auto y = clipper.DisplayStart; y < clipper.DisplayEnd; y++) {
                        auto lineSize = ((size - y * 0x10) < 0x10) ? size % 0x10 : 0x10;

                        std::string line = hex::format(" {:08X}:  ", y * 0x10);
                        for (u32 x = 0; x < 0x10; x++) {
                            if (x < lineSize)
                                line += hex::format("{:02X} ", m_buffer[y * 0x10 + x]);
                            else
                                line += "   ";

                            if (x == 7) line += " ";
                        }

                        line += "   ";

                        for (u32 x = 0; x < lineSize; x++) {
                            auto c = char(m_buffer[y * 0x10 + x]);
                            if (std::isprint(c))
                                line += c;
                            else
                                line += ".";
                        }

                        ImGuiExt::TextFormattedSelectable("{}", line.c_str());
                    }
                }
                clipper.End();
            }
            ImGui::EndChild();
        }

        void process() override {
            m_buffer = this->getBufferOnInput(0);
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
                std::string_view string = m_value;

                ImGuiListClipper clipper;
                clipper.Begin((string.length() + (LineLength - 1)) / LineLength);

                while (clipper.Step()) {
                    for (auto i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        auto line = string.substr(i * LineLength, LineLength);
                        ImGui::TextUnformatted("");
                        ImGui::SameLine();
                        ImGuiExt::TextFormattedSelectable("{}", line);
                    }
                }

                clipper.End();
            }
            ImGui::EndChild();
        }

        void process() override {
            const auto &input = this->getBufferOnInput(0);

            m_value = hex::encodeByteString(input);
        }

    private:
        std::string m_value;
    };

    class NodeDisplayBits : public dp::Node {
    public:
        NodeDisplayBits() : Node("hex.builtin.nodes.display.bits.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input") }) { }

        void drawNode() override {
            ImGui::PushItemWidth(100_scaled);
            ImGuiExt::TextFormattedSelectable("{}", m_display);
            ImGui::PopItemWidth();
        }

        void process() override {
            const auto &buffer = this->getBufferOnInput(0);
            // Display bits in groups of 4 bits
            std::string display;
            display.reserve(buffer.size() * 9 + 2); // 8 bits + 1 space at beginning + 1 space every 4 bits
            for (const auto &byte : buffer) {
                for (size_t i = 0; i < 8; i++) {
                    if (i % 4 == 0) {
                        display += ' ';
                    }
                    display += (byte & (1 << i)) != 0 ? '1' : '0';
                }
            }
            m_display = wolv::util::trim(display);
        }

    private:
        std::string m_display = "???";
    };

    void registerVisualDataProcessorNodes() {
        ContentRegistry::DataProcessorNode::add<NodeDisplayInteger>("hex.builtin.nodes.display", "hex.builtin.nodes.display.int");
        ContentRegistry::DataProcessorNode::add<NodeDisplayFloat>("hex.builtin.nodes.display", "hex.builtin.nodes.display.float");
        ContentRegistry::DataProcessorNode::add<NodeDisplayBuffer>("hex.builtin.nodes.display", "hex.builtin.nodes.display.buffer");
        ContentRegistry::DataProcessorNode::add<NodeDisplayString>("hex.builtin.nodes.display", "hex.builtin.nodes.display.string");
        ContentRegistry::DataProcessorNode::add<NodeDisplayBits>("hex.builtin.nodes.display", "hex.builtin.nodes.display.bits");
    }

}
