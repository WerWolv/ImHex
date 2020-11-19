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

    static bool beginPatternDataTable(prv::Provider* &provider, const std::vector<lang::PatternData*> &patterns, std::vector<lang::PatternData*> &sortedPatterns) {
        if (ImGui::BeginTable("##patterndatatable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Color", 0, -1, ImGui::GetID("color"));
            ImGui::TableSetupColumn("Name", 0, -1, ImGui::GetID("name"));
            ImGui::TableSetupColumn("Position", 0, -1, ImGui::GetID("position"));
            ImGui::TableSetupColumn("Size", 0, -1, ImGui::GetID("size"));
            ImGui::TableSetupColumn("Type", 0, -1, ImGui::GetID("type"));
            ImGui::TableSetupColumn("Value", 0, -1, ImGui::GetID("value"));

            auto sortSpecs = ImGui::TableGetSortSpecs();

            if (sortSpecs->SpecsDirty || sortedPatterns.empty()) {
                sortedPatterns = patterns;

                std::sort(sortedPatterns.begin(), sortedPatterns.end(), [&sortSpecs, &provider](lang::PatternData* left, lang::PatternData* right) -> bool {
                    return lang::PatternData::sortPatternDataTable(sortSpecs, provider, left, right);
                });

                for (auto &pattern : sortedPatterns)
                    pattern->sort(sortSpecs, provider);

                sortSpecs->SpecsDirty = false;
            }

            return true;
        }

        return false;
    }

    void ViewPatternData::createView() {
        if (!this->m_windowOpen)
            return;

        if (ImGui::Begin("Pattern Data", &this->m_windowOpen)) {
            ImGui::BeginChild("##scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);

            if (this->m_dataProvider != nullptr && this->m_dataProvider->isReadable()) {

                if (beginPatternDataTable(this->m_dataProvider, this->m_patternData, this->m_sortedPatternData)) {
                    if (this->m_sortedPatternData.size() > 0) {
                        ImGui::TableHeadersRow();

                        u32 rowCount = 0;
                        for (auto &patternData : this->m_sortedPatternData) {
                            patternData->createEntry(this->m_dataProvider);
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                                   ((rowCount % 2) == 0) ? 0xFF101010 : 0xFF303030);
                            rowCount++;
                        }

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