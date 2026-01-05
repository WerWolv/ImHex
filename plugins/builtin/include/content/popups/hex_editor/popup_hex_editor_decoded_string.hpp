
#pragma once

#include <content/views/view_hex_editor.hpp>
#include <hex/api/localization_manager.hpp>
#include <string>

namespace hex::plugin::builtin {

    class PopupDecodedString final : public ViewHexEditor::Popup {
    public:
        explicit PopupDecodedString(std::string decodedString);

        void draw(ViewHexEditor *) override;

        [[nodiscard]] UnlocalizedString getTitle() const override;

        [[nodiscard]] bool canBePinned() const override;
        [[nodiscard]] ImGuiWindowFlags getFlags() const override;

    private:
        std::string m_decodedString;
    };
}
