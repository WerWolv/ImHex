#include "views/view_information.hpp"

#include "providers/provider.hpp"

#include "utils.hpp"

#include <algorithm>
#include <vector>
#include <cstring>
#include <cmath>
#include <magic.h>

#include <span>

namespace hex {

    ViewInformation::ViewInformation(prv::Provider* &dataProvider) : View(), m_dataProvider(dataProvider) {
        View::subscribeEvent(Events::DataChanged, [this](void*) {
           this->m_shouldInvalidate = true;
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

    void ViewInformation::createView() {
        if (!this->m_windowOpen)
            return;

        if (ImGui::Begin("File Information", &this->m_windowOpen)) {
            ImGui::BeginChild("##scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);

            if (this->m_dataProvider != nullptr && this->m_dataProvider->isReadable()) {
                if (this->m_shouldInvalidate) {

                    {
                        std::vector<u8> buffer(512, 0x00);
                        std::memset(this->m_valueCounts.data(), 0x00, this->m_valueCounts.size() * sizeof(u32));
                        this->m_blockEntropy.clear();

                        for (u64 i = 0; i < this->m_dataProvider->getSize(); i += 512) {
                            std::array<float, 256> blockValueCounts = { 0 };
                            this->m_dataProvider->read(i, buffer.data(), std::min(512ULL, this->m_dataProvider->getSize() - i));

                            for (u16 j = 0; j < 512; j++) {
                                blockValueCounts[buffer[j]]++;
                                this->m_valueCounts[buffer[j]]++;
                            }
                            this->m_blockEntropy.push_back(calculateEntropy(blockValueCounts, 512));
                        }

                        this->m_averageEntropy = calculateEntropy(this->m_valueCounts, this->m_dataProvider->getSize());
                        this->m_highestBlockEntropy = *std::max_element(this->m_blockEntropy.begin(), this->m_blockEntropy.end());
                    }

                    {
                        std::vector<u8> buffer(this->m_dataProvider->getSize(), 0x00);
                        this->m_dataProvider->read(0x00, buffer.data(), buffer.size());

                        {
                            magic_t cookie = magic_open(MAGIC_NONE);
                            magic_load(cookie, "magic");
                            this->m_fileDescription = magic_buffer(cookie, buffer.data(), buffer.size());
                            magic_close(cookie);
                        }

                        {
                            magic_t cookie = magic_open(MAGIC_MIME);
                            magic_load(cookie, "magic");
                            this->m_mimeType = magic_buffer(cookie, buffer.data(), buffer.size());
                            magic_close(cookie);
                        }

                        this->m_shouldInvalidate = false;
                    }
                }

                ImGui::NewLine();

                ImGui::Text("Byte Distribution");
                ImGui::PlotHistogram("##nolabel", this->m_valueCounts.data(), 256, 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(0, 100));

                ImGui::NewLine();
                ImGui::Separator();
                ImGui::NewLine();

                ImGui::Text("Entropy");
                ImGui::PlotLines("##nolabel", this->m_blockEntropy.data(), this->m_blockEntropy.size(), 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(0, 100));

                ImGui::NewLine();

                ImGui::LabelText("Average entropy", "%.8f", this->m_averageEntropy);
                ImGui::LabelText("Highest entropy block", "%.8f", this->m_highestBlockEntropy);

                ImGui::NewLine();

                if (this->m_averageEntropy > 0.83 && this->m_highestBlockEntropy > 0.9)
                    ImGui::TextColored(ImVec4(0.92F, 0.25F, 0.2F, 1.0F), "This data is most likely encrypted or compressed!");

                ImGui::NewLine();
                ImGui::Separator();
                ImGui::NewLine();

                ImGui::TextUnformatted("Description:");
                ImGui::TextWrapped("%s", this->m_fileDescription.c_str());
                ImGui::NewLine();
                ImGui::TextUnformatted("MIME Type:");
                ImGui::TextWrapped("%s", this->m_mimeType.c_str());

                ImGui::NewLine();
            }

            ImGui::EndChild();
        }
        ImGui::End();
    }

    void ViewInformation::createMenu() {
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Entropy View", "", &this->m_windowOpen);
            ImGui::EndMenu();
        }
    }

}