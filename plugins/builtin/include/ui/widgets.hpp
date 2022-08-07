#pragma once

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::plugin::builtin::ui {


    enum class SelectedRegion : int {
        EntireData,
        Selection
    };

    inline void regionSelectionPicker(SelectedRegion *region, bool showHeader = true, bool firstEntry = false) {
        if (showHeader)
            ImGui::Header("hex.builtin.common.range"_lang, firstEntry);

        if (ImGui::RadioButton("hex.builtin.common.range.entire_data"_lang, *region == SelectedRegion::EntireData))
            *region = SelectedRegion::EntireData;
        if (ImGui::RadioButton("hex.builtin.common.range.selection"_lang, *region == SelectedRegion::Selection))
            *region = SelectedRegion::Selection;

    }

}