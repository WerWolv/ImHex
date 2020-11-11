#pragma once

#include <hex.hpp>

#include "imgui.h"
#include "views/view.hpp"
#include "views/highlight.hpp"

#include <vector>
#include <tuple>
#include <cstdio>

#include "providers/provider.hpp"

namespace hex {

    class ViewPatternData : public View {
    public:
        ViewPatternData(prv::Provider* &dataProvider, std::vector<Highlight> &highlights);
        virtual ~ViewPatternData();

        virtual void createView() override;
        virtual void createMenu() override;

    private:
        prv::Provider* &m_dataProvider;
        std::vector<Highlight> &m_highlights;
        bool m_windowOpen = true;
    };

}