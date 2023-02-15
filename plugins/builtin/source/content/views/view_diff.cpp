#include "content/views/view_diff.hpp"

#include <hex/api/imhex_api.hpp>

#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>

namespace hex::plugin::builtin {

    namespace {

        u32 getDiffColor(u32 color) {
            return (color & 0x00FFFFFF) | 0x40000000;
        }

    }

    ViewDiff::ViewDiff() : View("hex.builtin.view.diff.name") {

        EventManager::subscribe<EventProviderClosed>(this, [this](prv::Provider *) {
            this->m_columns[0].provider = -1;
            this->m_columns[1].provider = -1;
        });

        auto compareFunction = [this](int otherIndex) {
            return [this, otherIndex](u64 address, const u8 *data, size_t) -> std::optional<color_t> {
                const auto &providers = ImHexApi::Provider::getProviders();
                auto otherId = this->m_columns[otherIndex].provider;
                if (otherId < 0 || size_t(otherId) >= providers.size())
                    return std::nullopt;

                auto &otherProvider = providers[otherId];

                if (address > otherProvider->getActualSize()) {
                    if (otherIndex == 1)
                        return getDiffColor(ImGui::GetCustomColorU32(ImGuiCustomCol_ToolbarGreen));
                    else
                        return getDiffColor(ImGui::GetCustomColorU32(ImGuiCustomCol_ToolbarRed));
                }

                u8 otherByte = 0x00;
                otherProvider->read(address, &otherByte, 1);

                if (otherByte != *data)
                    return getDiffColor(ImGui::GetCustomColorU32(ImGuiCustomCol_ToolbarYellow));

                return std::nullopt;
            };
        };

        this->m_columns[0].hexEditor.setBackgroundHighlightCallback(compareFunction(1));
        this->m_columns[1].hexEditor.setBackgroundHighlightCallback(compareFunction(0));
    }

    ViewDiff::~ViewDiff() {
    }

    bool ViewDiff::drawDiffColumn(Column &column, float height) const {
        bool scrolled = false;
        ImGui::PushID(&column);

        float prevScroll = column.hexEditor.getScrollPosition();
        column.hexEditor.draw(height);
        float currScroll = column.hexEditor.getScrollPosition();

        if (prevScroll != currScroll) {
            scrolled = true;
            column.scrollLock = 5;
        }

        ImGui::PopID();

        return scrolled;
    }

    void ViewDiff::drawProviderSelector(Column &column) {
        ImGui::PushID(&column);

        auto &providers = ImHexApi::Provider::getProviders();
        auto &providerIndex = column.provider;

        std::string preview;
        if (ImHexApi::Provider::isValid() && providerIndex >= 0)
            preview = providers[providerIndex]->getName();

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        ImGui::BeginDisabled(this->m_diffTask.isRunning());
        if (ImGui::BeginCombo("", preview.c_str())) {

            for (size_t i = 0; i < providers.size(); i++) {
                if (ImGui::Selectable(providers[i]->getName().c_str())) {
                    providerIndex = i;
                    this->m_analyzed = false;
                }
            }

            ImGui::EndCombo();
        }
        ImGui::EndDisabled();

        ImGui::PopID();
    }

    std::string ViewDiff::getProviderName(Column &column) const {
        const auto &providers = ImHexApi::Provider::getProviders();
        return ((column.provider >= 0 && size_t(column.provider) < providers.size()) ? providers[column.provider]->getName() : "???") + "##" + hex::format("{:X}", u64(&column));
    }

    void ViewDiff::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.diff.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {

            auto &[a, b] = this->m_columns;

            a.hexEditor.enableSyncScrolling(false);
            b.hexEditor.enableSyncScrolling(false);

            if (a.scrollLock > 0) a.scrollLock--;
            if (b.scrollLock > 0) b.scrollLock--;

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

            if (!this->m_analyzed && a.provider != -1 && b.provider != -1 && !this->m_diffTask.isRunning()) {
                const auto &providers = ImHexApi::Provider::getProviders();
                auto providerA = providers[a.provider];
                auto providerB = providers[b.provider];

                auto commonSize = std::min(providerA->getActualSize(), providerB->getActualSize());
                this->m_diffTask = TaskManager::createTask("Diffing...", commonSize, [this, providerA, providerB](Task &task) {
                    std::vector<u8> bufferA(0x1000);
                    std::vector<u8> bufferB(0x1000);

                    std::vector<Diff> differences;

                    auto readerA = prv::BufferedReader(providerA);
                    auto readerB = prv::BufferedReader(providerB);

                    for (auto itA = readerA.begin(), itB = readerB.begin(); itA < readerA.end() && itB < readerB.end(); itA++, itB++) {
                        if (task.wasInterrupted())
                            break;

                        if (*itA != *itB) {
                            u64 start = itA.getAddress();
                            size_t end = 0;

                            while (itA != readerA.end() && itB != readerB.end() && *itA != *itB) {
                                itA++;
                                itB++;
                                end++;
                            }

                            differences.push_back(Diff { Region{ start, end }, ViewDiff::DifferenceType::Modified });
                        }
                    }

                    if (providerA->getActualSize() != providerB->getActualSize()) {
                        auto endA = providerA->getActualSize() - 1;
                        auto endB = providerB->getActualSize() - 1;

                        if (endA > endB)
                            differences.push_back(Diff { Region{ endB, endA - endB }, ViewDiff::DifferenceType::Added });
                        else
                            differences.push_back(Diff { Region{ endA, endB - endA }, ViewDiff::DifferenceType::Removed });
                    }

                    this->m_diffs = std::move(differences);
                    this->m_analyzed = true;
                });
            }

            const auto height = ImGui::GetContentRegionAvail().y;

            if (ImGui::BeginTable("##binary_diff", 2, ImGuiTableFlags_None, ImVec2(0, height - 200_scaled))) {
                ImGui::TableSetupColumn("hex.builtin.view.diff.provider_a"_lang);
                ImGui::TableSetupColumn("hex.builtin.view.diff.provider_b"_lang);
                ImGui::TableHeadersRow();

                ImGui::TableNextColumn();
                this->drawProviderSelector(a);

                ImGui::TableNextColumn();
                this->drawProviderSelector(b);

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                bool scrollB = drawDiffColumn(a, height - 250_scaled);

                ImGui::TableNextColumn();
                bool scrollA = drawDiffColumn(b, height - 250_scaled);

                if (scrollA && a.scrollLock == 0) {
                    a.hexEditor.setScrollPosition(b.hexEditor.getScrollPosition());
                    a.hexEditor.forceUpdateScrollPosition();
                }
                if (scrollB && b.scrollLock == 0) {
                    b.hexEditor.setScrollPosition(a.hexEditor.getScrollPosition());
                    b.hexEditor.forceUpdateScrollPosition();
                }

                ImGui::EndTable();
            }

            if (ImGui::BeginTable("##differences", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Resizable, ImVec2(0, 200_scaled))) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("hex.builtin.common.begin"_lang);
                ImGui::TableSetupColumn("hex.builtin.common.end"_lang);
                ImGui::TableSetupColumn("hex.builtin.common.type"_lang);
                ImGui::TableHeadersRow();

                if (this->m_analyzed) {
                    ImGuiListClipper clipper;
                    clipper.Begin(this->m_diffs.size());

                    while (clipper.Step())
                        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                            ImGui::TableNextRow();

                            if (size_t(i) >= this->m_diffs.size())
                                break;

                            ImGui::PushID(i);

                            const auto &diff = this->m_diffs[i];

                            ImGui::TableNextColumn();
                            if (ImGui::Selectable(hex::format("0x{:02X}", diff.region.getStartAddress()).c_str(), false, ImGuiSelectableFlags_SpanAllColumns)) {
                                a.hexEditor.setSelection(diff.region);
                                a.hexEditor.jumpToSelection();
                                b.hexEditor.setSelection(diff.region);
                                b.hexEditor.jumpToSelection();
                            }

                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(hex::format("0x{:02X}", diff.region.getEndAddress()).c_str());

                            ImGui::TableNextColumn();
                            switch (diff.type) {
                                case DifferenceType::Modified:
                                    ImGui::TextColored(ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarYellow), "hex.builtin.view.diff.modified"_lang);
                                    break;
                                case DifferenceType::Added:
                                    ImGui::TextColored(ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarGreen), "hex.builtin.view.diff.added"_lang);
                                    break;
                                case DifferenceType::Removed:
                                    ImGui::TextColored(ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed), "hex.builtin.view.diff.removed"_lang);
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