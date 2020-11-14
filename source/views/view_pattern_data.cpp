#include "views/view_pattern_data.hpp"

#include "providers/provider.hpp"

#include <cstring>

namespace hex {

    ViewPatternData::ViewPatternData(prv::Provider* &dataProvider, std::vector<hex::PatternData*> &patternData)
        : View(), m_dataProvider(dataProvider), m_patternData(patternData) {

    }

    ViewPatternData::~ViewPatternData() {

    }

    void ViewPatternData::createView() {
        if (!this->m_windowOpen)
            return;

        if (ImGui::Begin("Pattern Data", &this->m_windowOpen)) {
            ImGui::BeginChild("##scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);

            if (this->m_dataProvider != nullptr && this->m_dataProvider->isReadable()) {

                for (auto& patternData : this->m_patternData) {
                    ImGui::LabelText(patternData->getName().c_str(), "[0x%08lx:0x%08lx]    %s",
                                     patternData->getOffset(),
                                     patternData->getOffset() + patternData->getSize(),
                                     patternData->format(this->m_dataProvider).c_str());
                }
            }

            ImGui::EndChild();
        }
        ImGui::End();
    }

    void ViewPatternData::createMenu() {
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Data View", "", &this->m_windowOpen);
            ImGui::EndMenu();
        }
    }

}