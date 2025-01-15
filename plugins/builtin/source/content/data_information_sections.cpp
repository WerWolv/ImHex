#include <hex/api/content_registry.hpp>
#include <hex/helpers/magic.hpp>
#include <hex/providers/provider.hpp>

#include <imgui.h>
#include <content/helpers/diagrams.hpp>
#include <fonts/vscode_icons.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/ui/imgui_imhex_extensions.h>

#include <wolv/literals.hpp>

namespace hex::plugin::builtin {

    using namespace wolv::literals;

    class InformationProvider : public ContentRegistry::DataInformation::InformationSection {
    public:
        InformationProvider() : InformationSection("hex.builtin.information_section.provider_information") { }
        ~InformationProvider() override = default;

        void process(Task &task, prv::Provider *provider, Region region) override {
            std::ignore = task;

            m_provider = provider;
            m_region   = region;
        }

        void reset() override {
            m_provider = nullptr;
            m_region   = Region::Invalid();
        }

        void drawContent() override {
            if (ImGui::BeginTable("information", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoKeepColumnsVisible)) {
                ImGui::TableSetupColumn("type");
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                ImGui::TableNextRow();

                for (auto &[name, value] : m_provider->getDataDescription()) {
                    ImGui::TableNextColumn();
                    ImGuiExt::TextFormatted("{}", name);
                    ImGui::TableNextColumn();
                    ImGui::PushID(name.c_str());
                    ImGuiExt::TextFormattedWrappedSelectable("{}", value);
                    ImGui::PopID();
                }

                ImGui::TableNextColumn();
                ImGuiExt::TextFormatted("{}", "hex.ui.common.region"_lang);
                ImGui::TableNextColumn();
                ImGuiExt::TextFormattedSelectable("0x{:X} - 0x{:X}", m_region.getStartAddress(), m_region.getEndAddress());

                ImGui::EndTable();
            }
        }

    private:
        prv::Provider *m_provider = nullptr;
        Region m_region = Region::Invalid();
    };

    class InformationMagic : public ContentRegistry::DataInformation::InformationSection {
    public:
        InformationMagic() : InformationSection("hex.builtin.information_section.magic") { }
        ~InformationMagic() override = default;

        void process(Task &task, prv::Provider *provider, Region region) override {
            magic::compile();

            task.update();

            try {
                std::vector<u8> data(region.getSize());
                provider->read(region.getStartAddress(), data.data(), data.size());

                m_dataDescription       = magic::getDescription(data);
                m_dataMimeType          = magic::getMIMEType(data);
                m_dataAppleCreatorType  = magic::getAppleCreatorType(data);
                m_dataExtensions        = magic::getExtensions(data);
            } catch (const std::bad_alloc &) {
                hex::log::error("Failed to allocate enough memory for full file magic analysis!");

                // Retry analysis with only the first 100 KiB
                if (region.getSize() != 100_kiB) {
                    process(task, provider, { region.getStartAddress(), 100_kiB });
                }
            }
        }

        void drawContent() override {
            if (!(m_dataDescription.empty() && m_dataMimeType.empty())) {
                if (ImGui::BeginTable("magic", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("type");
                    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                    ImGui::TableNextRow();

                    if (!m_dataDescription.empty()) {
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("hex.builtin.information_section.magic.description"_lang);
                        ImGui::TableNextColumn();

                        if (m_dataDescription == "data") {
                            ImGuiExt::TextFormattedColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "{} ({})", "hex.builtin.information_section.magic.octet_stream_text"_lang, m_dataDescription);
                        } else {
                            ImGuiExt::TextFormattedWrappedSelectable("{}", m_dataDescription);
                        }
                    }

                    if (!m_dataMimeType.empty()) {
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("hex.builtin.information_section.magic.mime"_lang);
                        ImGui::TableNextColumn();

                        if (m_dataMimeType.contains("application/octet-stream")) {
                            ImGuiExt::TextFormatted("{}", m_dataMimeType);
                            ImGui::SameLine();
                            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                            ImGuiExt::HelpHover("hex.builtin.information_section.magic.octet_stream_warning"_lang, ICON_VS_INFO);
                            ImGui::PopStyleVar();
                        } else {
                            ImGuiExt::TextFormattedSelectable("{}", m_dataMimeType);
                        }
                    }

                    if (!m_dataAppleCreatorType.empty()) {
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("hex.builtin.information_section.magic.apple_type"_lang);
                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormattedSelectable("{}", m_dataAppleCreatorType);
                    }

                    if (!m_dataExtensions.empty()) {
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("hex.builtin.information_section.magic.extension"_lang);
                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormattedSelectable("{}", m_dataExtensions);
                    }

                    ImGui::EndTable();
                }

                ImGui::NewLine();
            }
        }

        void reset() override {
            m_dataDescription.clear();
            m_dataMimeType.clear();
            m_dataAppleCreatorType.clear();
            m_dataExtensions.clear();
        }

    private:
        std::string m_dataDescription;
        std::string m_dataMimeType;
        std::string m_dataAppleCreatorType;
        std::string m_dataExtensions;
    };

    class InformationByteAnalysis : public ContentRegistry::DataInformation::InformationSection {
    public:
        InformationByteAnalysis() : InformationSection("hex.builtin.information_section.info_analysis", "", true) {
            EventRegionSelected::subscribe(this, [this](const ImHexApi::HexEditor::ProviderRegion &region) {
                m_byteTypesDistribution.setHandlePosition(region.getStartAddress());
                m_chunkBasedEntropy.setHandlePosition(region.getStartAddress());
            });
        }
        ~InformationByteAnalysis() override {
            EventRegionSelected::unsubscribe(this);
        }

        void process(Task &task, prv::Provider *provider, Region region) override {
            if (m_inputChunkSize == 0)
                m_inputChunkSize = 256;

            m_blockSize = std::max<u32>(std::ceil(region.getSize() / 2048.0F), 256);

            m_byteDistribution.reset();
            m_byteTypesDistribution.reset(region.getStartAddress(), region.getEndAddress(), provider->getBaseAddress(), provider->getActualSize());
            m_chunkBasedEntropy.reset(m_inputChunkSize, region.getStartAddress(), region.getEndAddress(),
                provider->getBaseAddress(), provider->getActualSize());

            m_chunkBasedEntropy.enableAnnotations(m_showAnnotations);
            m_byteTypesDistribution.enableAnnotations(m_showAnnotations);

            // Create a handle to the file
            auto reader = prv::ProviderReader(provider);
            reader.seek(region.getStartAddress());
            reader.setEndAddress(region.getEndAddress());

            // Loop over each byte of the selection and update each analysis
            // one byte at a time to process the file only once
            for (u8 byte : reader) {
                m_byteDistribution.update(byte);
                m_byteTypesDistribution.update(byte);
                m_chunkBasedEntropy.update(byte);
                task.update();
            }

            m_averageEntropy                = m_chunkBasedEntropy.calculateEntropy(m_byteDistribution.get(), region.getSize());
            m_highestBlockEntropy           = m_chunkBasedEntropy.getHighestEntropyBlockValue();
            m_highestBlockEntropyAddress    = m_chunkBasedEntropy.getHighestEntropyBlockAddress();
            m_lowestBlockEntropy            = m_chunkBasedEntropy.getLowestEntropyBlockValue();
            m_lowestBlockEntropyAddress     = m_chunkBasedEntropy.getLowestEntropyBlockAddress();
            m_plainTextCharacterPercentage  = m_byteTypesDistribution.getPlainTextCharacterPercentage();
        }

        void reset() override {
            m_averageEntropy = -1.0;
            m_highestBlockEntropy = -1.0;
            m_plainTextCharacterPercentage = -1.0;
        }

        void drawSettings() override {
            ImGuiExt::SliderBytes("hex.builtin.information_section.info_analysis.block_size"_lang, &m_inputChunkSize, 0, 1_MiB);
            ImGui::Checkbox("hex.builtin.information_section.info_analysis.show_annotations"_lang, &m_showAnnotations);
        }

        void drawContent() override {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImGuiCol_WindowBg));
            ImPlot::PushStyleColor(ImPlotCol_FrameBg, ImGui::GetColorU32(ImGuiCol_WindowBg));

            // Display byte distribution analysis
            ImGui::TextUnformatted("hex.builtin.information_section.info_analysis.distribution"_lang);
            m_byteDistribution.draw(
                ImVec2(-1, 0),
                ImPlotFlags_NoLegend | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect
            );

            // Display byte types distribution analysis
            ImGui::TextUnformatted("hex.builtin.information_section.info_analysis.byte_types"_lang);
            m_byteTypesDistribution.draw(
                    ImVec2(-1, 0),
                    ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect,
                    true
            );

            // Display chunk-based entropy analysis
            ImGui::TextUnformatted("hex.builtin.information_section.info_analysis.entropy"_lang);
            m_chunkBasedEntropy.draw(
                ImVec2(-1, 0),
                ImPlotFlags_NoLegend | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect,
                true
            );

            ImPlot::PopStyleColor();
            ImGui::PopStyleColor();

            // Entropy information
            if (ImGui::BeginTable("entropy_info", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("type");
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGuiExt::TextFormatted("{}", "hex.builtin.information_section.info_analysis.block_size"_lang);
                ImGui::TableNextColumn();
                ImGuiExt::TextFormattedSelectable("hex.builtin.information_section.info_analysis.block_size.desc"_lang, m_chunkBasedEntropy.getSize(), m_chunkBasedEntropy.getChunkSize());

                ImGui::TableNextColumn();
                ImGuiExt::TextFormatted("{}", "hex.builtin.information_section.info_analysis.file_entropy"_lang);
                ImGui::TableNextColumn();
                if (m_averageEntropy < 0) {
                    ImGui::TextUnformatted("???");
                } else {
                    auto entropy = std::abs(m_averageEntropy);
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.1F);
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImGuiCol_TableRowBgAlt));
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImColor::HSV(0.3F - (0.3F * entropy), 0.6F, 0.8F, 1.0F).Value);
                    ImGui::ProgressBar(entropy, ImVec2(200_scaled, ImGui::GetTextLineHeight()), hex::format("{:.5f}", entropy).c_str());
                    ImGui::PopStyleColor(2);
                    ImGui::PopStyleVar();
                }

                ImGui::TableNextColumn();
                ImGuiExt::TextFormatted("{}", "hex.builtin.information_section.info_analysis.highest_entropy"_lang);
                ImGui::TableNextColumn();
                ImGuiExt::TextFormattedSelectable("{:.5f} @", m_highestBlockEntropy);
                ImGui::SameLine();
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                if (ImGui::Button(hex::format("0x{:06X}", m_highestBlockEntropyAddress).c_str())) {
                    ImHexApi::HexEditor::setSelection(m_highestBlockEntropyAddress, m_inputChunkSize);
                }
                ImGui::PopStyleColor();
                ImGui::PopStyleVar();

                ImGui::TableNextColumn();
                ImGuiExt::TextFormatted("{}", "hex.builtin.information_section.info_analysis.lowest_entropy"_lang);
                ImGui::TableNextColumn();
                ImGuiExt::TextFormattedSelectable("{:.5f} @", m_lowestBlockEntropy);
                ImGui::SameLine();
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                if (ImGui::Button(hex::format("0x{:06X}", m_lowestBlockEntropyAddress).c_str())) {
                    ImHexApi::HexEditor::setSelection(m_lowestBlockEntropyAddress, m_inputChunkSize);
                }
                ImGui::PopStyleColor();
                ImGui::PopStyleVar();

                ImGui::TableNextColumn();
                ImGuiExt::TextFormatted("{}", "hex.builtin.information_section.info_analysis.plain_text_percentage"_lang);
                ImGui::TableNextColumn();

                if (m_plainTextCharacterPercentage < 0) {
                    ImGui::TextUnformatted("???");
                } else {
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.1F);
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImGuiCol_TableRowBgAlt));
                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImColor::HSV(0.3F * (m_plainTextCharacterPercentage / 100.0F), 0.8F, 0.6F, 1.0F).Value);
                    ImGui::ProgressBar(m_plainTextCharacterPercentage / 100.0F, ImVec2(200_scaled, ImGui::GetTextLineHeight()));
                    ImGui::PopStyleColor(2);
                    ImGui::PopStyleVar();
                }

                ImGui::EndTable();
            }

            // General information
            if (ImGui::BeginTable("info", 1, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableNextRow();

                if (m_averageEntropy > 0.83 && m_highestBlockEntropy > 0.9) {
                    ImGui::TableNextColumn();
                    ImGuiExt::TextFormattedColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "{}", "hex.builtin.information_section.info_analysis.encrypted"_lang);
                }

                if (m_plainTextCharacterPercentage > 95) {
                    ImGui::TableNextColumn();
                    ImGuiExt::TextFormattedColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "{}", "hex.builtin.information_section.info_analysis.plain_text"_lang);
                }

                ImGui::EndTable();
            }
        }

        void load(const nlohmann::json &data) override {
            InformationSection::load(data);

            m_inputChunkSize  = data.value("block_size", 0);
            m_showAnnotations = data.value("annotations", true);
        }

        nlohmann::json store() override {
            auto result = InformationSection::store();
            result["block_size"]  = m_inputChunkSize;
            result["annotations"] = m_showAnnotations;

            return result;
        }

    private:
        u64 m_inputChunkSize = 0;

        u32 m_blockSize = 0;
        double m_averageEntropy = -1.0;

        double m_highestBlockEntropy = -1.0;
        u64 m_highestBlockEntropyAddress = 0x00;
        double m_lowestBlockEntropy = -1.0;
        u64 m_lowestBlockEntropyAddress = 0x00;
        double m_plainTextCharacterPercentage = -1.0;

        bool m_showAnnotations = true;

        DiagramByteDistribution m_byteDistribution;
        DiagramByteTypesDistribution m_byteTypesDistribution;
        DiagramChunkBasedEntropyAnalysis m_chunkBasedEntropy;
    };

class InformationByteRelationshipAnalysis : public ContentRegistry::DataInformation::InformationSection {
    public:
        InformationByteRelationshipAnalysis() : InformationSection("hex.builtin.information_section.relationship_analysis", "", true) {

        }

        void process(Task &task, prv::Provider *provider, Region region) override {
            updateSettings();
            m_digram.reset(region.getSize());
            m_layeredDistribution.reset(region.getSize());

            // Create a handle to the file
            auto reader = prv::ProviderReader(provider);
            reader.seek(region.getStartAddress());
            reader.setEndAddress(region.getEndAddress());

            // Loop over each byte of the selection and update each analysis
            // one byte at a time to process the file only once
            for (u8 byte : reader) {
                m_digram.update(byte);
                m_layeredDistribution.update(byte);
                task.update();
            }
        }

        void reset() override {
            m_digram.reset(m_sampleSize);
            m_layeredDistribution.reset(m_sampleSize);
            updateSettings();
        }

        void drawSettings() override {
            if (ImGuiExt::InputHexadecimal("hex.builtin.information_section.relationship_analysis.sample_size"_lang, &m_sampleSize)) {
                updateSettings();
            }

            if (ImGui::SliderFloat("hex.builtin.information_section.relationship_analysis.brightness"_lang, &m_brightness, 0.0F, 1.0F)) {
                updateSettings();
            }

            if (ImGui::Combo("hex.builtin.information_section.relationship_analysis.filter"_lang, reinterpret_cast<int*>(&m_filter), "Linear Interpolation\0Nearest Neighbour\0\0")) {
                updateSettings();
            }
        }

        void drawContent() override {
            auto availableWidth = ImGui::GetContentRegionAvail().x;

            if (availableWidth > 750_scaled) {
                availableWidth /= 2;
                availableWidth -= ImGui::GetStyle().FramePadding.x;

                if (ImGui::BeginTable("##RelationshipTable", 2)) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    ImGui::TextUnformatted("hex.builtin.information_section.relationship_analysis.digram"_lang);
                    m_digram.draw({ availableWidth, availableWidth });

                    ImGui::TableNextColumn();

                    ImGui::TextUnformatted("hex.builtin.information_section.relationship_analysis.layered_distribution"_lang);
                    m_layeredDistribution.draw({ availableWidth, availableWidth });

                    ImGui::EndTable();
                }
            } else {
                ImGui::TextUnformatted("hex.builtin.information_section.relationship_analysis.digram"_lang);
                m_digram.draw({ availableWidth, availableWidth });

                ImGui::TextUnformatted("hex.builtin.information_section.relationship_analysis.layered_distribution"_lang);
                m_layeredDistribution.draw({ availableWidth, availableWidth });
            }
        }

        void load(const nlohmann::json &data) override {
            InformationSection::load(data);

            m_filter = data.value("filter", ImGuiExt::Texture::Filter::Nearest);
            m_sampleSize = data.value("sample_size", 0x9000);
            m_brightness = data.value("brightness", 0.5F);

            updateSettings();
        }

        nlohmann::json store() override {
            auto result = InformationSection::store();
            result["sample_size"] = m_sampleSize;
            result["brightness"] = m_brightness;
            result["filter"] = m_filter;

            return result;
        }

    private:
        void updateSettings() {
            m_digram.setFiltering(m_filter);
            m_digram.setSampleSize(m_sampleSize);
            m_digram.setBrightness(m_brightness);

            m_layeredDistribution.setFiltering(m_filter);
            m_layeredDistribution.setSampleSize(m_sampleSize);
            m_layeredDistribution.setBrightness(m_brightness);
        }

    private:
        ImGuiExt::Texture::Filter m_filter = ImGuiExt::Texture::Filter::Nearest;
        u64 m_sampleSize = 0x9000;
        float m_brightness = 0.5F;

        DiagramDigram m_digram;
        DiagramLayeredDistribution m_layeredDistribution;
    };

    void registerDataInformationSections() {
        ContentRegistry::DataInformation::addInformationSection<InformationProvider>();
        ContentRegistry::DataInformation::addInformationSection<InformationMagic>();
        ContentRegistry::DataInformation::addInformationSection<InformationByteAnalysis>();
        ContentRegistry::DataInformation::addInformationSection<InformationByteRelationshipAnalysis>();
    }

}
