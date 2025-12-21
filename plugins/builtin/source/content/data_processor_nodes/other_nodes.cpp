#include <algorithm>
#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/content_registry/pattern_language.hpp>
#include <hex/api/content_registry/data_processor.hpp>
#include <hex/api/achievement_manager.hpp>
#include <hex/api/events/events_interaction.hpp>

#include <hex/providers/provider.hpp>
#include <hex/data_processor/node.hpp>
#include <hex/helpers/utils.hpp>

#include <numeric>
#include <wolv/utils/core.hpp>

#include <content/helpers/diagrams.hpp>
#include <pl/patterns/pattern.hpp>

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    class NodeReadData : public dp::Node {
    public:
        NodeReadData() : Node("hex.builtin.nodes.data_access.read.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.data_access.read.address"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.data_access.read.size"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.data_access.read.data") }) { }

        void process() override {
            const auto &address = u64(this->getIntegerOnInput(0));
            const auto &size    = u64(this->getIntegerOnInput(1));

            const auto provider = ImHexApi::Provider::get();
            if (address + size > provider->getActualSize())
                throwNodeError("Read exceeds file size");

            std::vector<u8> data;
            data.resize(size);

            provider->readRaw(address, data.data(), size);

            this->setBufferOnOutput(2, data);
        }
    };

    class NodeWriteData : public dp::Node {
    public:
        NodeWriteData() : Node("hex.builtin.nodes.data_access.write.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.data_access.write.address"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.data_access.write.data") }) { }

        void process() override {
            const auto &address = u64(this->getIntegerOnInput(0));
            const auto &data    = this->getBufferOnInput(1);

            if (!data.empty()) {
                AchievementManager::unlockAchievement("hex.builtin.achievement.data_processor", "hex.builtin.achievement.data_processor.modify_data.name");
            }

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
            EventRegionSelected::subscribe(this, [this](const auto &region) {
                m_address = region.address;
                m_size    = region.size;
            });
        }

        ~NodeDataSelection() override {
            EventRegionSelected::unsubscribe(this);
        }

        void process() override {
            this->setIntegerOnOutput(0, m_address);
            this->setIntegerOnOutput(1, m_size);
        }

    private:
        u64 m_address = 0;
        size_t m_size = 0;
    };

    class NodeCastIntegerToBuffer : public dp::Node {
    public:
        NodeCastIntegerToBuffer() : Node("hex.builtin.nodes.casting.int_to_buffer.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.buffer.size"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &input = this->getIntegerOnInput(0);
            auto size = u64(this->getIntegerOnInput(1));

            if (size == 0) {
                for (u32 i = 0; i < sizeof(input); i++) {
                    if ((input >> (i * 8)) == 0) {
                        size = i;
                        break;
                    }
                }

                if (size == 0)
                    size = 1;
            } else if (size > sizeof(input)) {
                throwNodeError("Integers cannot hold more than 16 bytes");
            }

            std::vector<u8> output(size, 0x00);
            std::memcpy(output.data(), &input, size);

            this->setBufferOnOutput(2, output);
        }
    };

    class NodeCastBufferToInteger : public dp::Node {
    public:
        NodeCastBufferToInteger() : Node("hex.builtin.nodes.casting.buffer_to_int.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &input = this->getBufferOnInput(0);

            i128 output = 0;
            if (input.empty() || input.size() > sizeof(output))
                throwNodeError("Buffer is empty or bigger than 128 bits");

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

            double output = 0;
            if (input.empty() || input.size() != sizeof(output))
                throwNodeError("Buffer is empty or not the right size to fit a float");

            std::memcpy(&output, input.data(), input.size());

            this->setFloatOnOutput(1, output);
        }
    };

    class NodeBufferCombine : public dp::Node {
    public:
        NodeBufferCombine() : Node("hex.builtin.nodes.buffer.combine.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input.a"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input.b"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            const auto &inputA = this->getBufferOnInput(0);
            const auto &inputB = this->getBufferOnInput(1);

            auto output = inputA;
            std::ranges::copy(inputB, std::back_inserter(output));

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
            const auto &count  = u64(this->getIntegerOnInput(1));

            std::vector<u8> output;
            output.resize(buffer.size() * count);

            for (u32 i = 0; i < count; i++)
                std::ranges::copy(buffer, output.begin() + buffer.size() * i);

            this->setBufferOnOutput(2, output);
        }
    };

    class NodeBufferPatch : public dp::Node {
    public:
        NodeBufferPatch() : Node("hex.builtin.nodes.buffer.patch.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.buffer.patch.input.patch"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.ui.common.address"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            auto buffer     = this->getBufferOnInput(0);
            const auto &patch      = this->getBufferOnInput(1);
            const auto &address    = i64(this->getIntegerOnInput(2));

            if (address < 0 || static_cast<u128>(address) >= buffer.size())
                throwNodeError("Address out of range");

            if (address + patch.size() > buffer.size())
                buffer.resize(address + patch.size());

            std::ranges::copy(patch, buffer.begin() + address);

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

    class NodeVisualizerDigram : public dp::Node {
    public:
        NodeVisualizerDigram() : Node("hex.builtin.nodes.visualizer.digram.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input") }) { }

        void drawNode() override {
            m_digram.draw(scaled({ 200, 200 }));

            if (ImGui::IsItemHovered() && ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
                ImGui::BeginTooltip();
                m_digram.draw(scaled({ 600, 600 }));
                ImGui::EndTooltip();
            }
        }

        void process() override {
            m_digram.process(this->getBufferOnInput(0));
        }

    private:
        DiagramDigram m_digram;
    };

    class NodeVisualizerLayeredDistribution : public dp::Node {
    public:
        NodeVisualizerLayeredDistribution() : Node("hex.builtin.nodes.visualizer.layered_dist.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input") }) { }

        void drawNode() override {
            m_layeredDistribution.draw(scaled({ 200, 200 }));
            if (ImGui::IsItemHovered() && ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
                ImGui::BeginTooltip();
                m_layeredDistribution.draw(scaled({ 600, 600 }));
                ImGui::EndTooltip();
            }
        }

        void process() override {
            m_layeredDistribution.process(this->getBufferOnInput(0));
        }

    private:
        DiagramLayeredDistribution m_layeredDistribution;
    };

    class NodeVisualizerImage : public dp::Node {
    public:
        NodeVisualizerImage() : Node("hex.builtin.nodes.visualizer.image.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input") }) { }

        void drawNode() override {
            if (!m_texture.isValid() && !m_data.empty()) {
                m_texture = ImGuiExt::Texture::fromImage(m_data.data(), m_data.size(), ImGuiExt::Texture::Filter::Nearest);
            }

            ImGui::Image(m_texture, scaled(ImVec2(m_texture.getAspectRatio() * 200, 200)));
            if (ImGui::IsItemHovered() && ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
                ImGui::BeginTooltip();
                ImGui::Image(m_texture, scaled(ImVec2(m_texture.getAspectRatio() * 600, 600)));
                ImGui::EndTooltip();
            }
        }

        void process() override {
            const auto &rawData = this->getBufferOnInput(0);

            m_data = rawData;
            m_texture.reset();
        }

    private:
        std::vector<u8> m_data;
        ImGuiExt::Texture m_texture;
    };

    class NodeVisualizerImageRGBA : public dp::Node {
    public:
        NodeVisualizerImageRGBA() : Node("hex.builtin.nodes.visualizer.image_rgba.header", { dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.width"), dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Integer, "hex.builtin.nodes.common.height") }) { }

        void drawNode() override {
            if (!m_texture.isValid() && !m_data.empty()) {
                m_texture = ImGuiExt::Texture::fromBitmap(m_data.data(), m_data.size(), m_width, m_height, ImGuiExt::Texture::Filter::Nearest);
            }

            ImGui::Image(m_texture, scaled(ImVec2(m_texture.getAspectRatio() * 200, 200)));
            if (ImGui::IsItemHovered() && ImGui::IsKeyDown(ImGuiKey_LeftShift)) {
                ImGui::BeginTooltip();
                ImGui::Image(m_texture, scaled(ImVec2(m_texture.getAspectRatio() * 600, 600)));
                ImGui::EndTooltip();
            }
        }

        void process() override {
            m_texture.reset();

            const auto &rawData = this->getBufferOnInput(0);
            const auto &width  = u64(this->getIntegerOnInput(1));
            const auto &height = u64(this->getIntegerOnInput(2));

            const size_t requiredBytes = width * height * 4;
            if (requiredBytes > rawData.size())
                throwNodeError(fmt::format("Image requires at least {} bytes of data, but only {} bytes are available", requiredBytes, rawData.size()));

            m_data.clear();
            m_data.resize(requiredBytes);
            std::copy_n(rawData.data(), requiredBytes, m_data.data());
            m_width = width;
            m_height = height;
            m_texture.reset();
        }

    private:
        std::vector<u8> m_data;
        ImGuiExt::Texture m_texture;
        u32 m_width = 0, m_height = 0;
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
                ImPlot::SetupAxes("Address", "Count", ImPlotAxisFlags_Lock, ImPlotAxisFlags_Lock);
                ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
                ImPlot::SetupAxesLimits(0, 256, 1, double(*std::ranges::max_element(m_counts)) * 1.1F, ImGuiCond_Always);

                static auto x = [] {
                    std::array<ImU64, 256> result { 0 };
                    std::iota(result.begin(), result.end(), 0); // NOLINT: std::ranges::iota not available on some platforms
                    return result;
                }();


                ImPlot::PlotBars<ImU64>("##bytes", x.data(), m_counts.data(), x.size(), 1);

                ImPlot::EndPlot();
            }
        }

        void process() override {
            const auto &buffer = this->getBufferOnInput(0);

            m_counts.fill(0x00);
            for (const auto &byte : buffer) {
                m_counts[byte]++;
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
            ImGui::InputText("##name", m_name);
            ImGui::PopItemWidth();
        }

        void process() override {
            auto lock = std::scoped_lock(ContentRegistry::PatternLanguage::getRuntimeLock());
            auto &runtime = ContentRegistry::PatternLanguage::getRuntime();

            const auto &outVars = runtime.getOutVariables();

            if (outVars.contains(m_name)) {
                std::visit(wolv::util::overloaded {
                    [](const std::string &) {},
                    [](const std::shared_ptr<pl::ptrn::Pattern> &) {},
                    [this](auto &&value) {
                        std::vector<u8> buffer(std::min<size_t>(sizeof(value), 8));
                        std::memcpy(buffer.data(), &value, buffer.size());

                        this->setBufferOnOutput(0, buffer);
                    }
                }, outVars.at(m_name));
            } else {
                throwNodeError(fmt::format("Out variable '{}' has not been defined!", m_name));
            }
        }

        void store(nlohmann::json &j) const override {
            j = nlohmann::json::object();

            j["name"] = m_name;
        }

        void load(const nlohmann::json &j) override {
            m_name = j["name"].get<std::string>();
        }

    private:
        std::string m_name;
    };

    class NodeBufferByteSwap : public dp::Node {
    public:
        NodeBufferByteSwap() : Node("hex.builtin.nodes.buffer.byte_swap.header", {dp::Attribute(dp::Attribute::IOType::In, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.input"), dp::Attribute(dp::Attribute::IOType::Out, dp::Attribute::Type::Buffer, "hex.builtin.nodes.common.output") }) { }

        void process() override {
            auto data = this->getBufferOnInput(0);
            std::ranges::reverse(data);
            this->setBufferOnOutput(1, data);
        }

    };

    void registerOtherDataProcessorNodes() {
        ContentRegistry::DataProcessor::add<NodeReadData>("hex.builtin.nodes.data_access", "hex.builtin.nodes.data_access.read");
        ContentRegistry::DataProcessor::add<NodeWriteData>("hex.builtin.nodes.data_access", "hex.builtin.nodes.data_access.write");
        ContentRegistry::DataProcessor::add<NodeDataSize>("hex.builtin.nodes.data_access", "hex.builtin.nodes.data_access.size");
        ContentRegistry::DataProcessor::add<NodeDataSelection>("hex.builtin.nodes.data_access", "hex.builtin.nodes.data_access.selection");

        ContentRegistry::DataProcessor::add<NodeCastIntegerToBuffer>("hex.builtin.nodes.casting", "hex.builtin.nodes.casting.int_to_buffer");
        ContentRegistry::DataProcessor::add<NodeCastBufferToInteger>("hex.builtin.nodes.casting", "hex.builtin.nodes.casting.buffer_to_int");
        ContentRegistry::DataProcessor::add<NodeCastFloatToBuffer>("hex.builtin.nodes.casting", "hex.builtin.nodes.casting.float_to_buffer");
        ContentRegistry::DataProcessor::add<NodeCastBufferToFloat>("hex.builtin.nodes.casting", "hex.builtin.nodes.casting.buffer_to_float");

        ContentRegistry::DataProcessor::add<NodeBufferCombine>("hex.builtin.nodes.buffer", "hex.builtin.nodes.buffer.combine");
        ContentRegistry::DataProcessor::add<NodeBufferSlice>("hex.builtin.nodes.buffer", "hex.builtin.nodes.buffer.slice");
        ContentRegistry::DataProcessor::add<NodeBufferRepeat>("hex.builtin.nodes.buffer", "hex.builtin.nodes.buffer.repeat");
        ContentRegistry::DataProcessor::add<NodeBufferPatch>("hex.builtin.nodes.buffer", "hex.builtin.nodes.buffer.patch");
        ContentRegistry::DataProcessor::add<NodeBufferSize>("hex.builtin.nodes.buffer", "hex.builtin.nodes.buffer.size");
        ContentRegistry::DataProcessor::add<NodeBufferByteSwap>("hex.builtin.nodes.buffer", "hex.builtin.nodes.buffer.byte_swap");

        ContentRegistry::DataProcessor::add<NodeVisualizerDigram>("hex.builtin.nodes.visualizer", "hex.builtin.nodes.visualizer.digram");
        ContentRegistry::DataProcessor::add<NodeVisualizerLayeredDistribution>("hex.builtin.nodes.visualizer", "hex.builtin.nodes.visualizer.layered_dist");
        ContentRegistry::DataProcessor::add<NodeVisualizerImage>("hex.builtin.nodes.visualizer", "hex.builtin.nodes.visualizer.image");
        ContentRegistry::DataProcessor::add<NodeVisualizerImageRGBA>("hex.builtin.nodes.visualizer", "hex.builtin.nodes.visualizer.image_rgba");
        ContentRegistry::DataProcessor::add<NodeVisualizerByteDistribution>("hex.builtin.nodes.visualizer", "hex.builtin.nodes.visualizer.byte_distribution");

        ContentRegistry::DataProcessor::add<NodePatternLanguageOutVariable>("hex.builtin.nodes.pattern_language", "hex.builtin.nodes.pattern_language.out_var");
    }

}
