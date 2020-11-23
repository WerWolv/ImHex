#include "views/view_strings.hpp"

#include "providers/provider.hpp"
#include "utils.hpp"

#include <cstring>

using namespace std::literals::string_literals;

namespace hex {

    ViewStrings::ViewStrings(prv::Provider* &dataProvider) : View(), m_dataProvider(dataProvider) {
        View::subscribeEvent(Events::DataChanged, [this](const void*){
            this->m_foundStrings.clear();
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
                if (ImGui::Button("Extract"))
                    this->m_shouldInvalidate = true;

                ImGui::Separator();
                ImGui::NewLine();

                ImGui::BeginChild("##scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);

                if (ImGui::BeginTable("##strings", 3,
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable |
                                      ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("Offset", 0, -1, ImGui::GetID("offset"));
                    ImGui::TableSetupColumn("Size", 0, -1, ImGui::GetID("size"));
                    ImGui::TableSetupColumn("String", 0, -1, ImGui::GetID("string"));

                    auto sortSpecs = ImGui::TableGetSortSpecs();

                    if (sortSpecs->SpecsDirty) {
                        std::sort(this->m_foundStrings.begin(), this->m_foundStrings.end(),
                                  [&sortSpecs](FoundString &left, FoundString &right) -> bool {
                                      if (sortSpecs->Specs->ColumnUserID == ImGui::GetID("offset")) {
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

                    ImGuiListClipper clipper;
                    clipper.Begin(this->m_foundStrings.size());

                    while (clipper.Step()) {
                        for (u64 i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                            auto &foundString = this->m_foundStrings[i];

                            if (strlen(this->m_filter) != 0 &&
                                foundString.string.find(this->m_filter) == std::string::npos)
                                continue;

                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            if (ImGui::Selectable(("##StringLine"s + std::to_string(i)).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                                Region selectRegion = { foundString.offset, foundString.size };
                                View::postEvent(Events::SelectionChangeRequest, &selectRegion);
                            }
                            ImGui::SameLine();
                            ImGui::Text("0x%08lx : 0x%08lx", foundString.offset, foundString.offset + foundString.size);
                            ImGui::TableNextColumn();
                            ImGui::Text("0x%04lx", foundString.size);
                            ImGui::TableNextColumn();
                            ImGui::Text("%s", foundString.string.c_str());
                        }
                    }
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