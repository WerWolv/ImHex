#pragma once

#include <hex/api/content_registry.hpp>

#include <hex/ui/view.hpp>
#include <hex/helpers/concepts.hpp>
#include <hex/helpers/encoding_file.hpp>

#include <content/helpers/provider_extra_data.hpp>

#include <algorithm>
#include <limits>

namespace hex::plugin::builtin {

    class ViewHexEditor : public View {
    public:
        ViewHexEditor();

        void drawContent() override;

    private:
        void registerShortcuts();
        void registerEvents();
        void registerMenuItems();

        enum class CellType { None, Hex, ASCII };

        void drawCell(u64 address, u8 *data, size_t size, bool hovered, CellType cellType);
        void drawPopup();
        void drawSelectionFrame(u32 x, u32 y, u64 byteAddress, u16 bytesPerCell, const ImVec2 &cellPos, const ImVec2 &cellSize);

    public:
        void setSelection(const Region &region) { this->setSelection(region.getStartAddress(), region.getEndAddress()); }
        void setSelection(u128 start, u128 end) {
            if (!ImHexApi::Provider::isValid())
                return;

            auto provider = ImHexApi::Provider::get();
            auto &data = ProviderExtraData::get(provider).editor;

            const size_t maxAddress = provider->getActualSize() + provider->getBaseAddress() - 1;

            this->m_selectionChanged = data.selectionStart != start || data.selectionEnd != end;

            data.selectionStart = std::clamp<u128>(start, 0, maxAddress);
            data.selectionEnd = std::clamp<u128>(end, 0, maxAddress);

            if (this->m_selectionChanged) {
                EventManager::post<EventRegionSelected>(this->getSelection());
            }
        }

        [[nodiscard]] Region getSelection() const {
            auto &data = ProviderExtraData::getCurrent().editor;

            if (!isSelectionValid())
                return Region::Invalid();

            const auto start = std::min(*data.selectionStart, *data.selectionEnd);
            const auto end   = std::max(*data.selectionStart, *data.selectionEnd);
            const size_t size = end - start + 1;

            return { start, size };
        }

        [[nodiscard]] bool isSelectionValid() const {
            auto &data = ProviderExtraData::getCurrent().editor;

            return data.selectionStart.has_value() && data.selectionEnd.has_value();
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
        std::optional<color_t> applySelectionColor(u64 byteAddress, std::optional<color_t> color);

    private:
        u16 m_bytesPerRow = 16;

        ContentRegistry::HexEditor::DataVisualizer *m_currDataVisualizer;

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
        bool m_syncScrolling = false;
        u32 m_byteCellPadding = 0, m_characterCellPadding = 0;

        bool m_shouldOpenPopup = false;
        std::unique_ptr<Popup> m_currPopup;

        std::optional<EncodingFile> m_currCustomEncoding;
    };

}