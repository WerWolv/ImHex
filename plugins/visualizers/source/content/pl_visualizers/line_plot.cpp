#include <hex/helpers/utils.hpp>

#include <content/visualizer_helpers.hpp>

#include <implot.h>
#include <imgui.h>

#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::plugin::visualizers {

    void drawLinePlotVisualizer(pl::ptrn::Pattern &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
        static std::vector<float> values;
        auto dataPattern = arguments[0].toPattern();

        if (ImPlot::BeginPlot("##plot", ImVec2(400, 250), ImPlotFlags_CanvasOnly)) {
            ImPlot::SetupAxes("X", "Y", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

            if (shouldReset) {
                values.clear();
                values = sampleData(patternToArray<float>(dataPattern.get()), ImPlot::GetPlotSize().x * 4);
            }

            ImPlot::PlotLine("##line", values.data(), values.size());

            ImPlot::EndPlot();
        }
    }

}