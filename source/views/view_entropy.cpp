#include "views/view_entropy.hpp"

#include "providers/provider.hpp"

#include "utils.hpp"

#include <vector>
#include <cstring>
#include <cmath>

namespace hex {

    ViewEntropy::ViewEntropy(prv::Provider* &dataProvider) : View(), m_dataProvider(dataProvider) {

    }

    ViewEntropy::~ViewEntropy() {

    }


    void ViewEntropy::createView() {
        if (!this->m_windowOpen)
            return;

        if (ImGui::Begin("Entropy", &this->m_windowOpen)) {
            ImGui::BeginChild("##scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);

            if (this->m_dataProvider != nullptr && this->m_dataProvider->isReadable()) {
                if (this->m_shouldInvalidate) {
                    std::vector<u8> buffer(512, 0x00);
                    std::memset(this->m_valueCounts, 0x00, sizeof(this->m_valueCounts));

                    for (u64 i = 0; i < this->m_dataProvider->getSize(); i += 512) {
                        this->m_dataProvider->read(i, buffer.data(), std::min(512ULL, this->m_dataProvider->getSize() - i));

                        for (u16 j = 0; j < 512; j++)
                            this->m_valueCounts[buffer[j]]++;
                    }

                    size_t numBytes = this->m_dataProvider->getSize();
                    this->m_entropy = 0;
                    for (u16 i = 0; i < 256; i++) {
                        this->m_valueCounts[i] /= numBytes;

                        if (this->m_valueCounts[i] > 0)
                            this->m_entropy -= this->m_valueCounts[i] * std::log2(this->m_valueCounts[i]);
                    }

                    this->m_entropy /= 8;

                    this->m_shouldInvalidate = false;
                }

                ImGui::LabelText("Entropy", "%.8f", this->m_entropy);

                if (this->m_entropy > 0.99)
                    ImGui::TextUnformatted("This file is most likely encrypted or compressed!");

                ImGui::NewLine();
                ImGui::Separator();
                ImGui::NewLine();
                ImGui::PlotHistogram("Byte Distribution", this->m_valueCounts, 256, 0, nullptr, FLT_MAX, FLT_MAX, ImVec2(0, 100));

                ImGui::NewLine();
                ImGui::Separator();
                ImGui::NewLine();

                if (ImGui::Button("Invalidate"))
                    this->m_shouldInvalidate = true;
            }

            ImGui::EndChild();
        }
        ImGui::End();
    }

    void ViewEntropy::createMenu() {
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Entropy View", "", &this->m_windowOpen);
            ImGui::EndMenu();
        }
    }

}