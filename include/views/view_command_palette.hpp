#pragma once

#include <hex.hpp>

#include "imgui.h"
#include "views/view.hpp"
#include "lang/pattern_data.hpp"

#include <vector>
#include <tuple>
#include <cstdio>

namespace hex {

    namespace prv { class Provider; }

    class ViewCommandPalette : public View {
    public:
        ViewCommandPalette();
        ~ViewCommandPalette() override;

        void createView() override;
        void createMenu() override;

        bool handleShortcut(int key, int mods) override;

        bool hasViewMenuItemEntry() override { return false; }
        ImVec2 getMinSize() override { return ImVec2(400, 100); }
        ImVec2 getMaxSize() override { return ImVec2(400, 100); }

    private:
        bool m_justOpened = false;
        std::vector<char> m_commandBuffer;
        std::vector<std::string> m_lastResults;
        std::string m_exactResult;

        std::vector<std::string> getCommandResults(std::string_view command);
    };

}