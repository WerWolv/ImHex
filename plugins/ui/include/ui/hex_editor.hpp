#pragma once

#include <hex.hpp>
#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/providers/provider.hpp>
#include <hex/helpers/encoding_file.hpp>

#include <imgui.h>
#include <hex/ui/view.hpp>

namespace hex::ui {

    class ScrollPosition {
    public:
        ScrollPosition() = default;

        // We explicitly don't assign any data during copy and move operations so that each instance of the
        // Hex Editor will get its own independent scroll position
        ScrollPosition(const ScrollPosition&) { }
        ScrollPosition(ScrollPosition&&) noexcept { }
        ScrollPosition& operator=(const ScrollPosition&) { return *this; }
        ScrollPosition& operator=(ScrollPosition&&) noexcept { return *this; }


        void setSynced(bool synced) {
            m_synced = synced;
        }

        void setProvider(prv::Provider *provider) {
            m_provider = provider;
        }

        ImS64& get() {
            if (m_synced)
                return m_syncedPosition;
            else
                return m_unsyncedPosition.get(m_provider);
        }

        const ImS64& get() const {
            if (m_synced)
                return m_syncedPosition;
            else
                return m_unsyncedPosition.get(m_provider);
        }

        operator ImS64&() {
            return this->get();
        }

        operator const ImS64&() const {
            return this->get();
        }

        ScrollPosition& operator=(ImS64 value) {
            this->get() = value;
            return *this;
        }

        auto operator<=>(const ScrollPosition &other) const {
            return this->get() <=> other.get();
        }

    private:
        bool m_synced = false;
        prv::Provider *m_provider = nullptr;

        ImS64 m_syncedPosition = 0;
        PerProvider<ImS64> m_unsyncedPosition;
    };

    class HexEditor {
    public:
        explicit HexEditor(prv::Provider *provider = nullptr);
        ~HexEditor();
        void draw(float height = ImGui::GetContentRegionAvail().y);

        void setProvider(prv::Provider *provider) {
            m_provider = provider;
            m_currValidRegion = { Region::Invalid(), false };
            m_scrollPosition.setProvider(provider);
        }
        void setUnknownDataCharacter(char character) { m_unknownDataCharacter = character; }
    private:
        enum class CellType { None, Hex, ASCII };

        void drawCell(u64 address, const u8 *data, size_t size, bool hovered, CellType cellType);
        void drawSelectionFrame(u32 x, u32 y, Region selection, u64 byteAddress, u16 bytesPerCell, const ImVec2 &cellPos, const ImVec2 &cellSize, const ImColor &backgroundColor) const;
        void drawEditor(const ImVec2 &size);
        void drawFooter(const ImVec2 &size);
        void drawTooltip(u64 address, const u8 *data, size_t size) const;

        void handleSelection(u64 address, u32 bytesPerCell, const u8 *data, bool cellHovered);
        std::optional<color_t> applySelectionColor(u64 byteAddress, std::optional<color_t> color);

    public:
        void setSelectionUnchecked(std::optional<u64> start, std::optional<u64> end) {
            m_selectionStart = start;
            m_selectionEnd = end;
            m_cursorPosition = end;
        }
        void setSelection(const Region &region) { this->setSelection(region.getStartAddress(), region.getEndAddress()); }
        void setSelection(u128 start, u128 end) {
            if (!ImHexApi::Provider::isValid())
                return;

            if (start > m_provider->getBaseAddress() + m_provider->getActualSize())
                return;

            if (start < m_provider->getBaseAddress())
                return;

            if (m_provider->getActualSize() == 0)
                return;

            const size_t maxAddress = m_provider->getActualSize() + m_provider->getBaseAddress() - 1;

            constexpr static auto alignDown = [](u128 value, u128 alignment) {
                return value & ~(alignment - 1);
            };

            m_selectionChanged = m_selectionStart != start || m_selectionEnd != end;

            if (!m_selectionStart.has_value()) m_selectionStart = start;
            if (!m_selectionEnd.has_value())   m_selectionEnd = end;

            if (auto bytesPerCell = m_currDataVisualizer->getBytesPerCell(); bytesPerCell > 1) {
                if (end > start) {
                    start = alignDown(start, bytesPerCell);
                    end   = alignDown(end, bytesPerCell) + (bytesPerCell - 1);
                } else {
                    start = alignDown(start, bytesPerCell) + (bytesPerCell - 1);
                    end   = alignDown(end, bytesPerCell);
                }
            }

            m_selectionStart = std::clamp<u128>(start, 0, maxAddress);
            m_selectionEnd = std::clamp<u128>(end, 0, maxAddress);
            m_cursorPosition = m_selectionEnd;

            if (m_selectionChanged) {
                auto selection = this->getSelection();
                EventRegionSelected::post(ImHexApi::HexEditor::ProviderRegion{ { selection.address, selection.size }, m_provider });
                m_shouldModifyValue = true;
            }
        }

        [[nodiscard]] Region getSelection() const {
            if (!isSelectionValid())
                return Region::Invalid();

            const auto start = std::min(m_selectionStart.value(), m_selectionEnd.value());
            const auto end   = std::max(m_selectionStart.value(), m_selectionEnd.value());
            const size_t size = end - start + 1;

            return { start, size };
        }

        [[nodiscard]] std::optional<u64> getCursorPosition() const {
            return m_cursorPosition;
        }

        void setCursorPosition(u64 cursorPosition) {
            m_cursorPosition = cursorPosition;
        }

        [[nodiscard]] bool isSelectionValid() const {
            return m_selectionStart.has_value() && m_selectionEnd.has_value();
        }

        void jumpToSelection(bool center = true) {
            m_shouldJumpToSelection = true;

            if (center)
                m_centerOnJump = true;
        }

        void scrollToSelection() {
            m_shouldScrollToSelection = true;
        }

        void jumpIfOffScreen() {
            m_shouldJumpWhenOffScreen = true;
        }

        [[nodiscard]] u16 getBytesPerRow() const {
            return m_bytesPerRow;
        }

        [[nodiscard]] u16 getBytesPerCell() const {
            return m_currDataVisualizer->getBytesPerCell();
        }

        void setBytesPerRow(u16 bytesPerRow) {
            m_bytesPerRow = bytesPerRow;
        }

        [[nodiscard]] u16 getVisibleRowCount() const {
            return m_visibleRowCount;
        }

        void setSelectionColor(color_t color) {
            m_selectionColor = color;
        }

        void enableUpperCaseHex(bool upperCaseHex) {
            m_upperCaseHex = upperCaseHex;
        }

        void enableGrayOutZeros(bool grayOutZeros) {
            m_grayOutZero = grayOutZeros;
        }

        void enableShowAscii(bool showAscii) {
            m_showAscii = showAscii;
        }

        void enableShowHumanReadableUnits(bool showHumanReadableUnits) {
            m_showHumanReadableUnits = showHumanReadableUnits;
        }

        void enableSyncScrolling(bool syncScrolling) {
            m_scrollPosition.setSynced(syncScrolling);
        }

        void setByteCellPadding(u32 byteCellPadding) {
            m_byteCellPadding = byteCellPadding;
        }

        void setCharacterCellPadding(u32 characterCellPadding) {
            m_characterCellPadding = characterCellPadding;
        }

        [[nodiscard]] const std::optional<EncodingFile>& getCustomEncoding() const {
            return m_currCustomEncoding;
        }

        void setCustomEncoding(const EncodingFile &encoding) {
            m_currCustomEncoding = encoding;
            m_encodingLineStartAddresses.clear();
        }

        void setCustomEncoding(EncodingFile &&encoding) {
            m_currCustomEncoding = std::move(encoding);
            m_encodingLineStartAddresses.clear();
        }

        void forceUpdateScrollPosition() {
            m_shouldUpdateScrollPosition = true;
        }

        void setForegroundHighlightCallback(const std::function<std::optional<color_t>(u64, const u8 *, size_t)> &callback) {
            m_foregroundColorCallback = callback;
        }

        void setBackgroundHighlightCallback(const std::function<std::optional<color_t>(u64, const u8 *, size_t)> &callback) {
            m_backgroundColorCallback = callback;
        }

        void setTooltipCallback(const std::function<void(u64, const u8 *, size_t)> &callback) {
            m_tooltipCallback = callback;
        }

        [[nodiscard]] i64 getScrollPosition() {
            return m_scrollPosition.get();
        }

        void setScrollPosition(i64 scrollPosition) {
            m_scrollPosition.get() = scrollPosition;
        }

        void setEditingAddress(u64 address) {
            m_editingAddress = address;
            m_shouldModifyValue = false;
            m_enteredEditingMode = true;

            m_editingBytes.resize(m_currDataVisualizer->getBytesPerCell());
            m_provider->read(address + m_provider->getBaseAddress(), m_editingBytes.data(), m_editingBytes.size());
            m_editingCellType = CellType::Hex;
        }

        void clearEditingAddress() {
            m_editingAddress = std::nullopt;
        }

    private:
        prv::Provider *m_provider;

        std::optional<u64> m_selectionStart;
        std::optional<u64> m_selectionEnd;
        std::optional<u64> m_cursorPosition;
        ScrollPosition m_scrollPosition;

        u16 m_bytesPerRow = 16;
        std::endian m_dataVisualizerEndianness = std::endian::little;
        std::shared_ptr<ContentRegistry::HexEditor::DataVisualizer> m_currDataVisualizer;
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

        color_t m_selectionColor = 0x60C08080;
        bool m_upperCaseHex = true;
        bool m_grayOutZero = true;
        bool m_showAscii = true;
        bool m_showCustomEncoding = true;
        bool m_showHumanReadableUnits = true;
        u32 m_byteCellPadding = 0, m_characterCellPadding = 0;
        bool m_footerCollapsed = true;

        std::optional<EncodingFile> m_currCustomEncoding;
        std::vector<u64> m_encodingLineStartAddresses;

        std::pair<Region, bool> m_currValidRegion = { Region::Invalid(), false };

        static std::optional<color_t> defaultColorCallback(u64, const u8 *, size_t) { return std::nullopt; }
        static void defaultTooltipCallback(u64, const u8 *, size_t) {  }
        std::function<std::optional<color_t>(u64, const u8 *, size_t)> m_foregroundColorCallback = defaultColorCallback, m_backgroundColorCallback = defaultColorCallback;
        std::function<void(u64, const u8 *, size_t)> m_tooltipCallback = defaultTooltipCallback;
    };

}
