#pragma once

#include <hex.hpp>

#include "imgui.h"
#include "views/view.hpp"
#include "views/pattern_data.hpp"

#include <vector>
#include <tuple>
#include <cstdio>

namespace hex {

    namespace prv { class Provider; }

    class ViewPatternData : public View {
    public:
        ViewPatternData(prv::Provider* &dataProvider, std::vector<hex::PatternData*> &patternData);
        ~ViewPatternData() override;

        void createView() override;
        void createMenu() override;

    private:
        prv::Provider* &m_dataProvider;
        std::vector<hex::PatternData*> &m_patternData;
        bool m_windowOpen = true;
    };

}