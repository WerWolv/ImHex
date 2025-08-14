#include <ui/widgets.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

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
                *region = { provider->getBaseAddress(), provider->getActualSize() };
                break;
            case RegionType::Selection:
                *region = ImHexApi::HexEditor::getSelection().value_or(
                    ImHexApi::HexEditor::ProviderRegion {
                        { 0, 1 },
                        provider }
                        ).getRegion();
                break;
            case RegionType::Region:
                ImGui::SameLine();

                const auto width = ImGui::GetContentRegionAvail().x / 2 - ImGui::CalcTextSize(" - ").x / 2;
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

                *region = { start, (end - start) + 1 };
                break;
        }

        ImGui::EndGroup();
    }

}