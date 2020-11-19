#pragma once

#include "lang/ast_node.hpp"

#include "views/view.hpp"
#include "lang/pattern_data.hpp"

#include "providers/provider.hpp"

#include <concepts>
#include <cstring>


#include "ImGuiFileBrowser.h"

namespace hex {

    class ViewPattern : public View {
    public:
        explicit ViewPattern(prv::Provider* &dataProvider, std::vector<lang::PatternData*> &patternData);
        ~ViewPattern() override;

        void createMenu() override;
        void createView() override;

    private:
        char *m_buffer = nullptr;

        std::vector<lang::PatternData*> &m_patternData;
        prv::Provider* &m_dataProvider;
        bool m_windowOpen = true;

        imgui_addons::ImGuiFileBrowser m_fileBrowser;

        void clearPatternData();
        void parsePattern(char *buffer);
    };

}