#pragma once

#include <hex/ui/view.hpp>

namespace hex::plugin::builtin {

    class ViewProviderSettings : public View::Modal {
    public:
        ViewProviderSettings();
        ~ViewProviderSettings() override;

        void drawContent() override;

        [[nodiscard]] bool hasViewMenuItemEntry() const override;

        [[nodiscard]] bool shouldDraw() const override { return true; }
    };

}