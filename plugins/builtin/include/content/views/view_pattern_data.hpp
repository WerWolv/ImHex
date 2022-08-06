#pragma once

#include "pattern_drawer.hpp"

#include <hex.hpp>

#include <imgui.h>
#include <hex/ui/view.hpp>

#include <vector>
#include <tuple>
#include <cstdio>

namespace hex::plugin::builtin {

    class ViewPatternData : public View {
    public:
        ViewPatternData();
        ~ViewPatternData() override;

        void drawContent() override;

    private:
        std::map<prv::Provider *, std::vector<pl::ptrn::Pattern*>> m_sortedPatterns;
        hex::PatternDrawer m_patternDrawer;
    };

}