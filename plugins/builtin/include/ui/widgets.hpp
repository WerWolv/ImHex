#pragma once

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

namespace pl::ptrn { class Pattern; }
namespace hex::prv { class Provider; }

namespace hex::plugin::builtin::ui {


    enum class RegionType : int {
        EntireData,
        Selection,
        Region
    };

    inline void regionSelectionPicker(Region *region, prv::Provider *provider, RegionType *type, bool showHeader = true, bool firstEntry = false) {
        if (showHeader)
            ImGui::Header("hex.builtin.common.range"_lang, firstEntry);

        if (ImGui::RadioButton("hex.builtin.common.range.entire_data"_lang, *type == RegionType::EntireData))
            *type = RegionType::EntireData;
        if (ImGui::RadioButton("hex.builtin.common.range.selection"_lang, *type == RegionType::Selection))
            *type = RegionType::Selection;
        if (ImGui::RadioButton("hex.builtin.common.region"_lang, *type == RegionType::Region))
            *type = RegionType::Region;

        switch (*type) {
            case RegionType::EntireData:
                *region = { provider->getBaseAddress(), provider->getActualSize() };
                break;
            case RegionType::Selection:
                *region = ImHexApi::HexEditor::getSelection().value_or(ImHexApi::HexEditor::ProviderRegion { { 0, 1 }, provider });
                break;
            case RegionType::Region:
                ImGui::SameLine();

                const auto width = ImGui::GetContentRegionAvail().x / 2 - ImGui::CalcTextSize(" - ").x / 2 - ImGui::GetStyle().FramePadding.x * 4;
                u64 start = region->getStartAddress(), end = region->getEndAddress();

                ImGui::PushItemWidth(width);
                ImGui::InputHexadecimal("##start", &start);
                ImGui::PopItemWidth();
                ImGui::SameLine();
                ImGui::TextUnformatted(" - ");
                ImGui::SameLine();
                ImGui::PushItemWidth(width);
                ImGui::InputHexadecimal("##end", &end);
                ImGui::PopItemWidth();

                *region = { start, (end - start) + 1 };
                break;
        }
    }

}