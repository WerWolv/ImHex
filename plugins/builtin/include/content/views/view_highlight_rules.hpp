#pragma once

#include <hex/api/content_registry.hpp>
#include <hex/ui/view.hpp>

#include <list>

#include <wolv/math_eval/math_evaluator.hpp>

namespace hex::plugin::builtin {

    class ViewHighlightRules : public View::Floating {
    public:
        ViewHighlightRules();
        ~ViewHighlightRules() override = default;

        void drawContent() override;

        [[nodiscard]] bool hasViewMenuItemEntry() const override { return false; }

        ImVec2 getMinSize() const override {
            return scaled({700, 400});
        }

        ImVec2 getMaxSize() const override {
            return scaled({700, 400});
        }

        ImGuiWindowFlags getWindowFlags() const override {
            return View::Floating::getWindowFlags() | ImGuiWindowFlags_NoResize;
        }

    private:
        struct Rule {
            struct Expression {
                Expression(std::string mathExpression, std::array<float, 3> color);
                ~Expression();
                Expression(const Expression&) = delete;
                Expression(Expression&&) noexcept;

                Expression& operator=(const Expression&) = delete;
                Expression& operator=(Expression&&) noexcept;

                std::string mathExpression;
                std::array<float, 3> color;

                u32 highlightId = 0;
                Rule *parentRule = nullptr;

                static wolv::math_eval::MathEvaluator<i128> s_evaluator;

            private:
                void addHighlight();
                void removeHighlight();
            };

            explicit Rule(std::string name);
            Rule(const Rule &) = delete;
            Rule(Rule &&) noexcept;

            Rule& operator=(const Rule &) = delete;
            Rule& operator=(Rule &&) noexcept;

            std::string name;
            std::list<Expression> expressions;
            bool enabled = true;

            void addExpression(Expression &&expression);
        };

    private:
        void drawRulesList();
        void drawRulesConfig();
    private:
        PerProvider<std::list<Rule>> m_rules;
        PerProvider<std::list<Rule>::iterator> m_selectedRule;
    };

}
