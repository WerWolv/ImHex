
#include "hex/ui/visualizer_drawer.hpp"
#include "imgui.h"

namespace hex::ui {
    void VisualizerDrawer::drawVisualizer(
            const std::map<std::string, ContentRegistry::PatternLanguage::impl::Visualizer> &visualizers,
            const std::vector<pl::core::Token::Literal> &arguments, pl::ptrn::Pattern &pattern, bool reset) {
        auto visualizerName = arguments.front().toString(true);

        if (auto entry = visualizers.find(visualizerName); entry != visualizers.end()) {
            const auto &[name, visualizer] = *entry;

            auto paramCount = arguments.size() - 1;
            auto [minParams, maxParams] = visualizer.parameterCount;

            if (paramCount >= minParams && paramCount <= maxParams) {
                try {
                    visualizer.callback(pattern, reset, {arguments.begin() + 1, arguments.end()});
                } catch (std::exception &e) {
                    m_lastVisualizerError = e.what();
                }
            } else {
                ImGui::TextUnformatted("hex.ui.pattern_drawer.visualizer.invalid_parameter_count"_lang);
            }
        } else {
            ImGui::TextUnformatted("hex.ui.pattern_drawer.visualizer.unknown"_lang);
        }

       if (!m_lastVisualizerError.empty())
            ImGui::TextUnformatted(m_lastVisualizerError.c_str());
    }

}