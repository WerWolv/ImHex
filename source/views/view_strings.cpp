#include "views/view_strings.hpp"

#include "providers/provider.hpp"

#include <cstring>

namespace hex {

    ViewStrings::ViewStrings(prv::Provider* &dataProvider) : View(), m_dataProvider(dataProvider) {
        View::subscribeEvent(Events::DataChanged, [this](void*){
            this->m_shouldInvalidate = true;
        });

        this->m_filter = new char[0xFFFF];
        std::memset(this->m_filter, 0x00, 0xFFFF);
    }

    ViewStrings::~ViewStrings() {
        View::unsubscribeEvent(Events::DataChanged);
        delete[] this->m_filter;
    }

    void ViewStrings::createView() {
        if (!this->m_windowOpen)
            return;

        if (this->m_shouldInvalidate) {
            this->m_shouldInvalidate = false;

            this->m_foundStrings.clear();


            std::vector<u8> buffer(1024, 0x00);
            u32 foundCharacters = 0;
            for (u64 offset = 0; offset < this->m_dataProvider->getSize(); offset += buffer.size()) {
                this->m_dataProvider->read(offset,  buffer.data(), buffer.size());

                for (u32 i = 0; i < buffer.size(); i++) {
                    if (buffer[i] >= 0x20 && buffer[i] <= 0x7E)
                        foundCharacters++;
                    else {
                        if (foundCharacters >= this->m_minimumLength) {
                            FoundString foundString;

                            foundString.offset = offset + i - foundCharacters;
                            foundString.size = foundCharacters;
                            foundString.string.reserve(foundCharacters);
                            foundString.string.resize(foundCharacters);
                            this->m_dataProvider->read(foundString.offset, foundString.string.data(), foundCharacters);

                            this->m_foundStrings.push_back(foundString);
                        }

                        foundCharacters = 0;
                    }
                }
            }
        }


        if (ImGui::Begin("Strings", &this->m_windowOpen)) {
            if (this->m_dataProvider != nullptr && this->m_dataProvider->isReadable()) {
                if (ImGui::InputInt("Minimum length", &this->m_minimumLength, 1, 0))
                    this->m_shouldInvalidate = true;

                ImGui::InputText("Filter", this->m_filter, 0xFFFF);

                ImGui::Separator();
                ImGui::NewLine();

                ImGui::BeginChild("##scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);

                if (ImGui::BeginTable("##strings", 3,
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable |
                                      ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("Position", 0, -1, ImGui::GetID("position"));
                    ImGui::TableSetupColumn("Size", 0, -1, ImGui::GetID("size"));
                    ImGui::TableSetupColumn("String", 0, -1, ImGui::GetID("string"));

                    auto sortSpecs = ImGui::TableGetSortSpecs();

                    if (sortSpecs->SpecsDirty) {
                        std::sort(this->m_foundStrings.begin(), this->m_foundStrings.end(),
                                  [this, &sortSpecs](FoundString &left, FoundString &right) -> bool {
                                      if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("position")) {
                                          if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                                              return left.offset > right.offset;
                                          else
                                              return left.offset < right.offset;
                                      } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("size")) {
                                          if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                                              return left.size > right.size;
                                          else
                                              return left.size < right.size;
                                      } else if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("string")) {
                                          if (sortSpecs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                                              return left.string > right.string;
                                          else
                                              return left.string < right.string;
                                      }

                                      return false;
                                  });

                        sortSpecs->SpecsDirty = false;
                    }

                    ImGui::TableHeadersRow();
                    u32 rowCount = 0;

                    ImGuiListClipper clipper;
                    clipper.Begin(this->m_foundStrings.size(), 14);
                    clipper.Step();

                    for (u64 i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        auto &foundString = this->m_foundStrings[i];

                        if (strlen(this->m_filter) != 0 && foundString.string.find(this->m_filter) == std::string::npos)
                            continue;

                        ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
                        ImGui::TableNextColumn();
                        ImGui::Text("0x%08lx:0x%08lx", foundString.offset, foundString.offset + foundString.size);
                        ImGui::TableNextColumn();
                        ImGui::Text("%08lx", foundString.size);
                        ImGui::TableNextColumn();
                        ImGui::Text("%s", foundString.string.c_str());
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                                               ((rowCount % 2) == 0) ? 0xFF101010 : 0xFF303030);
                        rowCount++;
                    }
                    clipper.Step();
                    clipper.End();

                    ImGui::EndTable();
                }

                ImGui::EndChild();
            }
        }
        ImGui::End();
    }

    void ViewStrings::createMenu() {
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Strings View", "", &this->m_windowOpen);
            ImGui::EndMenu();
        }
    }

}