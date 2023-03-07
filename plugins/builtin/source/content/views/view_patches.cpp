#include "content/views/view_patches.hpp"

#include <hex/providers/provider.hpp>

#include <hex/api/project_file_manager.hpp>
#include <nlohmann/json.hpp>

#include <string>

using namespace std::literals::string_literals;

namespace hex::plugin::builtin {

    ViewPatches::ViewPatches() : View("hex.builtin.view.patches.name") {

        ProjectFile::registerPerProviderHandler({
            .basePath = "patches.json",
            .required = false,
            .load = [](prv::Provider *provider, const std::fs::path &basePath, Tar &tar) {
                auto json = nlohmann::json::parse(tar.read(basePath));
                provider->getPatches() = json["patches"].get<std::map<u64, u8>>();
                return true;
            },
            .store = [](prv::Provider *provider, const std::fs::path &basePath, Tar &tar) {
                nlohmann::json json;
                json["patches"] = provider->getPatches();
                tar.write(basePath, json.dump(4));

                return true;
            }
        });

        ImHexApi::HexEditor::addForegroundHighlightingProvider([](u64 offset, const u8* buffer, size_t, bool) -> std::optional<color_t> {
            hex::unused(buffer);

            if (!ImHexApi::Provider::isValid())
                return std::nullopt;

            auto provider = ImHexApi::Provider::get();

            u8 byte = 0x00;
            provider->read(offset, &byte, sizeof(u8), false);

            const auto &patches = provider->getPatches();
            if (patches.contains(offset) && patches.at(offset) != byte)
                return ImGui::GetCustomColorU32(ImGuiCustomCol_ToolbarRed);
            else
                return std::nullopt;
        });
    }

    void ViewPatches::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.patches.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {
            auto provider = ImHexApi::Provider::get();

            if (ImHexApi::Provider::isValid() && provider->isReadable()) {

                if (ImGui::BeginTable("##patchesTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("hex.builtin.view.patches.offset"_lang);
                    ImGui::TableSetupColumn("hex.builtin.view.patches.orig"_lang);
                    ImGui::TableSetupColumn("hex.builtin.view.patches.patch"_lang);

                    ImGui::TableHeadersRow();

                    auto &patches = provider->getPatches();
                    u32 index     = 0;

                    ImGuiListClipper clipper;

                    clipper.Begin(patches.size());
                    while (clipper.Step()) {
                        auto iter = patches.begin();
                        for (auto i = 0; i < clipper.DisplayStart; i++)
                            iter++;

                        for (auto i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                            const auto &[address, patch] = *iter;

                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();

                            if (ImGui::Selectable(("##patchLine" + std::to_string(index)).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                                ImHexApi::HexEditor::setSelection(address, 1);
                            }
                            if (ImGui::IsMouseReleased(1) && ImGui::IsItemHovered()) {
                                ImGui::OpenPopup("PatchContextMenu");
                                this->m_selectedPatch = address;
                            }
                            ImGui::SameLine();
                            ImGui::TextFormatted("0x{0:08X}", address);

                            ImGui::TableNextColumn();
                            u8 previousValue = 0x00;
                            provider->readRaw(address, &previousValue, sizeof(u8));
                            ImGui::TextFormatted("0x{0:02X}", previousValue);

                            ImGui::TableNextColumn();
                            ImGui::TextFormatted("0x{0:02X}", patch);
                            index += 1;

                            iter++;
                        }
                    }

                    if (ImGui::BeginPopup("PatchContextMenu")) {
                        if (ImGui::MenuItem("hex.builtin.view.patches.remove"_lang)) {
                            patches.erase(this->m_selectedPatch);
                        }
                        ImGui::EndPopup();
                    }

                    ImGui::EndTable();
                }
            }
        }
        ImGui::End();
    }

}