#pragma once

#include <hex.hpp>

#include "imgui.h"
#include "views/view.hpp"
#include "lang/pattern_data.hpp"

#include <vector>
#include <tuple>
#include <cstdio>

namespace hex {

    namespace prv { class Provider; }

    class ViewHelp : public View {
    public:
        ViewHelp();
        ~ViewHelp() override;

        void drawContent() override;
        void drawMenu() override;

        bool hasViewMenuItemEntry() override { return false; }

    private:
        bool m_aboutWindowOpen = false;
        bool m_patternHelpWindowOpen = false;
        bool m_mathHelpWindowOpen = false;

        void drawAboutPopup();
        void drawPatternHelpPopup();
        void drawMathEvaluatorHelp();
    };

}