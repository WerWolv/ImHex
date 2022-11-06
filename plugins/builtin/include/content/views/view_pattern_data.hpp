#pragma once

#include <hex.hpp>
#include <hex/ui/view.hpp>

#include <content/helpers/pattern_drawer.hpp>
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
        std::map<hex::prv::Provider *, std::vector<pl::ptrn::Pattern*>> m_sortedPatterns;
        hex::PatternDrawer m_patternDrawer;
    };

}