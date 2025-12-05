#pragma once

#include <content/views/view_hex_editor.hpp>
#include <hex/api/localization_manager.hpp>

namespace hex::plugin::builtin {

    class PopupPageSize : public ViewHexEditor::Popup {
    public:
        explicit PopupPageSize(u64 pageSize);
        void draw(ViewHexEditor *editor) override;
        [[nodiscard]] UnlocalizedString getTitle() const override;

    private:
        static void setPageSize(u64 pageSize);
        u64 m_pageSize;
    };
}