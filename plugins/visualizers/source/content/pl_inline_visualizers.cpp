#include <hex/api/content_registry.hpp>

#include <imgui.h>

#include <pl/patterns/pattern.hpp>

#include <hex/helpers/fmt.hpp>

#include <fonts/vscode_icons.hpp>

namespace hex::plugin::visualizers {

    namespace {

        void drawColorInlineVisualizer(pl::ptrn::Pattern &, bool, std::span<const pl::core::Token::Literal> arguments) {
            auto r = arguments[0].toFloatingPoint();
            auto g = arguments[1].toFloatingPoint();
            auto b = arguments[2].toFloatingPoint();
            auto a = arguments[3].toFloatingPoint();

            ImGui::ColorButton("color", ImVec4(r / 255.0F, g / 255.0F, b / 255.0F, a / 255.0F), ImGuiColorEditFlags_NoTooltip, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()));
        }

        void drawGaugeInlineVisualizer(pl::ptrn::Pattern &, bool, std::span<const pl::core::Token::Literal> arguments) {
            auto value = arguments[0].toFloatingPoint();

            const auto color = ImGui::GetStyleColorVec4(ImGuiCol_Text);

            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(color.x, color.y, color.z, 0.2F));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0F, 0.0F, 0.0F, 0.0F));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(color.x, color.y, color.z, 0.5F));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0F);

            ImGui::ProgressBar(value / 100.0F, ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()), "");

            ImGui::PopStyleVar(1);
            ImGui::PopStyleColor(3);
        }

        void drawButtonInlineVisualizer(pl::ptrn::Pattern &pattern, bool, std::span<const pl::core::Token::Literal> arguments) {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0.5F));

            if (ImGui::Button(hex::format(" {}  {}", ICON_VS_PLAY, pattern.getFormattedValue()).c_str(), ImVec2(ImGui::GetColumnWidth(), ImGui::GetTextLineHeight()))) {
                auto *evaluator = pattern.getEvaluator();
                const auto functionName = arguments[0].toString(false);
                const auto &function = evaluator->findFunction(functionName);

                if (function.has_value())
                    function->func(evaluator, { pattern.clone() });
            }

            ImGui::PopStyleVar(2);
        }

    }

    void registerPatternLanguageInlineVisualizers() {
        using ParamCount = pl::api::FunctionParameterCount;

        ContentRegistry::PatternLanguage::addInlineVisualizer("color",  drawColorInlineVisualizer,  ParamCount::exactly(4));
        ContentRegistry::PatternLanguage::addInlineVisualizer("gauge",  drawGaugeInlineVisualizer,  ParamCount::exactly(1));
        ContentRegistry::PatternLanguage::addInlineVisualizer("button", drawButtonInlineVisualizer, ParamCount::exactly(1));
    }

}
