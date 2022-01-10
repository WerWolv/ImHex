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
        ~ViewProviderSettings() override;

        void drawContent() override;
        void drawAlwaysVisible() override;

        [[nodiscard]] bool hasViewMenuItemEntry() const override;

        [[nodiscard]] bool isAvailable() const override;
    };

}