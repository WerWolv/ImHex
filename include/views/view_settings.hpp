#pragma once

#include <hex/views/view.hpp>

#include <cstdio>
#include <string>

namespace hex {

    class ViewSettings : public View {
    public:
        explicit ViewSettings();
        ~ViewSettings() override;

        void drawContent() override;
        void drawMenu() override;
        bool isAvailable() override { return true; }

        bool hasViewMenuItemEntry() override { return false; }
    };

}