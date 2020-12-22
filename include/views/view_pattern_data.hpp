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

    class ViewPatternData : public View {
    public:
        ViewPatternData(prv::Provider* &dataProvider, std::vector<lang::PatternData*> &patternData);
        ~ViewPatternData() override;

        void drawContent() override;
        void drawMenu() override;

    private:

        prv::Provider* &m_dataProvider;
        std::vector<lang::PatternData*> &m_patternData;
        std::vector<lang::PatternData*> m_sortedPatternData;
    };

}