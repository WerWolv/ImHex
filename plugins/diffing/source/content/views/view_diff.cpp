#include "content/views/view_diff.hpp"
#include <toasts/toast_notification.hpp>

#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/events/requests_gui.hpp>
#include <hex/api/content_registry/user_interface.hpp>

#include <hex/helpers/fmt.hpp>
#include <hex/providers/buffered_reader.hpp>

#include <fonts/vscode_icons.hpp>
#include <fonts/tabler_icons.hpp>
#include <wolv/utils/guards.hpp>

namespace hex::plugin::diffing {

    using DifferenceType = ContentRegistry::Diffing::DifferenceType;

    ViewDiff::ViewDiff() : View::Window("hex.diffing.view.diff.name", ICON_VS_DIFF) {
        // Clear the selected diff providers when a provider is closed
        EventProviderClosed::subscribe(this, [this](prv::Provider *) {
            this->reset();
        });
        EventDataChanged::subscribe(this, [this](prv::Provider *) {
            m_analysisInterrupted = m_analyzed = false;
        });

        // Handle region selection
        EventRegionSelected::subscribe(this, [this](const auto &region) {
            // Save current selection
            if (!ImHexApi::Provider::isValid() || region == Region::Invalid()) {
                m_selectedProvider = nullptr;
            } else {
                m_selectedAddress = region.address;
                m_selectedProvider = region.getProvider();
            }
        });

        // Set the background highlight callbacks for the two hex editor columns
        m_columns[0].hexEditor.setBackgroundHighlightCallback(this->createCompareFunction(1));
        m_columns[1].hexEditor.setBackgroundHighlightCallback(this->createCompareFunction(0));

        this->registerMenuItems();
    }

    ViewDiff::~ViewDiff() {
        EventProviderClosed::unsubscribe(this);
        EventDataChanged::unsubscribe(this);
        EventRegionSelected::unsubscribe(this);
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

            std::erase_if(providers, [&](const prv::Provider *provider) {
                if (!provider->isAvailable() || !provider->isReadable())
                    return true;

                return false;
            });

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
        m_diffTask = TaskManager::createTask("hex.diffing.view.diff.task.diffing", commonSize, [this, providerA, providerB](Task &task) {
            task.setInterruptCallback([this]{ m_analysisInterrupted = true; });

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
            column.differences.clear();
        }
        m_analysisInterrupted = m_analyzed  = false;
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
        if (!m_analyzed && !m_analysisInterrupted && a.provider != -1 && b.provider != -1 && !m_diffTask.isRunning() && m_algorithm != nullptr) {
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
        diffingColumnSize.y *= 3.5F / 5.0F;
        diffingColumnSize.y -= ImGui::GetTextLineHeightWithSpacing();
        diffingColumnSize.y += height;

        if (availableSize.y > 1)
            diffingColumnSize.y = std::clamp(diffingColumnSize.y, 1.0F, std::max(1.0F, availableSize.y - ImGui::GetTextLineHeightWithSpacing() * 3));


        // Draw the two hex editor columns side by side
        if (ImGui::BeginTable("##binary_diff", 2, ImGuiTableFlags_None, diffingColumnSize)) {
            ImGui::TableSetupColumn(fmt::format(" {}", "hex.diffing.view.diff.provider_a"_lang).c_str());
            ImGui::TableSetupColumn(fmt::format(" {}", "hex.diffing.view.diff.provider_b"_lang).c_str());
            ImGui::TableHeadersRow();

            ImGui::BeginDisabled(m_diffTask.isRunning());
            {
                // Draw settings button
                ImGui::TableNextColumn();
                if (ImGuiExt::DimmedIconButton(ICON_VS_SETTINGS_GEAR, ImGui::GetStyleColorVec4(ImGuiCol_Text)))
                    RequestOpenPopup::post("##DiffingAlgorithmSettings");

                ImGui::SameLine();

                // Draw first provider selector
                if (drawProviderSelector(a)) m_analysisInterrupted = m_analyzed = false;

                // Draw second provider selector
                ImGui::TableNextColumn();
                if (drawProviderSelector(b)) m_analysisInterrupted = m_analyzed = false;
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
                        if (ImGui::Selectable(fmt::format("0x{:04X} - 0x{:04X}", regionA.start, regionA.end).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
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
                        ImGui::TextUnformatted(fmt::format("0x{:04X} - 0x{:04X}", regionB.start, regionB.end).c_str());

                        const auto &providers = ImHexApi::Provider::getProviders();
                        std::vector<u8> data;

                        // Draw changes
                        ImGui::TableNextColumn();
                        ImGui::Indent();
                        if (a.provider != -1 && b.provider != -1) {
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
                        m_analysisInterrupted = m_analyzed  = false;
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

    void ViewDiff::registerMenuItems() {
        ContentRegistry::UserInterface::addMenuItemSeparator({ "hex.builtin.menu.file" }, 1700, this);

        ContentRegistry::UserInterface::addMenuItemSubMenu({ "hex.builtin.menu.file", "hex.diffing.view.diff.menu.file.jumping" }, ICON_TA_ARROWS_MOVE_HORIZONTAL, 1710,
                                                           []{},
                                                           [this]{ return (bool) m_analyzed; },
                                                           this);

        ContentRegistry::UserInterface::addMenuItem({
                "hex.builtin.menu.file",
                "hex.diffing.view.diff.menu.file.jumping",
                "hex.diffing.view.diff.menu.file.jumping.prev_diff"
            },
            ICON_TA_ARROW_BAR_TO_LEFT_DASHED,
            1720,
            CTRLCMD + Keys::Left,
            [this] {
                if (m_selectedProvider == nullptr)
                    return;

                // Get the column of the currently selected region
                auto providers = ImHexApi::Provider::getProviders();
                Column *selectedColumn = nullptr;
                for (auto &column : m_columns) {
                    if (providers[column.provider] == m_selectedProvider) {
                        selectedColumn = &column;
                        break;
                    }
                }

                if (selectedColumn == nullptr)
                    return;

                // Jump to previous difference
                auto prevRange = selectedColumn->diffTree.prevInterval(m_selectedAddress);
                if (prevRange.has_value()) {
                    selectedColumn->hexEditor.setSelection(prevRange->interval.start, prevRange->interval.end);
                    selectedColumn->hexEditor.jumpToSelection();
                } else {
                    ui::ToastInfo::open("hex.diffing.view.diff.jumping.beginning_reached"_lang);
                }
            },
            [this]{ return (bool) m_analyzed; },
            this
        );

        ContentRegistry::UserInterface::addMenuItem({
                "hex.builtin.menu.file",
                "hex.diffing.view.diff.menu.file.jumping",
                "hex.diffing.view.diff.menu.file.jumping.next_diff"
            },
            ICON_TA_ARROW_BAR_TO_RIGHT_DASHED,
            1730,
            CTRLCMD + Keys::Right,
            [this] {
                if (m_selectedProvider == nullptr)
                    return;

                // Get the column of the currently selected region
                auto providers = ImHexApi::Provider::getProviders();
                Column *selectedColumn = nullptr;
                for (auto &column : m_columns) {
                    if (providers[column.provider] == m_selectedProvider) {
                        selectedColumn = &column;
                        break;
                    }
                }

                if (selectedColumn == nullptr)
                    return;

                // Jump to next difference
                auto nextRange = selectedColumn->diffTree.nextInterval(m_selectedAddress);
                if (nextRange.has_value()) {
                    selectedColumn->hexEditor.setSelection(nextRange->interval.start, nextRange->interval.end);
                    selectedColumn->hexEditor.jumpToSelection();
                } else {
                    ui::ToastInfo::open("hex.diffing.view.diff.jumping.end_reached"_lang);
                }
            },
            [this]{ return (bool) m_analyzed; },
            this
        );
    }

    void ViewDiff::drawHelpText() {
        ImGuiExt::TextFormattedWrapped("This view allows you to do binary comparisons between two data sources. Select the data sources you want to compare from the dropdown menus at the top. Once both data sources are selected, the differences will be calculated automatically.");
        ImGui::NewLine();
        ImGuiExt::TextFormattedWrapped("Differences are highlighted in the hex editors. Green indicates added bytes, red indicates removed bytes, and yellow indicates modified bytes. All differences are also listed in the table below the hex editors, where you can click on a difference to jump to it in both hex editors.");
        ImGui::NewLine();
        ImGuiExt::TextFormattedWrapped(
            "By default, a simple byte-by-byte comparison algorithm is used. This is quick but will only identify byte modifications but doesn't match insertions or deletions.\n"
            "For a more sophisticated comparison, you can select a different diffing algorithm from the settings menu (gear icon)."
        );
    }
}
