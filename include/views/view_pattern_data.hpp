#pragma once

#include <hex.hpp>

#include "imgui.h"
#include "views/view.hpp"
#include "views/highlight.hpp"

#include <vector>
#include <tuple>
#include <cstdio>

namespace hex {

    namespace prv { class Provider; }

    class ViewPatternData : public View {
    public:
        ViewPatternData(prv::Provider* &dataProvider, std::vector<Highlight> &highlights);
        ~ViewPatternData() override;

        void createView() override;
        void createMenu() override;

    private:
        prv::Provider* &m_dataProvider;
        std::vector<Highlight> &m_highlights;
        bool m_windowOpen = true;
    };

}