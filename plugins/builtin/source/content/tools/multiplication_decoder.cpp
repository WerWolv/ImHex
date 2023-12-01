#include <hex/helpers/utils.hpp>
#include <hex/api/localization_manager.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::plugin::builtin {

    void drawInvariantMultiplicationDecoder() {
        static u64 divisor = 1;
        static u64 multiplier = 1;
        static u64 numBits = 32;

        ImGuiExt::TextFormattedWrapped("{}", "hex.builtin.tools.invariant_multiplication.description"_lang);

        ImGui::NewLine();

        if (ImGui::BeginChild("##calculator", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 5), true)) {
            constexpr static u64 min = 1, max = 64;
            ImGui::SliderScalar("hex.builtin.tools.invariant_multiplication.num_bits"_lang, ImGuiDataType_U64, &numBits, &min, &max);
            ImGui::NewLine();

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_TableRowBgAlt));
            if (ImGui::BeginChild("##calculator", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() + 12_scaled), true)) {
                ImGui::PushItemWidth(100_scaled);

                ImGui::TextUnformatted("X /");
                ImGui::SameLine();
                if (ImGui::InputScalar("##divisor", ImGuiDataType_U64, &divisor)) {
                    if (divisor == 0)
                        divisor = 1;

                    multiplier = ((1LLU << (numBits + 1)) / divisor) + 1;
                }

                ImGui::SameLine();

                ImGui::TextUnformatted(" <=> ");

                ImGui::SameLine();
                ImGui::TextUnformatted("( X *");
                ImGui::SameLine();
                if (ImGuiExt::InputHexadecimal("##multiplier", &multiplier)) {
                    if (multiplier == 0)
                        multiplier = 1;
                    divisor = ((1LLU << (numBits + 1)) / multiplier) + 1;
                }

                ImGui::SameLine();
                ImGuiExt::TextFormatted(") >> {}", numBits + 1);

                ImGui::PopItemWidth();
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::PopStyleVar();
        }
        ImGui::EndChild();
    }

}