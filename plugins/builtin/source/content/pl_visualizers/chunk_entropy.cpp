#include <implot.h>
#include <imgui.h>
#include <content/helpers/diagrams.hpp>

#include <hex/ui/imgui_imhex_extensions.h>

#include <pl/patterns/pattern.hpp>

namespace hex::plugin::builtin {

    void drawChunkBasedEntropyVisualizer(pl::ptrn::Pattern &, bool shouldReset, std::span<const pl::core::Token::Literal> arguments) {
        // Variable used to store the result to avoid having to recalculate the result at each frame
        static DiagramChunkBasedEntropyAnalysis analyzer;

        // Compute data
        if (shouldReset) {
            auto pattern   = arguments[0].toPattern();
            auto chunkSize = arguments[1].toUnsigned();
            analyzer.process(pattern->getBytes(), chunkSize);
        }

        // Show results
        analyzer.draw(ImVec2(400, 250), ImPlotFlags_CanvasOnly);
    }

}