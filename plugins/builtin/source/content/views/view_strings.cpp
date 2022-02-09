#include "content/views/view_strings.hpp"

#include <hex/providers/provider.hpp>
#include <hex/helpers/fmt.hpp>

#include <cstring>
#include <thread>
#include <regex>

#include <llvm/Demangle/Demangle.h>
#include <hex/ui/imgui_imhex_extensions.h>

using namespace std::literals::string_literals;

namespace hex::plugin::builtin {

    ViewStrings::ViewStrings() : View("hex.builtin.view.strings.name") {
        EventManager::subscribe<EventDataChanged>(this, [this]() {
            this->m_foundStrings.clear();
        });

        this->m_filter.reserve(0xFFFF);
        std::memset(this->m_filter.data(), 0x00, this->m_filter.capacity());

        EventManager::subscribe<EventFileUnloaded>(this, [this] {
            this->m_foundStrings.clear();
        });
    }

    ViewStrings::~ViewStrings() {
        EventManager::unsubscribe<EventDataChanged>(this);
        EventManager::unsubscribe<EventFileUnloaded>(this);
    }

    std::string readString(const FoundString &foundString) {
        std::string string(foundString.size + 1, '\0');
        ImHexApi::Provider::get()->read(foundString.offset, string.data(), foundString.size);

        return string;
    }

    void ViewStrings::createStringContextMenu(const FoundString &foundString) {
        if (ImGui::TableGetColumnFlags(2) == ImGuiTableColumnFlags_IsHovered && ImGui::IsMouseReleased(1) && ImGui::IsItemHovered()) {
            ImGui::OpenPopup("StringContextMenu");
            this->m_selectedString = readString(foundString);
        }
        if (ImGui::BeginPopup("StringContextMenu")) {
            if (ImGui::MenuItem("hex.builtin.view.strings.copy"_lang)) {
                ImGui::SetClipboardText(this->m_selectedString.c_str());
            }
            ImGui::Separator();
            if (ImGui::MenuItem("hex.builtin.view.strings.demangle"_lang)) {
                this->m_demangledName = llvm::demangle(this->m_selectedString);
                if (!this->m_demangledName.empty())
                    ImHexApi::Tasks::doLater([] { ImGui::OpenPopup("hex.builtin.view.strings.demangle.name"_lang); });
            }
            ImGui::EndPopup();
        }
    }

    void ViewStrings::searchStrings() {
        this->m_foundStrings.clear();
        this->m_filterIndices.clear();
        this->m_searching = true;

        std::thread([this] {
            auto provider = ImHexApi::Provider::get();
            auto task     = ImHexApi::Tasks::createTask("hex.builtin.view.strings.searching", provider->getActualSize());

            std::vector<u8> buffer(1024, 0x00);
            u32 foundCharacters = 0;

            for (u64 offset = 0; offset < provider->getActualSize(); offset += buffer.size()) {
                task.update(offset);

                size_t readSize = std::min(u64(buffer.size()), provider->getActualSize() - offset);
                provider->read(offset + provider->getBaseAddress(), buffer.data(), readSize);

                for (u32 i = 0; i < readSize; i++) {
                    if (buffer[i] >= ' ' && buffer[i] <= '~' && offset < provider->getActualSize() - 1)
                        foundCharacters++;
                    else {
                        if (foundCharacters >= this->m_minimumLength) {
                            FoundString foundString = {
                                offset + i - foundCharacters + provider->getBaseAddress(),
                                foundCharacters
                            };

                            this->m_foundStrings.push_back(foundString);
                            this->m_filterIndices.push_back(this->m_foundStrings.size() - 1);
                        }

                        foundCharacters = 0;
                    }
                }
            }

            this->m_searching = false;
        }).detach();
    }

    void ViewStrings::drawContent() {
        auto provider = ImHexApi::Provider::get();

        if (ImGui::Begin(View::toWindowName("hex.builtin.view.strings.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            if (ImHexApi::Provider::isValid() && provider->isReadable()) {
                ImGui::Disabled([this] {
                    if (ImGui::InputInt("hex.builtin.view.strings.min_length"_lang, &this->m_minimumLength, 1, 0))
                        this->m_foundStrings.clear();

                    ImGui::Checkbox("Regex", &this->m_regex);

                    ImGui::InputText(
                        "hex.builtin.view.strings.filter"_lang, this->m_filter.data(), this->m_filter.capacity(), ImGuiInputTextFlags_CallbackEdit, [](ImGuiInputTextCallbackData *data) {
                            auto &view = *static_cast<ViewStrings *>(data->UserData);
                            view.m_filter.resize(data->BufTextLen);
                            view.m_filterIndices.clear();
                            std::regex pattern;
                            if (view.m_regex) {
                                try {
                                    pattern               = std::regex(data->Buf);
                                    view.m_pattern_parsed = true;
                                } catch (std::regex_error &e) {
                                    view.m_pattern_parsed = false;
                                }
                            }
                            for (u64 i = 0; i < view.m_foundStrings.size(); i++) {
                                if (view.m_regex) {
                                    if (view.m_pattern_parsed && std::regex_search(readString(view.m_foundStrings[i]), pattern))
                                        view.m_filterIndices.push_back(i);
                                } else if (readString(view.m_foundStrings[i]).find(data->Buf) != std::string::npos) {
                                    view.m_filterIndices.push_back(i);
                                }
                            }
                            return 0;
                        },
                        this);
                    if (this->m_regex && !this->m_pattern_parsed) {
                        ImGui::TextFormattedColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "{}", "hex.builtin.view.strings.regex_error"_lang);
                    }

                    if (ImGui::Button("hex.builtin.view.strings.extract"_lang))
                        this->searchStrings();
                },
                    this->m_searching);

                if (this->m_searching) {
                    ImGui::SameLine();
                    ImGui::TextSpinner("hex.builtin.view.strings.searching"_lang);
                } else if (this->m_foundStrings.size() > 0) {
                    ImGui::SameLine();
                    ImGui::TextFormatted("hex.builtin.view.strings.results"_lang, this->m_filterIndices.size());
                }


                ImGui::Separator();
                ImGui::NewLine();

                if (ImGui::BeginTable("##strings", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("hex.builtin.view.strings.offset"_lang, 0, -1, ImGui::GetID("offset"));
                    ImGui::TableSetupColumn("hex.builtin.view.strings.size"_lang, 0, -1, ImGui::GetID("size"));
                    ImGui::TableSetupColumn("hex.builtin.view.strings.string"_lang, 0, -1, ImGui::GetID("string"));

                    auto sortSpecs = ImGui::TableGetSortSpecs();

                    if (sortSpecs->SpecsDirty) {
                        std::sort(this->m_foundStrings.begin(), this->m_foundStrings.end(), [&sortSpecs](FoundString &left, FoundString &right) -> bool {
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
                                    return readString(left) > readString(right);
                                else
                                    return readString(left) < readString(right);
                            }

                            return false;
                        });

                        sortSpecs->SpecsDirty = false;
                    }

                    ImGui::TableHeadersRow();

                    ImGuiListClipper clipper;
                    clipper.Begin(this->m_filterIndices.size());

                    while (clipper.Step()) {
                        for (u64 i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                            auto &foundString = this->m_foundStrings[this->m_filterIndices[i]];
                            auto string       = readString(foundString);

                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            if (ImGui::Selectable(("##StringLine"s + std::to_string(i)).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                                EventManager::post<RequestSelectionChange>(Region { foundString.offset, foundString.size });
                            }
                            ImGui::PushID(i + 1);
                            createStringContextMenu(foundString);
                            ImGui::PopID();
                            ImGui::SameLine();
                            ImGui::TextFormatted("0x{0:08X} : 0x{1:08X}", foundString.offset, foundString.offset + foundString.size);
                            ImGui::TableNextColumn();
                            ImGui::TextFormatted("0x{0:04X}", foundString.size);
                            ImGui::TableNextColumn();

                            ImGui::TextUnformatted(string.c_str());
                        }
                    }
                    clipper.End();

                    ImGui::EndTable();
                }
            }
        }
        ImGui::End();

        if (ImGui::BeginPopup("hex.builtin.view.strings.demangle.title"_lang)) {
            if (ImGui::BeginChild("##scrolling", ImVec2(500, 150))) {
                ImGui::TextUnformatted("hex.builtin.view.strings.demangle.title"_lang);
                ImGui::Separator();
                ImGui::TextFormattedWrapped("{}", this->m_demangledName.c_str());
                ImGui::NewLine();
                if (ImGui::Button("hex.builtin.view.strings.demangle.copy"_lang))
                    ImGui::SetClipboardText(this->m_demangledName.c_str());
            }
            ImGui::EndChild();
            ImGui::EndPopup();
        }
    }

}