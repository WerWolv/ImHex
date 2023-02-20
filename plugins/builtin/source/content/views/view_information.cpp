#include "content/views/view_information.hpp"

#include <hex/api/content_registry.hpp>

#include <hex/providers/provider.hpp>
#include <hex/providers/buffered_reader.hpp>

#include <hex/helpers/fs.hpp>
#include <hex/helpers/magic.hpp>

#include <cstring>
#include <cmath>
#include <filesystem>
#include <numeric>
#include <span>

#include <implot.h>

namespace hex::plugin::builtin {

    using namespace hex::literals;

    ViewInformation::ViewInformation() : View("hex.builtin.view.information.name") {
        EventManager::subscribe<EventDataChanged>(this, [this]() {
            this->m_dataValid = false;
            this->m_plainTextCharacterPercentage = -1.0;
            this->m_averageEntropy = -1.0;
            this->m_highestBlockEntropy = -1.0;
            this->m_blockSize = 0;
            this->m_dataMimeType.clear();
            this->m_dataDescription.clear();
            this->m_analyzedRegion = { 0, 0 };
        });

        EventManager::subscribe<EventRegionSelected>(this, [this](Region region) {
            // set the position of the diagram relative to the place where 
            // the user clicked inside the hex editor view 
            if (this->m_blockSize != 0) 
                this->m_diagramHandlePosition = region.getStartAddress() / double(this->m_blockSize);
        });

        EventManager::subscribe<EventProviderDeleted>(this, [this](const auto*) {
            this->m_dataValid = false;
        });

        ContentRegistry::FileHandler::add({ ".mgc" }, [](const auto &path) {
            for (const auto &destPath : fs::getDefaultPaths(fs::ImHexPath::Magic)) {
                if (fs::copyFile(path, destPath / path.filename(), std::fs::copy_options::overwrite_existing)) {
                    View::showInfoPopup("hex.builtin.view.information.magic_db_added"_lang);
                    return true;
                }
            }

            return false;
        });
    }

    ViewInformation::~ViewInformation() {
        EventManager::unsubscribe<EventDataChanged>(this);
        EventManager::unsubscribe<EventRegionSelected>(this);
        EventManager::unsubscribe<EventProviderDeleted>(this);
    }

    /*
    static double calculateEntropy(std::array<ImU64, 256> &valueCounts, size_t blockSize) {
        double entropy = 0;

        for (auto count : valueCounts) {
            if (count == 0) [[unlikely]]
                continue;

            double probability = static_cast<double>(count) / blockSize;

            entropy += probability * std::log2(probability);
        }

        return std::min(1.0, (-entropy) / 8);    // log2(256) = 8
    }
    */

    void ViewInformation::analyze() {
        this->m_analyzerTask = TaskManager::createTask("hex.builtin.view.information.analyzing", 0, [this](auto &task) {
            auto provider = ImHexApi::Provider::get();

            if ((this->m_inputChunkSize <= 0) 
             || (this->m_inputStartAddress < 0) 
             || (this->m_inputStartAddress >= this->m_inputEndAddress)
             || ((size_t) this->m_inputEndAddress > provider->getSize())) {
                // invalid parameters, set default one
                this->m_inputChunkSize    = 256;
                this->m_inputStartAddress = 0;
                this->m_inputEndAddress   = provider->getSize(); 
            }

            task.setMaxValue(this->m_inputEndAddress - this->m_inputStartAddress);

            // modify the analyzed region  
            this->m_analyzedRegion = { 
                provider->getBaseAddress() + this->m_inputStartAddress, 
                size_t(this->m_inputEndAddress - this->m_inputStartAddress)
            };

            {
                magic::compile();

                this->m_dataDescription = magic::getDescription(provider);
                this->m_dataMimeType    = magic::getMIMEType(provider);
            }

            {
                this->m_blockSize = std::max<u32>(std::ceil(provider->getSize() / 2048.0F), 256);
                this->m_diagramHandlePosition = this->m_inputStartAddress / double(this->m_blockSize);

                this->m_averageEntropy = -1.0;
                this->m_highestBlockEntropy = -1.0;
                this->m_plainTextCharacterPercentage = -1.0;

                // setup / start each analysis

                this->m_byteDistribution.reset();
                this->m_digram.reset(this->m_inputEndAddress - this->m_inputStartAddress);
                this->m_layeredDistribution.reset(this->m_inputEndAddress - this->m_inputStartAddress);
                this->m_byteTypesDistribution.reset(this->m_inputStartAddress, this->m_inputEndAddress, 
                    provider->getBaseAddress(), provider->getSize());
                this->m_chunkBasedEntropy.reset(this->m_inputChunkSize, this->m_inputStartAddress, this->m_inputEndAddress,
                    provider->getBaseAddress(), provider->getSize());

                // create a handle to the file
                auto reader = prv::BufferedReader(provider);
                reader.seek(provider->getBaseAddress() + this->m_inputStartAddress);
                reader.setEndAddress(provider->getBaseAddress() + this->m_inputEndAddress);

                u64 count = 0;

                // loop over each byte of the [part of the] file and update each analysis 
                // one byte at the time in order to process the file only once
                for (u8 byte : reader) {
                    this->m_byteDistribution.update(byte);
                    this->m_byteTypesDistribution.update(byte);
                    this->m_chunkBasedEntropy.update(byte);
                    this->m_layeredDistribution.update(byte);
                    this->m_digram.update(byte);
                    ++count;
                    task.update(count);
                }

                this->m_averageEntropy = this->m_chunkBasedEntropy.calculateEntropy(this->m_byteDistribution.get(), this->m_inputEndAddress - this->m_inputStartAddress);
                this->m_highestBlockEntropy = this->m_chunkBasedEntropy.getHighestBlockEntropy();
                this->m_plainTextCharacterPercentage = this->m_byteTypesDistribution.getPlainTextCharacterPercentage();
            }
                
            this->m_dataValid = true;
        });
    }        

    void ViewInformation::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.information.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            if (ImGui::BeginChild("##scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav)) {

                auto provider = ImHexApi::Provider::get();
                if (ImHexApi::Provider::isValid() && provider->isReadable()) {
                    ImGui::BeginDisabled(this->m_analyzerTask.isRunning());
                    {
                        ImGui::Header("Parameters Selection");

                        ImGui::Text(" Block Size for Entropy Analysis: ");
                        ImGui::InputInt("##BlockSize", &this->m_inputChunkSize, ImGuiInputTextFlags_CharsDecimal);

                        ImGui::Text("Start Address: ");
                        ImGui::InputInt("##StartAddress", &this->m_inputStartAddress, ImGuiInputTextFlags_CharsDecimal);

                        ImGui::TextFormatted("End Address ({} max): ", provider->getSize());
                        ImGui::InputInt("##EndAddress", &this->m_inputEndAddress, ImGuiInputTextFlags_CharsDecimal);

                        if (ImGui::Button("hex.builtin.view.information.analyze"_lang, ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                            this->analyze();
                    }
                    ImGui::EndDisabled();

                    if (this->m_analyzerTask.isRunning()) {
                        ImGui::TextSpinner("hex.builtin.view.information.analyzing"_lang);
                    } else {
                        ImGui::NewLine();
                    }

                    if (!this->m_analyzerTask.isRunning() && this->m_dataValid) {

                        // Analyzed region
                        ImGui::Header("hex.builtin.view.information.region"_lang, true);

                        if (ImGui::BeginTable("information", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoKeepColumnsVisible)) {
                            ImGui::TableSetupColumn("type");
                            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                            ImGui::TableNextRow();

                            for (auto &[name, value] : provider->getDataDescription()) {
                                ImGui::TableNextColumn();
                                ImGui::TextFormatted("{}", name);
                                ImGui::TableNextColumn();
                                ImGui::TextFormattedWrapped("{}", value);
                            }

                            ImGui::TableNextColumn();
                            ImGui::TextFormatted("{}", "hex.builtin.view.information.region"_lang);
                            ImGui::TableNextColumn();
                            ImGui::TextFormatted("0x{:X} - 0x{:X}", this->m_analyzedRegion.getStartAddress(), this->m_analyzedRegion.getEndAddress());

                            ImGui::EndTable();
                        }

                        ImGui::NewLine();

                        // Magic information
                        if (!(this->m_dataDescription.empty() && this->m_dataMimeType.empty())) {
                            ImGui::Header("hex.builtin.view.information.magic"_lang);

                            if (ImGui::BeginTable("magic", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
                                ImGui::TableSetupColumn("type");
                                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                                ImGui::TableNextRow();

                                if (!this->m_dataDescription.empty()) {
                                    ImGui::TableNextColumn();
                                    ImGui::TextUnformatted("hex.builtin.view.information.description"_lang);
                                    ImGui::TableNextColumn();
                                    ImGui::TextFormattedWrapped("{}", this->m_dataDescription.c_str());
                                }

                                if (!this->m_dataMimeType.empty()) {
                                    ImGui::TableNextColumn();
                                    ImGui::TextUnformatted("hex.builtin.view.information.mime"_lang);
                                    ImGui::TableNextColumn();
                                    ImGui::TextFormattedWrapped("{}", this->m_dataMimeType.c_str());
                                }

                                ImGui::EndTable();
                            }
                        }

                        // Information analysis
                        {

                            ImGui::Header("hex.builtin.view.information.info_analysis"_lang);

                            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImGuiCol_WindowBg));
                            ImPlot::PushStyleColor(ImPlotCol_FrameBg, ImGui::GetColorU32(ImGuiCol_WindowBg));

                            // display byte distribution analysis
                            ImGui::TextUnformatted("hex.builtin.view.information.distribution"_lang);
                            this->m_byteDistribution.draw(
                                ImVec2(-1, 0), 
                                ImPlotFlags_NoChild | ImPlotFlags_NoLegend | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect
                            );

                            // display byte types distribution analysis
                            ImGui::TextUnformatted("hex.builtin.view.information.byte_types"_lang);
                            this->m_byteTypesDistribution.draw(
                                    ImVec2(-1, 0), 
                                    ImPlotFlags_NoChild | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect | ImPlotFlags_AntiAliased,
                                    &this->m_diagramHandlePosition
                            );

                            // display chunk based entropy analysis
                            ImGui::TextUnformatted("hex.builtin.view.information.entropy"_lang);
                            this->m_chunkBasedEntropy.draw(
                                ImVec2(-1, 0), 
                                ImPlotFlags_NoChild | ImPlotFlags_CanvasOnly | ImPlotFlags_AntiAliased,
                                &this->m_diagramHandlePosition
                            );

                            ImPlot::PopStyleColor();
                            ImGui::PopStyleColor();

                            ImGui::NewLine();

                            // clamp the value between the start/end of the region to analyze
                            this->m_diagramHandlePosition = std::clamp<double>(
                                    this->m_diagramHandlePosition,
                                    std::ceil(this->m_analyzedRegion.getStartAddress() / double(this->m_blockSize)),
                                    std::floor(this->m_analyzedRegion.getEndAddress() / double(this->m_blockSize)));
                        }

                        // Entropy information
                        if (ImGui::BeginTable("entropy_info", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
                            ImGui::TableSetupColumn("type");
                            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                            ImGui::TableNextRow();

                            ImGui::TableNextColumn();
                            ImGui::TextFormatted("{}", "hex.builtin.view.information.block_size"_lang);
                            ImGui::TableNextColumn();
                            ImGui::TextFormatted("hex.builtin.view.information.block_size.desc"_lang, this->m_chunkBasedEntropy.getSize(), this->m_chunkBasedEntropy.getChunkSize());

                            ImGui::TableNextColumn();
                            ImGui::TextFormatted("{}", "hex.builtin.view.information.file_entropy"_lang);
                            ImGui::TableNextColumn();
                            if (this->m_averageEntropy < 0) ImGui::TextUnformatted("???");
                            else ImGui::TextFormatted("{:.8f}", this->m_averageEntropy);

                            ImGui::TableNextColumn();
                            ImGui::TextFormatted("{}", "hex.builtin.view.information.highest_entropy"_lang);
                            ImGui::TableNextColumn();
                            if (this->m_highestBlockEntropy < 0) ImGui::TextUnformatted("???");
                            else ImGui::TextFormatted("{:.8f}", this->m_highestBlockEntropy);

                            ImGui::TableNextColumn();
                            ImGui::TextFormatted("{}", "hex.builtin.view.information.plain_text_percentage"_lang);
                            ImGui::TableNextColumn();
                            if (this->m_plainTextCharacterPercentage < 0) ImGui::TextUnformatted("???");
                            else ImGui::TextFormatted("{:.8f}", this->m_plainTextCharacterPercentage);

                            ImGui::EndTable();
                        }

                        ImGui::NewLine();

                        // General information
                        if (ImGui::BeginTable("info", 1, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
                            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
                            ImGui::TableNextRow();

                            if (this->m_averageEntropy > 0.83 && this->m_highestBlockEntropy > 0.9) {
                                ImGui::TableNextColumn();
                                ImGui::TextFormattedColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "{}", "hex.builtin.view.information.encrypted"_lang);
                            }

                            if (this->m_plainTextCharacterPercentage > 99) {
                                ImGui::TableNextColumn();
                                ImGui::TextFormattedColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "{}", "hex.builtin.view.information.plain_text"_lang);
                            }

                            ImGui::EndTable();
                        }

                        ImGui::NewLine();

                        ImGui::BeginGroup();
                        {
                            ImGui::TextUnformatted("hex.builtin.view.information.digram"_lang);
                            this->m_digram.draw(ImVec2(300, 300));
                        }
                        ImGui::EndGroup();

                        ImGui::SameLine();

                        ImGui::BeginGroup();
                        {
                            ImGui::TextUnformatted("hex.builtin.view.information.layered_distribution"_lang);
                            this->m_layeredDistribution.draw(ImVec2(300, 300));
                        }
                        ImGui::EndGroup();
                    }
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

}
