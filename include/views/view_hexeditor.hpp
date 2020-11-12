#pragma once

#include "utils.hpp"
#include "views/view.hpp"

#include "imgui_memory_editor.h"
#include "imfilebrowser.h"

#include <tuple>
#include <random>
#include <vector>

#include "views/highlight.hpp"

namespace hex {

    namespace prv { class Provider; }

    class ViewHexEditor : public View {
    public:
        ViewHexEditor(prv::Provider* &dataProvider, std::vector<Highlight> &highlights);
        ~ViewHexEditor() override;

        void createView() override;
        void createMenu() override;
        bool handleShortcut(int key, int mods) override;

    private:
        MemoryEditor m_memoryEditor;

        ImGui::FileBrowser m_fileBrowser;


        prv::Provider* &m_dataProvider;
        std::vector<Highlight> &m_highlights;

        char m_searchBuffer[0xFFFF] = { 0 };
        s64 m_lastSearchIndex = 0;
        std::vector<std::pair<u64, u64>> m_lastSearch;
        u64 m_gotoAddress = 0;


        void drawSearchPopup();
        void drawGotoPopup();
    };

}