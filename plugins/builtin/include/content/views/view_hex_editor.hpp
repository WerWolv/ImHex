#pragma once

#include <hex/api/content_registry.hpp>

#include <hex/ui/view.hpp>
#include <hex/helpers/concepts.hpp>
#include <hex/helpers/encoding_file.hpp>

#include <ui/hex_editor.hpp>

namespace hex::plugin::builtin {

    class ViewHexEditor : public View {
    public:
        ViewHexEditor();
        ~ViewHexEditor() override;

        void drawContent() override;

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

        bool isSelectionValid() {
            return this->m_hexEditor.isSelectionValid();
        }

        Region getSelection() {
            return this->m_hexEditor.getSelection();
        }

        void setSelection(const Region &region) {
            this->m_hexEditor.setSelection(region);
        }

        void setSelection(u128 start, u128 end) {
            this->m_hexEditor.setSelection(start, end);
        }

        void jumpToSelection() {
            this->m_hexEditor.jumpToSelection();
        }

    private:
        void drawPopup();

        void registerShortcuts();
        void registerEvents();
        void registerMenuItems();

        ui::HexEditor m_hexEditor;

        bool m_shouldOpenPopup = false;
        std::unique_ptr<Popup> m_currPopup;

        PerProvider<std::optional<u64>> m_selectionStart, m_selectionEnd;
        PerProvider<float> m_scrollPosition;
    };

}