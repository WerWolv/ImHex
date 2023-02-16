#pragma once

#include <hex.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/ui/view.hpp>

#include <array>
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
    };

}