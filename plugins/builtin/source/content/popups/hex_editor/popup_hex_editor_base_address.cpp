#include "content/popups/hex_editor/popup_hex_editor_base_address.hpp"
#include "content/views/view_hex_editor.hpp"

#include <hex/api/content_registry/settings.hpp>
#include <hex/api/content_registry/user_interface.hpp>
#include <content/differing_byte_searcher.hpp>

namespace hex::plugin::builtin {

    PopupBaseAddress::PopupBaseAddress(u64 baseAddress) : m_baseAddress(baseAddress) {}

    void PopupBaseAddress::draw(ViewHexEditor *editor) {
        ImGuiExt::InputHexadecimal("##base_address", &m_baseAddress);
        if (ImGui::IsItemFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) {
            setBaseAddress(m_baseAddress);
            editor->closePopup();
        }

        ImGuiExt::ConfirmButtons("hex.ui.common.set"_lang, "hex.ui.common.cancel"_lang,
            [&, this]{
                setBaseAddress(m_baseAddress);
                editor->closePopup();
            },
            [&]{
                editor->closePopup();
            }
        );
    }

    UnlocalizedString PopupBaseAddress::getTitle() const {
        return "hex.builtin.view.hex_editor.menu.edit.set_base";
    }

    void PopupBaseAddress::setBaseAddress(u64 baseAddress) {
        if (ImHexApi::Provider::isValid())
            ImHexApi::Provider::get()->setBaseAddress(baseAddress);
    }

}