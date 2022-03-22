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

        u64 base = 0x00;
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
        SearchFunction m_searchFunction                      = nullptr;
        std::vector<std::pair<u64, u64>> *m_lastSearchBuffer = nullptr;
        bool m_searchRequested                               = false;

        i64 m_lastSearchIndex = 0;
        std::vector<std::pair<u64, u64>> m_lastStringSearch;
        std::vector<std::pair<u64, u64>> m_lastHexSearch;

        std::string m_gotoAddressInput;
        bool m_gotoRequested = false;
        bool m_evaluateGoto = false;

        u64 m_baseAddress = 0;
        u64 m_resizeSize  = 0;

        std::vector<u8> m_dataToSave;
        std::set<pl::Pattern *> m_highlightedPatterns;

        std::string m_loaderScriptScriptPath;
        std::string m_loaderScriptFilePath;

        hex::EncodingFile m_currEncodingFile;
        u8 m_highlightAlpha = 0x80;

        std::list<HighlightBlock> m_highlights;

        bool m_processingImportExport  = false;
        bool m_advancedDecodingEnabled = false;

        void drawSearchPopup();
        void drawSearchInput(std::vector<char> *currBuffer, ImGuiInputTextFlags flags);
        void performSearch(const char *buffer);
        void performSearchNext();
        void performSearchPrevious();
        static int inputCallback(ImGuiInputTextCallbackData *data);

        void drawGotoPopup();
        void drawEditPopup();

        void openFile(const std::fs::path &path);

        void copyBytes() const;
        void pasteBytes() const;
        void copyString() const;

        void registerEvents();
        void registerShortcuts();
        void registerMenuItems();
    };

}