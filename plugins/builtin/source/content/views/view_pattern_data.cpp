#include "content/views/view_pattern_data.hpp"

#include <hex/providers/provider.hpp>

#include <pl/patterns/pattern.hpp>

#include <content/helpers/provider_extra_data.hpp>

namespace hex::plugin::builtin {

    ViewPatternData::ViewPatternData() : View("hex.builtin.view.pattern_data.name") {

        EventManager::subscribe<EventHighlightingChanged>(this, [this]() {
            if (!ImHexApi::Provider::isValid()) return;

            this->m_sortedPatterns[ImHexApi::Provider::get()].clear();
        });
    }

    ViewPatternData::~ViewPatternData() {
        EventManager::unsubscribe<EventHighlightingChanged>(this);
    }

    static bool sortPatterns(prv::Provider *provider, const ImGuiTableSortSpecs* sortSpecs, const pl::ptrn::Pattern * left, const pl::ptrn::Pattern * right) {
        if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("name")) {
            if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                return left->getDisplayName() > right->getDisplayName();
            else
                return left->getDisplayName() < right->getDisplayName();
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("offset")) {
            if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                return left->getOffset() > right->getOffset();
            else
                return left->getOffset() < right->getOffset();
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("size")) {
            if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                return left->getSize() > right->getSize();
            else
                return left->getSize() < right->getSize();
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("value")) {
            size_t biggerSize = std::max(left->getSize(), right->getSize());
            std::vector<u8> leftBuffer(biggerSize, 0x00), rightBuffer(biggerSize, 0x00);

            provider->read(left->getOffset(), leftBuffer.data(), left->getSize());
            provider->read(right->getOffset(), rightBuffer.data(), right->getSize());

            if (left->getEndian() != std::endian::native)
                std::reverse(leftBuffer.begin(), leftBuffer.end());
            if (right->getEndian() != std::endian::native)
                std::reverse(rightBuffer.begin(), rightBuffer.end());

            if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                return leftBuffer > rightBuffer;
            else
                return leftBuffer < rightBuffer;
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("type")) {
            if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                return left->getTypeName() > right->getTypeName();
            else
                return left->getTypeName() < right->getTypeName();
        } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("color")) {
            if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                return left->getColor() > right->getColor();
            else
                return left->getColor() < right->getColor();
        }

        return false;
    }

    static bool beginPatternTable(prv::Provider *&provider, const std::vector<std::shared_ptr<pl::ptrn::Pattern>> &patterns, std::vector<pl::ptrn::Pattern*> &sortedPatterns) {
        if (ImGui::BeginTable("##Patterntable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Hideable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("hex.builtin.view.pattern_data.var_name"_lang, ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("name"));
            ImGui::TableSetupColumn("hex.builtin.view.pattern_data.color"_lang,    ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("color"));
            ImGui::TableSetupColumn("hex.builtin.view.pattern_data.offset"_lang,   ImGuiTableColumnFlags_PreferSortAscending | ImGuiTableColumnFlags_DefaultSort, 0, ImGui::GetID("offset"));
            ImGui::TableSetupColumn("hex.builtin.view.pattern_data.size"_lang,     ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("size"));
            ImGui::TableSetupColumn("hex.builtin.view.pattern_data.type"_lang,     ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("type"));
            ImGui::TableSetupColumn("hex.builtin.view.pattern_data.value"_lang,    ImGuiTableColumnFlags_PreferSortAscending, 0, ImGui::GetID("value"));

            auto sortSpecs = ImGui::TableGetSortSpecs();

            if (sortSpecs->SpecsDirty || sortedPatterns.empty()) {
                sortedPatterns.clear();
                std::transform(patterns.begin(), patterns.end(), std::back_inserter(sortedPatterns), [](const std::shared_ptr<pl::ptrn::Pattern> &pattern) {
                    return pattern.get();
                });

                std::sort(sortedPatterns.begin(), sortedPatterns.end(), [&sortSpecs, &provider](pl::ptrn::Pattern *left, pl::ptrn::Pattern *right) -> bool {
                    return sortPatterns(provider, sortSpecs, left, right);
                });

                for (auto &pattern : sortedPatterns)
                    pattern->sort([&sortSpecs, &provider](const pl::ptrn::Pattern *left, const pl::ptrn::Pattern *right){
                        return sortPatterns(provider, sortSpecs, left, right);
                    });

                sortSpecs->SpecsDirty = false;
            }

            return true;
        }

        return false;
    }

    void ViewPatternData::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.pattern_data.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            if (ImHexApi::Provider::isValid()) {
                auto provider = ImHexApi::Provider::get();
                auto &patternLanguage = ProviderExtraData::get(provider).patternLanguage;

                if (provider->isReadable() && patternLanguage.runtime != nullptr && patternLanguage.executionDone) {
                    auto &sortedPatterns = this->m_sortedPatterns[ImHexApi::Provider::get()];
                    if (beginPatternTable(provider, ProviderExtraData::get(provider).patternLanguage.runtime->getAllPatterns(), sortedPatterns)) {
                        ImGui::TableHeadersRow();

                        if (!sortedPatterns.empty()) {
                            for (auto &patterns : sortedPatterns)
                                patterns->accept(this->m_patternDrawer);
                        }

                        ImGui::EndTable();
                    }
                }
            }
        }
        ImGui::End();
    }

}
