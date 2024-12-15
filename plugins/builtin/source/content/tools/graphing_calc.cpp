#include <hex/helpers/http_requests.hpp>

#include <wolv/math_eval/math_evaluator.hpp>

#include <imgui.h>
#include <implot.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <fonts/vscode_icons.hpp>

namespace hex::plugin::builtin {

    void drawGraphingCalculator() {
        static std::array<long double, 1000> x, y;
        static std::string mathInput;
        static ImPlotRect limits;
        static double prevPos = 0;
        static long double stepSize = 0.1;

        if (ImPlot::BeginPlot("Function", ImVec2(-1, 0), ImPlotFlags_NoTitle | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect | ImPlotFlags_NoMouseText | ImPlotFlags_NoFrame)) {
            ImPlot::SetupAxesLimits(-10, 10, -5, 5, ImPlotCond_Once);

            limits = ImPlot::GetPlotLimits(ImAxis_X1, ImAxis_Y1);

            ImPlot::PlotLine("f(x)", x.data(), y.data(), x.size());
            ImPlot::EndPlot();
        }

        ImGui::PushItemWidth(-1);
        ImGuiExt::InputTextIcon("##graphing_math_input", ICON_VS_SYMBOL_OPERATOR, mathInput, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        ImGui::PopItemWidth();

        if ((prevPos != limits.X.Min && (ImGui::IsMouseReleased(ImGuiMouseButton_Left) || ImGui::GetIO().MouseWheel != 0)) || (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter))) {
            wolv::math_eval::MathEvaluator<long double> evaluator;

            y = {};

            u32 i = 0;
            evaluator.setFunction("y", [&](auto args) -> std::optional<long double> {
                i32 index = i + args[0];
                if (index < 0 || u32(index) >= y.size())
                    return 0;
                else
                    return y[index];
            }, 1, 1);

            evaluator.registerStandardVariables();
            evaluator.registerStandardFunctions();

            stepSize = (limits.X.Size()) / x.size();

            for (i = 0; i < x.size(); i++) {
                evaluator.setVariable("x", limits.X.Min + i * stepSize);
                x[i] = limits.X.Min + i * stepSize;
                y[i] = evaluator.evaluate(mathInput).value_or(0);

                if (y[i] < limits.Y.Min)
                    limits.Y.Min = y[i];
                if (y[i] > limits.Y.Max)
                    limits.X.Max = y[i];

            }

            limits.X.Max = limits.X.Min + x.size() * stepSize;
            prevPos = limits.X.Min;
        }

    }

}