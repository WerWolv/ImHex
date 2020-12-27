#include "views/view_pattern_data.hpp"

#include "providers/provider.hpp"
#include "lang/pattern_data.hpp"

namespace hex {

    ViewPatternData::ViewPatternData(std::vector<lang::PatternData*> &patternData)
        : View("Pattern Data"), m_patternData(patternData) {

        this->subscribeEvent(Events::PatternChanged, [this](auto data) {
            this->m_sortedPatternData.clear();
        });
    }

    ViewPatternData::~ViewPatternData() {
        this->unsubscribeEvent(Events::PatternChanged);
    }

    static bool beginPatternDataTable(prv::Provider* &provider, const std::vector<lang::PatternData*> &patterns, std::vector<lang::PatternData*> &sortedPatterns) {
        if (ImGui::BeginTable("##patterndatatable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Name", 0, -1, ImGui::GetID("name"));
            ImGui::TableSetupColumn("Color", 0, -1, ImGui::GetID("color"));
            ImGui::TableSetupColumn("Offset", 0, -1, ImGui::GetID("offset"));
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

    void ViewPatternData::drawContent() {
        if (ImGui::Begin("Pattern Data", &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            auto provider = prv::Provider::getCurrentProvider();
            if (provider != nullptr && provider->isReadable()) {

                if (beginPatternDataTable(provider, this->m_patternData, this->m_sortedPatternData)) {
                    if (this->m_sortedPatternData.size() > 0) {
                        ImGui::TableHeadersRow();

                        for (auto &patternData : this->m_sortedPatternData)
                            patternData->createEntry(provider);

                    }

                    ImGui::EndTable();
                }

            }
        }
        ImGui::End();
    }

    void ViewPatternData::drawMenu() {

    }

}