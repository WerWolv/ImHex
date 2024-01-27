#pragma once

#include "content/views/view_hex_editor.hpp"
#include "hex/api/task_manager.hpp"

#include <atomic>
#include <optional>
#include <string>

namespace hex::plugin::builtin {

    class PopupFind : public ViewHexEditor::Popup {
    public:
        explicit PopupFind(ViewHexEditor *editor) noexcept;
        ~PopupFind() override;
        void draw(ViewHexEditor *editor) override;

    private:
        void drawSearchDirectionButtons();
        void drawTabContents();

        std::optional<Region> findByteSequence(const std::vector<u8> &sequence);

        std::string m_inputString;
        std::vector<u8> m_searchByteSequence;
        std::optional<Region> m_foundRegion = std::nullopt;

        bool m_requestFocus = true;
        std::atomic<bool> m_requestSearch = false;
        std::atomic<bool> m_searchBackwards = false;
        std::atomic<bool> m_reachedEnd = false;

        enum class Encoding {
            UTF8,
            UTF16,
            UTF32
        };

        enum class Endianness {
            Little,
            Big
        };

        enum class SearchMode : bool {
            ByteSequence,
            String
        };

        std::atomic<Encoding> m_stringEncoding = Encoding::UTF8;
        std::atomic<Endianness> m_stringEndianness = Endianness::Little;
        std::atomic<SearchMode> m_searchMode = SearchMode::ByteSequence;

        TaskHolder m_searchTask;

        void processInputString();
    };

} // namespace hex::plugin::builtin