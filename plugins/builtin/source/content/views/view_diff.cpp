#include "content/views/view_diff.hpp"

#include <hex/api/imhex_api.hpp>
#include <hex/providers/provider.hpp>

#include <hex/helpers/fmt.hpp>

#include <hex/api/content_registry.hpp>
#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    ViewDiff::ViewDiff() : View("hex.builtin.view.diff.name") {

        EventManager::subscribe<EventSettingsChanged>(this, [this] {
            {
                auto columnCount = ContentRegistry::Settings::getSetting("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.bytes_per_row");

                if (columnCount.is_number())
                    this->m_columnCount = static_cast<int>(columnCount);
            }

            {
                auto greyOutZeros = ContentRegistry::Settings::getSetting("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.grey_zeros");

                if (greyOutZeros.is_number())
                    this->m_greyedOutZeros = static_cast<int>(greyOutZeros);
            }

            {
                auto upperCaseHex = ContentRegistry::Settings::getSetting("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.uppercase_hex");

                if (upperCaseHex.is_number())
                    this->m_upperCaseHex = static_cast<int>(upperCaseHex);
            }
        });
    }

    ViewDiff::~ViewDiff() {
        EventManager::unsubscribe<EventSettingsChanged>(this);
    }

    static void drawProviderSelector(int &provider) {
        auto &providers = ImHexApi::Provider::getProviders();

        std::string preview;
        if (ImHexApi::Provider::isValid() && provider >= 0)
            preview = providers[provider]->getName();

        ImGui::SetNextItemWidth(200_scaled);
        if (ImGui::BeginCombo("", preview.c_str())) {

            for (size_t i = 0; i < providers.size(); i++) {
                if (ImGui::Selectable(providers[i]->getName().c_str())) {
                    provider = i;
                }
            }

            ImGui::EndCombo();
        }
    }

    static u32 getDiffColor(u32 color) {
        return (color & 0x00FFFFFF) | 0x40000000;
    }

    enum class DiffResult {
        Same,
        Changed,
        Added,
        Removed
    };
    struct LineInfo {
        std::vector<u8> bytes;
        i64 validBytes = 0;
    };

    static DiffResult diffBytes(u8 index, const LineInfo &a, const LineInfo &b) {
        /* Very simple binary diff. Only detects additions and changes */
        if (a.validBytes > index) {
            if (b.validBytes <= index)
                return DiffResult::Added;
            else if (a.bytes[index] != b.bytes[index])
                return DiffResult::Changed;
        }

        return DiffResult::Same;
    }

    void ViewDiff::drawDiffLine(const std::array<int, 2> &providerIds, u64 row) const {

        std::array<LineInfo, 2> lineInfo;

        u8 addressDigitCount = 0;
        for (u8 i = 0; i < 2; i++) {
            int id = providerIds[i];
            if (id < 0) continue;

            auto &provider = ImHexApi::Provider::getProviders()[id];

            // Read one line of each provider
            lineInfo[i].bytes.resize(this->m_columnCount);
            provider->read(row * this->m_columnCount, lineInfo[i].bytes.data(), lineInfo[i].bytes.size());
            lineInfo[i].validBytes = std::min<i64>(this->m_columnCount, provider->getSize() - row * this->m_columnCount);

            // Calculate address width
            u8 addressDigits = 0;
            for (size_t n = provider->getSize() - 1; n > 0; n >>= 4)
                addressDigits++;

            addressDigitCount = std::max(addressDigits, addressDigitCount);
        }

        ImDrawList *drawList = ImGui::GetWindowDrawList();

        auto glyphWidth           = ImGui::CalcTextSize("0").x + 1;
        static auto highlightSize = ImGui::CalcTextSize("00");

        auto startY = ImGui::GetCursorPosY();

        ImGui::TableNextColumn();
        ImGui::TextFormatted(this->m_upperCaseHex ? "{:0{}X}:" : "{:0{}x}:", row * this->m_columnCount, addressDigitCount);
        ImGui::SetCursorPosY(startY);

        const ImColor colorText     = ImGui::GetColorU32(ImGuiCol_Text);
        const ImColor colorDisabled = this->m_greyedOutZeros ? ImGui::GetColorU32(ImGuiCol_TextDisabled) : static_cast<u32>(colorText);


        for (i8 curr = 0; curr < 2; curr++) {
            ImGui::TableNextColumn();
            auto other = !curr;

            bool hasLastHighlight = false;
            ImVec2 lastHighlightEnd = { };

            for (i64 col = 0; col < lineInfo[curr].validBytes; col++) {
                auto pos = ImGui::GetCursorScreenPos();

                // Diff bytes
                std::optional<u32> highlightColor;
                switch (diffBytes(col, lineInfo[curr], lineInfo[other])) {
                    default:
                    case DiffResult::Same:
                        /* No highlight */
                        break;
                    case DiffResult::Changed:
                        highlightColor = getDiffColor(ImGui::GetCustomColorU32(ImGuiCustomCol_ToolbarYellow));
                        break;
                    case DiffResult::Added:
                        highlightColor = getDiffColor(ImGui::GetCustomColorU32(ImGuiCustomCol_ToolbarGreen));
                        break;
                    case DiffResult::Removed:
                        highlightColor = getDiffColor(ImGui::GetCustomColorU32(ImGuiCustomCol_ToolbarRed));
                        break;
                }

                // Draw byte
                u8 byte = lineInfo[curr].bytes[col];
                ImGui::TextFormattedColored(byte == 0x00 ? colorDisabled : colorText, this->m_upperCaseHex ? "{:02X}" : "{:02x}", byte);
                ImGui::SameLine(0.0F, col % 8 == 7 ? glyphWidth * 1.5F : glyphWidth * 0.75F);

                ImGui::SetCursorPosY(startY);

                // Draw highlighting
                if (highlightColor.has_value()) {
                    if (hasLastHighlight)
                        drawList->AddRectFilled(lastHighlightEnd, pos + highlightSize, highlightColor.value());
                    else
                        drawList->AddRectFilled(pos, pos + highlightSize, highlightColor.value());

                    hasLastHighlight = true;
                    lastHighlightEnd = pos + ImVec2((glyphWidth - 1) * 2, 0);
                } else {
                    hasLastHighlight = false;
                }
            }
        }
    }

    void ViewDiff::drawContent() {
        if (ImGui::Begin(View::toWindowName("hex.builtin.view.diff.name").c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse)) {

            ImGui::SameLine();
            ImGui::PushID(1);
            drawProviderSelector(this->m_providerA);
            ImGui::PopID();
            ImGui::SameLine();
            ImGui::Spacing();
            ImGui::SameLine();
            ImGui::TextUnformatted("<=>");
            ImGui::SameLine();
            ImGui::Spacing();
            ImGui::SameLine();
            ImGui::PushID(2);
            drawProviderSelector(this->m_providerB);
            ImGui::PopID();
            ImGui::Separator();

            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(20, 0));
            if (ImGui::BeginTable("diff", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupScrollFreeze(0, 1);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();


                // Draw header line
                {
                    auto glyphWidth = ImGui::CalcTextSize("0").x + 1;
                    for (u8 i = 0; i < 2; i++) {
                        ImGui::TableNextColumn();
                        for (u32 col = 0; col < this->m_columnCount; col++) {
                            ImGui::TextFormatted(this->m_upperCaseHex ? "{:02X}" : "{:02x}", col);
                            ImGui::SameLine(0.0F, col % 8 == 7 ? glyphWidth * 1.5F : glyphWidth * 0.75F);
                        }
                    }
                }

                if (this->m_providerA >= 0 && this->m_providerB >= 0) {
                    auto &providers = ImHexApi::Provider::getProviders();
                    ImGuiListClipper clipper;
                    clipper.Begin(std::max(providers[this->m_providerA]->getSize() / this->m_columnCount, providers[this->m_providerB]->getSize() / this->m_columnCount) + 1, ImGui::GetTextLineHeight());

                    // Draw diff lines
                    while (clipper.Step()) {
                        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                            ImGui::TableNextRow();
                            drawDiffLine({ this->m_providerA, this->m_providerB }, row);
                        }
                    }
                }
                ImGui::EndTable();
            }
            ImGui::PopStyleVar();
        }
        ImGui::End();
    }

}