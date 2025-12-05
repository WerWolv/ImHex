#include <content/popups/hex_editor/popup_hex_editor_resize.hpp>

#include <hex/api/imhex_api.hpp>
#include <hex/api/provider.hpp>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

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