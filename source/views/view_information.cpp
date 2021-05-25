#include "views/view_information.hpp"

#include <hex/providers/provider.hpp>
#include <hex/helpers/utils.hpp>

#include <cstring>
#include <cmath>
#include <filesystem>
#include <span>
#include <thread>
#include <vector>

#include <magic.h>

#include <imgui_imhex_extensions.h>
#include <implot.h>

namespace hex {

    ViewInformation::ViewInformation() : View("hex.view.information.name") {
        EventManager::subscribe<EventDataChanged>(this, [this]() {
            this->m_dataValid = false;
            this->m_highestBlockEntropy = 0;
            this->m_blockEntropy.clear();
            this->m_averageEntropy = 0;
            this->m_blockSize = 0;
            this->m_valueCounts.fill(0x00);
            this->m_mimeType = "";
            this->m_fileDescription = "";
            this->m_analyzedRegion = { 0, 0 };
        });

        EventManager::subscribe<EventRegionSelected>(this, [this](Region region) {
            if (this->m_blockSize != 0)
                this->m_entropyHandlePosition = region.address / this->m_blockSize;
        });
    }

    ViewInformation::~ViewInformation() {
        EventManager::unsubscribe<EventDataChanged>(this);
        EventManager::unsubscribe<EventRegionSelected>(this);
    }

    static float calculateEntropy(std::array<ImU64, 256> &valueCounts, size_t numBytes) {
        float entropy = 0;

        if (numBytes == 0)
            return 0.0F;

        std::array<float, 256> floatValueCounts{ 0 };
        std::copy(valueCounts.begin(), valueCounts.end(), floatValueCounts.begin());

        for (u16 i = 0; i < 256; i++) {
            floatValueCounts[i] /= float(numBytes);

            if (floatValueCounts[i] > 0)
                entropy -= (floatValueCounts[i] * std::log2(floatValueCounts[i]));
        }

        return entropy / 8;
    }

    void ViewInformation::analyze() {
        this->m_analyzing = true;

        std::thread([this]{
            auto provider = SharedData::currentProvider;

            this->m_analyzedRegion = { provider->getBaseAddress(), provider->getBaseAddress() + provider->getSize() };

            {
                this->m_blockSize = std::max<u32>(std::ceil(provider->getSize() / 2048.0F), 256);
                std::vector<u8> buffer(this->m_blockSize, 0x00);
                std::memset(this->m_valueCounts.data(), 0x00, this->m_valueCounts.size() * sizeof(u32));
                this->m_blockEntropy.clear();
                this->m_valueCounts.fill(0);

                for (u64 i = 0; i < provider->getSize(); i += this->m_blockSize) {
                    std::array<ImU64, 256> blockValueCounts = { 0 };
                    provider->readRelative(i, buffer.data(), std::min(u64(this->m_blockSize), provider->getSize() - i));

                    for (size_t j = 0; j < this->m_blockSize; j++) {
                        blockValueCounts[buffer[j]]++;
                        this->m_valueCounts[buffer[j]]++;
                    }
                    this->m_blockEntropy.push_back(calculateEntropy(blockValueCounts, this->m_blockSize));
                }

                this->m_averageEntropy = calculateEntropy(this->m_valueCounts, provider->getSize());
                this->m_highestBlockEntropy = *std::max_element(this->m_blockEntropy.begin(), this->m_blockEntropy.end());
            }

            {
                std::vector<u8> buffer(provider->getSize(), 0x00);
                provider->readRelative(0x00, buffer.data(), buffer.size());

                this->m_fileDescription.clear();
                this->m_mimeType.clear();

                std::string magicFiles;

                std::error_code error;
                for (const auto &dir : hex::getPath(ImHexPath::Magic)) {
                    for (const auto &entry : std::filesystem::directory_iterator(dir, error)) {
                        if (entry.is_regular_file() && entry.path().extension() == ".mgc")
                            magicFiles += entry.path().string() + MAGIC_PATH_SEPARATOR;
                    }
                }

                if (!error) {
                    magicFiles.pop_back();

                    {
                        magic_t cookie = magic_open(MAGIC_NONE);
                        if (magic_load(cookie, magicFiles.c_str()) != -1)
                            this->m_fileDescription = magic_buffer(cookie, buffer.data(), buffer.size());
                        else
                            this->m_fileDescription = "";

                        magic_close(cookie);
                    }

                    {
                        magic_t cookie = magic_open(MAGIC_MIME);
                        if (magic_load(cookie, magicFiles.c_str()) != -1)
                            this->m_mimeType = magic_buffer(cookie, buffer.data(), buffer.size());
                        else
                            this->m_mimeType = "";

                        magic_close(cookie);
                    }

                    this->m_dataValid = true;
                }
            }

            this->m_analyzing = false;
        }).detach();
    }

    void ViewInformation::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.view.information.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            ImGui::BeginChild("##scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);

            auto provider = SharedData::currentProvider;
            if (provider != nullptr && provider->isReadable()) {
                ImGui::TextUnformatted("hex.view.information.control"_lang);
                ImGui::Separator();

                ImGui::Disabled([this] {
                    if (ImGui::Button("hex.view.information.analyze"_lang))
                        this->analyze();
                }, this->m_analyzing);

                if (this->m_analyzing) {
                    ImGui::SameLine();
                    ImGui::TextSpinner("hex.view.information.analyzing"_lang);
                }

                if (this->m_dataValid) {

                    ImGui::NewLine();
                    ImGui::TextUnformatted("hex.view.information.region"_lang);
                    ImGui::Separator();

                    for (auto &[name, value] : (SharedData::currentProvider)->getDataInformation()) {
                        ImGui::LabelText(name.c_str(), "%s", value.c_str());
                    }

                    ImGui::LabelText("hex.view.information.region"_lang, "0x%llx - 0x%llx", this->m_analyzedRegion.first, this->m_analyzedRegion.second);

                    ImGui::NewLine();

                    if (!this->m_fileDescription.empty() || !this->m_mimeType.empty()) {
                        ImGui::TextUnformatted("hex.view.information.magic"_lang);
                        ImGui::Separator();
                    }

                    if (!this->m_fileDescription.empty()) {
                        ImGui::TextUnformatted("hex.view.information.description"_lang);
                        ImGui::TextWrapped("%s", this->m_fileDescription.c_str());
                        ImGui::NewLine();
                    }

                    if (!this->m_mimeType.empty()) {
                        ImGui::TextUnformatted("hex.view.information.mime"_lang);
                        ImGui::TextWrapped("%s", this->m_mimeType.c_str());
                        ImGui::NewLine();
                    }

                    ImGui::TextUnformatted("hex.view.information.info_analysis"_lang);
                    ImGui::Separator();

                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImGuiCol_WindowBg));

                    ImGui::TextUnformatted("hex.view.information.distribution"_lang);
                    ImPlot::SetNextPlotLimits(0, 256, 0, float(*std::max_element(this->m_valueCounts.begin(), this->m_valueCounts.end())) * 1.1F, ImGuiCond_Always);
                    if (ImPlot::BeginPlot("##distribution", "Address", "Count", ImVec2(-1,0), ImPlotFlags_NoLegend | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect, ImPlotAxisFlags_Lock, ImPlotAxisFlags_Lock))  {
                        static auto x = []{
                            std::array<ImU64, 256> result{ 0 };
                            std::iota(result.begin(), result.end(), 0);
                            return result;
                        }();


                        ImPlot::PlotBars<ImU64>("##bytes", x.data(), this->m_valueCounts.data(), x.size(), 0.67);

                        ImPlot::EndPlot();
                    }

                    ImGui::NewLine();

                    ImGui::TextUnformatted("hex.view.information.entropy"_lang);

                    ImPlot::SetNextPlotLimits(0, this->m_blockEntropy.size(), -0.1, 1.1, ImGuiCond_Always);
                    if (ImPlot::BeginPlot("##entropy", "Address", "Entropy", ImVec2(-1,0), ImPlotFlags_CanvasOnly, ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_Lock)) {
                        ImPlot::PlotLine("##entropy_line", this->m_blockEntropy.data(), this->m_blockEntropy.size());

                        if (ImPlot::DragLineX("Position", &this->m_entropyHandlePosition, false)) {
                            u64 address = u64(this->m_entropyHandlePosition * this->m_blockSize) + provider->getBaseAddress();
                            address = std::min(address, provider->getBaseAddress() + provider->getSize() - 1);
                            EventManager::post<RequestSelectionChange>( Region{ address, 1 });
                        }

                        ImPlot::EndPlot();
                    }

                    ImGui::PopStyleColor();

                    ImGui::NewLine();

                    ImGui::LabelText("hex.view.information.block_size"_lang, "%s", hex::format("hex.view.information.block_size.desc"_lang, this->m_blockEntropy.size(), this->m_blockSize).c_str());
                    ImGui::LabelText("hex.view.information.file_entropy"_lang, "%.8f", this->m_averageEntropy);
                    ImGui::LabelText("hex.view.information.highest_entropy"_lang, "%.8f", this->m_highestBlockEntropy);

                    if (this->m_averageEntropy > 0.83 && this->m_highestBlockEntropy > 0.9) {
                        ImGui::NewLine();
                        ImGui::TextColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "%s", static_cast<const char*>("hex.view.information.encrypted"_lang));
                    }

                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    void ViewInformation::drawMenu() {

    }

}