#include "content/popups/hex_editor/popup_hex_editor_base_address.hpp"
#include "content/views/view_hex_editor.hpp"

namespace hex::plugin::builtin {

    PopupBaseAddress::PopupBaseAddress(const ImHexApi::HexEditor::ProviderRegion &selection) 
            : m_selection(selection), m_baseAddress(0), m_byteAddress(0), m_byteOffset(0), 
              m_setBaseAddressFlag(true), m_setByteAddressFlag(false), m_setByteAddressEnable(false) {
        // If popup is opened without selecting bytes, try to use current provider
        if (selection.getProvider() == nullptr) {
            m_selection.provider = ImHexApi::Provider::get();
        }

        if (m_selection.getProvider() != nullptr) {
            m_baseAddress = m_selection.getProvider()->getBaseAddress();
            m_setByteAddressEnable = ((m_selection.getProvider()->getActualSize() == 0) || 
                                      (m_selection.getSize() != 1)) ? false : true;
            m_byteAddress = m_selection.getStartAddress();

            if (m_byteAddress >= m_baseAddress) {
                m_byteOffset = (m_byteAddress - m_baseAddress);
            }
        }
    }

    void PopupBaseAddress::draw(ViewHexEditor *editor) {
        const auto width = ImGui::GetWindowWidth();

        if (m_setBaseAddressFlag){ // Set base address
            ImGuiExt::InputHexadecimal("##base_address_input", &m_baseAddress, ImGuiInputTextFlags_AutoSelectAll);
        } else { // Set byte address
            ImGuiExt::InputHexadecimal("##byte_address_input", &m_byteAddress, ImGuiInputTextFlags_AutoSelectAll);
        }

        ImGui::Spacing();

        ImGui::BeginDisabled(!m_setByteAddressEnable);
        {
            if (ImGui::Checkbox("hex.builtin.view.hex_editor.menu.edit.set_base_address"_lang, &m_setBaseAddressFlag)) {
                if (m_setBaseAddressFlag) {
                    m_setByteAddressFlag = false;
                } else {
                    m_setBaseAddressFlag = true;
                }
            }

            if (ImGui::Checkbox("hex.builtin.view.hex_editor.menu.edit.set_byte_address"_lang, &m_setByteAddressFlag)) {
                if (m_setByteAddressFlag) {
                    m_setBaseAddressFlag = false;
                } else {
                    m_setByteAddressFlag = true;
                }
            }
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

        if (m_setBaseAddressFlag) { // Set base address
            if (providerSize == 0) {
                return true;
            }
        } else { // Set byte address
            if (providerSize == 0 || (m_byteAddress < m_byteOffset)) {
                return false;
            }
            newBaseAddress = (m_byteAddress - m_byteOffset);
        }

        return ((providerSize - 1) <= (std::numeric_limits<u64>::max() - newBaseAddress));
    }

    void PopupBaseAddress::setBaseAddress() const {
        auto newBaseAddress = m_baseAddress;

        if (m_setByteAddressFlag) { // Set byte address
            newBaseAddress = (m_byteAddress - m_byteOffset);
        }

        m_selection.getProvider()->setBaseAddress(newBaseAddress);
    }

    UnlocalizedString PopupBaseAddress::getTitle() const {
        return "hex.builtin.view.hex_editor.menu.edit.set_base";
    }
}