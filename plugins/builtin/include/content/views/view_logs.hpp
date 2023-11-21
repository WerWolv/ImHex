#pragma once

#include <hex/ui/view.hpp>

namespace hex::plugin::builtin {

    class ViewLogs : public View::Floating {
    public:
        ViewLogs();
        ~ViewLogs() override = default;

        void drawContent() override;

        [[nodiscard]] bool shouldDraw() const override { return true; }
        [[nodiscard]] bool hasViewMenuItemEntry() const override { return false; }

    private:
        int m_logLevel = 1;
    };

}