#pragma once

#include <hex/ui/view.hpp>

#include <ui/hex_editor.hpp>

namespace hex::plugin::builtin {

    class ViewHexEditor : public View::Window {
    public:
        ViewHexEditor();
        ~ViewHexEditor() override;

        void drawContent() override;
        [[nodiscard]] ImGuiWindowFlags getWindowFlags() const override {
            return ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        }

        class Popup {
        public:
            virtual ~Popup() = default;
            virtual void draw(ViewHexEditor *editor) = 0;

            [[nodiscard]] virtual UnlocalizedString getTitle() const { return {}; }

            [[nodiscard]] virtual bool canBePinned() const { return false; }
            [[nodiscard]] bool isPinned() const { return m_isPinned; }
            void setPinned(const bool pinned) { m_isPinned = pinned; }
        private:
            bool m_isPinned = false;
        };

        [[nodiscard]] bool isAnyPopupOpen() const {
            return m_currPopup != nullptr;
        }

        template<std::derived_from<Popup> T>
        [[nodiscard]] bool isPopupOpen() const {
            return dynamic_cast<T*>(m_currPopup.get()) != nullptr;
        }

        template<std::derived_from<Popup> T, typename ... Args>
        void openPopup(Args && ...args) {
            m_currPopup = std::make_unique<T>(std::forward<Args>(args)...);
            m_shouldOpenPopup = true;
        }

        void closePopup() {
            m_currPopup.reset();
        }

        bool isSelectionValid() const {
            return m_hexEditor.isSelectionValid();
        }

        Region getSelection() const {
            return m_hexEditor.getSelection();
        }

        void setSelection(const Region &region) {
            m_hexEditor.setSelection(region);
        }

        void setSelection(u128 start, u128 end) {
            m_hexEditor.setSelection(start, end);
        }

        void jumpToSelection() {
            m_hexEditor.jumpToSelection();
        }

        void jumpIfOffScreen() {
            m_hexEditor.jumpIfOffScreen();
        }

    private:
        void drawPopup();

        void registerShortcuts();
        void registerEvents();
        void registerMenuItems();

        /**
        * Method dedicated to handling paste behaviour when using the normal "Paste" option.
        * Decides what to do based on user settings, or opens a popup to let them decide.
        */
        void processPasteBehaviour(const Region &selection);

        ui::HexEditor m_hexEditor;

        bool m_shouldOpenPopup = false;
        bool m_currentPopupHasHovered = false; // This flag prevents the popup from initially appearing with the transparency effect
        bool m_currentPopupHover = false;
        bool m_currentPopupDetached = false;
        std::unique_ptr<Popup> m_currPopup;

        PerProvider<std::optional<u64>> m_selectionStart, m_selectionEnd;

        PerProvider<std::map<u64, color_t>> m_foregroundHighlights, m_backgroundHighlights;
        PerProvider<std::set<Region>> m_hoverHighlights;
    };

}