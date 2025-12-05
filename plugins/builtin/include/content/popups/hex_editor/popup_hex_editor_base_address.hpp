#pragma once

#include <content/views/view_hex_editor.hpp>
#include <hex/api/localization_manager.hpp>

namespace hex::plugin::builtin {

    class PopupBaseAddress : public ViewHexEditor::Popup {
    public:
        explicit PopupBaseAddress(u64 baseAddress);
        void draw(ViewHexEditor *editor) override;
        [[nodiscard]] UnlocalizedString getTitle() const override;

    private:
        static void setBaseAddress(u64 baseAddress);
        u64 m_baseAddress;
    };
}