#pragma once

#include <hex.hpp>
#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/providers/provider.hpp>
#include <hex/helpers/encoding_file.hpp>

#include <hex/api/events/events_interaction.hpp>

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
            if (m_synced || m_provider == nullptr)
                return m_syncedPosition;
            else
                return m_unsyncedPosition.get(m_provider);
        }

        [[nodiscard]] const ImS64& get() const {
            if (m_synced || m_provider == nullptr)
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
        void draw(float height = ImGui::GetContentRegionAvail().y);

        HexEditor(const HexEditor&) = default;
        HexEditor& operator=(const HexEditor&) = default;

        HexEditor(HexEditor &&editor) noexcept = default;
        HexEditor& operator=(HexEditor &&) noexcept = default;

        void setProvider(prv::Provider *provider) {
            m_provider = provider;
            m_currValidRegion = { Region::Invalid(), false };
            m_scrollPosition.setProvider(provider);
        }

        [[nodiscard]] prv::Provider* getProvider() const {
            return m_provider;
        }

        void setUnknownDataCharacter(char character) { m_unknownDataCharacter = character; }
    private:
        enum class CellType : u8 { None, Hex, ASCII };
        enum class AddressFormat : u8 { Hexadecimal, Decimal, Octal };

        void drawCell(u64 address, u8 *data, size_t size, bool hovered, CellType cellType);
        void drawSeparatorLine(u64 address, bool drawVerticalConnector);
        void drawFrame(u32 x, u32 y, Region region, u64 byteAddress, u16 bytesPerCell, const ImVec2 &cellPos, const ImVec2 &cellSize, const ImColor &frameColor) const;
        void drawInsertCursor(Region region, u64 byteAddress, const ImVec2 &cellPos, const ImVec2 &cellSize, const ImColor &frameColor) const;
        void drawBackgroundHighlight(const ImVec2 &cellPos, const ImVec2 &cellSize, const ImColor &backgroundColor) const;
        void drawSelection(u32 x, u32 y, Region region, u64 byteAddress, u16 bytesPerCell, const ImVec2 &cellPos, const ImVec2 &cellSize, const ImColor &frameColor) const;
        void drawEditor(const ImVec2 &size);
        void drawFooter(const ImVec2 &size);
        void drawTooltip(u64 address, const u8 *data, size_t size) const;
        void drawScrollbar(ImVec2 characterSize);
        void drawMinimap(ImVec2 characterSize);

        void handleSelection(u64 address, u32 bytesPerCell, const u8 *data, bool cellHovered);
        std::optional<color_t> applySelectionColor(u64 byteAddress, std::optional<color_t> color);

        std::string formatAddress(u64 address, u32 width = 2, bool prefix = false) const;

    public:
        void setSelectionUnchecked(std::optional<u64> start, std::optional<u64> end) {
            m_selectionStart = start;
            m_selectionEnd = end;
            m_cursorPosition = end;
        }
        void setSelection(const Region &region) { this->setSelection(region.getStartAddress(), region.getEndAddress()); }
        void setSelection(u64 start, u64 end) {
            if (!ImHexApi::Provider::isValid() || m_provider == nullptr)
                return;

            if (start > m_provider->getBaseAddress() + m_provider->getActualSize())
                return;

            if (start < m_provider->getBaseAddress())
                return;

            if (m_provider->getActualSize() == 0)
                return;

            const size_t maxAddress = m_provider->getActualSize() + m_provider->getBaseAddress() - 1;

            constexpr static auto alignDown = [](u64 value, u64 alignment) {
                return value & ~(alignment - 1);
            };

            m_selectionChanged = m_selectionStart != start || m_selectionEnd != end;

            if (!m_selectionStart.has_value()) m_selectionStart = start;
            if (!m_selectionEnd.has_value())   m_selectionEnd = end;

            if (auto bytesPerCell = m_currDataVisualizer == nullptr ? 1 : m_currDataVisualizer->getBytesPerCell(); bytesPerCell > 1) {
                if (end > start) {
                    start = alignDown(start, bytesPerCell);
                    end   = alignDown(end, bytesPerCell) + (bytesPerCell - 1);
                } else {
                    start = alignDown(start, bytesPerCell) + (bytesPerCell - 1);
                    end   = alignDown(end, bytesPerCell);
                }
            }

            m_selectionStart = std::clamp<u64>(start, 0, maxAddress);
            m_selectionEnd = std::clamp<u64>(end, 0, maxAddress);
            m_cursorPosition = m_selectionStart;

            if (m_selectionChanged) {
                auto selection = this->getSelection();
                EventRegionSelected::post(ImHexApi::HexEditor::ProviderRegion{ { selection.address, selection.size }, m_provider });
                m_shouldModifyValue = true;
            }

            if (m_mode == Mode::Insert) {
                m_selectionStart = m_selectionEnd;
                m_cursorBlinkTimer = -0.3F;
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

        void jumpToSelection(float pivot = 0.0F) {
            m_shouldJumpToSelection = true;
            m_jumpPivot = pivot;
        }

        void scrollToSelection() {
            m_shouldScrollToSelection = true;
        }

        void jumpIfOffScreen() {
            m_shouldScrollToSelection = true;
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

        void setHoverChangedCallback(const std::function<void(u64, size_t)> &callback) {
            m_hoverChangedCallback = callback;
        }

        void setTooltipCallback(const std::function<void(u64, const u8 *, size_t)> &callback) {
            m_tooltipCallback = callback;
        }

        void setShowSelectionInFooter(bool showSelection) {
            m_showSelectionInFooter = showSelection;
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

        enum class Mode : u8 {
            Overwrite,
            Insert
        };

        void setMode(Mode mode) {
            if (mode == Mode::Insert) {
                // Don't enter insert mode if the provider doesn't support resizing the underlying data
                if (!m_provider->isResizable())
                    return;

                // Get rid of any selection in insert mode
                m_selectionStart = m_selectionEnd;
                m_cursorPosition = m_selectionEnd;
                m_selectionChanged = true;
            }

            m_mode = mode;
        }

        [[nodiscard]] Mode getMode() const {
            return m_mode;
        }

    private:
        prv::Provider *m_provider = nullptr;

        std::optional<u64> m_selectionStart;
        std::optional<u64> m_selectionEnd;
        std::optional<u64> m_cursorPosition;
        ScrollPosition m_scrollPosition;

        Region m_frameStartSelectionRegion = Region::Invalid();
        Region m_hoveredRegion = Region::Invalid();

        u16 m_bytesPerRow = 16;
        std::endian m_dataVisualizerEndianness = std::endian::little;
        std::shared_ptr<ContentRegistry::HexEditor::DataVisualizer> m_currDataVisualizer;
        char m_unknownDataCharacter = '?';
        u64 m_separatorStride = 0;

        bool m_shouldJumpToSelection = false;
        float m_jumpPivot = 0.0F;
        bool m_shouldScrollToSelection = false;
        bool m_shouldJumpWhenOffScreen = false;
        bool m_shouldUpdateScrollPosition = false;

        bool m_selectionChanged = false;

        u16 m_visibleRowCount = 0;

        CellType m_editingCellType = CellType::None;
        AddressFormat m_addressFormat = AddressFormat::Hexadecimal;
        std::optional<u64> m_editingAddress;
        bool m_shouldModifyValue = false;
        bool m_enteredEditingMode = false;
        bool m_shouldUpdateEditingValue = false;
        std::vector<u8> m_editingBytes;
        u32 m_maxFittingColumns = 16;
        bool m_autoFitColumns = false;

        std::shared_ptr<ContentRegistry::HexEditor::MiniMapVisualizer> m_miniMapVisualizer;

        color_t m_selectionColor = 0x60C08080;
        bool m_upperCaseHex = true;
        bool m_grayOutZero = true;
        bool m_showAscii = true;
        bool m_showCustomEncoding = true;
        bool m_showMiniMap = false;
        bool m_showSelectionInFooter = false;
        int m_miniMapWidth = 5;
        u32 m_byteCellPadding = 0, m_characterCellPadding = 0;
        bool m_footerCollapsed = true;

        std::optional<EncodingFile> m_currCustomEncoding;
        std::vector<u64> m_encodingLineStartAddresses;

        std::pair<Region, bool> m_currValidRegion = { Region::Invalid(), false };

        static std::optional<color_t> defaultColorCallback(u64, const u8 *, size_t) { return std::nullopt; }
        static void defaultTooltipCallback(u64, const u8 *, size_t) {  }
        std::function<std::optional<color_t>(u64, const u8 *, size_t)> m_foregroundColorCallback = defaultColorCallback, m_backgroundColorCallback = defaultColorCallback;
        std::function<void(u64, size_t)> m_hoverChangedCallback = [](auto, auto){ };
        std::function<void(u64, const u8 *, size_t)> m_tooltipCallback = defaultTooltipCallback;

        Mode m_mode = Mode::Overwrite;
        float m_cursorBlinkTimer = -0.3F;
    };

}
