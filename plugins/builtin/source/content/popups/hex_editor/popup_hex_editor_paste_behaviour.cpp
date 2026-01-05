#include "content/popups/hex_editor/popup_hex_editor_paste_behaviour.hpp"
#include "content/views/view_hex_editor.hpp"

#include <hex/api/content_registry/settings.hpp>

namespace hex::plugin::builtin {

    PopupPasteBehaviour::PopupPasteBehaviour(const Region &selection, const std::function<void(const Region &selection, bool selectionCheck)> &pasteCallback) 
        : m_selection(), m_pasteCallback(pasteCallback) {
        m_selection = Region { .address=selection.getStartAddress(), .size=selection.getSize() };
    }

    void PopupPasteBehaviour::draw(ViewHexEditor *editor) {
        const auto width = ImGui::GetWindowWidth();

        ImGui::TextWrapped("%s", "hex.builtin.view.hex_editor.menu.edit.paste.popup.description"_lang.get());
        ImGui::TextUnformatted("hex.builtin.view.hex_editor.menu.edit.paste.popup.hint"_lang);

        ImGui::Separator();

        if (ImGui::Button("hex.builtin.view.hex_editor.menu.edit.paste.popup.button.selection"_lang, ImVec2(width / 4, 0))) {
            m_pasteCallback(m_selection, true);
            editor->closePopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("hex.builtin.view.hex_editor.menu.edit.paste.popup.button.everything"_lang, ImVec2(width / 4, 0))) {
            m_pasteCallback(m_selection, false);
            editor->closePopup();
        }

        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetCursorPosX() - (width / 6));
        if (ImGui::Button("hex.ui.common.cancel"_lang, ImVec2(width / 6, 0))) {
            // Cancel the action, without updating settings nor pasting.
            editor->closePopup();
        }
    }

    UnlocalizedString PopupPasteBehaviour::getTitle() const {
        return "hex.builtin.view.hex_editor.menu.edit.paste.popup.title";
    }

}