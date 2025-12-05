#pragma once

#include <content/views/view_hex_editor.hpp>
#include <hex/api/localization_manager.hpp>
#include <string>

namespace hex::plugin::builtin {

    class PopupFill : public ViewHexEditor::Popup {
    public:
        PopupFill(u64 address, size_t size);
        void draw(ViewHexEditor *editor) override;
        [[nodiscard]] UnlocalizedString getTitle() const override;

    private:
        static void fill(u64 address, size_t size, std::string input);
        u64 m_address;
        u64 m_size;
        std::string m_input;
    };
}