#include "content/popups/hex_editor/popup_hex_editor_base_address.hpp"
#include "content/views/view_hex_editor.hpp"

#include <hex/api/content_registry/settings.hpp>
#include <hex/api/content_registry/user_interface.hpp>
#include <content/differing_byte_searcher.hpp>

namespace hex::plugin::builtin {

    PopupBaseAddress::PopupBaseAddress(u64 baseAddress) : m_baseAddress(baseAddress) {}

    void PopupBaseAddress::draw(ViewHexEditor *editor) {
        const auto width = ImGui::GetWindowWidth();
        ImGuiExt::InputHexadecimal("##base_address", &m_baseAddress);
        if (ImGui::IsItemFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) {
            if (this->isValid()) {
                setBaseAddress(m_baseAddress);
                editor->closePopup();
            }
        }
        ImGui::BeginDisabled(!this->isValid());
        {
            ImGui::SetCursorPosX(width / 9);

            if (ImGui::Button("hex.ui.common.set"_lang,  ImVec2(width / 3, 0))) {
                setBaseAddress(m_baseAddress);
                editor->closePopup();
            }
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::SetCursorPosX(width / 9 * 5);
        if (ImGui::Button("hex.ui.common.cancel"_lang,  ImVec2(width / 3, 0))) {
            // Cancel the action, without updating settings nor pasting.
            editor->closePopup();
        }
    }

    UnlocalizedString PopupBaseAddress::getTitle() const {
        return "hex.builtin.view.hex_editor.menu.edit.set_base";
    }

    void PopupBaseAddress::setBaseAddress(u64 baseAddress) {
        if (ImHexApi::Provider::isValid())
            ImHexApi::Provider::get()->setBaseAddress(baseAddress);
    }

    bool PopupBaseAddress::isValid() const {
        if (ImHexApi::Provider::isValid()) {
            u64 dataSize = ImHexApi::Provider::get()->getActualSize();
            return (dataSize == 0 || dataSize - 1 <= std::numeric_limits<u64>::max() - m_baseAddress);
        }
        return false;
    }

}