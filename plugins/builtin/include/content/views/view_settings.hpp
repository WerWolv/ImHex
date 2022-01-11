#pragma once

#include <hex/views/view.hpp>

#include <cstdio>
#include <string>

namespace hex::plugin::builtin {

    class ViewSettings : public View {
    public:
        explicit ViewSettings();
        ~ViewSettings() override;

        void drawContent() override;
        void drawMenu() override;
        [[nodiscard]] bool isAvailable() const override { return true; }

        [[nodiscard]] bool hasViewMenuItemEntry() const override { return false; }

        [[nodiscard]] ImVec2 getMinSize() const override {
            return scaled(ImVec2(500, 300));
        }
    };

}