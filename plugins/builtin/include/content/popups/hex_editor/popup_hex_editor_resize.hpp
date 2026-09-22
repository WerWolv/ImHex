#pragma once

#include <content/views/view_hex_editor.hpp>
#include <hex/api/localization_manager.hpp>

namespace hex::plugin::builtin {

    class PopupResize : public ViewHexEditor::Popup {
    public:
        explicit PopupResize(const ImHexApi::HexEditor::ProviderRegion &selection);
        void draw(ViewHexEditor *editor) override;
        [[nodiscard]] UnlocalizedString getTitle() const override;
        void resizeProvider(ViewHexEditor *editor);

    private:
        ImHexApi::HexEditor::ProviderRegion m_selection;
        u64 m_actualSize;
        u64 m_selectionSize;
        bool m_isSelection;
    };
}