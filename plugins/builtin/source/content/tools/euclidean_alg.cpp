#include <hex/api/localization_manager.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <fonts/vscode_icons.hpp>

#include <numeric>

namespace hex::plugin::builtin {

    void drawEuclidianAlgorithm() {
        static u64 a, b;

        static i64 gcdResult = 0;
        static i64 lcmResult = 0;
        static i64 p = 0, q = 0;
        static bool overflow = false;

        constexpr static auto extendedGcd = []<typename T>(T a, T b) -> std::pair<T, T> {
            T x = 1, y = 0;

            T xLast = 0, yLast = 1;

            while (b > 0) {
                T quotient = a / b;

                std::tie(x, xLast)  = std::tuple { xLast, x - quotient * xLast };
                std::tie(y, yLast)  = std::tuple { yLast, y - quotient * yLast };
                std::tie(a, b)      = std::tuple { b, a - quotient * b };
            }

            return { x, y };
        };

        ImGuiExt::TextFormattedWrapped("{}", "hex.builtin.tools.euclidean_algorithm.description"_lang);

        ImGui::NewLine();

        if (ImGuiExt::BeginBox()) {
            bool hasChanged = false;
            hasChanged = ImGui::InputScalar("A", ImGuiDataType_U64, &a) || hasChanged;
            hasChanged = ImGui::InputScalar("B", ImGuiDataType_U64, &b) || hasChanged;

            // Update results when input changed
            if (hasChanged) {

                // Detect overflow
                const u64 multiplicationResult = a * b;
                if (a != 0 && multiplicationResult / a != b) {
                    gcdResult = 0;
                    lcmResult = 0;
                    p = 0;
                    q = 0;

                    overflow = true;
                } else {
                    gcdResult       = std::gcd<i128, i128>(a, b);
                    lcmResult       = std::lcm<i128, i128>(a, b);
                    std::tie(p, q)  = extendedGcd(a, b);

                    overflow = false;
                }
            }

            ImGui::Separator();

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().DisabledAlpha);
            ImGui::InputScalar("gcd(A, B)", ImGuiDataType_S64, &gcdResult, nullptr, nullptr, "%llu", ImGuiInputTextFlags_ReadOnly);

            ImGui::Indent();
            ImGuiExt::TextFormatted(ICON_VS_ARROW_RIGHT " a \u00D7 p  +  b \u00D7 q  =  ({0}) \u00D7 ({1})  +  ({2}) \u00D7 ({3})", a, p, b, q);
            ImGui::Unindent();

            ImGui::InputScalar("lcm(A, B)", ImGuiDataType_S64, &lcmResult, nullptr, nullptr, "%llu", ImGuiInputTextFlags_ReadOnly);
            ImGui::PopStyleVar();

            ImGuiExt::EndBox();
        }

        if (overflow)
            ImGuiExt::TextFormattedColored(ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed), "{}", "hex.builtin.tools.euclidean_algorithm.overflow"_lang);
        else
            ImGui::NewLine();

    }

}
