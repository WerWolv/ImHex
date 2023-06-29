#pragma once

#include <hex.hpp>
#include <hex/ui/view.hpp>

#include <ui/pattern_drawer.hpp>
#include <hex/providers/provider.hpp>

#include <vector>
#include <tuple>

namespace hex::plugin::builtin {

    class ViewPatternData : public View {
    public:
        ViewPatternData();
        ~ViewPatternData() override;

        void drawContent() override;

    private:
        ui::PatternDrawer m_patternDrawer;
        bool m_shouldReset = false;
        u64 m_lastRunId = 0;
    };

}