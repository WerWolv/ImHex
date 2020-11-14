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

    class ViewTools : public View {
    public:
        ViewTools();
        ~ViewTools() override;

        void createView() override;
        void createMenu() override;

    private:
        bool m_windowOpen = true;

        char *m_mangledBuffer = nullptr;
        char *m_demangledName = nullptr;
    };

}