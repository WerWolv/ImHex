#pragma once

#include <hex/helpers/utils.hpp>
#include <hex/views/view.hpp>
#include "helpers/encoding_file.hpp"

#include <imgui_memory_editor.h>

#include <list>
#include <tuple>
#include <random>
#include <vector>

#include <hex/lang/pattern_data.hpp>

namespace hex {

    namespace prv { class Provider; }

    using SearchFunction = std::vector<std::pair<u64, u64>> (*)(prv::Provider* &provider, std::string string);

    class ViewHexEditor : public View {
    public:
        ViewHexEditor(std::vector<lang::PatternData*> &patternData);
        ~ViewHexEditor() override;

        void drawContent() override;
        void drawAlwaysVisible() override;
        void drawMenu() override;
        bool handleShortcut(int key, int mods) override;

    private:
        MemoryEditor m_memoryEditor;

        std::vector<lang::PatternData*> &m_patternData;

        std::map<u64, u32> m_highlightedBytes;

        std::vector<char> m_searchStringBuffer;
        std::vector<char> m_searchHexBuffer;
        SearchFunction m_searchFunction = nullptr;
        std::vector<std::pair<u64, u64>> *m_lastSearchBuffer;

        s64 m_lastSearchIndex = 0;
        std::vector<std::pair<u64, u64>> m_lastStringSearch;
        std::vector<std::pair<u64, u64>> m_lastHexSearch;

        s64 m_gotoAddress = 0;

        char m_baseAddressBuffer[0x20] = { 0 };

        std::vector<u8> m_dataToSave;

        std::string m_loaderScriptScriptPath;
        std::string m_loaderScriptFilePath;

        hex::EncodingFile m_currEncodingFile;

        void drawSearchPopup();
        void drawGotoPopup();
        void drawEditPopup();

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