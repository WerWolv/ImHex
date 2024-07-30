#include <hex/helpers/utils.hpp>

#include <content/visualizer_helpers.hpp>

#include <implot.h>
#include <imgui.h>

#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::plugin::visualizers {

    void drawScatterPlotVisualizer(pl::ptrn::Pattern &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
        static std::vector<float> xValues, yValues;

        auto xPattern = arguments[0].toPattern();
        auto yPattern = arguments[1].toPattern();

        if (ImPlot::BeginPlot("##plot", ImVec2(400, 250), ImPlotFlags_CanvasOnly)) {
            ImPlot::SetupAxes("X", "Y", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

            if (shouldReset) {
                xValues.clear(); yValues.clear();
                xValues = sampleData(patternToArray<float>(xPattern.get()), ImPlot::GetPlotSize().x * 4);
                yValues = sampleData(patternToArray<float>(yPattern.get()), ImPlot::GetPlotSize().x * 4);
            }

            ImPlot::PlotScatter("##scatter", xValues.data(), yValues.data(), xValues.size());

            ImPlot::EndPlot();
        }
    }

}