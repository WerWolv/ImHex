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

#include <toasts/toast_notification.hpp>

namespace hex::plugin::builtin {

    using namespace hex::literals;

    ViewInformation::ViewInformation() : View::Window("hex.builtin.view.information.name") {
        EventDataChanged::subscribe(this, [this] {
            m_dataValid = false;
            m_plainTextCharacterPercentage = -1.0;
            m_averageEntropy = -1.0;
            m_highestBlockEntropy = -1.0;
            m_blockSize = 0;
            m_dataMimeType.clear();
            m_dataDescription.clear();
            m_analyzedRegion = { 0, 0 };
        });

        EventRegionSelected::subscribe(this, [this](Region region) {
            // Set the position of the diagram relative to the place where 
            // the user clicked inside the hex editor view 
            if (m_blockSize != 0) {
                m_byteTypesDistribution.setHandlePosition(region.getStartAddress());
                m_chunkBasedEntropy.setHandlePosition(region.getStartAddress());
            } 
        });

        EventProviderDeleted::subscribe(this, [this](const auto*) {
            m_dataValid = false;
        });

        ContentRegistry::FileHandler::add({ ".mgc" }, [](const auto &path) {
            for (const auto &destPath : fs::getDefaultPaths(fs::ImHexPath::Magic)) {
                if (wolv::io::fs::copyFile(path, destPath / path.filename(), std::fs::copy_options::overwrite_existing)) {
                    ui::ToastInfo::open("hex.builtin.view.information.magic_db_added"_lang);
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

        m_analyzerTask = TaskManager::createTask("hex.builtin.view.information.analyzing", 0, [this](auto &task) {
            auto provider = ImHexApi::Provider::get();

            if ((m_analyzedRegion.getStartAddress() >= m_analyzedRegion.getEndAddress()) || (m_analyzedRegion.getEndAddress() > provider->getActualSize())) {
                m_analyzedRegion = { provider->getBaseAddress(), provider->getActualSize() };
            }

            if (m_inputChunkSize == 0) {
                m_inputChunkSize = 256;
            }

            task.setMaxValue(m_analyzedRegion.getSize());

            {
                magic::compile();

                m_dataDescription = magic::getDescription(provider);
                m_dataMimeType    = magic::getMIMEType(provider);
            }

            {
                m_blockSize = std::max<u32>(std::ceil(provider->getActualSize() / 2048.0F), 256);

                m_averageEntropy = -1.0;
                m_highestBlockEntropy = -1.0;
                m_plainTextCharacterPercentage = -1.0;

                // Setup / start each analysis

                m_byteDistribution.reset();
                m_digram.reset(m_analysisRegion.getSize());
                m_layeredDistribution.reset(m_analysisRegion.getSize());
                m_byteTypesDistribution.reset(m_analysisRegion.getStartAddress(), m_analysisRegion.getEndAddress(), provider->getBaseAddress(), provider->getActualSize());
                m_chunkBasedEntropy.reset(m_inputChunkSize, m_analysisRegion.getStartAddress(), m_analysisRegion.getEndAddress(),
                    provider->getBaseAddress(), provider->getActualSize());

                // Create a handle to the file
                auto reader = prv::ProviderReader(provider);
                reader.seek(m_analysisRegion.getStartAddress());
                reader.setEndAddress(m_analysisRegion.getEndAddress());

                m_analyzedRegion = m_analysisRegion;

                u64 count = 0;

                // Loop over each byte of the selection and update each analysis
                // one byte at a time to process the file only once
                for (u8 byte : reader) {
                    m_byteDistribution.update(byte);
                    m_byteTypesDistribution.update(byte);
                    m_chunkBasedEntropy.update(byte);
                    m_layeredDistribution.update(byte);
                    m_digram.update(byte);
                    ++count;
                    task.update(count);
                }

                m_averageEntropy = m_chunkBasedEntropy.calculateEntropy(m_byteDistribution.get(), m_analyzedRegion.getSize());
                m_highestBlockEntropy = m_chunkBasedEntropy.getHighestEntropyBlockValue();
                m_highestBlockEntropyAddress = m_chunkBasedEntropy.getHighestEntropyBlockAddress();
                m_lowestBlockEntropy = m_chunkBasedEntropy.getLowestEntropyBlockValue();
                m_lowestBlockEntropyAddress = m_chunkBasedEntropy.getLowestEntropyBlockAddress();
                m_plainTextCharacterPercentage = m_byteTypesDistribution.getPlainTextCharacterPercentage();
            }
                
            m_dataValid = true;
        });
    }        

    void ViewInformation::drawContent() {
        if (ImGui::BeginChild("##scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav)) {

            auto provider = ImHexApi::Provider::get();
            if (ImHexApi::Provider::isValid() && provider->isReadable()) {
                ImGui::BeginDisabled(m_analyzerTask.isRunning());
                ImGuiExt::BeginSubWindow("hex.ui.common.settings"_lang);
                {
                    if (ImGui::BeginTable("SettingsTable", 2, ImGuiTableFlags_BordersInner | ImGuiTableFlags_SizingFixedSame, ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                        ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthStretch, 0.5F);
                        ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthStretch, 0.5F);
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ui::regionSelectionPicker(&m_analysisRegion, provider, &m_selectionType, false);

                        ImGui::TableNextColumn();
                        ImGuiExt::InputHexadecimal("hex.builtin.view.information.block_size"_lang, &m_inputChunkSize);

                        ImGui::EndTable();
                    }
                    ImGui::NewLine();

                    ImGui::SetCursorPosX(50_scaled);
                    if (ImGuiExt::DimmedButton("hex.builtin.view.information.analyze"_lang, ImVec2(ImGui::GetContentRegionAvail().x - 50_scaled, 0)))
                        this->analyze();
                }
                ImGuiExt::EndSubWindow();
                ImGui::EndDisabled();

                if (m_analyzerTask.isRunning()) {
                    ImGuiExt::TextSpinner("hex.builtin.view.information.analyzing"_lang);
                } else {
                    ImGui::NewLine();
                }

                if (!m_analyzerTask.isRunning() && m_dataValid) {

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
                        ImGuiExt::TextFormatted("0x{:X} - 0x{:X}", m_analyzedRegion.getStartAddress(), m_analyzedRegion.getEndAddress());

                        ImGui::EndTable();
                    }

                    // Magic information
                    if (!(m_dataDescription.empty() && m_dataMimeType.empty())) {
                        ImGuiExt::Header("hex.builtin.view.information.magic"_lang);

                        if (ImGui::BeginTable("magic", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
                            ImGui::TableSetupColumn("type");
                            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                            ImGui::TableNextRow();

                            if (!m_dataDescription.empty()) {
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted("hex.builtin.view.information.description"_lang);
                                ImGui::TableNextColumn();

                                if (m_dataDescription == "data") {
                                    ImGuiExt::TextFormattedColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "{} ({})", "hex.builtin.view.information.octet_stream_text"_lang, m_dataDescription);
                                } else {
                                    ImGuiExt::TextFormattedWrapped("{}", m_dataDescription);
                                }
                            }

                            if (!m_dataMimeType.empty()) {
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted("hex.builtin.view.information.mime"_lang);
                                ImGui::TableNextColumn();

                                if (m_dataMimeType == "application/octet-stream") {
                                    ImGuiExt::TextFormattedColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "{} ({})", "hex.builtin.view.information.octet_stream_text"_lang, m_dataMimeType);
                                    ImGui::SameLine();
                                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                                    ImGuiExt::HelpHover("hex.builtin.view.information.octet_stream_warning"_lang);
                                    ImGui::PopStyleVar();
                                } else {
                                    ImGuiExt::TextFormatted("{}", m_dataMimeType);
                                }
                            }

                            ImGui::EndTable();
                        }

                        ImGui::NewLine();
                    }

                    // Information analysis
                    if (m_analyzedRegion.getSize() > 0) {

                        ImGuiExt::Header("hex.builtin.view.information.info_analysis"_lang);

                        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImGuiCol_WindowBg));
                        ImPlot::PushStyleColor(ImPlotCol_FrameBg, ImGui::GetColorU32(ImGuiCol_WindowBg));

                        // Display byte distribution analysis
                        ImGui::TextUnformatted("hex.builtin.view.information.distribution"_lang);
                        m_byteDistribution.draw(
                            ImVec2(-1, 0),
                            ImPlotFlags_NoLegend | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect
                        );

                        // Display byte types distribution analysis
                        ImGui::TextUnformatted("hex.builtin.view.information.byte_types"_lang);
                        m_byteTypesDistribution.draw(
                                ImVec2(-1, 0),
                                ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect,
                                true
                        );

                        // Display chunk-based entropy analysis
                        ImGui::TextUnformatted("hex.builtin.view.information.entropy"_lang);
                        m_chunkBasedEntropy.draw(
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
                        ImGuiExt::TextFormatted("hex.builtin.view.information.block_size.desc"_lang, m_chunkBasedEntropy.getSize(), m_chunkBasedEntropy.getChunkSize());

                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("{}", "hex.builtin.view.information.file_entropy"_lang);
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
                        ImGuiExt::TextFormatted("{}", "hex.builtin.view.information.highest_entropy"_lang);
                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("{:.5f} @", m_highestBlockEntropy);
                        ImGui::SameLine();
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                        if (ImGui::Button(hex::format("0x{:06X}", m_highestBlockEntropyAddress).c_str())) {
                            ImHexApi::HexEditor::setSelection(m_highestBlockEntropyAddress, m_inputChunkSize);
                        }
                        ImGui::PopStyleColor();
                        ImGui::PopStyleVar();

                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("{}", "hex.builtin.view.information.lowest_entropy"_lang);
                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("{:.5f} @", m_lowestBlockEntropy);
                        ImGui::SameLine();
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                        if (ImGui::Button(hex::format("0x{:06X}", m_lowestBlockEntropyAddress).c_str())) {
                            ImHexApi::HexEditor::setSelection(m_lowestBlockEntropyAddress, m_inputChunkSize);
                        }
                        ImGui::PopStyleColor();
                        ImGui::PopStyleVar();

                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("{}", "hex.builtin.view.information.plain_text_percentage"_lang);
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

                    ImGui::NewLine();

                    // General information
                    if (ImGui::BeginTable("info", 1, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
                        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableNextRow();

                        if (m_averageEntropy > 0.83 && m_highestBlockEntropy > 0.9) {
                            ImGui::TableNextColumn();
                            ImGuiExt::TextFormattedColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "{}", "hex.builtin.view.information.encrypted"_lang);
                        }

                        if (m_plainTextCharacterPercentage > 95) {
                            ImGui::TableNextColumn();
                            ImGuiExt::TextFormattedColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "{}", "hex.builtin.view.information.plain_text"_lang);
                        }

                        ImGui::EndTable();
                    }

                    ImGui::BeginGroup();
                    {
                        ImGui::TextUnformatted("hex.builtin.view.information.digram"_lang);
                        m_digram.draw(scaled(ImVec2(300, 300)));
                    }
                    ImGui::EndGroup();

                    ImGui::SameLine();

                    ImGui::BeginGroup();
                    {
                        ImGui::TextUnformatted("hex.builtin.view.information.layered_distribution"_lang);
                        m_layeredDistribution.draw(scaled(ImVec2(300, 300)));
                    }
                    ImGui::EndGroup();
                }
            }
        }
        ImGui::EndChild();
    }

}
