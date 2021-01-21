#include "views/view_patches.hpp"

#include <hex/providers/provider.hpp>

#include <hex/helpers/utils.hpp>
#include "helpers/project_file_handler.hpp"

#include <string>

using namespace std::literals::string_literals;

namespace hex {

    ViewPatches::ViewPatches() : View("Patches") {
        View::subscribeEvent(Events::ProjectFileStore, [](auto) {
            auto provider = SharedData::currentProvider;
            if (provider != nullptr)
                ProjectFile::setPatches(provider->getPatches());
        });

        View::subscribeEvent(Events::ProjectFileLoad, [](auto) {
            auto provider = SharedData::currentProvider;
            if (provider != nullptr)
                provider->getPatches() = ProjectFile::getPatches();
        });
    }

    ViewPatches::~ViewPatches() {
        View::unsubscribeEvent(Events::ProjectFileStore);
        View::unsubscribeEvent(Events::ProjectFileLoad);
    }

    void ViewPatches::drawContent() {
        if (ImGui::Begin("Patches", &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            auto provider = SharedData::currentProvider;

            if (provider != nullptr && provider->isReadable()) {

                if (ImGui::BeginTable("##patchesTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable |
                                                        ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("Offset");
                    ImGui::TableSetupColumn("Previous Value");
                    ImGui::TableSetupColumn("Patched Value");

                    ImGui::TableHeadersRow();

                    auto& patches = provider->getPatches();
                    u32 index = 0;
                    for (const auto &[address, patch] : patches) {

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        if (ImGui::Selectable(("##patchLine" + std::to_string(index)).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                            Region selectRegion = { address, 1 };
                            View::postEvent(Events::SelectionChangeRequest, selectRegion);
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
                        if (ImGui::MenuItem("Remove")) {
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