#pragma once

#include <hex/ui/view.hpp>

namespace hex::plugin::builtin {

    class ViewSettings : public View::Modal {
    public:
        explicit ViewSettings();
        ~ViewSettings() override;

        void drawContent() override;
        void drawAlwaysVisibleContent() override;

        [[nodiscard]] bool shouldDraw() const override { return true; }
        [[nodiscard]] bool hasViewMenuItemEntry() const override { return false; }

        [[nodiscard]] ImVec2 getMinSize() const override { return scaled({ 700, 400 }); }
        [[nodiscard]] ImVec2 getMaxSize() const override { return scaled({ 700, 400 }); }

    private:
        bool m_restartRequested = false;
        bool m_triggerPopup = false;
    };

}