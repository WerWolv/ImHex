#include "content/popups/hex_editor/popup_hex_editor_select.hpp"
#include "content/views/view_hex_editor.hpp"

#include <hex/api/content_registry/settings.hpp>
#include <hex/api/content_registry/user_interface.hpp>

#include <content/differing_byte_searcher.hpp>

namespace hex::plugin::builtin {

    PopupSelect::PopupSelect(u64 address, size_t size) : m_region({.address=address, .size=size}) {}

    void PopupSelect::draw(ViewHexEditor *editor) {
        if (ImGui::BeginTabBar("select_tabs")) {
            if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.select.offset.region"_lang)) {
                u64 inputA = m_region.getStartAddress();
                u64 inputB = m_region.getEndAddress();

                if (m_justOpened) {
                    ImGui::SetKeyboardFocusHere();
                    m_justOpened = false;
                }
                ImGuiExt::InputHexadecimal("hex.builtin.view.hex_editor.select.offset.begin"_lang, &inputA, ImGuiInputTextFlags_AutoSelectAll);
                ImGuiExt::InputHexadecimal("hex.builtin.view.hex_editor.select.offset.end"_lang, &inputB, ImGuiInputTextFlags_AutoSelectAll);

                if (inputB < inputA)
                    inputB = inputA;

                m_region = { .address=inputA, .size=(inputB - inputA) + 1 };

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.select.offset.size"_lang)) {
                u64 inputA = m_region.getStartAddress();
                u64 inputB = m_region.getSize();

                if (m_justOpened) {
                    ImGui::SetKeyboardFocusHere();
                    m_justOpened = false;
                }
                ImGuiExt::InputHexadecimal("hex.builtin.view.hex_editor.select.offset.begin"_lang, &inputA, ImGuiInputTextFlags_AutoSelectAll);
                ImGuiExt::InputHexadecimal("hex.builtin.view.hex_editor.select.offset.size"_lang, &inputB, ImGuiInputTextFlags_AutoSelectAll);

                if (inputB <= 0)
                    inputB = 1;

                m_region = { .address=inputA, .size=inputB };
                ImGui::EndTabItem();
            }

            const auto provider = ImHexApi::Provider::get();
            bool isOffsetValid = m_region.getStartAddress() <= m_region.getEndAddress() &&
                                 m_region.getEndAddress() < provider->getActualSize();
            ImGui::BeginDisabled(!isOffsetValid);
            {
                if (ImGui::Button("hex.builtin.view.hex_editor.select.select"_lang) ||
                    (ImGui::IsWindowFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)))) {
                    editor->setSelection(m_region.getStartAddress(), m_region.getEndAddress());
                    editor->jumpToSelection();

                    if (!this->isPinned())
                        editor->closePopup();
                }
            }
            ImGui::EndDisabled();

            ImGui::EndTabBar();
        }
    }

    UnlocalizedString PopupSelect::getTitle() const {
        return "hex.builtin.view.hex_editor.menu.edit.select";
    }

    bool PopupSelect::canBePinned() const {
        return true;
    }

}