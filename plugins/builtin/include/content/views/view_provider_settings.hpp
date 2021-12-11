#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/views/view.hpp>

#include <array>
#include <string>

namespace hex::plugin::builtin {

    class ViewProviderSettings : public hex::View {
    public:
        ViewProviderSettings();
        ~ViewProviderSettings();

        void drawContent() override;
        void drawAlwaysVisible() override;

        bool hasViewMenuItemEntry() override;

        bool isAvailable();
    };

}