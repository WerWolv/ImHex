#include "views/view_pattern_data.hpp"

#include "providers/provider.hpp"
#include "lang/pattern_data.hpp"

namespace hex {

    ViewPatternData::ViewPatternData(prv::Provider* &dataProvider, std::vector<lang::PatternData*> &patternData)
        : View(), m_dataProvider(dataProvider), m_patternData(patternData) {

        this->subscribeEvent(Events::PatternChanged, [this](auto data) {
            this->m_sortedPatternData.clear();
        });
    }

    ViewPatternData::~ViewPatternData() {
        this->unsubscribeEvent(Events::PatternChanged);
    }

    void ViewPatternData::createView() {
        if (!this->m_windowOpen)
            return;

        if (ImGui::Begin("Pattern Data", &this->m_windowOpen)) {
            ImGui::BeginChild("##scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);

            if (this->m_dataProvider != nullptr && this->m_dataProvider->isReadable()) {

                if (lang::PatternData::beginPatternDataTable(this->m_dataProvider, this->m_patternData, this->m_sortedPatternData)) {
                    u32 rowCount = 0;
                    for (auto& patternData : this->m_sortedPatternData) {
                        patternData->createEntry(this->m_dataProvider);
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ((rowCount % 2) == 0) ? 0xFF101010 : 0xFF303030);
                        rowCount++;
                    }

                    ImGui::EndTable();
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