#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/views/view.hpp>
#include <hex/lang/pattern_data.hpp>

#include <vector>
#include <tuple>
#include <cstdio>

namespace hex {

    namespace prv { class Provider; }

    class ViewPatternData : public View {
    public:
        ViewPatternData();
        ~ViewPatternData() override;

        void drawContent() override;
        void drawMenu() override;

    private:
        std::vector<lang::PatternData*> m_sortedPatternData;
    };

}