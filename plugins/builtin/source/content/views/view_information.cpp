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
            this->m_highestBlockEntropy = 0;
            this->m_blockEntropy.clear();
            this->m_averageEntropy = 0;
            this->m_blockSize = 0;
            this->m_valueCounts.fill(0x00);
            this->m_dataMimeType.clear();
            this->m_dataDescription.clear();
            this->m_analyzedRegion  = { 0, 0 };
        });

        EventManager::subscribe<EventRegionSelected>(this, [this](Region region) {
            if (this->m_blockSize != 0)
                this->m_entropyHandlePosition = region.getStartAddress() / this->m_blockSize;
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

    static float calculateEntropy(std::array<ImU64, 256> &valueCounts, size_t blockSize) {
        float entropy = 0;

        for (auto count : valueCounts) {
            if (count == 0) continue;

            float probability = static_cast<float>(count) / blockSize;

            entropy += probability * std::log2(probability);
        }

        return (-entropy) / 8;    // log2(256) = 8
    }

    void ViewInformation::analyze() {
        this->m_analyzerTask = TaskManager::createTask("hex.builtin.view.information.analyzing", 0, [this](auto &task) {
            auto provider = ImHexApi::Provider::get();

            task.setMaxValue(provider->getActualSize());

            this->m_analyzedRegion = { provider->getBaseAddress(), provider->getBaseAddress() + provider->getSize() };

            {
                magic::compile();

                this->m_dataDescription = magic::getDescription(provider);
                this->m_dataMimeType    = magic::getMIMEType(provider);
            }

            this->m_dataValid = true;

            {
                this->m_blockSize = std::max<u32>(std::ceil(provider->getActualSize() / 2048.0F), 256);

                std::array<ImU64, 256> blockValueCounts = { 0 };

                this->m_blockEntropy.clear();
                this->m_valueCounts.fill(0);

                auto reader = prv::BufferedReader(provider);

                u64 count = 0;
                for (u8 byte : reader) {
                    this->m_valueCounts[byte]++;
                    blockValueCounts[byte]++;

                    count++;
                    if ((count % this->m_blockSize) == 0) [[unlikely]] {
                        this->m_blockEntropy.push_back(calculateEntropy(blockValueCounts, this->m_blockSize));
                        blockValueCounts = { 0 };
                        task.update(count);
                    }
                }

                this->m_averageEntropy = calculateEntropy(this->m_valueCounts, provider->getSize());
                if (!this->m_blockEntropy.empty())
                    this->m_highestBlockEntropy = *std::max_element(this->m_blockEntropy.begin(), this->m_blockEntropy.end());
                else
                    this->m_highestBlockEntropy = 0;
            }
        });
    }

    void ViewInformation::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.information.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            if (ImGui::BeginChild("##scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav)) {

                auto provider = ImHexApi::Provider::get();
                if (ImHexApi::Provider::isValid() && provider->isReadable()) {
                    ImGui::BeginDisabled(this->m_analyzerTask.isRunning());
                    {
                        if (ImGui::Button("hex.builtin.view.information.analyze"_lang, ImVec2(ImGui::GetContentRegionAvail().x, 0)))
                            this->analyze();
                    }
                    ImGui::EndDisabled();

                    if (this->m_analyzerTask.isRunning()) {
                        ImGui::TextSpinner("hex.builtin.view.information.analyzing"_lang);
                    } else {
                        ImGui::NewLine();
                    }

                    if (this->m_dataValid) {

                        // Analyzed region
                        ImGui::Header("hex.builtin.view.information.region"_lang, true);

                        if (ImGui::BeginTable("information", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
                            ImGui::TableSetupColumn("type");
                            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                            ImGui::TableNextRow();

                            for (auto &[name, value] : provider->getDataInformation()) {
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

                            ImGui::TextUnformatted("hex.builtin.view.information.distribution"_lang);
                            if (ImPlot::BeginPlot("##distribution", ImVec2(-1, 0), ImPlotFlags_NoChild | ImPlotFlags_NoLegend | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect)) {
                                ImPlot::SetupAxes("Address", "Count", ImPlotAxisFlags_Lock, ImPlotAxisFlags_Lock | ImPlotAxisFlags_LogScale);
                                ImPlot::SetupAxesLimits(0, 256, 1, double(*std::max_element(this->m_valueCounts.begin(), this->m_valueCounts.end())) * 1.1F, ImGuiCond_Always);

                                static auto x = [] {
                                    std::array<ImU64, 256> result { 0 };
                                    std::iota(result.begin(), result.end(), 0);
                                    return result;
                                }();

                                ImPlot::PlotBars<ImU64>("##bytes", x.data(), this->m_valueCounts.data(), x.size(), 1.0);

                                ImPlot::EndPlot();
                            }

                            ImGui::NewLine();

                            ImGui::TextUnformatted("hex.builtin.view.information.entropy"_lang);

                            if (ImPlot::BeginPlot("##entropy", ImVec2(-1, 0), ImPlotFlags_NoChild | ImPlotFlags_CanvasOnly)) {
                                ImPlot::SetupAxes("Address", "Entropy", ImPlotAxisFlags_Lock, ImPlotAxisFlags_Lock);
                                ImPlot::SetupAxesLimits(0, this->m_blockEntropy.size(), -0.1F, 1.1F, ImGuiCond_Always);

                                ImPlot::PlotLine("##entropy_line", this->m_blockEntropy.data(), this->m_blockEntropy.size());

                                if (ImPlot::DragLineX(1, &this->m_entropyHandlePosition, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                                    u64 address = u64(std::max<double>(this->m_entropyHandlePosition, 0) * this->m_blockSize) + provider->getBaseAddress();
                                    address     = std::min(address, provider->getBaseAddress() + provider->getSize() - 1);
                                    ImHexApi::HexEditor::setSelection(address, 1);
                                }

                                ImPlot::EndPlot();
                            }

                            ImPlot::PopStyleColor();
                            ImGui::PopStyleColor();

                            ImGui::NewLine();
                        }

                        // Entropy information
                        if (ImGui::BeginTable("entropy", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
                            ImGui::TableSetupColumn("type");
                            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                            ImGui::TableNextRow();

                            ImGui::TableNextColumn();
                            ImGui::TextFormatted("{}", "hex.builtin.view.information.block_size"_lang);
                            ImGui::TableNextColumn();
                            ImGui::TextFormatted("hex.builtin.view.information.block_size.desc"_lang, this->m_blockEntropy.size(), this->m_blockSize);

                            ImGui::TableNextColumn();
                            ImGui::TextFormatted("{}", "hex.builtin.view.information.file_entropy"_lang);
                            ImGui::TableNextColumn();
                            ImGui::TextFormatted("{:.8f}", this->m_averageEntropy);

                            ImGui::TableNextColumn();
                            ImGui::TextFormatted("{}", "hex.builtin.view.information.highest_entropy"_lang);
                            ImGui::TableNextColumn();
                            ImGui::TextFormatted("{:.8f}", this->m_highestBlockEntropy);

                            ImGui::EndTable();
                        }

                        if (this->m_averageEntropy > 0.83 && this->m_highestBlockEntropy > 0.9) {
                            ImGui::NewLine();
                            ImGui::TextFormattedColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "{}", "hex.builtin.view.information.encrypted"_lang);
                        }
                    }
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

}