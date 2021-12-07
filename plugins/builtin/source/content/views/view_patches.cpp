#include "content/views/view_patches.hpp"

#include <hex/providers/provider.hpp>

#include <hex/helpers/project_file_handler.hpp>

#include <string>

using namespace std::literals::string_literals;

namespace hex::plugin::builtin {

    ViewPatches::ViewPatches() : View("hex.builtin.view.patches.name") {
        EventManager::subscribe<EventProjectFileStore>(this, []{
            auto provider = ImHexApi::Provider::get();
            if (ImHexApi::Provider::isValid())
                ProjectFile::setPatches(provider->getPatches());
        });

        EventManager::subscribe<EventProjectFileLoad>(this, []{
            auto provider = ImHexApi::Provider::get();
            if (ImHexApi::Provider::isValid())
                provider->getPatches() = ProjectFile::getPatches();
        });
    }

    ViewPatches::~ViewPatches() {
        EventManager::unsubscribe<EventProjectFileStore>(this);
        EventManager::unsubscribe<EventProjectFileLoad>(this);
    }

    void ViewPatches::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.patches.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            auto provider = ImHexApi::Provider::get();

            if (ImHexApi::Provider::isValid() && provider->isReadable()) {

                if (ImGui::BeginTable("##patchesTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable |
                                                        ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("hex.builtin.view.patches.offset"_lang);
                    ImGui::TableSetupColumn("hex.builtin.view.patches.orig"_lang);
                    ImGui::TableSetupColumn("hex.builtin.view.patches.patch"_lang);

                    ImGui::TableHeadersRow();

                    auto& patches = provider->getPatches();
                    u32 index = 0;
                    for (const auto &[address, patch] : patches) {

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        if (ImGui::Selectable(("##patchLine" + std::to_string(index)).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                            EventManager::post<RequestSelectionChange>(Region { address, 1 });
                        }
                        if (ImGui::IsMouseReleased(1) && ImGui::IsItemHovered()) {
                            ImGui::OpenPopup("PatchContextMenu");
                            this->m_selectedPatch = address;
                        }
                        ImGui::SameLine();
                        ImGui::Text("0x%08lX", address);

                        ImGui::TableNextColumn();
                        u8 previousValue = 0x00;
                        provider->readRaw(address, &previousValue, sizeof(u8));
                        ImGui::Text("0x%02X", previousValue);

                        ImGui::TableNextColumn();
                        ImGui::Text("0x%02X", patch);
                        index += 1;
                    }

                    if (ImGui::BeginPopup("PatchContextMenu")) {
                        if (ImGui::MenuItem("hex.builtin.view.patches.remove"_lang)) {
                            patches.erase(this->m_selectedPatch);
                            ProjectFile::markDirty();
                        }
                        ImGui::EndPopup();
                    }

                    ImGui::EndTable();
                }
            }
        }
        ImGui::End();
    }

    void ViewPatches::drawMenu() {

    }

}