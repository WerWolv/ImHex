#include "content/popups/hex_editor/popup_hex_editor_resize.hpp"
#include "content/views/view_hex_editor.hpp"

#include <hex/api/content_registry/settings.hpp>
#include <hex/api/content_registry/user_interface.hpp>

#include <content/differing_byte_searcher.hpp>

namespace hex::plugin::builtin {

    PopupResize::PopupResize(u64 currSize) : m_size(currSize) {}

    void PopupResize::draw(ViewHexEditor *editor) {
        ImGuiExt::InputHexadecimal("##resize", &m_size);
        if (ImGui::IsItemFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) {
            this->resize(m_size);
            editor->closePopup();
        }

        ImGuiExt::ConfirmButtons("hex.ui.common.set"_lang, "hex.ui.common.cancel"_lang,
            [&, this]{
                this->resize(m_size);
                editor->closePopup();
            },
            [&]{
                editor->closePopup();
            });
    }

    UnlocalizedString PopupResize::getTitle() const {
        return "hex.builtin.view.hex_editor.menu.edit.resize";
    }

    void PopupResize::resize(size_t newSize) {
        if (ImHexApi::Provider::isValid())
            ImHexApi::Provider::get()->resize(newSize);
    }

}