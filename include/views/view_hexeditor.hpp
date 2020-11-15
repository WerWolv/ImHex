#pragma once

#include "utils.hpp"
#include "views/view.hpp"

#include "imgui_memory_editor.h"
#include "imfilebrowser.h"

#include <tuple>
#include <random>
#include <vector>

#include "views/pattern_data.hpp"

namespace hex {

    namespace prv { class Provider; }

    using SearchFunction = std::vector<std::pair<u64, u64>> (*)(prv::Provider* &provider, std::string string);

    class ViewHexEditor : public View {
    public:
        ViewHexEditor(prv::Provider* &dataProvider, std::vector<hex::PatternData*> &patternData);
        ~ViewHexEditor() override;

        void createView() override;
        void createMenu() override;
        bool handleShortcut(int key, int mods) override;

    private:
        MemoryEditor m_memoryEditor;

        ImGui::FileBrowser m_fileBrowser;


        prv::Provider* &m_dataProvider;
        std::vector<hex::PatternData*> &m_patternData;

        char m_searchStringBuffer[0xFFFF] = { 0 };
        char m_searchHexBuffer[0xFFFF] = { 0 };
        SearchFunction m_searchFunction = nullptr;
        std::vector<std::pair<u64, u64>> *m_lastSearchBuffer;

        s64 m_lastSearchIndex = 0;
        std::vector<std::pair<u64, u64>> m_lastStringSearch;
        std::vector<std::pair<u64, u64>> m_lastHexSearch;

        u64 m_gotoAddress = 0;


        void drawSearchPopup();
        void drawGotoPopup();

    };

}