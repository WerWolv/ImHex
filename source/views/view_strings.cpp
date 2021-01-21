#include "views/view_strings.hpp"

#include <hex/providers/provider.hpp>
#include <hex/helpers/utils.hpp>

#include <cstring>

#include <llvm/Demangle/Demangle.h>

using namespace std::literals::string_literals;

namespace hex {

    ViewStrings::ViewStrings() : View("Strings") {
        View::subscribeEvent(Events::DataChanged, [this](auto){
            this->m_foundStrings.clear();
        });

        this->m_filter = new char[0xFFFF];
        std::memset(this->m_filter, 0x00, 0xFFFF);
    }

    ViewStrings::~ViewStrings() {
        View::unsubscribeEvent(Events::DataChanged);
        delete[] this->m_filter;
    }


    void ViewStrings::createStringContextMenu(const FoundString &foundString) {
        if (ImGui::TableGetHoveredColumn() == 2  && ImGui::IsMouseReleased(1) && ImGui::IsItemHovered()) {
            ImGui::OpenPopup("StringContextMenu");
            this->m_selectedString = foundString.string;
        }
        if (ImGui::BeginPopup("StringContextMenu")) {
            if (ImGui::MenuItem("Copy string")) {
                ImGui::SetClipboardText(this->m_selectedString.c_str());
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Demangle")) {
                this->m_demangledName = llvm::demangle(this->m_selectedString);
                if (!this->m_demangledName.empty())
                    View::doLater([]{ ImGui::OpenPopup("Demangled Name"); });
            }
            ImGui::EndPopup();
        }
    }


    void ViewStrings::drawContent() {
        auto provider = SharedData::currentProvider;

        if (this->m_shouldInvalidate) {
            this->m_shouldInvalidate = false;

            this->m_foundStrings.clear();

            std::vector<u8> buffer(1024, 0x00);
            u32 foundCharacters = 0;

            for (u64 offset = 0; offset < provider->getSize(); offset += buffer.size()) {
                size_t readSize = std::min(u64(buffer.size()), provider->getSize() - offset);
                provider->read(offset,  buffer.data(), readSize);

                for (u32 i = 0; i < readSize; i++) {
                    if (buffer[i] >= 0x20 && buffer[i] <= 0x7E)
                        foundCharacters++;
                    else {
                        if (foundCharacters >= this->m_minimumLength) {
                            FoundString foundString;

                            foundString.offset = offset + i - foundCharacters;
                            foundString.size = foundCharacters;
                            foundString.string.reserve(foundCharacters);
                            foundString.string.resize(foundCharacters);
                            provider->read(foundString.offset, foundString.string.data(), foundCharacters);

                            this->m_foundStrings.push_back(foundString);
                        }

                        foundCharacters = 0;
                    }
                }
            }
        }


        if (ImGui::Begin("Strings", &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            if (provider != nullptr && provider->isReadable()) {
                if (ImGui::InputInt("Minimum length", &this->m_minimumLength, 1, 0))
                    this->m_shouldInvalidate = true;

                ImGui::InputText("Filter", this->m_filter, 0xFFFF);
                if (ImGui::Button("Extract"))
                    this->m_shouldInvalidate = true;

                ImGui::Separator();
                ImGui::NewLine();

                if (ImGui::BeginTable("##strings", 3,
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable |
                                      ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                    ImGui::TableSetupScrollFreeze(0, 1);
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
                                View::postEvent(Events::SelectionChangeRequest, selectRegion);
                            }
                            ImGui::PushID(i + 1);
                            createStringContextMenu(foundString);
                            ImGui::PopID();
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
            }
        }
        ImGui::End();

        if (ImGui::BeginPopup("Demangled Name")) {
            if (ImGui::BeginChild("##scrolling", ImVec2(500, 150))) {
                ImGui::Text("Demangled Name");
                ImGui::Separator();
                ImGui::TextWrapped("%s", this->m_demangledName.c_str());
                ImGui::EndChild();
                ImGui::NewLine();
                if (ImGui::Button("Copy"))
                    ImGui::SetClipboardText(this->m_demangledName.c_str());
            }
            ImGui::EndPopup();
        }
    }

    void ViewStrings::drawMenu() {

    }

}