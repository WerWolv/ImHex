#pragma once

#include <content/views/view_hex_editor.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/helpers/utils.hpp>

namespace hex::plugin::builtin {

    class PopupSelect : public ViewHexEditor::Popup {
    public:
        PopupSelect(u64 address, size_t size);
        void draw(ViewHexEditor *editor) override;
        [[nodiscard]] UnlocalizedString getTitle() const override;
        [[nodiscard]] bool canBePinned() const override;

    private:
        Region m_region = { 0, 1 };
        bool m_justOpened = true;
    };
}