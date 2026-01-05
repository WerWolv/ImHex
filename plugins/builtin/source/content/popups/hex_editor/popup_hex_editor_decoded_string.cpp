#include "content/popups/hex_editor/popup_hex_editor_decoded_string.hpp"
#include "content/views/view_hex_editor.hpp"

#include <hex/api/content_registry/settings.hpp>
#include <hex/api/content_registry/pattern_language.hpp>

#include <content/differing_byte_searcher.hpp>

#include <popups/popup_file_chooser.hpp>

namespace hex::plugin::builtin {

    PopupDecodedString::PopupDecodedString(std::string decodedString) : m_decodedString(std::move(decodedString)) {
    }

    void PopupDecodedString::draw(ViewHexEditor *) {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2());
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0F);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4());

        auto text = wolv::util::trim(wolv::util::wrapMonospacedString(
            m_decodedString,
            ImGui::CalcTextSize("M").x,
            std::max(100_scaled, ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ScrollbarSize - ImGui::GetStyle().FrameBorderSize)
        ));

        ImGui::InputTextMultiline(
                "##",
                text.data(),
                text.size() + 1,
                ImGui::GetContentRegionAvail(),
                ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_NoHorizontalScroll
        );

        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }

    UnlocalizedString PopupDecodedString::getTitle() const {
        return "hex.builtin.view.hex_editor.menu.edit.decoded_string.popup.title";
    }

    bool PopupDecodedString::canBePinned() const { return true; }
    ImGuiWindowFlags PopupDecodedString::getFlags() const { return ImGuiWindowFlags_None; }

}
