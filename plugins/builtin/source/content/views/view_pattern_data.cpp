#include "content/views/view_pattern_data.hpp"

#include <hex/providers/provider.hpp>
#include <hex/pattern_language/pattern_data.hpp>

namespace hex::plugin::builtin {

    ViewPatternData::ViewPatternData() : View("hex.builtin.view.pattern_data.name") {

        EventManager::subscribe<EventPatternChanged>(this, [this](auto&) {
            this->m_sortedPatternData.clear();
        });
    }

    ViewPatternData::~ViewPatternData() {
        EventManager::unsubscribe<EventPatternChanged>(this);
    }

    static bool beginPatternDataTable(prv::Provider* &provider, const std::vector<pl::PatternData*> &patterns, std::vector<pl::PatternData*> &sortedPatterns) {
        if (ImGui::BeginTable("##patterndatatable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("hex.builtin.view.pattern_data.var_name"_lang,  0, 0, ImGui::GetID("name"));
            ImGui::TableSetupColumn("hex.builtin.view.pattern_data.color"_lang,     0, 0, ImGui::GetID("color"));
            ImGui::TableSetupColumn("hex.builtin.view.pattern_data.offset"_lang,    0, 0, ImGui::GetID("offset"));
            ImGui::TableSetupColumn("hex.builtin.view.pattern_data.size"_lang,      0, 0, ImGui::GetID("size"));
            ImGui::TableSetupColumn("hex.builtin.view.pattern_data.type"_lang,      0, 0, ImGui::GetID("type"));
            ImGui::TableSetupColumn("hex.builtin.view.pattern_data.value"_lang,     0, 0, ImGui::GetID("value"));

            auto sortSpecs = ImGui::TableGetSortSpecs();

            if (sortSpecs->SpecsDirty || sortedPatterns.empty()) {
                sortedPatterns = patterns;

                std::sort(sortedPatterns.begin(), sortedPatterns.end(), [&sortSpecs, &provider](pl::PatternData* left, pl::PatternData* right) -> bool {
                    return pl::PatternData::sortPatternDataTable(sortSpecs, provider, left, right);
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
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.pattern_data.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            auto provider = ImHexApi::Provider::get();
            if (ImHexApi::Provider::isValid() && provider->isReadable()) {

                if (beginPatternDataTable(provider, SharedData::patternData, this->m_sortedPatternData)) {
                    ImGui::TableHeadersRow();
                    if (this->m_sortedPatternData.size() > 0) {

                        for (auto &patternData : this->m_sortedPatternData)
                            patternData->draw(provider);

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