#pragma once

#include <hex/ui/view.hpp>
#include <hex/helpers/encoding_file.hpp>

#include <imgui_memory_editor.h>

#include <list>
#include <tuple>
#include <random>
#include <vector>

namespace hex::prv {
    class Provider;
}

namespace hex::plugin::builtin {

    using SearchFunction = std::vector<std::pair<u64, u64>> (*)(prv::Provider *&provider, std::string string);

    struct HighlightBlock {
        struct Highlight {
            color_t color;
            std::vector<std::string> tooltips;
        };

        constexpr static size_t Size = 0x2000;

        u64 base;
        std::array<Highlight, Size> highlight;
    };

    class ViewHexEditor : public View {
    public:
        ViewHexEditor();
        ~ViewHexEditor() override;

        void drawContent() override;
        void drawAlwaysVisible() override;

    private:
        MemoryEditor m_memoryEditor;

        std::vector<char> m_searchStringBuffer;
        std::vector<char> m_searchHexBuffer;
        SearchFunction m_searchFunction = nullptr;
        std::vector<std::pair<u64, u64>> *m_lastSearchBuffer;
        bool m_searchRequested = false;

        i64 m_lastSearchIndex = 0;
        std::vector<std::pair<u64, u64>> m_lastStringSearch;
        std::vector<std::pair<u64, u64>> m_lastHexSearch;

        u64 m_gotoAddressAbsolute = 0;
        i64 m_gotoAddressRelative = 0;
        bool m_gotoRequested = false;

        char m_baseAddressBuffer[0x20] = { 0 };
        u64 m_resizeSize               = 0;

        std::vector<u8> m_dataToSave;
        std::set<pl::PatternData *> m_highlightedPatterns;

        std::string m_loaderScriptScriptPath;
        std::string m_loaderScriptFilePath;

        hex::EncodingFile m_currEncodingFile;
        u8 m_highlightAlpha = 0x80;

        std::list<HighlightBlock> m_highlights;

        bool m_processingImportExport  = false;
        bool m_advancedDecodingEnabled = false;

        void drawSearchPopup();
        void drawGotoPopup();
        void drawEditPopup();

        void openFile(const fs::path &path);

        void copyBytes() const;
        void pasteBytes() const;
        void copyString() const;

        void registerEvents();
        void registerShortcuts();
        void registerMenuItems();
    };

}