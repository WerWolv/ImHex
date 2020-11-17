#include "views/view_pattern_data.hpp"

#include "providers/provider.hpp"

namespace hex {

    ViewPatternData::ViewPatternData(prv::Provider* &dataProvider, std::vector<hex::PatternData*> &patternData)
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

                if (ImGui::BeginTable("##patterndatatable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("Color", 0, -1, ImGui::GetID("color"));
                    ImGui::TableSetupColumn("Name", 0, -1, ImGui::GetID("name"));
                    ImGui::TableSetupColumn("Position", 0, -1, ImGui::GetID("position"));
                    ImGui::TableSetupColumn("Size", 0, -1, ImGui::GetID("size"));
                    ImGui::TableSetupColumn("Type", 0, -1, ImGui::GetID("type"));
                    ImGui::TableSetupColumn("Value", 0, -1, ImGui::GetID("value"));

                    auto sortSpecs = ImGui::TableGetSortSpecs();

                    if (sortSpecs->SpecsDirty || this->m_sortedPatternData.empty()) {
                        this->m_sortedPatternData = this->m_patternData;

                        std::sort(this->m_sortedPatternData.begin(), this->m_sortedPatternData.end(), [this, &sortSpecs](PatternData* left, PatternData* right) -> bool {
                            if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("name")) {
                                if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                                    return left->getName() > right->getName();
                                else
                                    return left->getName() < right->getName();
                            }
                            else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("position")) {
                                if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                                    return left->getOffset() > right->getOffset();
                                else
                                    return left->getOffset() < right->getOffset();
                            }
                            else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("size")) {
                                if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                                    return left->getSize() > right->getSize();
                                else
                                    return left->getSize() < right->getSize();
                            }
                            else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("value")) {
                                size_t biggerSize = std::max(left->getSize(), right->getSize());
                                std::vector<u8> leftBuffer(biggerSize, 0x00), rightBuffer(biggerSize, 0x00);

                                this->m_dataProvider->read(left->getOffset(), leftBuffer.data(), left->getSize());
                                this->m_dataProvider->read(right->getOffset(), rightBuffer.data(), right->getSize());

                                if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                                    return leftBuffer > rightBuffer;
                                else
                                    return leftBuffer < rightBuffer;
                            }
                            else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("type")) {
                                if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                                    return left->getTypeName() > right->getTypeName();
                                else
                                    return left->getTypeName() < right->getTypeName();
                            }
                            else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("color")) {
                                if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                                    return left->getColor() > right->getColor();
                                else
                                    return left->getColor() < right->getColor();
                            }

                            return false;
                        });

                        sortSpecs->SpecsDirty = false;
                    }

                    ImGui::TableHeadersRow();
                    u32 rowCount = 0;
                    for (auto& patternData : this->m_sortedPatternData) {
                        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
                        ImGui::TableNextColumn();
                        ImGui::ColorButton("color", ImColor(patternData->getColor()), ImGuiColorEditFlags_NoTooltip);
                        ImGui::TableNextColumn();
                        ImGui::Text("%s", patternData->getName().c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text("0x%08lx : 0x%08lx", patternData->getOffset(), patternData->getOffset() + patternData->getSize());
                        ImGui::TableNextColumn();
                        ImGui::Text("0x%08lx", patternData->getSize());
                        ImGui::TableNextColumn();
                        ImGui::Text("%s", patternData->getTypeName().c_str());
                        ImGui::TableNextColumn();
                        ImGui::Text("%s", patternData->format(this->m_dataProvider).c_str());
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