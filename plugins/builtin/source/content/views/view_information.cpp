#include "content/views/view_information.hpp"

#include <hex/api/content_registry.hpp>

#include <hex/providers/provider.hpp>
#include <hex/helpers/paths.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/literals.hpp>

#include <cstring>
#include <cmath>
#include <filesystem>
#include <numeric>
#include <span>
#include <thread>
#include <vector>

#include <hex/helpers/magic.hpp>

#include <implot.h>

namespace hex::plugin::builtin {

    using namespace hex::literals;

    ViewInformation::ViewInformation() : View("hex.builtin.view.information.name") {
        EventManager::subscribe<EventDataChanged>(this, [this]() {
            this->m_dataValid           = false;
            this->m_highestBlockEntropy = 0;
            this->m_blockEntropy.clear();
            this->m_averageEntropy = 0;
            this->m_blockSize      = 0;
            this->m_valueCounts.fill(0x00);
            this->m_mimeType        = "";
            this->m_fileDescription = "";
            this->m_analyzedRegion  = { 0, 0 };
        });

        EventManager::subscribe<EventRegionSelected>(this, [this](Region region) {
            if (this->m_blockSize != 0)
                this->m_entropyHandlePosition = region.address / this->m_blockSize;
        });

        EventManager::subscribe<EventFileUnloaded>(this, [this] {
            this->m_dataValid = false;
        });

        ContentRegistry::FileHandler::add({ ".mgc" }, [](const auto &path) {
            for (const auto &destPath : hex::getPath(ImHexPath::Magic)) {
                std::error_code error;
                if (fs::copy_file(path, destPath / path.filename(), fs::copy_options::overwrite_existing, error)) {
                    View::showMessagePopup("hex.builtin.view.information.magic_db_added"_lang);
                    return true;
                }
            }

            return false;
        });
    }

    ViewInformation::~ViewInformation() {
        EventManager::unsubscribe<EventDataChanged>(this);
        EventManager::unsubscribe<EventRegionSelected>(this);
        EventManager::unsubscribe<EventFileUnloaded>(this);
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
        this->m_analyzing = true;

        std::thread([this] {
            auto provider = ImHexApi::Provider::get();

            auto task = ImHexApi::Tasks::createTask("hex.builtin.view.information.analyzing", provider->getSize());

            this->m_analyzedRegion = { provider->getBaseAddress(), provider->getBaseAddress() + provider->getSize() };

            {
                magic::compile();

                this->m_fileDescription = magic::getDescription(provider);
                this->m_mimeType        = magic::getMIMEType(provider);
            }

            this->m_dataValid = true;

            {
                this->m_blockSize = std::max<u32>(std::ceil(provider->getSize() / 2048.0F), 256);
                std::vector<u8> buffer(this->m_blockSize, 0x00);
                std::memset(this->m_valueCounts.data(), 0x00, this->m_valueCounts.size() * sizeof(u32));
                this->m_blockEntropy.clear();
                this->m_valueCounts.fill(0);

                for (u64 i = 0; i < provider->getSize(); i += this->m_blockSize) {
                    std::array<ImU64, 256> blockValueCounts = { 0 };
                    provider->read(i + provider->getBaseAddress(), buffer.data(), std::min(u64(this->m_blockSize), provider->getSize() - i));

                    for (size_t j = 0; j < this->m_blockSize; j++) {
                        blockValueCounts[buffer[j]]++;
                        this->m_valueCounts[buffer[j]]++;
                    }

                    this->m_blockEntropy.push_back(calculateEntropy(blockValueCounts, this->m_blockSize));
                    task.update(i);
                }

                this->m_averageEntropy      = calculateEntropy(this->m_valueCounts, provider->getSize());
                this->m_highestBlockEntropy = *std::max_element(this->m_blockEntropy.begin(), this->m_blockEntropy.end());
            }

            this->m_analyzing = false;
        }).detach();
    }

    void ViewInformation::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.information.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            if (ImGui::BeginChild("##scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav)) {


                auto provider = ImHexApi::Provider::get();
                if (ImHexApi::Provider::isValid() && provider->isReadable()) {
                    ImGui::TextUnformatted("hex.builtin.view.information.control"_lang);
                    ImGui::Separator();

                    ImGui::Disabled([this] {
                        if (ImGui::Button("hex.builtin.view.information.analyze"_lang))
                            this->analyze();
                    },
                        this->m_analyzing);

                    if (this->m_analyzing) {
                        ImGui::TextSpinner("hex.builtin.view.information.analyzing"_lang);
                    } else {
                        ImGui::NewLine();
                    }

                    ImGui::NewLine();
                    ImGui::TextUnformatted("hex.builtin.view.information.region"_lang);
                    ImGui::Separator();

                    for (auto &[name, value] : provider->getDataInformation()) {
                        ImGui::LabelText(name.c_str(), "%s", value.c_str());
                    }

                    if (this->m_dataValid) {

                        ImGui::LabelText("hex.builtin.view.information.region"_lang, "0x%llx - 0x%llx", this->m_analyzedRegion.first, this->m_analyzedRegion.second);

                        ImGui::NewLine();

                        if (!this->m_fileDescription.empty() || !this->m_mimeType.empty()) {
                            ImGui::TextUnformatted("hex.builtin.view.information.magic"_lang);
                            ImGui::Separator();
                        }

                        if (!this->m_fileDescription.empty()) {
                            ImGui::TextUnformatted("hex.builtin.view.information.description"_lang);
                            ImGui::TextFormattedWrapped("{}", this->m_fileDescription.c_str());
                            ImGui::NewLine();
                        }

                        if (!this->m_mimeType.empty()) {
                            ImGui::TextUnformatted("hex.builtin.view.information.mime"_lang);
                            ImGui::TextFormattedWrapped("{}", this->m_mimeType.c_str());
                            ImGui::NewLine();
                        }

                        ImGui::TextUnformatted("hex.builtin.view.information.info_analysis"_lang);
                        ImGui::Separator();

                        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImGuiCol_WindowBg));

                        ImGui::TextUnformatted("hex.builtin.view.information.distribution"_lang);
                        ImPlot::SetNextPlotLimits(0, 256, 0.5, float(*std::max_element(this->m_valueCounts.begin(), this->m_valueCounts.end())) * 1.1F, ImGuiCond_Always);
                        if (ImPlot::BeginPlot("##distribution", "Address", "Count", ImVec2(-1, 0), ImPlotFlags_NoLegend | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect, ImPlotAxisFlags_Lock, ImPlotAxisFlags_Lock | ImPlotAxisFlags_LogScale)) {
                            static auto x = [] {
                                std::array<ImU64, 256> result { 0 };
                                std::iota(result.begin(), result.end(), 0);
                                return result;
                            }();


                            ImPlot::PlotBars<ImU64>("##bytes", x.data(), this->m_valueCounts.data(), x.size(), 0.67);

                            ImPlot::EndPlot();
                        }

                        ImGui::NewLine();

                        ImGui::TextUnformatted("hex.builtin.view.information.entropy"_lang);

                        ImPlot::SetNextPlotLimits(0, this->m_blockEntropy.size(), -0.1, 1.1, ImGuiCond_Always);
                        if (ImPlot::BeginPlot("##entropy", "Address", "Entropy", ImVec2(-1, 0), ImPlotFlags_CanvasOnly | ImPlotFlags_AntiAliased, ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_Lock)) {
                            ImPlot::PlotLine("##entropy_line", this->m_blockEntropy.data(), this->m_blockEntropy.size());

                            if (ImPlot::DragLineX("Position", &this->m_entropyHandlePosition, false)) {
                                u64 address = u64(this->m_entropyHandlePosition * this->m_blockSize) + provider->getBaseAddress();
                                address     = std::min(address, provider->getBaseAddress() + provider->getSize() - 1);
                                ImHexApi::HexEditor::setSelection(address, 1);
                            }

                            ImPlot::EndPlot();
                        }

                        ImGui::PopStyleColor();

                        ImGui::NewLine();

                        ImGui::LabelText("hex.builtin.view.information.block_size"_lang, "%s", hex::format("hex.builtin.view.information.block_size.desc"_lang, this->m_blockEntropy.size(), this->m_blockSize).c_str());
                        ImGui::LabelText("hex.builtin.view.information.file_entropy"_lang, "%.8f", this->m_averageEntropy);
                        ImGui::LabelText("hex.builtin.view.information.highest_entropy"_lang, "%.8f", this->m_highestBlockEntropy);

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