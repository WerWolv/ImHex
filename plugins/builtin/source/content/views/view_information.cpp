#include "content/views/view_information.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/api/achievement_manager.hpp>

#include <hex/providers/provider.hpp>
#include <hex/providers/buffered_reader.hpp>

#include <hex/helpers/fs.hpp>
#include <hex/helpers/magic.hpp>

#include <cmath>
#include <filesystem>

#include <implot.h>

#include <content/popups/popup_notification.hpp>

namespace hex::plugin::builtin {

    using namespace hex::literals;

    ViewInformation::ViewInformation() : View::Window("hex.builtin.view.information.name") {
        EventDataChanged::subscribe(this, [this] {
            this->m_dataValid = false;
            this->m_plainTextCharacterPercentage = -1.0;
            this->m_averageEntropy = -1.0;
            this->m_highestBlockEntropy = -1.0;
            this->m_blockSize = 0;
            this->m_dataMimeType.clear();
            this->m_dataDescription.clear();
            this->m_analyzedRegion = { 0, 0 };
        });

        EventRegionSelected::subscribe(this, [this](Region region) {
            // Set the position of the diagram relative to the place where 
            // the user clicked inside the hex editor view 
            if (this->m_blockSize != 0) {
                this->m_byteTypesDistribution.setHandlePosition(region.getStartAddress());
                this->m_chunkBasedEntropy.setHandlePosition(region.getStartAddress());
            } 
        });

        EventProviderDeleted::subscribe(this, [this](const auto*) {
            this->m_dataValid = false;
        });

        ContentRegistry::FileHandler::add({ ".mgc" }, [](const auto &path) {
            for (const auto &destPath : fs::getDefaultPaths(fs::ImHexPath::Magic)) {
                if (wolv::io::fs::copyFile(path, destPath / path.filename(), std::fs::copy_options::overwrite_existing)) {
                    PopupInfo::open("hex.builtin.view.information.magic_db_added"_lang);
                    return true;
                }
            }

            return false;
        });
    }

    ViewInformation::~ViewInformation() {
        EventDataChanged::unsubscribe(this);
        EventRegionSelected::unsubscribe(this);
        EventProviderDeleted::unsubscribe(this);
    }

    void ViewInformation::analyze() {
        AchievementManager::unlockAchievement("hex.builtin.achievement.misc", "hex.builtin.achievement.misc.analyze_file.name");

        this->m_analyzerTask = TaskManager::createTask("hex.builtin.view.information.analyzing", 0, [this](auto &task) {
            auto provider = ImHexApi::Provider::get();

            if ((this->m_analyzedRegion.getStartAddress() >= this->m_analyzedRegion.getEndAddress()) || (this->m_analyzedRegion.getEndAddress() > provider->getActualSize())) {
                this->m_analyzedRegion = { provider->getBaseAddress(), provider->getActualSize() };
            }

            if (this->m_inputChunkSize == 0) {
                this->m_inputChunkSize = 256;
            }

            task.setMaxValue(this->m_analyzedRegion.getSize());

            {
                magic::compile();

                this->m_dataDescription = magic::getDescription(provider);
                this->m_dataMimeType    = magic::getMIMEType(provider);
            }

            {
                this->m_blockSize = std::max<u32>(std::ceil(provider->getActualSize() / 2048.0F), 256);

                this->m_averageEntropy = -1.0;
                this->m_highestBlockEntropy = -1.0;
                this->m_plainTextCharacterPercentage = -1.0;

                // Setup / start each analysis

                this->m_byteDistribution.reset();
                this->m_digram.reset(this->m_analysisRegion.getSize());
                this->m_layeredDistribution.reset(this->m_analysisRegion.getSize());
                this->m_byteTypesDistribution.reset(this->m_analysisRegion.getStartAddress(), this->m_analysisRegion.getEndAddress(), provider->getBaseAddress(), provider->getActualSize());
                this->m_chunkBasedEntropy.reset(this->m_inputChunkSize, this->m_analysisRegion.getStartAddress(), this->m_analysisRegion.getEndAddress(),
                    provider->getBaseAddress(), provider->getActualSize());

                // Create a handle to the file
                auto reader = prv::ProviderReader(provider);
                reader.seek(this->m_analysisRegion.getStartAddress());
                reader.setEndAddress(this->m_analysisRegion.getEndAddress());

                this->m_analyzedRegion = this->m_analysisRegion;

                u64 count = 0;

                // Loop over each byte of the selection and update each analysis
                // one byte at a time to process the file only once
                for (u8 byte : reader) {
                    this->m_byteDistribution.update(byte);
                    this->m_byteTypesDistribution.update(byte);
                    this->m_chunkBasedEntropy.update(byte);
                    this->m_layeredDistribution.update(byte);
                    this->m_digram.update(byte);
                    ++count;
                    task.update(count);
                }

                this->m_averageEntropy = this->m_chunkBasedEntropy.calculateEntropy(this->m_byteDistribution.get(), this->m_analyzedRegion.getSize());
                this->m_highestBlockEntropy = this->m_chunkBasedEntropy.getHighestEntropyBlockValue();
                this->m_highestBlockEntropyAddress = this->m_chunkBasedEntropy.getHighestEntropyBlockAddress();
                this->m_lowestBlockEntropy = this->m_chunkBasedEntropy.getLowestEntropyBlockValue();
                this->m_lowestBlockEntropyAddress = this->m_chunkBasedEntropy.getLowestEntropyBlockAddress();
                this->m_plainTextCharacterPercentage = this->m_byteTypesDistribution.getPlainTextCharacterPercentage();
            }
                
            this->m_dataValid = true;
        });
    }        

    void ViewInformation::drawContent() {
        if (ImGui::BeginChild("##scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav)) {

            auto provider = ImHexApi::Provider::get();
            if (ImHexApi::Provider::isValid() && provider->isReadable()) {
                ImGui::BeginDisabled(this->m_analyzerTask.isRunning());
                ImGuiExt::BeginSubWindow("hex.builtin.common.settings"_lang);
                {
                    if (ImGui::BeginTable("SettingsTable", 2, ImGuiTableFlags_BordersInner | ImGuiTableFlags_SizingFixedSame, ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                        ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthStretch, 0.5F);
                        ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthStretch, 0.5F);
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ui::regionSelectionPicker(&this->m_analysisRegion, provider, &this->m_selectionType, false);

                        ImGui::TableNextColumn();
                        ImGuiExt::InputHexadecimal("hex.builtin.view.information.block_size"_lang, &this->m_inputChunkSize);

                        ImGui::EndTable();
                    }
                    ImGui::NewLine();

                    ImGui::SetCursorPosX(50_scaled);
                    if (ImGuiExt::DimmedButton("hex.builtin.view.information.analyze"_lang, ImVec2(ImGui::GetContentRegionAvail().x - 50_scaled, 0)))
                        this->analyze();


                }
                ImGuiExt::EndSubWindow();
                ImGui::EndDisabled();

                if (this->m_analyzerTask.isRunning()) {
                    ImGuiExt::TextSpinner("hex.builtin.view.information.analyzing"_lang);
                } else {
                    ImGui::NewLine();
                }

                if (!this->m_analyzerTask.isRunning() && this->m_dataValid) {

                    // Provider information
                    ImGuiExt::Header("hex.builtin.view.information.provider_information"_lang, true);

                    if (ImGui::BeginTable("information", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoKeepColumnsVisible)) {
                        ImGui::TableSetupColumn("type");
                        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                        ImGui::TableNextRow();

                        for (auto &[name, value] : provider->getDataDescription()) {
                            ImGui::TableNextColumn();
                            ImGuiExt::TextFormatted("{}", name);
                            ImGui::TableNextColumn();
                            ImGuiExt::TextFormattedWrapped("{}", value);
                        }

                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("{}", "hex.builtin.view.information.region"_lang);
                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("0x{:X} - 0x{:X}", this->m_analyzedRegion.getStartAddress(), this->m_analyzedRegion.getEndAddress());

                        ImGui::EndTable();
                    }

                    // Magic information
                    if (!(this->m_dataDescription.empty() && this->m_dataMimeType.empty())) {
                        ImGuiExt::Header("hex.builtin.view.information.magic"_lang);

                        if (ImGui::BeginTable("magic", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
                            ImGui::TableSetupColumn("type");
                            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                            ImGui::TableNextRow();

                            if (!this->m_dataDescription.empty()) {
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted("hex.builtin.view.information.description"_lang);
                                ImGui::TableNextColumn();

                                if (this->m_dataDescription == "data") {
                                    ImGuiExt::TextFormattedColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "{} ({})", "hex.builtin.view.information.octet_stream_text"_lang, this->m_dataDescription);
                                } else {
                                    ImGuiExt::TextFormattedWrapped("{}", this->m_dataDescription);
                                }
                            }

                            if (!this->m_dataMimeType.empty()) {
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted("hex.builtin.view.information.mime"_lang);
                                ImGui::TableNextColumn();

                                if (this->m_dataMimeType == "application/octet-stream") {
                                    ImGuiExt::TextFormattedColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "{} ({})", "hex.builtin.view.information.octet_stream_text"_lang, this->m_dataMimeType);
                                    ImGui::SameLine();
                                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                                    ImGuiExt::HelpHover("hex.builtin.view.information.octet_stream_warning"_lang);
                                    ImGui::PopStyleVar();
                                } else {
                                    ImGuiExt::TextFormatted("{}", this->m_dataMimeType);
                                }
                            }

                            ImGui::EndTable();
                        }

                        ImGui::NewLine();
                    }

                    // Information analysis
                    if (this->m_analyzedRegion.getSize() > 0) {

                        ImGuiExt::Header("hex.builtin.view.information.info_analysis"_lang);

                        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImGuiCol_WindowBg));
                        ImPlot::PushStyleColor(ImPlotCol_FrameBg, ImGui::GetColorU32(ImGuiCol_WindowBg));

                        // Display byte distribution analysis
                        ImGui::TextUnformatted("hex.builtin.view.information.distribution"_lang);
                        this->m_byteDistribution.draw(
                            ImVec2(-1, 0),
                            ImPlotFlags_NoLegend | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect
                        );

                        // Display byte types distribution analysis
                        ImGui::TextUnformatted("hex.builtin.view.information.byte_types"_lang);
                        this->m_byteTypesDistribution.draw(
                                ImVec2(-1, 0),
                                ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect,
                                true
                        );

                        // Display chunk-based entropy analysis
                        ImGui::TextUnformatted("hex.builtin.view.information.entropy"_lang);
                        this->m_chunkBasedEntropy.draw(
                            ImVec2(-1, 0),
                            ImPlotFlags_NoLegend | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect,
                            true
                        );

                        ImPlot::PopStyleColor();
                        ImGui::PopStyleColor();

                        ImGui::NewLine();
                    }

                    // Entropy information
                    if (ImGui::BeginTable("entropy_info", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
                        ImGui::TableSetupColumn("type");
                        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                        ImGui::TableNextRow();

                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("{}", "hex.builtin.view.information.block_size"_lang);
                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("hex.builtin.view.information.block_size.desc"_lang, this->m_chunkBasedEntropy.getSize(), this->m_chunkBasedEntropy.getChunkSize());

                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("{}", "hex.builtin.view.information.file_entropy"_lang);
                        ImGui::TableNextColumn();
                        if (this->m_averageEntropy < 0)
                            ImGui::TextUnformatted("???");
                        else {
                            auto entropy = std::abs(this->m_averageEntropy);
                            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.1F);
                            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImGuiCol_TableRowBgAlt));
                            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImColor::HSV(0.3F - (0.3F * entropy), 0.6F, 0.8F, 1.0F).Value);
                            ImGui::ProgressBar(entropy, ImVec2(200_scaled, ImGui::GetTextLineHeight()), hex::format("{:.5f}", entropy).c_str());
                            ImGui::PopStyleColor(2);
                            ImGui::PopStyleVar();
                        }

                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("{}", "hex.builtin.view.information.highest_entropy"_lang);
                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("{:.5f} @", this->m_highestBlockEntropy);
                        ImGui::SameLine();
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                        if (ImGui::Button(hex::format("0x{:06X}", this->m_highestBlockEntropyAddress).c_str())) {
                            ImHexApi::HexEditor::setSelection(this->m_highestBlockEntropyAddress, this->m_inputChunkSize);
                        }
                        ImGui::PopStyleColor();
                        ImGui::PopStyleVar();

                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("{}", "hex.builtin.view.information.lowest_entropy"_lang);
                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("{:.5f} @", this->m_lowestBlockEntropy);
                        ImGui::SameLine();
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                        if (ImGui::Button(hex::format("0x{:06X}", this->m_lowestBlockEntropyAddress).c_str())) {
                            ImHexApi::HexEditor::setSelection(this->m_lowestBlockEntropyAddress, this->m_inputChunkSize);
                        }
                        ImGui::PopStyleColor();
                        ImGui::PopStyleVar();

                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("{}", "hex.builtin.view.information.plain_text_percentage"_lang);
                        ImGui::TableNextColumn();
                        if (this->m_plainTextCharacterPercentage < 0)
                            ImGui::TextUnformatted("???");
                        else {
                            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.1F);
                            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImGuiCol_TableRowBgAlt));
                            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImColor::HSV(0.3F * (this->m_plainTextCharacterPercentage / 100.0F), 0.8F, 0.6F, 1.0F).Value);
                            ImGui::ProgressBar(this->m_plainTextCharacterPercentage / 100.0F, ImVec2(200_scaled, ImGui::GetTextLineHeight()));
                            ImGui::PopStyleColor(2);
                            ImGui::PopStyleVar();
                        }

                        ImGui::EndTable();
                    }

                    ImGui::NewLine();

                    // General information
                    if (ImGui::BeginTable("info", 1, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
                        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableNextRow();

                        if (this->m_averageEntropy > 0.83 && this->m_highestBlockEntropy > 0.9) {
                            ImGui::TableNextColumn();
                            ImGuiExt::TextFormattedColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "{}", "hex.builtin.view.information.encrypted"_lang);
                        }

                        if (this->m_plainTextCharacterPercentage > 95) {
                            ImGui::TableNextColumn();
                            ImGuiExt::TextFormattedColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "{}", "hex.builtin.view.information.plain_text"_lang);
                        }

                        ImGui::EndTable();
                    }

                    ImGui::BeginGroup();
                    {
                        ImGui::TextUnformatted("hex.builtin.view.information.digram"_lang);
                        this->m_digram.draw(scaled(ImVec2(300, 300)));
                    }
                    ImGui::EndGroup();

                    ImGui::SameLine();

                    ImGui::BeginGroup();
                    {
                        ImGui::TextUnformatted("hex.builtin.view.information.layered_distribution"_lang);
                        this->m_layeredDistribution.draw(scaled(ImVec2(300, 300)));
                    }
                    ImGui::EndGroup();
                }
            }
        }
        ImGui::EndChild();
    }

}
