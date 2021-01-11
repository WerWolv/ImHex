#pragma once

#include "views/view.hpp"

#include <cstdio>
#include <string>

namespace hex {

    class ViewSettings : public View {
    public:
        explicit ViewSettings();
        ~ViewSettings() override;

        void drawContent() override;
        void drawMenu() override;

        bool hasViewMenuItemEntry() override { return false; }
    private:
        bool m_settingsWindowOpen = false;
    };

}