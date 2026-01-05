#include <ui/visualizer_drawer.hpp>

#include <hex/api/localization_manager.hpp>
#include <hex/helpers/scaling.hpp>

#include "imgui.h"

namespace hex::ui {
    void VisualizerDrawer::drawVisualizer(
            const std::map<std::string, ContentRegistry::PatternLanguage::impl::Visualizer> &visualizers,
            const std::vector<pl::core::Token::Literal> &arguments, pl::ptrn::Pattern &pattern, bool reset) {
        auto visualizerName = arguments.front().toString(true);

        if (m_lastVisualizerError.empty()) {
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
        }

       if (!m_lastVisualizerError.empty()) {
           auto textSize = ImGui::CalcTextSize(m_lastVisualizerError.c_str(), nullptr, false, 300_scaled);
           if (ImGui::BeginChild("##error_message", textSize + ImGui::GetStyle().WindowPadding * 2)) {
               ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0F, 0.0F, 0.0F, 1.0F));
               ImGui::TextWrapped("%s", m_lastVisualizerError.c_str());
               ImGui::PopStyleColor();
           }
           ImGui::EndChild();
       }

    }

}