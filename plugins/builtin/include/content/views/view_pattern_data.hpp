#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/views/view.hpp>

#include <vector>
#include <tuple>
#include <cstdio>

namespace hex {

    namespace prv { class Provider; }
    namespace lang { class PatternData; }

}


namespace hex::plugin::builtin {

    class ViewPatternData : public View {
    public:
        ViewPatternData();
        ~ViewPatternData() override;

        void drawContent() override;

    private:
        std::vector<pl::PatternData*> m_sortedPatternData;
    };

}