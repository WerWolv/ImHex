#pragma once

#include <content/views/view_hex_editor.hpp>
#include <hex/api/localization_manager.hpp>

namespace hex::plugin::builtin {

    class PopupInsert : public ViewHexEditor::Popup {
    public:
        PopupInsert(u64 address, size_t size);
        void draw(ViewHexEditor *editor) override;
        [[nodiscard]] UnlocalizedString getTitle() const override;

    private:
        static void insert(u64 address, size_t size);
        u64 m_address;
        u64 m_size;
    };
}