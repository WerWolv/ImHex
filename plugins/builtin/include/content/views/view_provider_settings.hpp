#pragma once

#include <hex/ui/view.hpp>

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