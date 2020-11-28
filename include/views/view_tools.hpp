#pragma once

#include <hex.hpp>

#include "imgui.h"
#include "views/view.hpp"
#include "helpers/math_evaluator.hpp"

#include <array>
#include <string>

namespace hex {

    namespace prv { class Provider; }

    class ViewTools : public View {
    public:
        ViewTools();
        ~ViewTools() override;

        void createView() override;
        void createMenu() override;

    private:
        char *m_mangledBuffer = nullptr;
        std::string m_demangledName;

        bool m_asciiTableShowOctal = false;

        char *m_regexInput = nullptr;
        char *m_regexPattern = nullptr;
        char *m_replacePattern = nullptr;
        std::string m_regexOutput;

        std::array<float, 4> m_pickedColor;

        MathEvaluator m_mathEvaluator;
        std::vector<long double> m_mathHistory;
        std::string m_lastMathError;
        char *m_mathInput = nullptr;

        void drawDemangler();
        void drawASCIITable();
        void drawRegexReplacer();
        void drawColorPicker();
        void drawMathEvaluator();
    };

}