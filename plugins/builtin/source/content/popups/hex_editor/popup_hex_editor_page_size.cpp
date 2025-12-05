#include "content/popups/hex_editor/popup_hex_editor_page_size.hpp"
#include "content/views/view_hex_editor.hpp"

#include <hex/api/content_registry/settings.hpp>
#include <hex/api/content_registry/user_interface.hpp>

#include <content/differing_byte_searcher.hpp>

namespace hex::plugin::builtin {

    PopupPageSize::PopupPageSize(u64 pageSize) : m_pageSize(pageSize) {}

    void PopupPageSize::draw(ViewHexEditor *editor) {
        ImGuiExt::InputHexadecimal("##page_size", &m_pageSize);
        if (ImGui::IsItemFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) {
            setPageSize(m_pageSize);
            editor->closePopup();
        }

        ImGuiExt::ConfirmButtons("hex.ui.common.set"_lang, "hex.ui.common.cancel"_lang,
            [&, this]{
                setPageSize(m_pageSize);
                editor->closePopup();
            },
            [&]{
                editor->closePopup();
            }
        );
    }

    UnlocalizedString PopupPageSize::getTitle() const {
        return "hex.builtin.view.hex_editor.menu.edit.set_page_size";
    }

    void PopupPageSize::setPageSize(u64 pageSize) {
        if (ImHexApi::Provider::isValid()) {
            auto provider = ImHexApi::Provider::get();

            provider->setPageSize(pageSize);
            provider->setCurrentPage(0);
        }
    }

}