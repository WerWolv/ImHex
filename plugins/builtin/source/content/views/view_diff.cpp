#include "content/views/view_diff.hpp"

#include <hex/api/imhex_api.hpp>

#include <hex/helpers/fmt.hpp>
#include <hex/providers/buffered_reader.hpp>
#include <wolv/utils/guards.hpp>

#include <edlib.h>

namespace hex::plugin::builtin {

    namespace {

        constexpr u32 getDiffColor(u32 color) {
            return (color & 0x00FFFFFF) | 0x40000000;
        }

    }

    class AlgorithmSimple : public ViewDiff::Algorithm {
    public:
        [[nodiscard]] const char *getName() const override { return "Simple"; }
        [[nodiscard]] std::vector<ViewDiff::DiffTree> analyze(Task &task, prv::Provider *providerA, prv::Provider *providerB) override {
            wolv::container::IntervalTree<ViewDiff::DifferenceType> differences;

            // Set up readers for both providers
            auto readerA = prv::ProviderReader(providerA);
            auto readerB = prv::ProviderReader(providerB);

            // Iterate over both providers and compare the bytes
            for (auto itA = readerA.begin(), itB = readerB.begin(); itA < readerA.end() && itB < readerB.end(); ++itA, ++itB) {
                // Stop comparing if the diff task was canceled
                if (task.wasInterrupted())
                    break;

                // If the bytes are different, find the end of the difference
                if (*itA != *itB) {
                    u64 start = itA.getAddress();
                    size_t size = 0;

                    while (itA != readerA.end() && itB != readerB.end() && *itA != *itB) {
                        ++itA;
                        ++itB;
                        ++size;
                    }

                    // Add the difference to the list
                    differences.emplace({ start, (start + size) - 1 }, ViewDiff::DifferenceType::Mismatch);
                }

                // Update the progress bar
                task.update(itA.getAddress());
            }

            auto otherDifferences = differences;

            // If one provider is larger than the other, add the extra bytes to the list
            if (providerA->getActualSize() != providerB->getActualSize()) {
                auto endA = providerA->getActualSize() + 1;
                auto endB = providerB->getActualSize() + 1;

                if (endA > endB) {
                    differences.emplace({ endB, endA }, ViewDiff::DifferenceType::Insertion);
                    otherDifferences.emplace({ endB, endA }, ViewDiff::DifferenceType::Deletion);
                }
                else {
                    differences.emplace({ endA, endB }, ViewDiff::DifferenceType::Insertion);
                    otherDifferences.emplace({ endB, endA }, ViewDiff::DifferenceType::Insertion);
                }
            }

            return { differences, otherDifferences };
        }
    };

    class AlgorithmMyers : public ViewDiff::Algorithm {
    public:
        [[nodiscard]] const char *getName() const override { return "Myers"; }
        [[nodiscard]] std::vector<ViewDiff::DiffTree> analyze(Task &task, prv::Provider *providerA, prv::Provider *providerB) override {
            ViewDiff::DiffTree differencesA, differencesB;

            EdlibAlignConfig edlibConfig;
            edlibConfig.k = -1;
            edlibConfig.additionalEqualities = nullptr;
            edlibConfig.additionalEqualitiesLength = 0;
            edlibConfig.mode = EdlibAlignMode::EDLIB_MODE_NW;
            edlibConfig.task = EdlibAlignTask::EDLIB_TASK_PATH;

            const auto windowStart = std::max(providerA->getBaseAddress(), providerB->getBaseAddress());
            const auto windowEnd   = std::min(providerA->getBaseAddress() + providerA->getActualSize(), providerB->getBaseAddress() + providerB->getActualSize());

            const auto WindowSize = 64 * 1024;
            for (u64 address = windowStart; address < windowEnd; address += WindowSize) {
                task.update(address);

                auto currWindowSize = std::min<u64>(WindowSize, windowEnd - address);
                std::vector<u8> dataA(currWindowSize), dataB(currWindowSize);

                providerA->read(address, dataA.data(), dataA.size());
                providerB->read(address, dataB.data(), dataB.size());

                EdlibAlignResult result = edlibAlign(
                    reinterpret_cast<const char*>(dataA.data()), dataA.size(),
                    reinterpret_cast<const char*>(dataB.data()), dataB.size(),
                    edlibConfig
                );

                auto currentOperation = ViewDiff::DifferenceType(0xFF);
                Region regionA = {}, regionB = {};
                u64 currentAddressA = 0x00, currentAddressB = 0x00;

                const auto insertDifference = [&] {
                    switch (currentOperation) {
                        using enum ViewDiff::DifferenceType;

                        case Match:
                            break;
                        case Mismatch:
                            differencesA.insert({ regionA.getStartAddress(), regionA.getEndAddress() }, currentOperation);
                            differencesB.insert({ regionB.getStartAddress(), regionB.getEndAddress() }, currentOperation);
                            break;
                        case Insertion:
                            differencesA.insert({ regionA.getStartAddress(), regionA.getEndAddress() }, currentOperation);
                            currentAddressB += regionA.size;
                            break;
                        case Deletion:
                            differencesB.insert({ regionB.getStartAddress(), regionB.getEndAddress() }, currentOperation);
                            currentAddressA += regionB.size;
                            break;
                    }

                    currentAddressA--;
                    currentAddressB--;
                };

                for (const u8 alignmentType : std::span(result.alignment, result.alignmentLength)) {
                    ON_SCOPE_EXIT {
                        currentAddressA++;
                        currentAddressB++;
                    };

                    if (currentOperation == ViewDiff::DifferenceType(alignmentType)) {
                        regionA.size++;

                        continue;
                    } else if (currentOperation != ViewDiff::DifferenceType(0xFF)) {
                        insertDifference();

                        currentOperation = ViewDiff::DifferenceType(0xFF);
                    }

                    currentOperation = ViewDiff::DifferenceType(alignmentType);
                    regionA.address = currentAddressA;
                    regionB.address = currentAddressB;
                    regionA.size = 1;
                    regionB.size = 1;
                }

                insertDifference();
            }


            return { differencesA, differencesB };
        }
    };


    ViewDiff::ViewDiff() : View::Window("hex.builtin.view.diff.name", ICON_VS_DIFF_SIDEBYSIDE) {
        // Clear the selected diff providers when a provider is closed
        EventProviderClosed::subscribe(this, [this](prv::Provider *) {
            for (auto &column : m_columns) {
                column.provider = -1;
                column.hexEditor.setSelectionUnchecked(std::nullopt, std::nullopt);
                column.diffTree.clear();
            }

        });

        m_algorithm = std::make_unique<AlgorithmMyers>();

        // Set the background highlight callbacks for the two hex editor columns
        m_columns[0].hexEditor.setBackgroundHighlightCallback(this->createCompareFunction(1));
        m_columns[1].hexEditor.setBackgroundHighlightCallback(this->createCompareFunction(0));
    }

    ViewDiff::~ViewDiff() {
        EventProviderClosed::unsubscribe(this);
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

            auto &providers = ImHexApi::Provider::getProviders();
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
        auto commonSize = std::min(providerA->getActualSize(), providerB->getActualSize());
        m_diffTask = TaskManager::createTask("Diffing...", commonSize, [this, providerA, providerB](Task &task) {
            auto differences = m_algorithm->analyze(task, providerA, providerB);

            // Move the calculated differences over so they can be displayed
            for (size_t i = 0; i < m_columns.size(); i++) {
                auto &column = m_columns[i];
                column.diffTree = std::move(differences[i]);
            }
            m_analyzed = true;
        });
    }

    std::function<std::optional<color_t>(u64, const u8*, size_t)> ViewDiff::createCompareFunction(size_t otherIndex) const {
        return [=, this](u64 address, const u8 *, size_t size) -> std::optional<color_t> {
            auto matches = m_columns[otherIndex == 0 ? 1 : 0].diffTree.overlapping({ address, (address + size) - 1 });
            if (matches.empty())
                return std::nullopt;

            auto type = matches[0].value;

            if (type == DifferenceType::Mismatch) {
                return ImGuiExt::GetCustomColorU32(ImGuiCustomCol_DiffChanged);
            }

            if (otherIndex == 0) {
                if (type == DifferenceType::Insertion) {
                    return ImGuiExt::GetCustomColorU32(ImGuiCustomCol_DiffRemoved);
                } else if (type == DifferenceType::Deletion) {
                    return ImGuiExt::GetCustomColorU32(ImGuiCustomCol_DiffAdded);
                }
            } else if (otherIndex == 1) {
                if (type == DifferenceType::Insertion) {
                    return ImGuiExt::GetCustomColorU32(ImGuiCustomCol_DiffAdded);
                } else if (type == DifferenceType::Deletion) {
                    return ImGuiExt::GetCustomColorU32(ImGuiCustomCol_DiffRemoved);
                }
            }

            return std::nullopt;
        };
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
        if (!m_analyzed && a.provider != -1 && b.provider != -1 && !m_diffTask.isRunning()) {
            const auto &providers = ImHexApi::Provider::getProviders();
            auto providerA = providers[a.provider];
            auto providerB = providers[b.provider];

            this->analyze(providerA, providerB);
        }

        const auto height = ImGui::GetContentRegionAvail().y;

        // Draw the two hex editor columns side by side
        if (ImGui::BeginTable("##binary_diff", 2, ImGuiTableFlags_None, ImVec2(0, height - 250_scaled))) {
            ImGui::TableSetupColumn("hex.builtin.view.diff.provider_a"_lang);
            ImGui::TableSetupColumn("hex.builtin.view.diff.provider_b"_lang);
            ImGui::TableHeadersRow();

            ImGui::BeginDisabled(m_diffTask.isRunning());
            {
                // Draw first provider selector
                ImGui::TableNextColumn();
                if (drawProviderSelector(a)) m_analyzed = false;

                // Draw second provider selector
                ImGui::TableNextColumn();
                if (drawProviderSelector(b)) m_analyzed = false;
            }
            ImGui::EndDisabled();

            ImGui::TableNextRow();

            // Draw first hex editor column
            ImGui::TableNextColumn();
            bool scrollB = drawDiffColumn(a, height - 250_scaled);

            // Draw second hex editor column
            ImGui::TableNextColumn();
            bool scrollA = drawDiffColumn(b, height - 250_scaled);

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

        // Draw the differences table
        if (ImGui::BeginTable("##differences", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Resizable, ImVec2(0, 200_scaled))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("hex.ui.common.begin"_lang);
            ImGui::TableSetupColumn("hex.ui.common.end"_lang);
            ImGui::TableSetupColumn("hex.ui.common.type"_lang);
            ImGui::TableHeadersRow();

            // Draw the differences if the providers have been analyzed
            if (m_analyzed) {
                ImGuiListClipper clipper;

                auto &diffTree = m_columns[0].diffTree;
                clipper.Begin(int(diffTree.size()));

                auto diffIter = diffTree.begin();
                while (clipper.Step()) {
                    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        ImGui::TableNextRow();

                        // Prevent the list from trying to access non-existing diffs
                        if (size_t(i) >= diffTree.size())
                            break;

                        ImGui::PushID(i);

                        const auto &[start, rest] = *diffIter;
                        const auto &[end, type] = rest;
                        std::advance(diffIter, 1);

                        // Draw a clickable row for each difference that will select the difference in both hex editors

                        // Draw start address
                        ImGui::TableNextColumn();
                        if (ImGui::Selectable(hex::format("0x{:02X}", start).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                            a.hexEditor.setSelection({ start, ((end - start) + 1) });
                            a.hexEditor.jumpToSelection();
                            b.hexEditor.setSelection({ start, ((end - start) + 1) });
                            b.hexEditor.jumpToSelection();
                        }

                        // Draw end address
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(hex::format("0x{:02X}", end).c_str());

                        // Draw difference type
                        ImGui::TableNextColumn();
                        switch (type) {
                            case DifferenceType::Mismatch:
                                ImGuiExt::TextFormattedColored(ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_DiffChanged), "hex.builtin.view.diff.modified"_lang);
                                break;
                            case DifferenceType::Insertion:
                                ImGuiExt::TextFormattedColored(ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_DiffAdded), "hex.builtin.view.diff.added"_lang);
                                break;
                            case DifferenceType::Deletion:
                                ImGuiExt::TextFormattedColored(ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_DiffRemoved), "hex.builtin.view.diff.removed"_lang);
                                break;
                            default:
                                break;
                        }

                        ImGui::PopID();
                    }
                }
            }

            ImGui::EndTable();
        }
    }

}
