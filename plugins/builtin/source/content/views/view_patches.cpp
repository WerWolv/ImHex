#include "content/views/view_patches.hpp"

#include <hex/providers/provider.hpp>

#include <hex/api/events/events_interaction.hpp>
#include <nlohmann/json.hpp>

#include <content/providers/undo_operations/operation_write.hpp>
#include <content/providers/undo_operations/operation_insert.hpp>
#include <content/providers/undo_operations/operation_remove.hpp>

#include <ranges>
#include <string>

using namespace std::literals::string_literals;

namespace hex::plugin::builtin {

    ViewPatches::ViewPatches() : View::Window("hex.builtin.view.patches.name"_unlocalized, ICON_VS_GIT_PULL_REQUEST_NEW_CHANGES) {

        MovePerProviderData::subscribe(this, [this](prv::Provider *from, prv::Provider *to) {
             m_savedOperations.get(from) = 0;
             m_savedOperations.get(to)   = 0;
        });

        ImHexApi::HexEditor::addForegroundHighlightingProvider([this](u64 offset, const u8* buffer, size_t size, bool) -> std::optional<color_t> {
            std::lock_guard lock(prv::undo::Stack::getMutex());

            if (!ImHexApi::Provider::isValid())
                return std::nullopt;

            auto provider = ImHexApi::Provider::get();
            if (!provider->isSavable())
                return std::nullopt;

            offset -= provider->getBaseAddress();

            const auto &insertedAddresses = m_insertedAddresses.get(provider);
            const auto &originalByteValues = m_originalByteValues.get(provider);

            // Highlight the cell if any byte in it is inserted, or its live value
            // differs from the value it had when it was first touched.
            for (u64 i = 0; i < size; i++) {
                const auto address = offset + i;

                if (insertedAddresses.contains(address))
                    return ImGuiExt::GetCustomColorU32(ImGuiCustomCol_Patches);

                if (auto it = originalByteValues.find(address); it != originalByteValues.end() && it->second != buffer[i])
                    return ImGuiExt::GetCustomColorU32(ImGuiCustomCol_Patches);
            }

            return std::nullopt;
        });

        EventProviderSaved::subscribe([this](prv::Provider *provider) {
            m_savedOperations.get(provider) = provider->getUndoStack().getAppliedOperations().size();
            m_insertedAddresses.get(provider).clear();
            m_originalByteValues.get(provider).clear();
            EventHighlightingChanged::post();
        });

        EventProviderDataModified::subscribe(this, [this](prv::Provider *provider, u64 offset, u64 size, const u8 *data) {
            if (size == 0)
                return;

            offset -= provider->getBaseAddress();

            std::vector<u8> oldData(size, 0x00);
            provider->read(offset, oldData.data(), size);

            // Record the true original value the first time a byte is touched, and
            // only for bytes that actually change. A later edit that restores that
            // value will then correctly stop being highlighted.
            auto &originalByteValues = m_originalByteValues.get(provider);
            bool anyByteChanged = false;
            for (u64 i = 0; i < size; i++) {
                if (oldData[i] != data[i]) {
                    originalByteValues.try_emplace(offset + i, oldData[i]);
                    anyByteChanged = true;
                }
            }

            if (!anyByteChanged)
                return;

            provider->getUndoStack().add<undo::OperationWrite>(offset, size, oldData.data(), data);
        });

        EventProviderDataInserted::subscribe(this, [](prv::Provider *provider, u64 offset, u64 size) {
            provider->getUndoStack().add<undo::OperationInsert>(offset, size);
        });

        EventProviderDataRemoved::subscribe(this, [](prv::Provider *provider, u64 offset, u64 size) {
            offset -= provider->getBaseAddress();

            provider->getUndoStack().add<undo::OperationRemove>(offset, size);
        });

        EventDataChanged::subscribe(this, [this](prv::Provider *provider) {
            std::lock_guard lock(prv::undo::Stack::getMutex());

            const auto &undoStack = provider->getUndoStack();
            const auto stackSize = undoStack.getAppliedOperations().size();
            const auto savedStackSize = m_savedOperations.get(provider);

            // Inserted bytes are new; there's no "original" value to diff against, so
            // track which addresses are currently inserted from the undo stack instead.
            auto &insertedAddresses = m_insertedAddresses.get(provider);
            insertedAddresses.clear();
            if (stackSize == savedStackSize) {
                // Do nothing
            } else if (stackSize > savedStackSize) {
                for (const auto &operation : undoStack.getAppliedOperations() | std::views::drop(savedStackSize)) {
                    if (dynamic_cast<const undo::OperationInsert *>(operation.get()) == nullptr)
                        continue;

                    auto region = operation->getRegion();
                    for (u64 addr = region.getStartAddress(); addr <= region.getEndAddress(); addr++) {
                        insertedAddresses.insert(addr);
                    }
                }
            } else {
                for (const auto &operation : undoStack.getUndoneOperations() | std::views::reverse | std::views::take(savedStackSize - stackSize)) {
                    if (dynamic_cast<const undo::OperationInsert *>(operation.get()) == nullptr)
                        continue;

                    auto region = operation->getRegion();
                    for (u64 addr = region.getStartAddress(); addr <= region.getEndAddress(); addr++) {
                        insertedAddresses.insert(addr);
                    }
                }
            }
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

                std::lock_guard lock(prv::undo::Stack::getMutex());

                const auto &undoRedoStack = provider->getUndoStack();
                const auto &undoneOps = undoRedoStack.getUndoneOperations();
                const auto &appliedOps = undoRedoStack.getAppliedOperations();

                const u32 totalSize = undoneOps.size() + appliedOps.size();

                ImGuiListClipper clipper;
                clipper.Begin(totalSize);

                while (clipper.Step()) {
                    auto undoneOperationsCount = undoneOps.size();
                    for (u64 i = clipper.DisplayStart; i < u64(clipper.DisplayEnd); i++) {
                        prv::undo::Operation* operation;

                        if (i < undoneOps.size()) {
                            // Element from undone operations
                            operation = undoneOps[i].get();
                        } else {
                            // Element from applied operations (reversed)
                            u32 appliedIndex = appliedOps.size() - 1 - (i - undoneOps.size());
                            operation = appliedOps[appliedIndex].get();
                        }

                        const auto [address, size] = operation->getRegion();

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        bool isUndone = size_t(i) < undoneOperationsCount;
                        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(isUndone ? ImGuiCol_TextDisabled : ImGuiCol_Text));
                        if (ImGui::Selectable(fmt::format("{} {}", i == undoneOperationsCount ? ICON_VS_ARROW_SMALL_RIGHT : "  ", i).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                            if (ImGui::GetIO().KeyShift) {
                                if (isUndone) {
                                    const u32 count = undoneOperationsCount - u32(i);
                                    provider->getUndoStack().redo(count);
                                } else {
                                    const u32 count = u32(i) - undoneOperationsCount;
                                    provider->getUndoStack().undo(count);
                                }
                            } else {
                                ImHexApi::HexEditor::setSelection(address, size);
                            }
                        }
                        ImGui::PopStyleColor();

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

    void ViewPatches::drawHelpText() {
        ImGuiExt::TextFormattedWrapped("This view shows a list of all patches (modifications, insertions, deletions) that were made to the current data source so far.");
        ImGui::NewLine();
        ImGuiExt::TextFormattedWrapped("The small arrow next to a patch indicates the current position in the undo/redo stack. When undoing operations, the arrow will move downwards and modifying any data will create new patches from the current position, discarding any patches above it.");
        ImGuiExt::TextFormattedWrapped("Hovering over a patch will also show a tooltip with more detailed information about the patch and clicking on a patch will select the modified region in the hex editor.");
    }

}
