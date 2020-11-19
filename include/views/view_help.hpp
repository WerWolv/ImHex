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

        void createView() override;
        void createMenu() override;

    private:
        bool m_aboutWindowOpen = false;
        bool m_patternHelpWindowOpen = false;

        void drawAboutPopup();
        void drawPatternHelpPopup();
    };

}