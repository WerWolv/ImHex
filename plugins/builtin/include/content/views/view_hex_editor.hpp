#pragma once

#include <hex/api/content_registry.hpp>

#include <hex/ui/view.hpp>
#include <hex/helpers/concepts.hpp>
#include <hex/helpers/encoding_file.hpp>

#include <algorithm>
#include <limits>

namespace hex::plugin::builtin {

    class ViewHexEditor : public View {
    public:
        ViewHexEditor();

        void drawContent() override;
        void drawAlwaysVisible() override;

    private:
        constexpr static auto InvalidSelection = std::numeric_limits<u64>::max();

        void openFile(const std::fs::path &path);
        void registerShortcuts();
        void registerEvents();
        void registerMenuItems();

        void drawCell(u64 address, u8 *data, size_t size, bool hovered);
        void drawPopup();
        void drawSelectionFrame(u32 x, u32 y, u64 byteAddress, u16 bytesPerCell, const ImVec2 &cellPos, const ImVec2 &cellSize);

    public:
        void setSelection(const Region &region) { this->setSelection(region.getStartAddress(), region.getEndAddress()); }
        void setSelection(i128 start, i128 end) {
            if (!ImHexApi::Provider::isValid()) return;

            const size_t maxAddress = ImHexApi::Provider::get()->getActualSize() - 1;

            this->m_selectionChanged = this->m_selectionStart != start || this->m_selectionEnd != end;

            this->m_selectionStart = std::clamp<decltype(start)>(start, 0, maxAddress);
            this->m_selectionEnd = std::clamp<decltype(end)>(end, 0, maxAddress);

            if (this->m_selectionChanged) {
                EventManager::post<EventRegionSelected>(this->getSelection());
            }
        }

        [[nodiscard]] Region getSelection() const {
            const auto start = std::min(this->m_selectionStart, this->m_selectionEnd);
            const auto end   = std::max(this->m_selectionStart, this->m_selectionEnd);
            const size_t size = end - start + 1;

            return { start, size };
        }

        [[nodiscard]] bool isSelectionValid() const {
            return this->m_selectionStart != InvalidSelection && this->m_selectionEnd != InvalidSelection;
        }

        void jumpToSelection() {
            this->m_shouldJumpToSelection = true;
        }

        void scrollToSelection() {
            this->m_shouldScrollToSelection = true;
        }

    public:
        class Popup {
        public:
            virtual ~Popup() = default;
            virtual void draw(ViewHexEditor *editor) = 0;
        };

        [[nodiscard]] bool isAnyPopupOpen() const {
            return this->m_currPopup != nullptr;
        }

        template<std::derived_from<Popup> T>
        [[nodiscard]] bool isPopupOpen() const {
            return dynamic_cast<T*>(this->m_currPopup.get()) != nullptr;
        }

        template<std::derived_from<Popup> T, typename ... Args>
        void openPopup(Args && ...args) {
            this->m_currPopup = std::make_unique<T>(std::forward<Args>(args)...);
            this->m_shouldOpenPopup = true;
        }

        void closePopup() {
            this->m_currPopup.reset();
        }

    private:
        void drawEditor(const ImVec2 &size);
        void drawFooter(const ImVec2 &size);

        void handleSelection(u64 address, u32 bytesPerCell, const u8 *data, bool cellHovered);

    private:
        u16 m_bytesPerRow = 16;

        ContentRegistry::HexEditor::DataVisualizer *m_currDataVisualizer;

        bool m_shouldJumpToSelection = false;
        bool m_shouldScrollToSelection = false;

        bool m_selectionChanged = false;
        u64 m_selectionStart = InvalidSelection;
        u64 m_selectionEnd = InvalidSelection;

        u16 m_visibleRowCount = 0;

        std::optional<u64> m_editingAddress;
        bool m_shouldModifyValue = false;
        bool m_enteredEditingMode = false;
        std::vector<u8> m_editingBytes;

        u8 m_highlightAlpha = 0x80;
        bool m_upperCaseHex = true;
        bool m_grayOutZero = true;
        bool m_showAscii = true;

        bool m_shouldOpenPopup = false;
        std::unique_ptr<Popup> m_currPopup;

        std::optional<EncodingFile> m_currCustomEncoding;
    };

}