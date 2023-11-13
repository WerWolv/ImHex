#include "content/views/view_patches.hpp"

#include <hex/providers/provider.hpp>

#include <hex/api/project_file_manager.hpp>
#include <nlohmann/json.hpp>

#include <string>

using namespace std::literals::string_literals;

namespace hex::plugin::builtin {

    ViewPatches::ViewPatches() : View::Window("hex.builtin.view.patches.name") {

        ProjectFile::registerPerProviderHandler({
            .basePath = "patches.json",
            .required = false,
            .load = [](prv::Provider *provider, const std::fs::path &basePath, Tar &tar) {
                auto json = nlohmann::json::parse(tar.readString(basePath));
                auto patches = json.at("patches").get<std::map<u64, u8>>();

                for (const auto &[address, value] : patches) {
                    provider->write(address, &value, sizeof(value));
                }

                provider->getUndoStack().groupOperations(patches.size());

                return true;
            },
            .store = [](prv::Provider *, const std::fs::path &, Tar &) {

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

                    return ImGuiExt::GetCustomColorU32(ImGuiCustomCol_Patches);
            const auto &undoStack = provider->getUndoStack();
            for (const auto &operation : undoStack.getOperations()) {
                if (offset >= operation->getRegion().getStartAddress() && offset <= operation->getRegion().getEndAddress())
                    return ImGuiExt::GetCustomColorU32(ImGuiCustomCol_Patches);
            }

            return std::nullopt;
        });

        EventManager::subscribe<EventProviderSaved>([](auto *) {
            EventManager::post<EventHighlightingChanged>();
        });
    }

    void ViewPatches::drawContent() {
        auto provider = ImHexApi::Provider::get();

        if (ImHexApi::Provider::isValid() && provider->isReadable()) {

                if (ImGui::BeginTable("##patchesTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("hex.builtin.view.patches.id"_lang);
                    ImGui::TableSetupColumn("hex.builtin.view.patches.offset"_lang);
                    ImGui::TableSetupColumn("hex.builtin.view.patches.patch"_lang);

                ImGui::TableHeadersRow();

                    auto &operations = provider->getUndoStack().getOperations();
                    u32 index     = 0;

                ImGuiListClipper clipper;

                    clipper.Begin(operations.size());
                    while (clipper.Step()) {
                        auto iter = operations.begin();
                        for (auto i = 0; i < clipper.DisplayStart; i++)
                            ++iter;

                        for (auto i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                            const auto &operation = *iter;

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                            ImGui::TextFormatted("{}", index);

                            ImGui::TableNextColumn();

                            const auto [address, size] = operation->getRegion();

                            if (ImGui::Selectable(("##patchLine" + std::to_string(index)).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                                ImHexApi::HexEditor::setSelection(address, size);
                            }
                            if (ImGui::IsMouseReleased(1) && ImGui::IsItemHovered()) {
                                ImGui::OpenPopup("PatchContextMenu");
                                this->m_selectedPatch = address;
                            }
                            ImGui::SameLine();
                            ImGuiExt::TextFormatted("0x{0:08X}", address);

                            ImGui::TableNextColumn();
                            ImGuiExt::TextFormatted("{}", operation->format());
                            index += 1;

                            ++iter;
                        }
                    }
                }

                    ImGui::EndTable();
                }

                ImGui::EndTable();
            }
        }
    }

    void ViewPatches::drawAlwaysVisibleContent() {
        if (auto provider = ImHexApi::Provider::get(); provider != nullptr) {
            const auto &operations = provider->getUndoStack().getOperations();
            if (this->m_numOperations.get(provider) != operations.size()) {
                this->m_numOperations.get(provider) = operations.size();
                EventManager::post<EventHighlightingChanged>();
            }
        }
    }

}