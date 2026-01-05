#include <ui/widgets.hpp>

#include <imgui.h>
#include <fonts/vscode_icons.hpp>
#include <hex/ui/imgui_imhex_extensions.h>
#include <hex/helpers/scaling.hpp>

#include <hex/api/imhex_api/hex_editor.hpp>

namespace hex::ui {

    void regionSelectionPicker(Region *region, prv::Provider *provider, RegionType *type, bool showHeader, bool firstEntry) {
        ImGui::BeginGroup();
        if (showHeader)
            ImGuiExt::Header("hex.ui.common.range"_lang, firstEntry);

        if (ImGui::RadioButton("hex.ui.common.range.entire_data"_lang, *type == RegionType::EntireData))
            *type = RegionType::EntireData;
        if (ImGui::RadioButton("hex.ui.common.range.selection"_lang, *type == RegionType::Selection))
            *type = RegionType::Selection;
        if (ImGui::RadioButton("hex.ui.common.region"_lang, *type == RegionType::Region))
            *type = RegionType::Region;

        switch (*type) {
            case RegionType::EntireData:
                *region = { .address=provider->getBaseAddress(), .size=provider->getActualSize() };
                break;
            case RegionType::Selection:
                *region = ImHexApi::HexEditor::getSelection().value_or(
                    ImHexApi::HexEditor::ProviderRegion {
                        { .address=0, .size=1 },
                        provider }
                        ).getRegion();
                break;
            case RegionType::Region:
                ImGui::SameLine(0, 10_scaled);

                if (ImGuiExt::DimmedIconButton(ICON_VS_TRIANGLE_RIGHT, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                    ImGui::OpenPopup("RegionSelectionPopup");
                }

                ImGui::SameLine(0, 0);

                ImGui::SetNextWindowPos(ImGui::GetCursorScreenPos());
                if (ImGui::BeginPopup("RegionSelectionPopup")) {
                    const auto width = 150_scaled;
                    u64 start = region->getStartAddress(), end = region->getEndAddress();

                    if (end < start)
                        end = start;

                    ImGui::PushItemWidth(width);
                    ImGuiExt::InputHexadecimal("##start", &start);
                    ImGui::PopItemWidth();
                    ImGui::SameLine(0, 0);
                    ImGui::TextUnformatted(" - ");
                    ImGui::SameLine(0, 0);
                    ImGui::PushItemWidth(width);
                    ImGuiExt::InputHexadecimal("##end", &end);
                    ImGui::PopItemWidth();

                    *region = { .address=start, .size=(end - start) + 1 };

                    ImGui::EndPopup();
                }
                break;
        }

        ImGui::EndGroup();
    }

    bool endiannessSlider(std::endian &endian) {
        int selection = [&] {
            switch (endian) {
                default:
                case std::endian::little:
                    return 0;
                case std::endian::big:
                    return 1;
            }
        }();

        std::array options = {
            fmt::format("{}:  {}", "hex.ui.common.endian"_lang, "hex.ui.common.little"_lang),
            fmt::format("{}:  {}", "hex.ui.common.endian"_lang, "hex.ui.common.big"_lang)
        };

        if (ImGui::SliderInt("##endian", &selection, 0, options.size() - 1, options[selection].c_str(), ImGuiSliderFlags_NoInput)) {
            switch (selection) {
                default:
                case 0:
                    endian = std::endian::little;
                    break;
                case 1:
                    endian = std::endian::big;
                    break;
            }

            return true;
        }

        return false;
    }

}