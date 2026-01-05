#include "content/popups/hex_editor/popup_hex_editor_remove.hpp"
#include "content/views/view_hex_editor.hpp"

#include <hex/api/content_registry/settings.hpp>
#include <hex/api/content_registry/user_interface.hpp>

#include <content/differing_byte_searcher.hpp>

namespace hex::plugin::builtin {

    PopupRemove::PopupRemove(u64 address, size_t size) : m_address(address), m_size(size) {}

    void PopupRemove::draw(ViewHexEditor *editor) {
        ImGuiExt::InputHexadecimal("hex.ui.common.address"_lang, &m_address);
        ImGuiExt::InputHexadecimal("hex.ui.common.size"_lang, &m_size);

        ImGuiExt::ConfirmButtons("hex.ui.common.set"_lang, "hex.ui.common.cancel"_lang,
            [&, this]{
                remove(m_address, m_size);
                editor->closePopup();
            },
            [&]{
                editor->closePopup();
            });

        if (ImGui::IsWindowFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) {
            remove(m_address, m_size);
            editor->closePopup();
        }
    }

    UnlocalizedString PopupRemove::getTitle() const {
        return "hex.builtin.view.hex_editor.menu.edit.remove";
    }

    void PopupRemove::remove(u64 address, size_t size) {
        if (ImHexApi::Provider::isValid())
            ImHexApi::Provider::get()->remove(address, size);
    }

}