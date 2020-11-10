#pragma once

#include <hex.hpp>

#include "imgui.h"
#include "views/view.hpp"
#include "views/highlight.hpp"

#include <vector>
#include <tuple>
#include <cstdio>

namespace hex {

    class ViewPatternData : public View {
    public:
        ViewPatternData(FILE* &file,std::vector<Highlight> &highlights);
        virtual ~ViewPatternData();

        virtual void createView() override;
        virtual void createMenu() override;

    private:
        FILE* &m_file;
        std::vector<Highlight> &m_highlights;
        bool m_windowOpen = true;
    };

}