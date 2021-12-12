#pragma once

#include <hex.hpp>

#include <imgui.h>
#include <hex/views/view.hpp>

#include <vector>
#include <tuple>
#include <cstdio>

namespace hex::plugin::builtin {

    namespace prv { class Provider; }

    class ViewHelp : public View {
    public:
        ViewHelp();
        ~ViewHelp() override;

        void drawContent() override;
        void drawMenu() override;
        bool isAvailable() override { return true; }

        bool hasViewMenuItemEntry() override { return false; }

        ImVec2 getMinSize() override {
            return ImVec2(400, 300);
        }

    private:
        bool m_aboutWindowOpen = false;

        void drawAboutPopup();

        void drawAboutMainPage();
        void drawContributorPage();
        void drawLibraryCreditsPage();
        void drawPathsPage();
    };

}