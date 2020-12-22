#pragma once

#include "helpers/utils.hpp"
#include "views/view.hpp"

#include "imgui_memory_editor.h"
#include "ImGuiFileBrowser.h"

#include <tuple>
#include <random>
#include <vector>

#include "lang/pattern_data.hpp"

namespace hex {

    namespace prv { class Provider; }

    using SearchFunction = std::vector<std::pair<u64, u64>> (*)(prv::Provider* &provider, std::string string);

    class ViewHexEditor : public View {
    public:
        ViewHexEditor(prv::Provider* &dataProvider, std::vector<lang::PatternData*> &patternData);
        ~ViewHexEditor() override;

        void drawContent() override;
        void drawMenu() override;
        bool handleShortcut(int key, int mods) override;

    private:
        MemoryEditor m_memoryEditor;
        imgui_addons::ImGuiFileBrowser m_fileBrowser;

        prv::Provider* &m_dataProvider;
        std::vector<lang::PatternData*> &m_patternData;

        char m_searchStringBuffer[0xFFFF] = { 0 };
        char m_searchHexBuffer[0xFFFF] = { 0 };
        SearchFunction m_searchFunction = nullptr;
        std::vector<std::pair<u64, u64>> *m_lastSearchBuffer;

        s64 m_lastSearchIndex = 0;
        std::vector<std::pair<u64, u64>> m_lastStringSearch;
        std::vector<std::pair<u64, u64>> m_lastHexSearch;

        s64 m_gotoAddress = 0;

        std::vector<u8> m_dataToSave;

        std::string m_loaderScriptScriptPath;
        std::string m_loaderScriptFilePath;

        void drawSearchPopup();
        void drawGotoPopup();

        void openFile(std::string path);
        bool saveToFile(std::string path, const std::vector<u8>& data);
        bool loadFromFile(std::string path, std::vector<u8>& data);

        enum class Language { C, Cpp, CSharp, Rust, Python, Java, JavaScript };
        void copyBytes();
        void copyString();
        void copyLanguageArray(Language language);
        void copyHexView();
        void copyHexViewHTML();

    };

}