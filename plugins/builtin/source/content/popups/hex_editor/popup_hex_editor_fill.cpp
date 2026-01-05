#include "content/popups/hex_editor/popup_hex_editor_fill.hpp"
#include "content/views/view_hex_editor.hpp"

#include <hex/api/content_registry/settings.hpp>
#include <hex/api/content_registry/user_interface.hpp>
#include <hex/api/content_registry/pattern_language.hpp>
#include <hex/api/achievement_manager.hpp>

#include <content/differing_byte_searcher.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/crypto.hpp>

#include <fonts/vscode_icons.hpp>

#include <wolv/literals.hpp>

using namespace wolv::literals;

namespace hex::plugin::builtin {

    PopupFill::PopupFill(u64 address, size_t size) : m_address(address), m_size(size) {}

    void PopupFill::draw(ViewHexEditor *editor) {
        ImGuiExt::InputHexadecimal("hex.ui.common.address"_lang, &m_address);
        ImGuiExt::InputHexadecimal("hex.ui.common.size"_lang, &m_size);

        ImGui::Separator();

        ImGuiExt::InputTextIcon("hex.ui.common.bytes"_lang, ICON_VS_SYMBOL_NAMESPACE, m_input);

        ImGuiExt::ConfirmButtons("hex.ui.common.set"_lang, "hex.ui.common.cancel"_lang,
        [&, this] {
            fill(m_address, m_size, m_input);
            editor->closePopup();
        },
        [&] {
            editor->closePopup();
        });

        if (ImGui::IsWindowFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) {
            fill(m_address, m_size, m_input);
            editor->closePopup();
        }
    }

    UnlocalizedString PopupFill::getTitle() const {
        return "hex.builtin.view.hex_editor.menu.edit.fill";
    }

    void PopupFill::fill(u64 address, size_t size, std::string input) {
        if (!ImHexApi::Provider::isValid())
            return;

        std::erase(input, ' ');

        auto bytes = crypt::decode16(input);
        if (bytes.empty())
            return;

        auto provider = ImHexApi::Provider::get();
        u32 patchCount = 0;

        // Group the fill pattern into a larger chunk
        constexpr static auto BatchFillSize = 1_MiB;
        std::vector<u8> batchData;
        if (bytes.size() < BatchFillSize) {
            batchData.resize(std::min<u64>(alignTo<u64>(BatchFillSize, bytes.size()), size));
            for (u64 i = 0; i < batchData.size(); i += bytes.size()) {
                auto remainingSize = std::min<size_t>(batchData.size() - i, bytes.size());
                std::copy_n(bytes.begin(), remainingSize, batchData.begin() + i);
            }
        } else {
            batchData = std::move(bytes);
        }

        const auto startAddress = provider->getBaseAddress() + address;
        for (u64 i = 0; i < size; i += batchData.size()) {
            auto remainingSize = std::min<size_t>(size - i, batchData.size());
            provider->write(startAddress + i, batchData.data(), remainingSize);
            patchCount += 1;
        }
        provider->getUndoStack().groupOperations(patchCount, "hex.builtin.undo_operation.fill");

        AchievementManager::unlockAchievement("hex.builtin.achievement.hex_editor", "hex.builtin.achievement.hex_editor.fill.name");
    }

}