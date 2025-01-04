#include "content/views/view_highlight_rules.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/api/project_file_manager.hpp>
#include <wolv/utils/guards.hpp>

#include <fonts/vscode_icons.hpp>

namespace hex::plugin::builtin {

    wolv::math_eval::MathEvaluator<i128> ViewHighlightRules::Rule::Expression::s_evaluator;

    ViewHighlightRules::Rule::Rule(std::string name) : name(std::move(name)) { }

    ViewHighlightRules::Rule::Rule(Rule &&other) noexcept {
        this->name = std::move(other.name);
        this->expressions = std::move(other.expressions);
        this->enabled = other.enabled;

        // Set the parent rule to the new rule for all expressions
        for (auto &expression : this->expressions)
            expression.parentRule = this;
    }

    ViewHighlightRules::Rule& ViewHighlightRules::Rule::operator=(Rule &&other) noexcept {
        this->name = std::move(other.name);
        this->expressions = std::move(other.expressions);
        this->enabled = other.enabled;

        // Set the parent rule to the new rule for all expressions
        for (auto &expression : this->expressions)
            expression.parentRule = this;

        return *this;
    }

    void ViewHighlightRules::Rule::addExpression(Expression &&expression) {
        // Add the expression to the list and set the parent rule
        expression.parentRule = this;
        this->expressions.emplace_back(std::move(expression));
    }

    ViewHighlightRules::Rule::Expression::Expression(std::string mathExpression, std::array<float, 3> color) : mathExpression(std::move(mathExpression)), color(color) {
        // Create a new highlight provider function for this expression
        this->addHighlight();
    }

    ViewHighlightRules::Rule::Expression::~Expression() {
        // If there was a highlight, remove it
        if (this->highlightId > 0)
            ImHexApi::HexEditor::removeForegroundHighlightingProvider(this->highlightId);
    }

    ViewHighlightRules::Rule::Expression::Expression(Expression &&other) noexcept : mathExpression(std::move(other.mathExpression)), color(other.color), parentRule(other.parentRule) {
        // Remove the highlight from the other expression and add a new one for this one
        // This is necessary as the highlight provider function holds a reference to the expression
        // so to avoid dangling references, we need to destroy the old one before the expression itself
        // is deconstructed
        other.removeHighlight();
        this->addHighlight();
    }

    ViewHighlightRules::Rule::Expression& ViewHighlightRules::Rule::Expression::operator=(Expression &&other) noexcept {
        this->mathExpression = std::move(other.mathExpression);
        this->color = other.color;
        this->parentRule = other.parentRule;

        // Remove the highlight from the other expression and add a new one for this one
        other.removeHighlight();
        this->addHighlight();

        return *this;
    }

    void ViewHighlightRules::Rule::Expression::addHighlight() {
        this->highlightId = ImHexApi::HexEditor::addForegroundHighlightingProvider([this](u64 offset, const u8 *buffer, size_t size, bool) -> std::optional<color_t>{
            // If the rule containing this expression is disabled, don't highlight anything
            if (!this->parentRule->enabled)
                return std::nullopt;

            // If the expression is empty, don't highlight anything
            if (this->mathExpression.empty())
                return std::nullopt;

            // Load the bytes that are being highlighted into a variable
            u64 value = 0;
            std::memcpy(&value, buffer, std::min(sizeof(value), size));

            // Add the value and offset variables to the evaluator
            s_evaluator.setVariable("value", value);
            s_evaluator.setVariable("offset", offset);

            // Evaluate the expression
            auto result = s_evaluator.evaluate(this->mathExpression);

            // If the evaluator has returned a value and it's not 0, return the selected color
            if (result.has_value() && result.value() != 0)
                return ImGui::ColorConvertFloat4ToU32(ImVec4(this->color[0], this->color[1], this->color[2], 1.0F));
            else
                return std::nullopt;
        });
        ImHexApi::Provider::markDirty();
    }

    void ViewHighlightRules::Rule::Expression::removeHighlight() {
        ImHexApi::HexEditor::removeForegroundHighlightingProvider(this->highlightId);
        this->highlightId = 0;
        ImHexApi::Provider::markDirty();
    }


    ViewHighlightRules::ViewHighlightRules() : View::Floating("hex.builtin.view.highlight_rules.name") {
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.view.highlight_rules.menu.file.rules" }, ICON_VS_TAG, 1650, Shortcut::None, [&, this] {
            this->getWindowOpenState() = true;
        }, ImHexApi::Provider::isValid);

        ProjectFile::registerPerProviderHandler({
            .basePath = "highlight_rules.json",
            .required = false,
            .load = [this](prv::Provider *provider, const std::fs::path &basePath, const Tar &tar) -> bool {
                const auto json = nlohmann::json::parse(tar.readString(basePath));

                auto &rules = m_rules.get(provider);
                rules.clear();

                for (const auto &entry : json) {
                    Rule rule(entry["name"].get<std::string>());

                    rule.enabled = entry["enabled"].get<bool>();

                    for (const auto &expression : entry["expressions"]) {
                        rule.addExpression(Rule::Expression(
                            expression["mathExpression"].get<std::string>(),
                            expression["color"].get<std::array<float, 3>>()
                        ));
                    }

                    rules.emplace_back(std::move(rule));
                }

                return true;
            },
            .store = [this](prv::Provider *provider, const std::fs::path &basePath, const Tar &tar) -> bool {
                nlohmann::json result = nlohmann::json::array();
                for (const auto &rule : m_rules.get(provider)) {
                    nlohmann::json content;

                    content["name"]    = rule.name;
                    content["enabled"] = rule.enabled;

                    for (const auto &expression : rule.expressions) {
                        content["expressions"].push_back({
                            { "mathExpression", expression.mathExpression },
                            { "color", expression.color }
                        });
                    }

                    result.push_back(content);
                }

                tar.writeString(basePath, result.dump(4));

                return true;
            }
        });

        // Initialize the selected rule iterators to point to the end of the rules lists
        m_selectedRule = m_rules->end();
        EventProviderCreated::subscribe([this](prv::Provider *provider) {
            m_selectedRule.get(provider) = m_rules.get(provider).end();
        });
    }

    void ViewHighlightRules::drawRulesList() {
        // Draw a table containing all the existing highlighting rules
        if (ImGui::BeginTable("RulesList", 2, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY, ImGui::GetContentRegionAvail() - ImVec2(0, ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().WindowPadding.y))) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1);
            ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, 10_scaled);

            for (auto it = m_rules->begin(); it != m_rules->end(); ++it) {
                auto &rule = *it;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                // Add a selectable for each rule to be able to switch between them
                ImGui::PushID(&rule);
                ImGui::BeginDisabled(!rule.enabled);
                if (ImGui::Selectable(rule.name.c_str(), m_selectedRule == it, ImGuiSelectableFlags_SpanAvailWidth)) {
                    m_selectedRule = it;
                }
                ImGui::EndDisabled();

                // Draw enabled checkbox
                ImGui::TableNextColumn();
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2());
                if (ImGui::Checkbox("##enabled", &rule.enabled)) {
                    EventHighlightingChanged::post();
                }
                ImGui::PopStyleVar();

                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        // Draw button to add a new rule
        if (ImGuiExt::DimmedIconButton(ICON_VS_ADD, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
            m_rules->emplace_back("hex.builtin.view.highlight_rules.new_rule"_lang);

            if (m_selectedRule == m_rules->end())
                m_selectedRule = m_rules->begin();
        }


        ImGui::SameLine();

        // Draw button to remove the selected rule
        ImGui::BeginDisabled(m_selectedRule == m_rules->end());
        if (ImGuiExt::DimmedIconButton(ICON_VS_REMOVE, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
            auto next = std::next(*m_selectedRule);
            m_rules->erase(*m_selectedRule);
            m_selectedRule = next;
        }
        ImGui::EndDisabled();
    }


    void ViewHighlightRules::drawRulesConfig() {
        if (ImGuiExt::BeginSubWindow("hex.builtin.view.highlight_rules.config"_lang, nullptr, ImGui::GetContentRegionAvail())) {
            if (m_selectedRule != m_rules->end()) {

                // Draw text input field for the rule name
                ImGui::PushItemWidth(-1);
                ImGui::InputTextWithHint("##name", "Name", m_selectedRule.get()->name);
                ImGui::PopItemWidth();

                auto &rule = *m_selectedRule;

                // Draw a table containing all the expressions for the selected rule
                ImGui::PushID(&rule);
                ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2());
                if (ImGui::BeginTable("Expressions", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY, ImGui::GetContentRegionAvail() - ImVec2(0, ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().WindowPadding.y))) {
                    ImGui::TableSetupColumn("Color", ImGuiTableColumnFlags_WidthFixed, 19_scaled);
                    ImGui::TableSetupColumn("Expression", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("##Remove", ImGuiTableColumnFlags_WidthFixed, 19_scaled);

                    for (auto it = rule->expressions.begin(); it != rule->expressions.end(); ++it) {
                        auto &expression = *it;

                        bool updateHighlight = false;
                        ImGui::PushID(&expression);
                        ON_SCOPE_EXIT { ImGui::PopID(); };

                        ImGui::TableNextRow();

                        // Draw color picker
                        ImGui::TableNextColumn();
                        updateHighlight = ImGui::ColorEdit3("##color", expression.color.data(), ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoBorder) || updateHighlight;

                        // Draw math expression input field
                        ImGui::TableNextColumn();
                        ImGui::PushItemWidth(-1);
                        updateHighlight = ImGui::InputTextWithHint("##expression", "hex.builtin.view.highlight_rules.expression"_lang, expression.mathExpression) || updateHighlight;
                        ImGui::PopItemWidth();

                        // Draw a button to remove the expression
                        ImGui::TableNextColumn();
                        if (ImGuiExt::DimmedIconButton(ICON_VS_REMOVE, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                            rule->expressions.erase(it);
                            break;
                        }

                        // If any of the inputs have changed, update the highlight
                        if (updateHighlight)
                            EventHighlightingChanged::post();
                    }

                    ImGui::EndTable();
                }
                ImGui::PopStyleVar();

                // Draw button to add a new expression
                if (ImGuiExt::DimmedIconButton(ICON_VS_ADD, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                    m_selectedRule.get()->addExpression(Rule::Expression("", {}));
                    ImHexApi::Provider::markDirty();
                }

                ImGui::SameLine();

                // Draw help info for the expressions
                ImGuiExt::HelpHover("hex.builtin.view.highlight_rules.help_text"_lang, ICON_VS_INFO);

                ImGui::PopID();
            } else {
                ImGuiExt::TextFormattedCentered("hex.builtin.view.highlight_rules.no_rule"_lang);
            }

        }
        ImGuiExt::EndSubWindow();
    }


    void ViewHighlightRules::drawContent() {
        if (ImGui::BeginTable("Layout", 2)) {
            ImGui::TableSetupColumn("##left", ImGuiTableColumnFlags_WidthStretch, 0.33F);
            ImGui::TableSetupColumn("##right", ImGuiTableColumnFlags_WidthStretch, 0.66F);

            ImGui::TableNextRow();

            // Draw rules list
            ImGui::TableNextColumn();
            this->drawRulesList();

            // Draw rules config
            ImGui::TableNextColumn();
            this->drawRulesConfig();

            ImGui::EndTable();
        }
    }

}
