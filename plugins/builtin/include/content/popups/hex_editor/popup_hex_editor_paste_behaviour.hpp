#pragma once

#include <content/views/view_hex_editor.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/helpers/utils.hpp>
#include <functional>

namespace hex::plugin::builtin {

    class PopupPasteBehaviour final : public ViewHexEditor::Popup {
    public:
        explicit PopupPasteBehaviour(const Region &selection, const std::function<void(const Region &selection, bool selectionCheck)> &pasteCallback);
        void draw(ViewHexEditor *editor) override;
        [[nodiscard]] UnlocalizedString getTitle() const override;

    private:
        Region m_selection;
        std::function<void(const Region &selection, bool selectionCheck)> m_pasteCallback;
    };
}