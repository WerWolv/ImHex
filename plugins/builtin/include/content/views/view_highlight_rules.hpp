#pragma once

#include <hex/ui/view.hpp>
#include <hex/providers/provider_data.hpp>
#include <hex/providers/file_backed_provider_data.hpp>

#include <list>

#include <wolv/math_eval/math_evaluator.hpp>

namespace hex::plugin::builtin {

    class ViewHighlightRules : public View::Floating {
    public:
        ViewHighlightRules();
        ~ViewHighlightRules() override;

        void drawContent() override;
        void drawHelpText() override;

        [[nodiscard]] bool hasViewMenuItemEntry() const override { return false; }

        ImVec2 getMinSize() const override {
            return scaled({700, 400});
        }

        ImVec2 getMaxSize() const override {
            return scaled({700, 400});
        }

        ImGuiWindowFlags getWindowFlags() const override {
            return ImGuiWindowFlags_NoResize;
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
        using Rules = std::list<Rule>;

        static FileBackedProviderData<Rules>::SerializedData encodeRules(const Rules &rules);
        static std::optional<Rules> decodeRules(std::span<const u8> data);

        void drawRulesList();
        void drawRulesConfig();
    private:
        FileBackedProviderData<Rules> m_rules;
        PerProvider<std::optional<size_t>> m_selectedRule;
    };

}
