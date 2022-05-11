#pragma once

#include <hex/api/content_registry.hpp>

#include <hex/ui/view.hpp>
#include <hex/helpers/concepts.hpp>

#include <limits>

namespace hex::plugin::builtin {

    class ViewHexEditorNew : public View {
    public:
        ViewHexEditorNew();

        void drawContent() override;
        void drawAlwaysVisible() override;

    private:
        constexpr static auto InvalidSelection = std::numeric_limits<u64>::max();

        void openFile(const std::fs::path &path);
        void drawCell(u64 address, const u8 *data, size_t size);

        void setSelection(hex::integral auto start, hex::integral auto end) {
            if (!ImHexApi::Provider::isValid()) return;

            start = std::max<decltype(start)>(0, start);
            end = std::max<decltype(end)>(0, end);

            if (start > end) std::swap(start, end);

            const size_t maxAddress = ImHexApi::Provider::get()->getSize() - 1;

            this->m_selectionStart = std::min<u64>(start, maxAddress);
            this->m_selectionEnd = std::min<u64>(end, maxAddress);
            this->m_selectionChanged = true;
        }

        u16 m_columnCount = 16;

        ContentRegistry::HexEditor::DataVisualizer *m_currDataVisualizer;

        bool m_selectionChanged = false;
        u64 m_selectionStart = InvalidSelection;
        u64 m_selectionEnd = InvalidSelection;

        u8 m_highlightAlpha = 0x80;
        bool m_upperCaseHex = true;
        bool m_grayOutZero = true;
        bool m_showAscii = true;
    };

}