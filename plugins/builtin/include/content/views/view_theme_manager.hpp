#pragma once

#include <hex/ui/view.hpp>

#include <string>

namespace hex::plugin::builtin {

    class ViewThemeManager : public View {
    public:
        ViewThemeManager();
        ~ViewThemeManager() override = default;

        void drawContent() override;

        [[nodiscard]] bool isAvailable() const override { return true; }
        [[nodiscard]] bool hasViewMenuItemEntry() const override { return false; }

    private:
        std::string m_themeName;
        bool m_viewOpen = false;

        std::optional<ImColor> m_startingColor;
        std::optional<u32> m_hoveredColorId;
        std::optional<std::string> m_hoveredHandlerName;
    };

}