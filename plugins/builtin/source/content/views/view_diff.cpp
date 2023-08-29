#include "content/views/view_diff.hpp"

#include <hex/api/imhex_api.hpp>

#include <hex/helpers/fmt.hpp>

namespace hex::plugin::builtin {

    namespace {

        u32 getDiffColor(u32 color) {
            return (color & 0x00FFFFFF) | 0x40000000;
        }

    }

    ViewDiff::ViewDiff() : View("hex.builtin.view.diff.name") {

        // Clear the selected diff providers when a provider is closed
        EventManager::subscribe<EventProviderClosed>(this, [this](prv::Provider *) {
            for (u8 i = 0; i < 2; i++) {
                this->m_columns[i].provider = -1;
                this->m_columns[i].hexEditor.setSelectionUnchecked(std::nullopt, std::nullopt);
            }

            this->m_diffs.clear();
        });

        // Set the background highlight callbacks for the two hex editor columns
        this->m_columns[0].hexEditor.setBackgroundHighlightCallback(this->createCompareFunction(1));
        this->m_columns[1].hexEditor.setBackgroundHighlightCallback(this->createCompareFunction(0));
    }

    ViewDiff::~ViewDiff() {
        EventManager::unsubscribe<EventProviderClosed>(this);
    }

    namespace {

        bool drawDiffColumn(ViewDiff::Column &column, float height) {
            bool scrolled = false;
            ImGui::PushID(&column);

            // Draw the hex editor
            float prevScroll = column.hexEditor.getScrollPosition();
            column.hexEditor.draw(height);
            float currScroll = column.hexEditor.getScrollPosition();

            // Check if the user scrolled the hex editor
            if (prevScroll != currScroll) {
                scrolled = true;
                column.scrollLock = 5;
            }

            ImGui::PopID();

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

    std::function<std::optional<color_t>(u64, const u8*, size_t)> ViewDiff::createCompareFunction(size_t otherIndex) {
        // Create a function that will handle highlighting the differences between the two providers
        // This is a stupidly simple diffing implementation. It will highlight bytes that are different in yellow
        // and if one provider is larger than the other it will highlight the extra bytes in green or red depending on which provider is larger
        // TODO: Use an actual binary diffing algorithm that searches for the longest common subsequences

        return [this, otherIndex](u64 address, const u8 *data, size_t) -> std::optional<color_t> {
            const auto &providers = ImHexApi::Provider::getProviders();
            auto otherId = this->m_columns[otherIndex].provider;

            // Check if the other provider is valid
            if (otherId < 0 || size_t(otherId) >= providers.size())
                return std::nullopt;

            auto &otherProvider = providers[otherId];

            // Handle the case where one provider is larger than the other one
            if (address > otherProvider->getActualSize()) {
                if (otherIndex == 1)
                    return getDiffColor(ImGui::GetCustomColorU32(ImGuiCustomCol_ToolbarGreen));
                else
                    return getDiffColor(ImGui::GetCustomColorU32(ImGuiCustomCol_ToolbarRed));
            }

            // Read the current byte from the other provider
            u8 otherByte = 0x00;
            otherProvider->read(address, &otherByte, 1);

            // Compare the two bytes, highlight both in yellow if they are different
            if (otherByte != *data)
                return getDiffColor(ImGui::GetCustomColorU32(ImGuiCustomCol_ToolbarYellow));

            // No difference
            return std::nullopt;
        };
    }

    void ViewDiff::analyze(prv::Provider *providerA, prv::Provider *providerB) {
        auto commonSize = std::min(providerA->getActualSize(), providerB->getActualSize());
        this->m_diffTask = TaskManager::createTask("Diffing...", commonSize, [this, providerA, providerB](Task &task) {
            std::vector<Diff> differences;

            // Set up readers for both providers
            auto readerA = prv::ProviderReader(providerA);
            auto readerB = prv::ProviderReader(providerB);

            // Iterate over both providers and compare the bytes
            for (auto itA = readerA.begin(), itB = readerB.begin(); itA < readerA.end() && itB < readerB.end(); itA++, itB++) {
                // Stop comparing if the diff task was canceled
                if (task.wasInterrupted())
                    break;

                // If the bytes are different, find the end of the difference
                if (*itA != *itB) {
                    u64 start = itA.getAddress();
                    size_t end = 0;

                    while (itA != readerA.end() && itB != readerB.end() && *itA != *itB) {
                        itA++;
                        itB++;
                        end++;
                    }

                    // Add the difference to the list
                    differences.push_back(Diff { Region{ start, end }, ViewDiff::DifferenceType::Modified });
                }

                // Update the progress bar
                task.update(itA.getAddress());
            }

            // If one provider is larger than the other, add the extra bytes to the list
            if (providerA->getActualSize() != providerB->getActualSize()) {
                auto endA = providerA->getActualSize() + 1;
                auto endB = providerB->getActualSize() + 1;

                if (endA > endB)
                    differences.push_back(Diff { Region{ endB, endA - endB }, ViewDiff::DifferenceType::Added });
                else
                    differences.push_back(Diff { Region{ endA, endB - endA }, ViewDiff::DifferenceType::Removed });
            }

            // Move the calculated differences over so they can be displayed
            this->m_diffs = std::move(differences);
            this->m_analyzed = true;
        });
    }

    void ViewDiff::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.diff.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {

            auto &[a, b] = this->m_columns;

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
            if (!this->m_analyzed && a.provider != -1 && b.provider != -1 && !this->m_diffTask.isRunning()) {
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

                ImGui::BeginDisabled(this->m_diffTask.isRunning());
                {
                    // Draw first provider selector
                    ImGui::TableNextColumn();
                    if (drawProviderSelector(a)) this->m_analyzed = false;

                    // Draw second provider selector
                    ImGui::TableNextColumn();
                    if (drawProviderSelector(b)) this->m_analyzed = false;
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
                ImGui::TableSetupColumn("hex.builtin.common.begin"_lang);
                ImGui::TableSetupColumn("hex.builtin.common.end"_lang);
                ImGui::TableSetupColumn("hex.builtin.common.type"_lang);
                ImGui::TableHeadersRow();

                // Draw the differences if the providers have been analyzed
                if (this->m_analyzed) {
                    ImGuiListClipper clipper;
                    clipper.Begin(int(this->m_diffs.size()));

                    while (clipper.Step())
                        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                            ImGui::TableNextRow();

                            // Prevent the list from trying to access non-existing diffs
                            if (size_t(i) >= this->m_diffs.size())
                                break;

                            ImGui::PushID(i);

                            const auto &diff = this->m_diffs[i];

                            // Draw a clickable row for each difference that will select the difference in both hex editors

                            // Draw start address
                            ImGui::TableNextColumn();
                            if (ImGui::Selectable(hex::format("0x{:02X}", diff.region.getStartAddress()).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                                a.hexEditor.setSelection(diff.region);
                                a.hexEditor.jumpToSelection();
                                b.hexEditor.setSelection(diff.region);
                                b.hexEditor.jumpToSelection();
                            }

                            // Draw end address
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(hex::format("0x{:02X}", diff.region.getEndAddress()).c_str());

                            // Draw difference type
                            ImGui::TableNextColumn();
                            switch (diff.type) {
                                case DifferenceType::Modified:
                                    ImGui::TextFormattedColored(ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarYellow), "hex.builtin.view.diff.modified"_lang);
                                    break;
                                case DifferenceType::Added:
                                    ImGui::TextFormattedColored(ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarGreen), "hex.builtin.view.diff.added"_lang);
                                    break;
                                case DifferenceType::Removed:
                                    ImGui::TextFormattedColored(ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed), "hex.builtin.view.diff.removed"_lang);
                                    break;
                            }

                            ImGui::PopID();
                        }
                }

                ImGui::EndTable();
            }

        }
        ImGui::End();
    }

}