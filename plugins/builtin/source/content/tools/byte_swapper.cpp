#include <hex/api/localization_manager.hpp>

#include <algorithm>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <fonts/vscode_icons.hpp>

namespace hex::plugin::builtin {

    void drawByteSwapper() {
        static std::string input, buffer, output;

        if (ImGuiExt::InputTextIcon("hex.builtin.tools.input"_lang, ICON_VS_SYMBOL_NUMERIC, input, ImGuiInputTextFlags_CharsHexadecimal)) {
            auto nextAlignedSize = std::max<size_t>(2, std::bit_ceil(input.size()));

            buffer.clear();
            buffer.resize(nextAlignedSize - input.size(), '0');
            buffer += input;

            output.clear();
            for (u32 i = 0; i < buffer.size(); i += 2) {
                output += buffer[buffer.size() - i - 2];
                output += buffer[buffer.size() - i - 1];
            }
        }

        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().DisabledAlpha);
        ImGuiExt::InputTextIcon("hex.builtin.tools.output"_lang, ICON_VS_SYMBOL_NUMERIC, output, ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleVar();
    }

}
