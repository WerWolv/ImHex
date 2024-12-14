#include "content/views/view_patches.hpp"

#include <hex/providers/provider.hpp>

#include <hex/api/project_file_manager.hpp>
#include <nlohmann/json.hpp>

#include <content/providers/undo_operations/operation_write.hpp>
#include <content/providers/undo_operations/operation_insert.hpp>
#include <content/providers/undo_operations/operation_remove.hpp>

#include <ranges>
#include <string>

using namespace std::literals::string_literals;

namespace hex::plugin::builtin {

    ViewPatches::ViewPatches() : View::Window("hex.builtin.view.patches.name", ICON_VS_GIT_PULL_REQUEST_NEW_CHANGES) {

        ProjectFile::registerPerProviderHandler({
            .basePath = "patches.json",
            .required = false,
            .load = [](prv::Provider *provider, const std::fs::path &basePath, const Tar &tar) {
                auto json = nlohmann::json::parse(tar.readString(basePath));
                auto patches = json.at("patches").get<std::map<u64, u8>>();

                for (const auto &[address, value] : patches) {
                    provider->write(address, &value, sizeof(value));
                }

                provider->getUndoStack().groupOperations(patches.size(), "hex.builtin.undo_operation.patches");

                return true;
            },
            .store = [](prv::Provider *, const std::fs::path &, Tar &) {
                return true;
            }
        });

        MovePerProviderData::subscribe(this, [this](prv::Provider *from, prv::Provider *to) {
             m_savedOperations.get(from) = 0;
             m_savedOperations.get(to)   = 0;
        });

        ImHexApi::HexEditor::addForegroundHighlightingProvider([this](u64 offset, const u8* buffer, size_t, bool) -> std::optional<color_t> {
            std::ignore = buffer;

            if (!ImHexApi::Provider::isValid())
                return std::nullopt;

            auto provider = ImHexApi::Provider::get();
            if (!provider->isSavable())
                return std::nullopt;

            offset -= provider->getBaseAddress();

            const auto &undoStack = provider->getUndoStack();
            const auto stackSize = undoStack.getAppliedOperations().size();
            const auto savedStackSize = m_savedOperations.get(provider);

            if (stackSize == savedStackSize) {
                // Do nothing
            } else if (stackSize > savedStackSize) {
                for (const auto &operation : undoStack.getAppliedOperations() | std::views::drop(savedStackSize)) {
                    if (!operation->shouldHighlight())
                        continue;

                    if (operation->getRegion().overlaps(Region { offset, 1}))
                        return ImGuiExt::GetCustomColorU32(ImGuiCustomCol_Patches);
                }
            } else {
                for (const auto &operation : undoStack.getUndoneOperations() | std::views::reverse | std::views::take(savedStackSize - stackSize)) {
                    if (!operation->shouldHighlight())
                        continue;

                    if (operation->getRegion().overlaps(Region { offset, 1}))
                        return ImGuiExt::GetCustomColorU32(ImGuiCustomCol_Patches);
                }
            }

            return std::nullopt;
        });

        EventProviderSaved::subscribe([this](prv::Provider *provider) {
            m_savedOperations.get(provider) = provider->getUndoStack().getAppliedOperations().size();
            EventHighlightingChanged::post();
        });

        EventProviderDataModified::subscribe(this, [](prv::Provider *provider, u64 offset, u64 size, const u8 *data) {
            if (size == 0)
                return;

            offset -= provider->getBaseAddress();

            std::vector<u8> oldData(size, 0x00);
            provider->read(offset, oldData.data(), size);
            provider->getUndoStack().add<undo::OperationWrite>(offset, size, oldData.data(), data);
        });

        EventProviderDataInserted::subscribe(this, [](prv::Provider *provider, u64 offset, u64 size) {
            offset -= provider->getBaseAddress();

            provider->getUndoStack().add<undo::OperationInsert>(offset, size);
        });

        EventProviderDataRemoved::subscribe(this, [](prv::Provider *provider, u64 offset, u64 size) {
            offset -= provider->getBaseAddress();

            provider->getUndoStack().add<undo::OperationRemove>(offset, size);
        });
    }

    ViewPatches::~ViewPatches() {
        MovePerProviderData::unsubscribe(this);
        EventProviderSaved::unsubscribe(this);
        EventProviderDataModified::unsubscribe(this);
        EventProviderDataInserted::unsubscribe(this);
        EventProviderDataRemoved::unsubscribe(this);
    }


    void ViewPatches::drawContent() {
        auto provider = ImHexApi::Provider::get();

        if (ImHexApi::Provider::isValid() && provider->isReadable()) {

            if (ImGui::BeginTable("##patchesTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("##PatchID", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoReorder | ImGuiTableColumnFlags_NoResize);
                ImGui::TableSetupColumn("hex.builtin.view.patches.offset"_lang, ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("hex.builtin.view.patches.patch"_lang, ImGuiTableColumnFlags_WidthStretch);

                ImGui::TableHeadersRow();

                const auto &undoRedoStack = provider->getUndoStack();
                std::vector<prv::undo::Operation*> operations;
                for (const auto &operation : undoRedoStack.getUndoneOperations())
                    operations.push_back(operation.get());
                for (const auto &operation : undoRedoStack.getAppliedOperations() | std::views::reverse)
                    operations.push_back(operation.get());

                u32 index = 0;

                ImGuiListClipper clipper;

                clipper.Begin(operations.size());
                while (clipper.Step()) {
                    auto iter = operations.begin();
                    for (auto i = 0; i < clipper.DisplayStart; i++)
                        ++iter;

                    auto undoneOperationsCount = undoRedoStack.getUndoneOperations().size();
                    for (auto i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        const auto &operation = *iter;

                        const auto [address, size] = operation->getRegion();

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        ImGui::BeginDisabled(size_t(i) < undoneOperationsCount);

                        if (ImGui::Selectable(hex::format("{} {}", index == undoneOperationsCount ? ICON_VS_ARROW_SMALL_RIGHT : " ", index).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                            ImHexApi::HexEditor::setSelection(address, size);
                        }
                        if (ImGui::IsItemHovered()) {
                            const auto content = operation->formatContent();
                            if (!content.empty()) {
                                if (ImGui::BeginTooltip()) {
                                    if (ImGui::BeginTable("##content_table", 1, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
                                        for (const auto &entry : content) {
                                            ImGui::TableNextRow();
                                            ImGui::TableNextColumn();

                                            ImGuiExt::TextFormatted("{}", entry);
                                        }
                                        ImGui::EndTable();
                                    }
                                    ImGui::EndTooltip();
                                }
                            }
                        }
                        if (ImGui::IsMouseReleased(1) && ImGui::IsItemHovered()) {
                            ImGui::OpenPopup("PatchContextMenu");
                            m_selectedPatch = address;
                        }

                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("0x{0:08X}", address);

                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("{}", operation->format());
                        index += 1;

                        ++iter;

                        ImGui::EndDisabled();
                    }
                }

                ImGui::EndTable();
            }
        }
    }

    void ViewPatches::drawAlwaysVisibleContent() {
        if (auto provider = ImHexApi::Provider::get(); provider != nullptr) {
            const auto &operations = provider->getUndoStack().getAppliedOperations();
            if (m_numOperations.get(provider) != operations.size()) {
                m_numOperations.get(provider) = operations.size();
                EventHighlightingChanged::post();
            }
        }
    }

}