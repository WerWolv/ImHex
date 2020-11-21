#pragma once

#include "lang/ast_node.hpp"

#include "views/view.hpp"
#include "lang/pattern_data.hpp"

#include "providers/provider.hpp"

#include <concepts>
#include <cstring>
#include <filesystem>

#include "ImGuiFileBrowser.h"
#include "TextEditor.h"

namespace hex {

    class ViewPattern : public View {
    public:
        explicit ViewPattern(prv::Provider* &dataProvider, std::vector<lang::PatternData*> &patternData);
        ~ViewPattern() override;

        void createMenu() override;
        void createView() override;

    private:
        std::vector<lang::PatternData*> &m_patternData;
        prv::Provider* &m_dataProvider;
        std::filesystem::path m_possiblePatternFile;
        bool m_windowOpen = true;

        TextEditor m_textEditor;
        imgui_addons::ImGuiFileBrowser m_fileBrowser;

        void loadPatternFile(std::string path);
        void clearPatternData();
        void parsePattern(char *buffer);
    };

}