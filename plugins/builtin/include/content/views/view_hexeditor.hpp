#pragma once

#include <hex/views/view.hpp>
#include <hex/helpers/encoding_file.hpp>

#include <imgui_memory_editor.h>

#include <list>
#include <tuple>
#include <random>
#include <vector>

namespace hex::prv { class Provider; }


namespace hex::plugin::builtin {


    using SearchFunction = std::vector<std::pair<u64, u64>> (*)(prv::Provider* &provider, std::string string);

    class ViewHexEditor : public View {
    public:
        ViewHexEditor();
        ~ViewHexEditor() override;

        void drawContent() override;
        void drawAlwaysVisible() override;
        void drawMenu() override;

    private:
        MemoryEditor m_memoryEditor;

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
        u64 m_resizeSize = 0;

        std::vector<u8> m_dataToSave;

        std::string m_loaderScriptScriptPath;
        std::string m_loaderScriptFilePath;

        hex::EncodingFile m_currEncodingFile;
        u8 m_highlightAlpha = 0x80;

        bool m_processingImportExport = false;

        void drawSearchPopup();
        void drawGotoPopup();
        void drawEditPopup();

        bool createFile(const std::string &path);
        void openFile(const std::string &path);
        bool saveToFile(const std::string &path, const std::vector<u8>& data);
        bool loadFromFile(const std::string &path, std::vector<u8>& data);

        enum class Language { C, Cpp, CSharp, Rust, Python, Java, JavaScript };
        void copyBytes() const;
        void pasteBytes() const;
        void copyString() const;
        void copyLanguageArray(Language language) const;
        void copyHexView() const;
        void copyHexViewHTML() const;

        void registerEvents();
        void registerShortcuts();
    };

}