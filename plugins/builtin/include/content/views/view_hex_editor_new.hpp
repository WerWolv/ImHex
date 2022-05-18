#pragma once

#include <hex/api/content_registry.hpp>

#include <hex/ui/view.hpp>
#include <hex/helpers/concepts.hpp>

#include <algorithm>
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
        void drawCell(u64 address, u8 *data, size_t size, bool hovered);

        void setSelection(hex::integral auto start, hex::integral auto end) {
            if (!ImHexApi::Provider::isValid()) return;

            const size_t maxAddress = ImHexApi::Provider::get()->getSize() - 1;

            if (this->m_selectionStart != start || this->m_selectionEnd != end)
                this->m_selectionChanged = true;

            this->m_selectionStart = std::clamp<decltype(start)>(start, 0, maxAddress);
            this->m_selectionEnd = std::clamp<decltype(end)>(end, 0, maxAddress);
        }

        void jumpToSelection() {
            this->m_shouldScrollToSelection = true;
        }

        u16 m_bytesPerRow = 16;

        ContentRegistry::HexEditor::DataVisualizer *m_currDataVisualizer;

        bool m_shouldScrollToSelection = false;
        bool m_selectionChanged = false;
        u64 m_selectionStart = InvalidSelection;
        u64 m_selectionEnd = InvalidSelection;

        u16 m_visibleRowCount = 0;

        std::optional<u64> m_editingAddress;
        bool m_shouldModifyValue = false;
        std::vector<u8> m_editingBytes;

        u8 m_highlightAlpha = 0x80;
        bool m_upperCaseHex = true;
        bool m_grayOutZero = true;
        bool m_showAscii = true;
    };

}