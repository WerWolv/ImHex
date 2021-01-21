#include "views/view_information.hpp"

#include <hex/providers/provider.hpp>
#include <hex/helpers/utils.hpp>

#include <cstring>
#include <cmath>
#include <filesystem>
#include <span>
#include <vector>

#include <magic.h>

namespace hex {

    ViewInformation::ViewInformation() : View("Information") {
        View::subscribeEvent(Events::DataChanged, [this](auto) {
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
    }

    ViewInformation::~ViewInformation() {
        View::unsubscribeEvent(Events::DataChanged);
    }

    static float calculateEntropy(std::array<float, 256> &valueCounts, size_t numBytes) {
        float entropy = 0;

        for (u16 i = 0; i < 256; i++) {
            valueCounts[i] /= numBytes;

            if (valueCounts[i] > 0)
                entropy -= (valueCounts[i] * std::log2(valueCounts[i]));
        }

        return entropy / 8;
    }

    void ViewInformation::drawContent() {
        if (ImGui::Begin("Data Information", &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            ImGui::BeginChild("##scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);

            auto provider = SharedData::currentProvider;

            if (provider != nullptr && provider->isReadable()) {
                if (this->m_shouldInvalidate) {

                    this->m_analyzedRegion = { provider->getBaseAddress(), provider->getBaseAddress() + provider->getSize() };

                    {
                        this->m_blockSize = std::ceil(provider->getSize() / 2048.0F);
                        std::vector<u8> buffer(this->m_blockSize, 0x00);
                        std::memset(this->m_valueCounts.data(), 0x00, this->m_valueCounts.size() * sizeof(u32));
                        this->m_blockEntropy.clear();

                        for (u64 i = 0; i < provider->getSize(); i += this->m_blockSize) {
                            std::array<float, 256> blockValueCounts = { 0 };
                            provider->read(i, buffer.data(), std::min(u64(this->m_blockSize), provider->getSize() - i));

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
                        provider->read(0x00, buffer.data(), buffer.size());

                        this->m_fileDescription.clear();
                        this->m_mimeType.clear();

                        std::string magicFiles;

                        std::error_code error;
                        for (const auto &entry : std::filesystem::directory_iterator("magic", error)) {
                            if (entry.is_regular_file() && entry.path().extension() == ".mgc")
                                magicFiles += entry.path().string() + MAGIC_PATH_SEPARATOR;
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

                        }


                        this->m_shouldInvalidate = false;
                        this->m_dataValid = true;
                    }
                }

                ImGui::NewLine();

                if (ImGui::Button("Analyze current page"))
                    this->m_shouldInvalidate = true;

                ImGui::NewLine();
                ImGui::Separator();
                ImGui::NewLine();

                if (this->m_dataValid) {

                    for (auto &[name, value] : (SharedData::currentProvider)->getDataInformation()) {
                        ImGui::LabelText(name.c_str(), "%s", value.c_str());
                    }

                    ImGui::LabelText("Analyzed region", "0x%llx - 0x%llx", this->m_analyzedRegion.first, this->m_analyzedRegion.second);

                    ImGui::NewLine();
                    ImGui::Separator();
                    ImGui::NewLine();

                    if (!this->m_fileDescription.empty()) {
                        ImGui::TextUnformatted("Description:");
                        ImGui::TextWrapped("%s", this->m_fileDescription.c_str());
                        ImGui::NewLine();
                    }

                    if (!this->m_mimeType.empty()) {
                        ImGui::TextUnformatted("MIME Type:");
                        ImGui::TextWrapped("%s", this->m_mimeType.c_str());
                        ImGui::NewLine();
                    }

                    ImGui::Separator();
                    ImGui::NewLine();

                    ImGui::Text("Byte Distribution");
                    ImGui::PlotHistogram("##nolabel", this->m_valueCounts.data(), 256, 0, nullptr, FLT_MAX, FLT_MAX,ImVec2(0, 100));

                    ImGui::NewLine();
                    ImGui::Separator();
                    ImGui::NewLine();

                    ImGui::Text("Entropy");
                    ImGui::PlotLines("##nolabel", this->m_blockEntropy.data(), this->m_blockEntropy.size(), 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(0, 100));

                    ImGui::NewLine();

                    ImGui::LabelText("Block size", "2048 blocks of %lu bytes", this->m_blockSize);
                    ImGui::LabelText("Average entropy", "%.8f", this->m_averageEntropy);
                    ImGui::LabelText("Highest entropy block", "%.8f", this->m_highestBlockEntropy);

                    if (this->m_averageEntropy > 0.83 && this->m_highestBlockEntropy > 0.9) {
                        ImGui::NewLine();
                        ImGui::TextColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F),"This data is most likely encrypted or compressed!");
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