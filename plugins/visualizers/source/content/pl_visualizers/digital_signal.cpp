#include <hex/helpers/utils.hpp>

#include <content/visualizer_helpers.hpp>

#include <implot.h>
#include <imgui.h>

#include <hex/ui/imgui_imhex_extensions.h>
#include <pl/patterns/pattern_bitfield.hpp>

namespace hex::plugin::visualizers {

    void drawDigitalSignalVisualizer(pl::ptrn::Pattern &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
        auto *bitfield = dynamic_cast<pl::ptrn::PatternBitfield*>(arguments[0].toPattern().get());
        if (bitfield == nullptr)
            throw std::logic_error("Digital signal visualizer only works with bitfields.");


        struct DataPoint {
            std::array<ImVec2, 2> points;
            std::string label;
            std::string value;
            ImColor color;
        };
        static std::vector<DataPoint> dataPoints;
        static ImVec2 lastPoint;

        if (shouldReset) {
            dataPoints.clear();
            lastPoint = { 0, 0 };

            bitfield->forEachEntry(0, bitfield->getEntryCount(), [&](u64, pl::ptrn::Pattern *entry) {
                size_t bitSize;
                if (auto bitfieldField = dynamic_cast<pl::ptrn::PatternBitfieldField*>(entry); bitfieldField != nullptr)
                    bitSize = bitfieldField->getBitSize();
                else
                    bitSize = entry->getSize() * 8;

                auto value = entry->getValue();
                bool high = value.toUnsigned() > 0;
                dataPoints.emplace_back(
                     std::array<ImVec2, 2> { lastPoint, { lastPoint.x, high ? 1.0F : 0.0F } },
                    entry->getDisplayName(),
                    entry->getFormattedValue(),
                    entry->getColor()
                );

                lastPoint = dataPoints.back().points[1];
                lastPoint.x += float(bitSize);
            });

            dataPoints.push_back({
                .points = { lastPoint, { lastPoint.x, 0 } },
                .label = "",
                .value = "",
                .color = ImColor(0x00)
            });
        }

        if (ImPlot::BeginPlot("##Signal", ImVec2(600_scaled, 200_scaled), ImPlotFlags_NoLegend | ImPlotFlags_NoFrame | ImPlotFlags_NoMenus | ImPlotFlags_NoMouseText)) {
            ImPlot::SetupAxisLimitsConstraints(ImAxis_X1, 0, lastPoint.x);

            ImPlot::SetupAxis(ImAxis_Y1, "", ImPlotAxisFlags_LockMin | ImPlotAxisFlags_LockMax);
            ImPlot::SetupAxisFormat(ImAxis_Y1, "");
            ImPlot::SetupAxisLimits(ImAxis_Y1, -0.1F, 1.1F);

            for (size_t i = 0; i < dataPoints.size() - 1; i += 1) {
                const auto &left = dataPoints[i];
                const auto &right = dataPoints[i + 1];

                {
                    auto x = left.points[1].x + ((right.points[0].x - left.points[1].x) / 2);
                    ImPlot::Annotation(x, 0.55F, left.color, {}, false, "%s", left.label.c_str());
                    ImPlot::Annotation(x, 0.40F, left.color, {}, false, "%s", left.value.c_str());
                }

                {
                    ImVec2 min = ImPlot::PlotToPixels(ImPlotPoint(left.points[0].x, 0));
                    ImVec2 max = ImPlot::PlotToPixels(ImPlotPoint(right.points[1].x, 1));

                    ImPlot::PushPlotClipRect();
                    auto transparentColor = left.color;
                    transparentColor.Value.w = 0.2F;
                    ImPlot::GetPlotDrawList()->AddRectFilled(min, max, transparentColor);
                    ImPlot::PopPlotClipRect();
                }
            }

            ImPlot::PushStyleVar(ImPlotStyleVar_LineWeight, 2_scaled);
            ImPlot::PlotLineG("Signal", [](int idx, void*) -> ImPlotPoint {
                return dataPoints[idx / 2].points[idx % 2];
            }, nullptr, dataPoints.size() * 2);
            ImPlot::PopStyleVar();

            ImPlot::EndPlot();
        }
    }

}
