#pragma once

#include <content/views/view_hex_editor.hpp>
#include <hex/api/localization_manager.hpp>

namespace hex::plugin::builtin {

    class PopupResize : public ViewHexEditor::Popup {
    public:
        explicit PopupResize(u64 currSize);
        void draw(ViewHexEditor *editor) override;
        [[nodiscard]] UnlocalizedString getTitle() const override;

    private:
        void resize(size_t newSize);
        u64 m_size;
    };
}