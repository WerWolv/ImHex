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

    ViewInformation::ViewInformation() : View::Window("hex.builtin.view.information.name", ICON_VS_GRAPH_LINE) {
        EventDataChanged::subscribe(this, [this](prv::Provider *provider) {
            auto &analysis = m_analysis.get(provider);

            analysis.dataValid = false;
            analysis.plainTextCharacterPercentage = -1.0;
            analysis.averageEntropy = -1.0;
            analysis.highestBlockEntropy = -1.0;
            analysis.blockSize = 0;
            analysis.dataMimeType.clear();
            analysis.dataDescription.clear();
            analysis.dataAppleCreatorType.clear();
            analysis.dataExtensions.clear();
            analysis.analyzedRegion = { 0, 0 };
        });

        EventRegionSelected::subscribe(this, [this](ImHexApi::HexEditor::ProviderRegion region) {
            auto &analysis = m_analysis.get(region.getProvider());

            // Set the position of the diagram relative to the place where 
            // the user clicked inside the hex editor view 
            if (analysis.blockSize != 0) {
                analysis.byteTypesDistribution->setHandlePosition(region.getStartAddress());
                analysis.chunkBasedEntropy->setHandlePosition(region.getStartAddress());
            } 
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

        m_analysis.setOnCreateCallback([](prv::Provider *provider, AnalysisData &analysis) {
            analysis.dataValid = false;
            analysis.blockSize = 0;
            analysis.averageEntropy = -1.0;

            analysis.lowestBlockEntropy     = -1.0;
            analysis.highestBlockEntropy    = -1.0;
            analysis.lowestBlockEntropyAddress  = 0x00;
            analysis.highestBlockEntropyAddress = 0x00;

            analysis.plainTextCharacterPercentage = -1.0;

            analysis.analysisRegion = { 0, 0 };
            analysis.analyzedRegion = { 0, 0 };
            analysis.analyzedProvider = provider;

            analysis.inputChunkSize = 0;
            analysis.selectionType = ui::RegionType::EntireData;

            analysis.digram                 = std::make_shared<DiagramDigram>();
            analysis.layeredDistribution    = std::make_shared<DiagramLayeredDistribution>();
            analysis.byteDistribution       = std::make_shared<DiagramByteDistribution>();
            analysis.byteTypesDistribution  = std::make_shared<DiagramByteTypesDistribution>();
            analysis.chunkBasedEntropy      = std::make_shared<DiagramChunkBasedEntropyAnalysis>();
        });
    }

    ViewInformation::~ViewInformation() {
        EventDataChanged::unsubscribe(this);
        EventRegionSelected::unsubscribe(this);
        EventProviderOpened::unsubscribe(this);
    }

    void ViewInformation::analyze() {
        AchievementManager::unlockAchievement("hex.builtin.achievement.misc", "hex.builtin.achievement.misc.analyze_file.name");

        auto provider = ImHexApi::Provider::get();

        m_analysis.get(provider).analyzerTask = TaskManager::createTask("hex.builtin.view.information.analyzing", 0, [this, provider](auto &task) {
            auto &analysis = m_analysis.get(provider);
            const auto &region = analysis.analysisRegion;

            analysis.analyzedProvider = provider;

            if ((region.getStartAddress() >= region.getEndAddress()) || (region.getEndAddress() > provider->getActualSize())) {
                analysis.analyzedRegion = { provider->getBaseAddress(), provider->getActualSize() };
            }

            if (analysis.inputChunkSize == 0) {
                analysis.inputChunkSize = 256;
            }

            task.setMaxValue(analysis.analyzedRegion.getSize());

            {
                magic::compile();

                analysis.dataDescription       = magic::getDescription(provider);
                analysis.dataMimeType          = magic::getMIMEType(provider);
                analysis.dataAppleCreatorType  = magic::getAppleCreatorType(provider);
                analysis.dataExtensions        = magic::getExtensions(provider);
            }

            {
                analysis.blockSize = std::max<u32>(std::ceil(provider->getActualSize() / 2048.0F), 256);

                analysis.averageEntropy = -1.0;
                analysis.highestBlockEntropy = -1.0;
                analysis.plainTextCharacterPercentage = -1.0;

                // Setup / start each analysis

                analysis.byteDistribution->reset();
                analysis.digram->reset(region.getSize());
                analysis.layeredDistribution->reset(region.getSize());
                analysis.byteTypesDistribution->reset(region.getStartAddress(), region.getEndAddress(), provider->getBaseAddress(), provider->getActualSize());
                analysis.chunkBasedEntropy->reset(analysis.inputChunkSize, region.getStartAddress(), region.getEndAddress(),
                    provider->getBaseAddress(), provider->getActualSize());

                // Create a handle to the file
                auto reader = prv::ProviderReader(provider);
                reader.seek(region.getStartAddress());
                reader.setEndAddress(region.getEndAddress());

                analysis.analyzedRegion = region;

                u64 count = 0;

                // Loop over each byte of the selection and update each analysis
                // one byte at a time to process the file only once
                for (u8 byte : reader) {
                    analysis.byteDistribution->update(byte);
                    analysis.byteTypesDistribution->update(byte);
                    analysis.chunkBasedEntropy->update(byte);
                    analysis.layeredDistribution->update(byte);
                    analysis.digram->update(byte);
                    ++count;
                    task.update(count);
                }

                analysis.averageEntropy = analysis.chunkBasedEntropy->calculateEntropy(analysis.byteDistribution->get(), analysis.analyzedRegion.getSize());
                analysis.highestBlockEntropy = analysis.chunkBasedEntropy->getHighestEntropyBlockValue();
                analysis.highestBlockEntropyAddress = analysis.chunkBasedEntropy->getHighestEntropyBlockAddress();
                analysis.lowestBlockEntropy = analysis.chunkBasedEntropy->getLowestEntropyBlockValue();
                analysis.lowestBlockEntropyAddress = analysis.chunkBasedEntropy->getLowestEntropyBlockAddress();
                analysis.plainTextCharacterPercentage = analysis.byteTypesDistribution->getPlainTextCharacterPercentage();
            }

            analysis.dataValid = true;
        });
    }        

    void ViewInformation::drawContent() {
        if (ImGui::BeginChild("##scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav)) {

            auto provider = ImHexApi::Provider::get();
            if (ImHexApi::Provider::isValid() && provider->isReadable()) {
                auto &analysis = m_analysis.get(provider);

                ImGui::BeginDisabled(analysis.analyzerTask.isRunning());
                ImGuiExt::BeginSubWindow("hex.ui.common.settings"_lang);
                {
                    if (ImGui::BeginTable("SettingsTable", 2, ImGuiTableFlags_BordersInner | ImGuiTableFlags_SizingFixedSame, ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                        ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthStretch, 0.5F);
                        ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthStretch, 0.5F);
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ui::regionSelectionPicker(&analysis.analysisRegion, provider, &analysis.selectionType, false);

                        ImGui::TableNextColumn();
                        ImGuiExt::InputHexadecimal("hex.builtin.view.information.block_size"_lang, &analysis.inputChunkSize);

                        ImGui::EndTable();
                    }
                    ImGui::NewLine();

                    ImGui::SetCursorPosX(50_scaled);
                    if (ImGuiExt::DimmedButton("hex.builtin.view.information.analyze"_lang, ImVec2(ImGui::GetContentRegionAvail().x - 50_scaled, 0)))
                        this->analyze();
                }
                ImGuiExt::EndSubWindow();
                ImGui::EndDisabled();

                if (analysis.analyzerTask.isRunning()) {
                    ImGuiExt::TextSpinner("hex.builtin.view.information.analyzing"_lang);
                } else {
                    ImGui::NewLine();
                }

                if (!analysis.analyzerTask.isRunning() && analysis.dataValid && analysis.analyzedProvider != nullptr) {

                    // Provider information
                    ImGuiExt::Header("hex.builtin.view.information.provider_information"_lang, true);

                    if (ImGui::BeginTable("information", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoKeepColumnsVisible)) {
                        ImGui::TableSetupColumn("type");
                        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                        ImGui::TableNextRow();

                        for (auto &[name, value] : analysis.analyzedProvider->getDataDescription()) {
                            ImGui::TableNextColumn();
                            ImGuiExt::TextFormatted("{}", name);
                            ImGui::TableNextColumn();
                            ImGuiExt::TextFormattedWrapped("{}", value);
                        }

                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("{}", "hex.builtin.view.information.region"_lang);
                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("0x{:X} - 0x{:X}", analysis.analyzedRegion.getStartAddress(), analysis.analyzedRegion.getEndAddress());

                        ImGui::EndTable();
                    }

                    // Magic information
                    if (!(analysis.dataDescription.empty() && analysis.dataMimeType.empty())) {
                        ImGuiExt::Header("hex.builtin.view.information.magic"_lang);

                        if (ImGui::BeginTable("magic", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
                            ImGui::TableSetupColumn("type");
                            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                            ImGui::TableNextRow();

                            if (!analysis.dataDescription.empty()) {
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted("hex.builtin.view.information.description"_lang);
                                ImGui::TableNextColumn();

                                if (analysis.dataDescription == "data") {
                                    ImGuiExt::TextFormattedColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "{} ({})", "hex.builtin.view.information.octet_stream_text"_lang, analysis.dataDescription);
                                } else {
                                    ImGuiExt::TextFormattedWrapped("{}", analysis.dataDescription);
                                }
                            }

                            if (!analysis.dataMimeType.empty()) {
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted("hex.builtin.view.information.mime"_lang);
                                ImGui::TableNextColumn();

                                if (analysis.dataMimeType.contains("application/octet-stream")) {
                                    ImGuiExt::TextFormatted("{}", analysis.dataMimeType);
                                    ImGui::SameLine();
                                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                                    ImGuiExt::HelpHover("hex.builtin.view.information.octet_stream_warning"_lang);
                                    ImGui::PopStyleVar();
                                } else {
                                    ImGuiExt::TextFormatted("{}", analysis.dataMimeType);
                                }
                            }

                            if (!analysis.dataAppleCreatorType.empty()) {
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted("hex.builtin.view.information.apple_type"_lang);
                                ImGui::TableNextColumn();
                                ImGuiExt::TextFormatted("{}", analysis.dataAppleCreatorType);
                            }

                            if (!analysis.dataExtensions.empty()) {
                                ImGui::TableNextColumn();
                                ImGui::TextUnformatted("hex.builtin.view.information.extension"_lang);
                                ImGui::TableNextColumn();
                                ImGuiExt::TextFormatted("{}", analysis.dataExtensions);
                            }

                            ImGui::EndTable();
                        }

                        ImGui::NewLine();
                    }

                    // Information analysis
                    if (analysis.analyzedRegion.getSize() > 0) {

                        ImGuiExt::Header("hex.builtin.view.information.info_analysis"_lang);

                        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImGuiCol_WindowBg));
                        ImPlot::PushStyleColor(ImPlotCol_FrameBg, ImGui::GetColorU32(ImGuiCol_WindowBg));

                        // Display byte distribution analysis
                        ImGui::TextUnformatted("hex.builtin.view.information.distribution"_lang);
                        analysis.byteDistribution->draw(
                            ImVec2(-1, 0),
                            ImPlotFlags_NoLegend | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect
                        );

                        // Display byte types distribution analysis
                        ImGui::TextUnformatted("hex.builtin.view.information.byte_types"_lang);
                        analysis.byteTypesDistribution->draw(
                                ImVec2(-1, 0),
                                ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect,
                                true
                        );

                        // Display chunk-based entropy analysis
                        ImGui::TextUnformatted("hex.builtin.view.information.entropy"_lang);
                        analysis.chunkBasedEntropy->draw(
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
                        ImGuiExt::TextFormatted("hex.builtin.view.information.block_size.desc"_lang, analysis.chunkBasedEntropy->getSize(), analysis.chunkBasedEntropy->getChunkSize());

                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("{}", "hex.builtin.view.information.file_entropy"_lang);
                        ImGui::TableNextColumn();
                        if (analysis.averageEntropy < 0) {
                            ImGui::TextUnformatted("???");
                        } else {
                            auto entropy = std::abs(analysis.averageEntropy);
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
                        ImGuiExt::TextFormatted("{:.5f} @", analysis.highestBlockEntropy);
                        ImGui::SameLine();
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                        if (ImGui::Button(hex::format("0x{:06X}", analysis.highestBlockEntropyAddress).c_str())) {
                            ImHexApi::HexEditor::setSelection(analysis.highestBlockEntropyAddress, analysis.inputChunkSize);
                        }
                        ImGui::PopStyleColor();
                        ImGui::PopStyleVar();

                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("{}", "hex.builtin.view.information.lowest_entropy"_lang);
                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("{:.5f} @", analysis.lowestBlockEntropy);
                        ImGui::SameLine();
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                        if (ImGui::Button(hex::format("0x{:06X}", analysis.lowestBlockEntropyAddress).c_str())) {
                            ImHexApi::HexEditor::setSelection(analysis.lowestBlockEntropyAddress, analysis.inputChunkSize);
                        }
                        ImGui::PopStyleColor();
                        ImGui::PopStyleVar();

                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("{}", "hex.builtin.view.information.plain_text_percentage"_lang);
                        ImGui::TableNextColumn();
                        if (analysis.plainTextCharacterPercentage < 0) {
                            ImGui::TextUnformatted("???");
                        } else {
                            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.1F);
                            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImGuiCol_TableRowBgAlt));
                            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImColor::HSV(0.3F * (analysis.plainTextCharacterPercentage / 100.0F), 0.8F, 0.6F, 1.0F).Value);
                            ImGui::ProgressBar(analysis.plainTextCharacterPercentage / 100.0F, ImVec2(200_scaled, ImGui::GetTextLineHeight()));
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

                        if (analysis.averageEntropy > 0.83 && analysis.highestBlockEntropy > 0.9) {
                            ImGui::TableNextColumn();
                            ImGuiExt::TextFormattedColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "{}", "hex.builtin.view.information.encrypted"_lang);
                        }

                        if (analysis.plainTextCharacterPercentage > 95) {
                            ImGui::TableNextColumn();
                            ImGuiExt::TextFormattedColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "{}", "hex.builtin.view.information.plain_text"_lang);
                        }

                        ImGui::EndTable();
                    }

                    ImGui::BeginGroup();
                    {
                        ImGui::TextUnformatted("hex.builtin.view.information.digram"_lang);
                        analysis.digram->draw(scaled(ImVec2(300, 300)));
                    }
                    ImGui::EndGroup();

                    ImGui::SameLine();

                    ImGui::BeginGroup();
                    {
                        ImGui::TextUnformatted("hex.builtin.view.information.layered_distribution"_lang);
                        analysis.layeredDistribution->draw(scaled(ImVec2(300, 300)));
                    }
                    ImGui::EndGroup();
                }
            }
        }
        ImGui::EndChild();
    }

}
