#pragma once

#include <hex.hpp>
#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/providers/provider.hpp>
#include <hex/helpers/encoding_file.hpp>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include <hex/ui/imgui_imhex_extensions.h>
#include <fonts/codicons_font.h>
#include <content/helpers/math_evaluator.hpp>
#include <hex/ui/view.hpp>
#include <hex/helpers/crypto.hpp>
#include <hex/providers/provider.hpp>
#include <hex/providers/buffered_reader.hpp>

namespace hex::plugin::builtin::ui {

    class HexEditor {
    public:
        explicit HexEditor(prv::Provider *provider = nullptr);
        ~HexEditor();
        void draw(float height = ImGui::GetContentRegionAvail().y);

        void setProvider(prv::Provider *provider) {
            this->m_provider = provider;
            this->m_currValidRegion = { Region::Invalid(), false };
        }
        void setUnknownDataCharacter(char character) { this->m_unknownDataCharacter = character; }
    private:
        enum class CellType { None, Hex, ASCII };

        void drawCell(u64 address, u8 *data, size_t size, bool hovered, CellType cellType);
        void drawSelectionFrame(u32 x, u32 y, u64 byteAddress, u16 bytesPerCell, const ImVec2 &cellPos, const ImVec2 &cellSize) const;
        void drawEditor(const ImVec2 &size);
        void drawFooter(const ImVec2 &size);
        void drawTooltip(u64 address, const u8 *data, size_t size);

        void handleSelection(u64 address, u32 bytesPerCell, const u8 *data, bool cellHovered);
        std::optional<color_t> applySelectionColor(u64 byteAddress, std::optional<color_t> color);

    public:
        void setSelectionUnchecked(std::optional<u64> start, std::optional<u64> end) {
            this->m_selectionStart = start;
            this->m_selectionEnd = end;
            this->m_cursorPosition = end;
        }
        void setSelection(const Region &region) { this->setSelection(region.getStartAddress(), region.getEndAddress()); }
        void setSelection(u128 start, u128 end) {
            if (!ImHexApi::Provider::isValid())
                return;

            const size_t maxAddress = this->m_provider->getActualSize() + this->m_provider->getBaseAddress() - 1;

            this->m_selectionChanged = this->m_selectionStart != start || this->m_selectionEnd != end;

            this->m_selectionStart = std::clamp<u128>(start, 0, maxAddress);
            this->m_selectionEnd = std::clamp<u128>(end, 0, maxAddress);
            this->m_cursorPosition = this->m_selectionEnd;

            if (this->m_selectionChanged) {
                auto selection = this->getSelection();
                EventManager::post<EventRegionSelected>(ImHexApi::HexEditor::ProviderRegion{ { selection.address, selection.size }, this->m_provider });
                this->m_shouldModifyValue = true;
            }
        }

        [[nodiscard]] Region getSelection() const {
            if (!isSelectionValid())
                return Region::Invalid();

            const auto start = std::min(this->m_selectionStart.value(), this->m_selectionEnd.value());
            const auto end   = std::max(this->m_selectionStart.value(), this->m_selectionEnd.value());
            const size_t size = end - start + 1;

            return { start, size };
        }

        [[nodiscard]] std::optional<u64> getCursorPosition() const {
            return this->m_cursorPosition;
        }

        void setCursorPosition(u64 cursorPosition) {
            this->m_cursorPosition = cursorPosition;
        }

        [[nodiscard]] bool isSelectionValid() const {
            return this->m_selectionStart.has_value() && this->m_selectionEnd.has_value();
        }

        void jumpToSelection(bool center = true) {
            this->m_shouldJumpToSelection = true;

            if (center)
                this->m_centerOnJump = true;
        }

        void scrollToSelection() {
            this->m_shouldScrollToSelection = true;
        }

        void jumpIfOffScreen() {
            this->m_shouldJumpWhenOffScreen = true;
        }

        [[nodiscard]] u16 getBytesPerRow() const {
            return this->m_bytesPerRow;
        }

        void setBytesPerRow(u16 bytesPerRow) {
            this->m_bytesPerRow = bytesPerRow;
        }

        [[nodiscard]] u16 getVisibleRowCount() const {
            return this->m_visibleRowCount;
        }

        void setSelectionColor(color_t color) {
            this->m_selectionColor = color;
        }

        void enableUpperCaseHex(bool upperCaseHex) {
            this->m_upperCaseHex = upperCaseHex;
        }

        void enableGrayOutZeros(bool grayOutZeros) {
            this->m_grayOutZero = grayOutZeros;
        }

        void enableShowAscii(bool showAscii) {
            this->m_showAscii = showAscii;
        }

        void enableSyncScrolling(bool syncScrolling) {
            this->m_syncScrolling = syncScrolling;
        }

        void setByteCellPadding(u32 byteCellPadding) {
            this->m_byteCellPadding = byteCellPadding;
        }

        void setCharacterCellPadding(u32 characterCellPadding) {
            this->m_characterCellPadding = characterCellPadding;
        }

        [[nodiscard]] const std::optional<EncodingFile>& getCustomEncoding() const {
            return this->m_currCustomEncoding;
        }

        void setCustomEncoding(EncodingFile encoding) {
            this->m_currCustomEncoding = std::move(encoding);
            this->m_encodingLineStartAddresses.clear();
        }

        void forceUpdateScrollPosition() {
            this->m_shouldUpdateScrollPosition = true;
        }

        void setForegroundHighlightCallback(const std::function<std::optional<color_t>(u64, const u8 *, size_t)> &callback) {
            this->m_foregroundColorCallback = callback;
        }

        void setBackgroundHighlightCallback(const std::function<std::optional<color_t>(u64, const u8 *, size_t)> &callback) {
            this->m_backgroundColorCallback = callback;
        }

        void setTooltipCallback(const std::function<void(u64, const u8 *, size_t)> &callback) {
            this->m_tooltipCallback = callback;
        }

        [[nodiscard]] float getScrollPosition() const {
            return this->m_scrollPosition;
        }

        void setScrollPosition(float scrollPosition) {
            this->m_scrollPosition = scrollPosition;
        }

    private:
        prv::Provider *m_provider;

        std::optional<u64> m_selectionStart;
        std::optional<u64> m_selectionEnd;
        std::optional<u64> m_cursorPosition;
        float m_scrollPosition = 0;

        u16 m_bytesPerRow = 16;
        ContentRegistry::HexEditor::DataVisualizer *m_currDataVisualizer;
        u32 m_grayZeroHighlighter = 0;
        char m_unknownDataCharacter = '?';

        bool m_shouldJumpToSelection = false;
        bool m_centerOnJump = false;
        bool m_shouldScrollToSelection = false;
        bool m_shouldJumpWhenOffScreen = false;
        bool m_shouldUpdateScrollPosition = false;

        bool m_selectionChanged = false;

        u16 m_visibleRowCount = 0;

        CellType m_editingCellType = CellType::None;
        std::optional<u64> m_editingAddress;
        bool m_shouldModifyValue = false;
        bool m_enteredEditingMode = false;
        bool m_shouldUpdateEditingValue = false;
        std::vector<u8> m_editingBytes;

        color_t m_selectionColor = 0x00;
        bool m_upperCaseHex = true;
        bool m_grayOutZero = true;
        bool m_showAscii = true;
        bool m_showCustomEncoding = true;
        bool m_syncScrolling = false;
        u32 m_byteCellPadding = 0, m_characterCellPadding = 0;

        std::optional<EncodingFile> m_currCustomEncoding;
        std::vector<u64> m_encodingLineStartAddresses;

        std::pair<Region, bool> m_currValidRegion = { Region::Invalid(), false };

        static inline std::optional<color_t> defaultColorCallback(u64, const u8 *, size_t) { return std::nullopt; }
        static inline void defaultTooltipCallback(u64, const u8 *, size_t) {  }
        std::function<std::optional<color_t>(u64, const u8 *, size_t)> m_foregroundColorCallback = defaultColorCallback, m_backgroundColorCallback = defaultColorCallback;
        std::function<void(u64, const u8 *, size_t)> m_tooltipCallback = defaultTooltipCallback;
    };

}