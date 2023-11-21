#pragma once

#include <hex/ui/view.hpp>

#include <ui/pattern_drawer.hpp>

namespace hex::plugin::builtin {

    class ViewPatternData : public View::Window {
    public:
        ViewPatternData();
        ~ViewPatternData() override;

        void drawContent() override;

    private:
        std::unique_ptr<ui::PatternDrawer> m_patternDrawer;
    };

}