#include "content/popups/hex_editor/popup_hex_editor_resize.hpp"
#include "content/views/view_hex_editor.hpp"

#include <fonts/tabler_icons.hpp>

namespace hex::plugin::builtin {

    PopupResize::PopupResize(const ImHexApi::HexEditor::ProviderRegion &selection) : m_selection(selection), m_actualSize(0), m_selectionSize(0), m_isSelection(false) {
        if (selection.provider != nullptr) {
            m_actualSize = selection.provider->getActualSize();
            m_selectionSize = selection.size;
        }
    }

    void PopupResize::draw(ViewHexEditor *editor) {
        // Resize input
        ImGuiExt::InputHexadecimal("##resize", (m_isSelection ? &m_selectionSize : &m_actualSize), ImGuiInputTextFlags_AutoSelectAll);

        ImGui::SameLine();

        // Resize selection toggle
        ImGui::BeginDisabled(m_selection.getSize() == 0);
        {
            ImGuiExt::DimmedIconToggle(ICON_TA_ARTICLE, ICON_TA_ARTICLE_OFF, &m_isSelection);
            ImGuiExt::InfoTooltip("hex.builtin.view.hex_editor.menu.edit.resize_selection"_lang);
        }
        ImGui::EndDisabled();

        ImGui::Spacing();
        ImGui::Separator();

        if (ImGui::IsWindowFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) {
            resizeProvider(editor);
            editor->closePopup();
        }

        ImGuiExt::ConfirmButtons("hex.ui.common.set"_lang, "hex.ui.common.cancel"_lang,
            [&, this]{
                resizeProvider(editor);
                editor->closePopup();
            },
            [&]{
                editor->closePopup();
            }
        );
    }

    UnlocalizedString PopupResize::getTitle() const {
        return "hex.builtin.view.hex_editor.menu.edit.resize"_unlocalized;
    }

    void PopupResize::resizeProvider(ViewHexEditor *editor) {
        if (m_selection.provider == nullptr) {
            return;
        }

        if (m_isSelection) {
            const auto resizeOffset = (m_selection.getEndAddress() - m_selection.provider->getBaseAddress());
            const i64 sizeDiff = (static_cast<i64>(m_selectionSize) - static_cast<i64>(m_selection.size));

            if (sizeDiff > 0) {
                m_selection.provider->insert((resizeOffset + 1), sizeDiff);
            } else if (sizeDiff < 0) {
                m_selection.provider->remove((resizeOffset + sizeDiff + 1), -sizeDiff);
            } else {
                // Same size
            }
            m_selection.size = m_selectionSize;
        } else {
            m_selection.provider->resize(m_actualSize);
            m_selection.address = m_selection.provider->getBaseAddress();
            m_selection.size = m_actualSize;
        }

        // Handle selection
        editor->setSelection(m_selection.getStartAddress(), m_selection.getEndAddress());
        editor->setCursorPosition(m_selection.getEndAddress());
    }
}