#include "content/popups/hex_editor/popup_hex_editor_base_address.hpp"
#include "content/views/view_hex_editor.hpp"

#include <fonts/tabler_icons.hpp>

namespace hex::plugin::builtin {

    PopupBaseAddress::PopupBaseAddress(const ImHexApi::HexEditor::ProviderRegion &selection) 
            : m_selection(selection), m_baseAddress(0), m_byteAddress(0), m_byteOffset(0), 
              m_byteAddressMode(false), m_byteAddressDisable(true) {
        if (m_selection.getProvider() != nullptr) {
            m_baseAddress = m_selection.getProvider()->getBaseAddress();
            m_byteAddressDisable = ((m_selection.getProvider()->getActualSize() == 0) || (m_selection.getSize() != 1));
            m_byteAddress = m_selection.getStartAddress();

            if (m_byteAddress >= m_baseAddress) {
                m_byteOffset = (m_byteAddress - m_baseAddress);
            }
        }
    }

    void PopupBaseAddress::draw(ViewHexEditor *editor) {
        const auto width = ImGui::GetWindowWidth();

        // Set base address
        ImGuiExt::InputHexadecimal("##base_address_input", 
                                   m_byteAddressMode ? &m_byteAddress : &m_baseAddress, 
                                   ImGuiInputTextFlags_AutoSelectAll);

        ImGui::SameLine();

        ImGui::BeginDisabled(m_byteAddressDisable);
        {
            ImGuiExt::DimmedIconToggle(ICON_TA_ANCHOR, ICON_TA_ANCHOR_OFF, &m_byteAddressMode);
            ImGuiExt::InfoTooltip("hex.builtin.view.hex_editor.menu.edit.set_byte_address"_lang);
        }
        ImGui::EndDisabled();

        ImGui::Spacing();
        ImGui::Separator();

        // Set
        ImGui::BeginDisabled(!isNewAddressValid());
        {
            ImGui::SetCursorPosX(width / 9);
            if (ImGuiExt::DimmedButton("hex.ui.common.set"_lang,  ImVec2(width / 3, 0)) ||
                    (ImGui::IsWindowFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)))) {
                if (isNewAddressValid()) {
                    // Save old base address
                    auto oldBaseAddress = m_selection.getProvider()->getBaseAddress();

                    // Set new base address
                    setBaseAddress();

                    // Adjust the selection relative to the new base address
                    u64 startAddress = (m_selection.getProvider()->getBaseAddress() + (m_selection.getStartAddress() - oldBaseAddress));
                    u64 endAddress = (m_selection.getProvider()->getBaseAddress() + (m_selection.getEndAddress() - oldBaseAddress));

                    editor->setSelection(startAddress, endAddress);
                    editor->setCursorPosition(endAddress);
                    editor->closePopup();
                }
            }
        }
        ImGui::EndDisabled();

        ImGui::SameLine();

        // Cancel
        ImGui::SetCursorPosX(width / 9 * 5);
        if (ImGuiExt::DimmedButton("hex.ui.common.cancel"_lang,  ImVec2(width / 3, 0)) ||
                (ImGui::IsWindowFocused() && (ImGui::IsKeyPressed(ImGuiKey_Escape)))) {
            editor->closePopup();
        }
    }

    bool PopupBaseAddress::isNewAddressValid() const {
        if (m_selection.getProvider() == nullptr) {
            return false;
        }

        const auto providerSize = m_selection.getProvider()->getActualSize();
        auto newBaseAddress = m_baseAddress;

        if (!m_byteAddressMode) { // Set base address
            if (providerSize == 0) {
                return true;
            }
        } else { // Set byte address
            if ((providerSize == 0) || (m_byteAddress < m_byteOffset)) {
                return false;
            }
            newBaseAddress = (m_byteAddress - m_byteOffset);
        }

        return ((providerSize - 1) <= (std::numeric_limits<u64>::max() - newBaseAddress));
    }

    void PopupBaseAddress::setBaseAddress() const {
        auto newBaseAddress = m_baseAddress;

        if (m_byteAddressMode) { // Set byte address
            newBaseAddress = (m_byteAddress - m_byteOffset);
        }

        m_selection.getProvider()->setBaseAddress(newBaseAddress);
    }

    UnlocalizedString PopupBaseAddress::getTitle() const {
        return "hex.builtin.view.hex_editor.menu.edit.set_base";
    }
}