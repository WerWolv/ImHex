#pragma once

#include <hex.hpp>

#include "imgui.h"

namespace hex {

    class View {
    public:
        View() { }
        virtual ~View() { }

        virtual void createView() = 0;
        virtual void createMenu() { }
        virtual bool handleShortcut(int key, int mods) { return false; }
    };

}