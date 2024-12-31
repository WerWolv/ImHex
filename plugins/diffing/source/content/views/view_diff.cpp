#include "content/views/view_diff.hpp"

#include <hex/api/imhex_api.hpp>

#include <hex/helpers/fmt.hpp>
#include <hex/providers/buffered_reader.hpp>

#include <fonts/vscode_icons.hpp>
#include <wolv/utils/guards.hpp>

namespace hex::plugin::diffing {

    using DifferenceType = ContentRegistry::Diffing::DifferenceType;

    ViewDiff::ViewDiff() : View::Window("hex.diffing.view.diff.name", ICON_VS_DIFF_SIDEBYSIDE) {
        // Clear the selected diff providers when a provider is closed
        EventProviderClosed::subscribe(this, [this](prv::Provider *) {
            this->reset();
        });
        EventDataChanged::subscribe(this, [this](prv::Provider *) {
            m_analyzed = false;
        });

        // Set the background highlight callbacks for the two hex editor columns
        m_columns[0].hexEditor.setBackgroundHighlightCallback(this->createCompareFunction(1));
        m_columns[1].hexEditor.setBackgroundHighlightCallback(this->createCompareFunction(0));
    }

    ViewDiff::~ViewDiff() {
        EventProviderClosed::unsubscribe(this);
        EventDataChanged::unsubscribe(this);
    }

    namespace {

        bool drawDiffColumn(ViewDiff::Column &column, float height) {
            if (height < 0)
                return false;

            bool scrolled = false;
            ImGui::PushID(&column);
            ON_SCOPE_EXIT { ImGui::PopID(); };

            // Draw the hex editor
            float prevScroll = column.hexEditor.getScrollPosition();
            column.hexEditor.draw(height);
            float currScroll = column.hexEditor.getScrollPosition();

            // Check if the user scrolled the hex editor
            if (prevScroll != currScroll) {
                scrolled = true;
                column.scrollLock = 5;
            }

            return scrolled;
        }

        bool drawProviderSelector(ViewDiff::Column &column) {
            bool shouldReanalyze = false;

            ImGui::PushID(&column);

            auto providers = ImHexApi::Provider::getProviders();
            auto &providerIndex = column.provider;

            // Get the name of the currently selected provider
            std::string preview;
            if (ImHexApi::Provider::isValid() && providerIndex >= 0)
                preview = providers[providerIndex]->getName();

            // Draw combobox with all available providers
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::BeginCombo("", preview.c_str())) {

                for (size_t i = 0; i < providers.size(); i++) {
                    ImGui::PushID(i + 1);
                    if (ImGui::Selectable(providers[i]->getName().c_str())) {
                        providerIndex = i;
                        shouldReanalyze = true;
                    }
                    ImGui::PopID();
                }

                ImGui::EndCombo();
            }

            ImGui::PopID();

            return shouldReanalyze;
        }

    }

    void ViewDiff::analyze(prv::Provider *providerA, prv::Provider *providerB) {
        auto commonSize = std::max(providerA->getActualSize(), providerB->getActualSize());
        m_diffTask = TaskManager::createTask("hex.diffing.view.diff.task.diffing"_lang, commonSize, [this, providerA, providerB](Task &) {
            auto differences = m_algorithm->analyze(providerA, providerB);

            auto providers = ImHexApi::Provider::getProviders();

            // Move the calculated differences over so they can be displayed
            for (size_t i = 0; i < m_columns.size(); i++) {
                auto &column = m_columns[i];
                auto &provider = providers[column.provider];

                column.differences = differences[i].overlapping({ provider->getBaseAddress(), provider->getBaseAddress() + provider->getActualSize() });
                std::ranges::sort(
                    column.differences,
                    std::less(),
                    [](const auto &a) { return a.interval; }
                );

                column.diffTree = std::move(differences[i]);
            }
            m_analyzed = true;
        });
    }

    void ViewDiff::reset() {
        for (auto &column : m_columns) {
            column.provider = -1;
            column.hexEditor.setSelectionUnchecked(std::nullopt, std::nullopt);
            column.diffTree.clear();
        }
    }


    std::function<std::optional<color_t>(u64, const u8*, size_t)> ViewDiff::createCompareFunction(size_t otherIndex) const {
        const auto currIndex = otherIndex == 0 ? 1 : 0;
        return [=, this](u64 address, const u8 *, size_t size) -> std::optional<color_t> {
            if (!m_analyzed)
                return std::nullopt;

            const auto matches = m_columns[currIndex].diffTree.overlapping({ address, (address + size) - 1 });
            if (matches.empty())
                return std::nullopt;

            const auto type = matches[0].value;

            if (type == DifferenceType::Mismatch) {
                return ImGuiExt::GetCustomColorU32(ImGuiCustomCol_DiffChanged);
            } else if (type == DifferenceType::Insertion && currIndex == 0) {
                return ImGuiExt::GetCustomColorU32(ImGuiCustomCol_DiffAdded);
            } else if (type == DifferenceType::Deletion && currIndex == 1) {
                return ImGuiExt::GetCustomColorU32(ImGuiCustomCol_DiffRemoved);
            }

            return std::nullopt;
        };
    }

    static void drawByteString(const std::vector<u8> &bytes) {
        for (u64 i = 0; i < bytes.size(); i += 1) {
            if (i >= 16) {
                ImGui::TextDisabled(ICON_VS_ELLIPSIS);
                ImGui::SameLine(0, 0);
                break;
            }

            u8 byte = bytes[i];
            ImGuiExt::TextFormattedDisabled("{0:02X} ", byte);
            ImGui::SameLine(0, (i % 4 == 3) ? 4_scaled : 0);
        }
    }

    void ViewDiff::drawContent() {
        auto &[a, b] = m_columns;

        a.hexEditor.enableSyncScrolling(false);
        b.hexEditor.enableSyncScrolling(false);

        if (a.scrollLock > 0) a.scrollLock--;
        if (b.scrollLock > 0) b.scrollLock--;

        // Change the hex editor providers if the user selected a new provider
        {
            const auto &providers = ImHexApi::Provider::getProviders();
            if (a.provider >= 0 && size_t(a.provider) < providers.size())
                a.hexEditor.setProvider(providers[a.provider]);
            else
                a.hexEditor.setProvider(nullptr);

            if (b.provider >= 0 && size_t(b.provider) < providers.size())
                b.hexEditor.setProvider(providers[b.provider]);
            else
                b.hexEditor.setProvider(nullptr);
        }

        // Analyze the providers if they are valid and the user selected a new provider
        if (!m_analyzed && a.provider != -1 && b.provider != -1 && !m_diffTask.isRunning() && m_algorithm != nullptr) {
            const auto &providers = ImHexApi::Provider::getProviders();
            auto providerA = providers[a.provider];
            auto providerB = providers[b.provider];

            this->analyze(providerA, providerB);
        }

        if (auto &algorithms = ContentRegistry::Diffing::impl::getAlgorithms(); m_algorithm == nullptr && !algorithms.empty())
            m_algorithm = algorithms.front().get();

        static float height = 0;
        static bool dragging = false;

        const auto availableSize = ImGui::GetContentRegionAvail();
        auto diffingColumnSize = availableSize;
        diffingColumnSize.y *= 3.5 / 5.0;
        diffingColumnSize.y -= ImGui::GetTextLineHeightWithSpacing();
        diffingColumnSize.y += height;

        if (availableSize.y > 1)
            diffingColumnSize.y = std::clamp(diffingColumnSize.y, 1.0F, std::max(1.0F, availableSize.y - ImGui::GetTextLineHeightWithSpacing() * 3));


        // Draw the two hex editor columns side by side
        if (ImGui::BeginTable("##binary_diff", 2, ImGuiTableFlags_None, diffingColumnSize)) {
            ImGui::TableSetupColumn("hex.diffing.view.diff.provider_a"_lang);
            ImGui::TableSetupColumn("hex.diffing.view.diff.provider_b"_lang);
            ImGui::TableHeadersRow();

            ImGui::BeginDisabled(m_diffTask.isRunning());
            {
                // Draw settings button
                ImGui::TableNextColumn();
                if (ImGuiExt::DimmedIconButton(ICON_VS_SETTINGS_GEAR, ImGui::GetStyleColorVec4(ImGuiCol_Text)))
                    RequestOpenPopup::post("##DiffingAlgorithmSettings");

                ImGui::SameLine();

                // Draw first provider selector
                if (drawProviderSelector(a)) m_analyzed = false;

                // Draw second provider selector
                ImGui::TableNextColumn();
                if (drawProviderSelector(b)) m_analyzed = false;
            }
            ImGui::EndDisabled();

            ImGui::TableNextRow();

            // Draw first hex editor column
            ImGui::TableNextColumn();
            bool scrollB = drawDiffColumn(a, diffingColumnSize.y);

            // Draw second hex editor column
            ImGui::TableNextColumn();
            bool scrollA = drawDiffColumn(b, diffingColumnSize.y);

            // Sync the scroll positions of the hex editors
            {
                if (scrollA && a.scrollLock == 0) {
                    a.hexEditor.setScrollPosition(b.hexEditor.getScrollPosition());
                    a.hexEditor.forceUpdateScrollPosition();
                }
                if (scrollB && b.scrollLock == 0) {
                    b.hexEditor.setScrollPosition(a.hexEditor.getScrollPosition());
                    b.hexEditor.forceUpdateScrollPosition();
                }
            }

            ImGui::EndTable();
        }

        ImGui::Button("##table_drag_bar", ImVec2(ImGui::GetContentRegionAvail().x, 2_scaled));
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0)) {
            if (ImGui::IsItemHovered())
                dragging = true;
        } else {
            dragging = false;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        }

        if (dragging) {
            height += ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0).y;
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        }

        // Draw the differences table
        if (ImGui::BeginTable("##differences", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Reorderable | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("##Type", ImGuiTableColumnFlags_NoReorder);
            ImGui::TableSetupColumn("hex.diffing.view.diff.provider_a"_lang);
            ImGui::TableSetupColumn("hex.diffing.view.diff.provider_b"_lang);
            ImGui::TableSetupColumn("hex.diffing.view.diff.changes"_lang);
            ImGui::TableHeadersRow();

            // Draw the differences if the providers have been analyzed
            if (m_analyzed) {
                ImGuiListClipper clipper;

                auto &differencesA = m_columns[0].differences;
                auto &differencesB = m_columns[1].differences;
                clipper.Begin(int(std::min(differencesA.size(), differencesB.size())));

                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        ImGui::TableNextRow();

                        ImGui::PushID(i);

                        const auto &[regionA, typeA] = differencesA[i];
                        const auto &[regionB, typeB] = differencesB[i];

                        // Draw a clickable row for each difference that will select the difference in both hex editors

                        // Draw difference type
                        ImGui::TableNextColumn();
                        switch (typeA) {
                            case DifferenceType::Mismatch:
                                ImGuiExt::TextFormattedColored(ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_DiffChanged), ICON_VS_DIFF_MODIFIED);
                                ImGui::SetItemTooltip("%s", "hex.diffing.view.diff.modified"_lang.get());
                                break;
                            case DifferenceType::Insertion:
                                ImGuiExt::TextFormattedColored(ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_DiffAdded), ICON_VS_DIFF_ADDED);
                                ImGui::SetItemTooltip("%s", "hex.diffing.view.diff.added"_lang.get());
                                break;
                            case DifferenceType::Deletion:
                                ImGuiExt::TextFormattedColored(ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_DiffRemoved), ICON_VS_DIFF_REMOVED);
                                ImGui::SetItemTooltip("%s", "hex.diffing.view.diff.removed"_lang.get());
                                break;
                            default:
                                break;
                        }

                        // Draw start address
                        ImGui::TableNextColumn();
                        if (ImGui::Selectable(hex::format("0x{:04X} - 0x{:04X}", regionA.start, regionA.end).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                            const Region selectionA = { regionA.start, ((regionA.end - regionA.start) + 1) };
                            const Region selectionB = { regionB.start, ((regionB.end - regionB.start) + 1) };

                            a.hexEditor.setSelection(selectionA);
                            a.hexEditor.jumpToSelection();
                            b.hexEditor.setSelection(selectionB);
                            b.hexEditor.jumpToSelection();

                            const auto &providers = ImHexApi::Provider::getProviders();
                            auto openProvider = ImHexApi::Provider::get();

                            if (providers[a.provider] == openProvider)
                                ImHexApi::HexEditor::setSelection(selectionA);
                            else if (providers[b.provider] == openProvider)
                                ImHexApi::HexEditor::setSelection(selectionB);
                        }

                        // Draw end address
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(hex::format("0x{:04X} - 0x{:04X}", regionB.start, regionB.end).c_str());

                        const auto &providers = ImHexApi::Provider::getProviders();
                        std::vector<u8> data;

                        // Draw changes
                        ImGui::TableNextColumn();
                        ImGui::Indent();
                        switch (typeA) {
                            case DifferenceType::Insertion:
                                data.resize(std::min<u64>(17, (regionA.end - regionA.start) + 1));
                                providers[a.provider]->read(regionA.start, data.data(), data.size());
                                drawByteString(data);
                                break;
                            case DifferenceType::Mismatch:
                                data.resize(std::min<u64>(17, (regionA.end - regionA.start) + 1));
                                providers[a.provider]->read(regionA.start, data.data(), data.size());
                                drawByteString(data);

                                ImGui::SameLine(0, 0);
                                ImGuiExt::TextFormatted(" {}  ", ICON_VS_ARROW_RIGHT);
                                ImGui::SameLine(0, 0);

                                data.resize(std::min<u64>(17, (regionB.end - regionB.start) + 1));
                                providers[b.provider]->read(regionB.start, data.data(), data.size());
                                drawByteString(data);
                                break;
                            case DifferenceType::Deletion:
                                data.resize(std::min<u64>(17, (regionB.end - regionB.start) + 1));
                                providers[b.provider]->read(regionB.start, data.data(), data.size());
                                drawByteString(data);
                                break;
                            default:
                                break;
                        }
                        ImGui::Unindent();

                        ImGui::PopID();
                    }
                }
            }

            ImGui::EndTable();
        }
    }

    void ViewDiff::drawAlwaysVisibleContent() {
        ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(400_scaled, 600_scaled));
        if (ImGui::BeginPopup("##DiffingAlgorithmSettings")) {
            ImGuiExt::Header("hex.diffing.view.diff.algorithm"_lang, true);
            ImGui::PushItemWidth(300_scaled);
            if (ImGui::BeginCombo("##Algorithm", m_algorithm == nullptr ? "" : Lang(m_algorithm->getUnlocalizedName()))) {
                for (const auto &algorithm : ContentRegistry::Diffing::impl::getAlgorithms()) {
                    ImGui::PushID(algorithm.get());
                    if (ImGui::Selectable(Lang(algorithm->getUnlocalizedName()))) {
                        m_algorithm = algorithm.get();
                        m_analyzed  = false;
                    }
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            if (m_algorithm != nullptr) {
                ImGuiExt::TextFormattedWrapped("{}", Lang(m_algorithm->getUnlocalizedDescription()));
            }

            ImGuiExt::Header("hex.diffing.view.diff.settings"_lang);
            if (m_algorithm != nullptr) {
                auto drawList = ImGui::GetWindowDrawList();
                auto prevIdx = drawList->_VtxCurrentIdx;
                m_algorithm->drawSettings();
                auto currIdx = drawList->_VtxCurrentIdx;

                if (prevIdx == currIdx)
                    ImGuiExt::TextFormatted("hex.diffing.view.diff.settings.no_settings"_lang);
            }

            ImGui::EndPopup();
        }
    }


}
