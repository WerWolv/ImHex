#include "content/views/view_hex_editor.hpp"

#include <hex/api/content_registry/settings.hpp>
#include <hex/api/content_registry/user_interface.hpp>
#include <hex/api/content_registry/data_formatter.hpp>
#include <hex/api/content_registry/pattern_language.hpp>
#include <hex/api/shortcut_manager.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/api/achievement_manager.hpp>

#include <content/differing_byte_searcher.hpp>

#include <hex/api/events/events_provider.hpp>
#include <hex/api/events/requests_interaction.hpp>
#include <hex/api/events/requests_gui.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/crypto.hpp>
#include <hex/helpers/default_paths.hpp>

#include <hex/providers/buffered_reader.hpp>
#include <toasts/toast_notification.hpp>

#include <wolv/math_eval/math_evaluator.hpp>

#include <content/providers/view_provider.hpp>

#include <fonts/vscode_icons.hpp>

#include <popups/popup_file_chooser.hpp>

#include <content/popups/hex_editor/popup_hex_editor_goto.hpp>
#include <content/popups/hex_editor/popup_hex_editor_select.hpp>
#include <content/popups/hex_editor/popup_hex_editor_base_address.hpp>
#include <content/popups/hex_editor/popup_hex_editor_page_size.hpp>
#include <content/popups/hex_editor/popup_hex_editor_resize.hpp>
#include <content/popups/hex_editor/popup_hex_editor_insert.hpp>
#include <content/popups/hex_editor/popup_hex_editor_remove.hpp>
#include <content/popups/hex_editor/popup_hex_editor_fill.hpp>
#include <content/popups/hex_editor/popup_hex_editor_paste_behaviour.hpp>
#include <content/popups/hex_editor/popup_hex_editor_decoded_string.hpp>
#include <content/popups/popup_blocking_task.hpp>
#include <content/popups/hex_editor/popup_hex_editor_find.hpp>
#include <pl/patterns/pattern.hpp>
#include <hex/helpers/menu_items.hpp>
#include <wolv/literals.hpp>

using namespace std::literals::string_literals;
using namespace wolv::literals;

namespace hex::plugin::builtin {

    /* Hex Editor */

    ViewHexEditor::ViewHexEditor() : View::Window("hex.builtin.view.hex_editor.name", ICON_VS_FILE_BINARY) {
        m_hexEditor.setForegroundHighlightCallback([this](u64 address, const u8 *data, size_t size) -> std::optional<color_t> {
            if (auto highlight = m_foregroundHighlights->find(address); highlight != m_foregroundHighlights->end())
                return highlight->second;

            std::optional<color_t> result;
            for (const auto &[id, callback] : ImHexApi::HexEditor::impl::getForegroundHighlightingFunctions()) {
                if (auto color = callback(address, data, size, result.has_value()); color.has_value()) {
                    result = color;
                    break;
                }
            }

            if (!result.has_value()) {
                for (const auto &[id, highlighting] : ImHexApi::HexEditor::impl::getForegroundHighlights()) {
                    if (highlighting.getRegion().overlaps({ .address=address, .size=size }))
                        return highlighting.getColor();
                }
            }

            if (result.has_value())
                m_foregroundHighlights->insert({ address, result.value() });

            return result;
        });

        static bool showHighlights = true;
        ContentRegistry::Settings::onChange("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.show_highlights", [](const ContentRegistry::Settings::SettingsValue &value) {
            showHighlights = value.get<bool>(true);
        });

        ContentRegistry::Settings::onChange("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.gray_out_zeros", [this](const ContentRegistry::Settings::SettingsValue &value) {
            m_hexEditor.enableGrayOutZeros(value.get<bool>(true));
        });

        ContentRegistry::Settings::onChange("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.upper_case_hex", [this](const ContentRegistry::Settings::SettingsValue &value) {
            m_hexEditor.enableUpperCaseHex(value.get<bool>(true));
        });

        ContentRegistry::Settings::onChange("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.show_ascii", [this](const ContentRegistry::Settings::SettingsValue &value) {
            m_hexEditor.enableShowAscii(value.get<bool>(true));
        });

        ContentRegistry::Settings::onChange("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.show_extended_ascii", [this](const ContentRegistry::Settings::SettingsValue &value) {
            m_hexEditor.enableShowExtendedAscii(value.get<bool>(false));
        });

        ContentRegistry::Settings::onChange("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.minimap", [this](const ContentRegistry::Settings::SettingsValue &value) {
            m_hexEditor.setMiniMapVisualizer(value.get<std::string>(""));
        });

        ContentRegistry::Settings::onChange("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.minimap_width", [this](const ContentRegistry::Settings::SettingsValue &value) {
            m_hexEditor.setMiniMapWidth(value.get<int>(m_hexEditor.getMiniMapWidth()));
        });

        ContentRegistry::Settings::onSave([this] {
            ContentRegistry::Settings::write<int>("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.bytes_per_row", m_hexEditor.getBytesPerRow());
            ContentRegistry::Settings::write<bool>("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.gray_out_zeros", m_hexEditor.shouldGrayOutZeros());
            ContentRegistry::Settings::write<bool>("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.upper_case_hex", m_hexEditor.shouldUpperCaseHex());
            ContentRegistry::Settings::write<bool>("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.show_ascii", m_hexEditor.shouldShowAscii());
            ContentRegistry::Settings::write<bool>("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.show_extended_ascii", m_hexEditor.shouldShowExtendedAscii());
            ContentRegistry::Settings::write<std::string>("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.minimap", m_hexEditor.getMiniMapVisualizer().value_or(""));
            ContentRegistry::Settings::write<int>("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.minimap_width", m_hexEditor.getMiniMapWidth());
        });

        m_hexEditor.setBackgroundHighlightCallback([this](u64 address, const u8 *data, size_t size) -> std::optional<color_t> {
            if (!showHighlights)
                return std::nullopt;

            if (auto highlight = m_backgroundHighlights->find(address); highlight != m_backgroundHighlights->end()) {
                if (std::ranges::any_of(*m_hoverHighlights, [region = Region(address, size)](const Region &highlight) { return highlight.overlaps(region); }))
                    return ImAlphaBlendColors(highlight->second, 0xA0FFFFFF);
                else
                    return highlight->second;
            }

            std::optional<color_t> result;
            for (const auto &[id, callback] : ImHexApi::HexEditor::impl::getBackgroundHighlightingFunctions()) {
                if (auto color = callback(address, data, size, result.has_value()); color.has_value()) {
                    result = blendColors(result, color);
                }
            }

            if (!result.has_value()) {
                for (const auto &[id, highlighting] : ImHexApi::HexEditor::impl::getBackgroundHighlights()) {
                    if (highlighting.getRegion().overlaps({ .address=address, .size=size })) {
                        result = blendColors(result, highlighting.getColor());
                    }
                }
            }

            if (result.has_value())
                m_backgroundHighlights->insert({ address, result.value() });

            return result;
        });

        m_hexEditor.setHoverChangedCallback([this](u64 address, size_t size) {
            if (!showHighlights)
                return;

            m_hoverHighlights->clear();

            if (Region(address, size) == Region::Invalid())
                return;

            for (const auto &[id, hoverFunction] : ImHexApi::HexEditor::impl::getHoveringFunctions()) {
                auto highlightedAddresses = hoverFunction(m_hexEditor.getProvider(), address, size);
                m_hoverHighlights->merge(highlightedAddresses);
            }
        });

        m_hexEditor.setTooltipCallback([](u64 address, const u8 *data, size_t size) {
            if (!showHighlights)
                return;

            for (const auto &[id, callback] : ImHexApi::HexEditor::impl::getTooltipFunctions()) {
                callback(address, data, size);
            }

            for (const auto &[id, tooltip] : ImHexApi::HexEditor::impl::getTooltips()) {
                if (tooltip.getRegion().overlaps({ .address=address, .size=size })) {
                    ImGui::BeginTooltip();
                    if (ImGui::BeginTable("##tooltips", 1, ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoClip)) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        ImGui::ColorButton(tooltip.getValue().c_str(), ImColor(tooltip.getColor()), ImGuiColorEditFlags_AlphaOpaque);
                        ImGui::SameLine(0, 10);
                        ImGui::TextUnformatted(tooltip.getValue().c_str());

                        ImGui::PushStyleColor(ImGuiCol_TableRowBg, tooltip.getColor());
                        ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, tooltip.getColor());
                        ImGui::EndTable();
                        ImGui::PopStyleColor(2);
                    }
                    ImGui::EndTooltip();
                }
            }
        });

        this->registerShortcuts();
        this->registerEvents();
        this->registerMenuItems();
    }

    ViewHexEditor::~ViewHexEditor() {
        RequestHexEditorSelectionChange::unsubscribe(this);
        EventProviderChanged::unsubscribe(this);
        EventProviderOpened::unsubscribe(this);
        EventHighlightingChanged::unsubscribe(this);
        EventImHexClosing::unsubscribe(this);
    }

    void ViewHexEditor::drawPopup() {
        bool open = true;

        ImGui::SetNextWindowPos(ImGui::GetCurrentWindowRead()->ContentRegionRect.Min - ImGui::GetStyle().WindowPadding, ImGuiCond_Once);
        const auto configuredAlpha = ImGuiExt::GetCustomStyle().PopupWindowAlpha;
        bool alphaIsChanged = false;
        if (m_currPopup != nullptr && !m_currentPopupHover && m_currentPopupHasHovered && m_currentPopupDetached && configuredAlpha < 0.99F && configuredAlpha > 0.01F) {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, configuredAlpha);
            alphaIsChanged = true;
        }

        if (m_currPopup != nullptr) {
            if (ImGui::Begin(fmt::format("##{}", m_currPopup->getTitle().get()).c_str(), &open, m_currPopup->getFlags() | ImGuiWindowFlags_NoDocking)) {
                if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                    this->closePopup();
                } else {
                    float titleOffset = 7_scaled;

                    const ImVec2 originalCursorPos = ImGui::GetCursorPos();
                    if (m_currPopup->canBePinned()) {
                        titleOffset += 16_scaled;
                        ImGui::SetCursorPos(ImVec2(5_scaled, 0.0F));
                        bool pinned = m_currPopup->isPinned();
                        if (ImGuiExt::PopupTitleBarButton(pinned ? ICON_VS_PINNED : ICON_VS_PIN, pinned)) {
                            m_currPopup->setPinned(!pinned);
                        }
                    }

                    const auto popupTitle = m_currPopup->getTitle();
                    if (!popupTitle.empty()) {
                        ImGui::SetCursorPos(ImVec2(titleOffset, 0.0F));
                        ImGuiExt::PopupTitleBarText(Lang(popupTitle));
                    }

                    ImGui::SetCursorPos(originalCursorPos);

                    if (ImGui::IsWindowAppearing()) {
                        ImGui::SetKeyboardFocusHere();
                        m_currentPopupHasHovered = false;
                    }

                    m_currentPopupHover = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | ImGuiHoveredFlags_RootAndChildWindows);
                    m_currentPopupDetached = !ImGui::GetCurrentWindow()->ViewportOwned;
                    m_currentPopupHasHovered |= m_currentPopupHover;

                    m_currPopup->draw(this);
                }
            } else {
                this->closePopup();
            }

            if ((m_currPopup != nullptr && !m_currPopup->isPinned() && !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::IsWindowHovered()) || !open) {
                this->closePopup();
            }

            ImGui::End();
        }

        if (alphaIsChanged)
            ImGui::PopStyleVar();

        // Right click menu
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && !ImGui::IsAnyItemHovered() && !ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            RequestOpenPopup::post("hex.builtin.menu.edit");
            ImGui::SetWindowFocus();
        }
    }

    void ViewHexEditor::drawContent() {
        m_hexEditor.setProvider(ImHexApi::Provider::get());

        m_hexEditor.draw();

        this->drawPopup();
    }

    static void save() {
        auto provider = ImHexApi::Provider::get();

        if (provider != nullptr)
            provider->save();
    }

    static void saveAs() {
        auto provider = ImHexApi::Provider::get();
        if (provider == nullptr)
            return;

        fs::openFileBrowser(fs::DialogMode::Save, {}, [provider](const auto &path) {
            PopupBlockingTask::open(TaskManager::createTask("hex.builtin.task.saving_data", TaskManager::NoProgress, [=](Task &){
                provider->saveAs(path);
            }));
        });
    }

    static void copyBytes(const Region &selection) {
        constexpr static auto Format = "{0:02X} ";

        auto provider = ImHexApi::Provider::get();
        if (provider == nullptr)
            return;

        auto reader = prv::ProviderReader(provider);
        reader.seek(selection.getStartAddress());
        reader.setEndAddress(selection.getEndAddress());

        std::string result;
        result.reserve(fmt::format(Format, 0x00).size() * selection.getSize());

        for (const auto &byte : reader)
            result += fmt::format(Format, byte);
        result.pop_back();

        ImGui::SetClipboardText(result.c_str());
    }

    static void pasteBytes(const Region &selection, bool selectionCheck, bool asPlainText) {
        auto provider = ImHexApi::Provider::get();
        if (provider == nullptr)
            return;

        auto clipboard = ImGui::GetClipboardText();
        if (clipboard == nullptr)
            return;

        std::vector<u8> buffer;
        if (asPlainText) {
            // Directly reinterpret clipboard as an array of bytes
            std::string cp = clipboard;
            buffer = std::vector<u8>(cp.begin(), cp.end());
        }
        else
            buffer = parseHexString(clipboard);

        if (!selectionCheck) {
            if (selection.getStartAddress() + buffer.size() >= provider->getActualSize())
                provider->resize(selection.getStartAddress() + buffer.size());
        }

        // Write bytes
        auto size = selectionCheck ? std::min(buffer.size(), selection.getSize()) : buffer.size();
        provider->write(selection.getStartAddress(), buffer.data(), size);
    }

    void ViewHexEditor::processPasteBehaviour(const Region &selection) {
        if (selection.getSize() > 1) {
            // Apply normal "paste over selection" behaviour when pasting over several bytes
            pasteBytes(selection, true, false);
            return;
        }

        // Selection is over one byte, we have to check the settings to decide the course of action

        auto setting = ContentRegistry::Settings::read<std::string>(
            "hex.builtin.setting.hex_editor",
            "hex.builtin.setting.hex_editor.paste_behaviour",
            "none");

        if (setting == "everything")
            pasteBytes(selection, false, false);
        else if (setting == "selection")
            pasteBytes(selection, true, false);
        else
            this->openPopup<PopupPasteBehaviour>(selection,
                [](const Region &selection, const bool selectionCheck) {
                    ContentRegistry::Settings::write<std::string>(
                        "hex.builtin.setting.hex_editor",
                        "hex.builtin.setting.hex_editor.paste_behaviour",
                        selectionCheck ? "selection" : "everything");
                    pasteBytes(selection, selectionCheck, false);
                });

    }

    static void copyString(const Region &selection) {
        auto provider = ImHexApi::Provider::get();
        if (provider == nullptr)
            return;

        std::string buffer(selection.size, 0x00);
        buffer.reserve(selection.size);
        provider->read(selection.getStartAddress(), buffer.data(), selection.size);

        ImGui::SetClipboardText(buffer.c_str());
    }

    static void copyCustomEncoding(const EncodingFile &customEncoding, const Region &selection) {
        auto provider = ImHexApi::Provider::get();
        if (provider == nullptr)
            return;

        std::vector<u8> buffer(customEncoding.getLongestSequence(), 0x00);
        std::string string;

        u64 offset = selection.getStartAddress();
        while (offset < selection.getEndAddress()) {
            provider->read(offset, buffer.data(), std::min<size_t>(buffer.size(), selection.size - (offset - selection.getStartAddress())));
            auto [result, size] = customEncoding.getEncodingFor(buffer);

            string += result;
            offset += size;
        }

        ImGui::SetClipboardText(string.c_str());
    }

    void ViewHexEditor::registerShortcuts() {

        // Remove selection
        ShortcutManager::addShortcut(this, Keys::Escape, "hex.builtin.view.hex_editor.shortcut.remove_selection", [this] {
            auto provider = ImHexApi::Provider::get();
            if (provider == nullptr)
                return;

            m_selectionStart->reset();
            m_selectionEnd->reset();

            m_hexEditor.setSelectionUnchecked(std::nullopt, std::nullopt);

            EventRegionSelected::post(ImHexApi::HexEditor::ProviderRegion{ Region::Invalid(), provider });
        });

        ShortcutManager::addShortcut(this, Keys::Enter, "hex.builtin.view.hex_editor.shortcut.enter_editing", [this] {
            if (auto cursor = m_hexEditor.getCursorPosition(); cursor.has_value())
                m_hexEditor.setEditingAddress(cursor.value());
        });

        // Move cursor around
        ShortcutManager::addShortcut(this, Keys::Up, "hex.builtin.view.hex_editor.shortcut.cursor_up", [this] {
            auto selection = getSelection();
            auto cursor = m_hexEditor.getCursorPosition().value_or(selection.getEndAddress());

            if (cursor >= m_hexEditor.getBytesPerRow()) {
                auto pos = cursor - m_hexEditor.getBytesPerRow();
                this->setSelection(pos, pos);
                m_hexEditor.jumpIfOffScreen();
            }
        });
        ShortcutManager::addShortcut(this, Keys::Down, "hex.builtin.view.hex_editor.shortcut.cursor_down", [this] {
            auto selection = getSelection();
            auto cursor = m_hexEditor.getCursorPosition().value_or(selection.getEndAddress());

            auto pos = cursor + m_hexEditor.getBytesPerRow();
            this->setSelection(pos, pos);
            m_hexEditor.jumpIfOffScreen();
        });
        ShortcutManager::addShortcut(this, Keys::Left, "hex.builtin.view.hex_editor.shortcut.cursor_left", [this] {
            auto selection = getSelection();
            auto cursor = m_hexEditor.getCursorPosition().value_or(selection.getEndAddress());

            if (cursor > 0) {
                auto pos = cursor - m_hexEditor.getBytesPerCell();
                this->setSelection(pos, pos);
                m_hexEditor.jumpIfOffScreen();
            }
        });
        ShortcutManager::addShortcut(this, Keys::Right, "hex.builtin.view.hex_editor.shortcut.cursor_right", [this] {
            auto selection = getSelection();
            auto cursor = m_hexEditor.getCursorPosition().value_or(selection.getEndAddress());

            auto pos = cursor + m_hexEditor.getBytesPerCell();
            this->setSelection(pos, pos);
            m_hexEditor.jumpIfOffScreen();
        });

        ShortcutManager::addShortcut(this, Keys::PageUp, "hex.builtin.view.hex_editor.shortcut.cursor_page_up", [this] {
            const i64 visibleRowCount = m_hexEditor.getVisibleRowCount();
            m_hexEditor.setScrollPosition(m_hexEditor.getScrollPosition() - visibleRowCount);
        });
        ShortcutManager::addShortcut(this, Keys::PageDown, "hex.builtin.view.hex_editor.shortcut.cursor_page_down", [this] {
            const i64 visibleRowCount = m_hexEditor.getVisibleRowCount();
            m_hexEditor.setScrollPosition(m_hexEditor.getScrollPosition() + visibleRowCount);
        });

        ShortcutManager::addShortcut(this, Keys::Home, "hex.builtin.view.hex_editor.shortcut.cursor_start", [this] {
            auto selection = getSelection();
            auto cursor = m_hexEditor.getCursorPosition().value_or(selection.getEndAddress());

            auto pos = cursor - cursor % m_hexEditor.getBytesPerRow();
            this->setSelection(pos, (pos + m_hexEditor.getBytesPerCell()) - 1);
            m_hexEditor.jumpIfOffScreen();
        });

        ShortcutManager::addShortcut(this, Keys::End, "hex.builtin.view.hex_editor.shortcut.cursor_end", [this] {
            auto selection = getSelection();
            auto cursor = m_hexEditor.getCursorPosition().value_or(selection.getEndAddress());

            auto pos = cursor - cursor % m_hexEditor.getBytesPerRow() + m_hexEditor.getBytesPerRow() - m_hexEditor.getBytesPerCell();
            this->setSelection(pos, (pos + m_hexEditor.getBytesPerCell()) - 1);
            m_hexEditor.jumpIfOffScreen();
        });

        // Move selection around
        ShortcutManager::addShortcut(this, SHIFT + Keys::Up, "hex.builtin.view.hex_editor.shortcut.selection_up", [this] {
            auto selection = getSelection();
            auto cursor = m_hexEditor.getCursorPosition();

            if (cursor != selection.getStartAddress()) {
                auto newCursor = std::max<u64>(cursor.value_or(selection.getEndAddress()), m_hexEditor.getBytesPerRow()) - m_hexEditor.getBytesPerRow();
                setSelection(selection.getStartAddress(), newCursor);
                m_hexEditor.setCursorPosition(newCursor);
            } else {
                auto newCursor = std::max<u64>(cursor.value_or(selection.getEndAddress()), m_hexEditor.getBytesPerRow()) - m_hexEditor.getBytesPerRow();
                setSelection(newCursor, selection.getEndAddress());
                m_hexEditor.setCursorPosition(newCursor);
            }

            m_hexEditor.jumpIfOffScreen();
        });
        ShortcutManager::addShortcut(this, SHIFT + Keys::Down, "hex.builtin.view.hex_editor.shortcut.selection_down", [this] {
            auto selection = getSelection();
            auto cursor = m_hexEditor.getCursorPosition();

            if (cursor != selection.getStartAddress()) {
                auto newCursor = cursor.value_or(selection.getEndAddress()) + m_hexEditor.getBytesPerRow();
                setSelection(selection.getStartAddress(), newCursor);
                m_hexEditor.setCursorPosition(newCursor);
            } else {
                auto newCursor = cursor.value_or(selection.getEndAddress()) + m_hexEditor.getBytesPerRow();
                setSelection(newCursor, selection.getEndAddress());
                m_hexEditor.setCursorPosition(newCursor);
            }

            m_hexEditor.jumpIfOffScreen();
        });
        ShortcutManager::addShortcut(this, SHIFT + Keys::Left, "hex.builtin.view.hex_editor.shortcut.selection_left", [this] {
            auto selection = getSelection();
            auto cursor = m_hexEditor.getCursorPosition();

            if (cursor != selection.getStartAddress()) {
                auto newCursor = cursor.value_or(selection.getEndAddress()) - m_hexEditor.getBytesPerCell();
                setSelection(selection.getStartAddress(), newCursor);
                m_hexEditor.setCursorPosition(newCursor);
            } else {
                auto newCursor = cursor.value_or(selection.getEndAddress()) - m_hexEditor.getBytesPerCell();
                setSelection(newCursor, selection.getEndAddress());
                m_hexEditor.setCursorPosition(newCursor);
            }

            m_hexEditor.jumpIfOffScreen();
        });
        ShortcutManager::addShortcut(this, SHIFT + Keys::Right, "hex.builtin.view.hex_editor.shortcut.selection_right", [this] {
            auto selection = getSelection();
            auto cursor = m_hexEditor.getCursorPosition();

            if (cursor != selection.getStartAddress()) {
                auto newCursor = cursor.value_or(selection.getEndAddress()) + m_hexEditor.getBytesPerCell();
                setSelection(selection.getStartAddress(), newCursor);
                m_hexEditor.setCursorPosition(newCursor);
            } else {
                auto newCursor = cursor.value_or(selection.getEndAddress()) + m_hexEditor.getBytesPerCell();
                setSelection(newCursor, selection.getEndAddress());
                m_hexEditor.setCursorPosition(newCursor);
            }

            m_hexEditor.jumpIfOffScreen();
        });
        ShortcutManager::addShortcut(this, SHIFT + Keys::PageUp, "hex.builtin.view.hex_editor.shortcut.selection_page_up", [this] {
            auto selection = getSelection();
            auto cursor = m_hexEditor.getCursorPosition().value_or(selection.getEndAddress());

            if (cursor != selection.getStartAddress()) {
                auto newCursor = std::max<u64>(cursor, m_hexEditor.getBytesPerRow()) - m_hexEditor.getBytesPerRow() * m_hexEditor.getVisibleRowCount();
                setSelection(selection.getStartAddress(), newCursor);
                m_hexEditor.setCursorPosition(newCursor);
            } else {
                auto newCursor = std::max<u64>(cursor, m_hexEditor.getBytesPerRow()) - m_hexEditor.getBytesPerRow() * m_hexEditor.getVisibleRowCount();
                setSelection(newCursor, selection.getEndAddress());
                m_hexEditor.setCursorPosition(newCursor);
            }

            m_hexEditor.jumpIfOffScreen();
        });
        ShortcutManager::addShortcut(this, SHIFT + Keys::PageDown, "hex.builtin.view.hex_editor.shortcut.selection_page_down", [this] {
            auto selection = getSelection();
            auto cursor = m_hexEditor.getCursorPosition().value_or(selection.getEndAddress());

            if (cursor != selection.getStartAddress()) {
                auto newCursor = cursor + (m_hexEditor.getBytesPerRow() * m_hexEditor.getVisibleRowCount());
                setSelection(selection.getStartAddress(), newCursor);
                m_hexEditor.setCursorPosition(newCursor);
            } else {
                auto newCursor = cursor + (m_hexEditor.getBytesPerRow() * m_hexEditor.getVisibleRowCount());
                setSelection(newCursor, selection.getEndAddress());
                m_hexEditor.setCursorPosition(newCursor);
            }

            m_hexEditor.jumpIfOffScreen();
        });

    }

    void ViewHexEditor::registerEvents() {
        RequestHexEditorSelectionChange::subscribe(this, [this](ImHexApi::HexEditor::ProviderRegion region) {
            auto provider = region.getProvider();

            if (region == Region::Invalid() || provider == nullptr) {
                m_selectionStart->reset();
                m_selectionEnd->reset();
                EventRegionSelected::post(ImHexApi::HexEditor::ProviderRegion({ Region::Invalid(), nullptr }));

                return;
            }

            auto page = provider->getPageOfAddress(region.getStartAddress());
            if (!page.has_value())
                return;

            if (region.size != 0) {
                provider->setCurrentPage(page.value());
                m_hexEditor.setProvider(provider);
                this->setSelection(region);
                this->jumpIfOffScreen();
            }
        });

        EventProviderChanged::subscribe(this, [this](auto *oldProvider, auto *newProvider) {
            if (oldProvider != nullptr) {
                auto selection = m_hexEditor.getSelection();

                if (selection != Region::Invalid()) {
                    m_selectionStart.get(oldProvider)  = selection.getStartAddress();
                    m_selectionEnd.get(oldProvider)    = selection.getEndAddress();
                }
            }

            if (newProvider != nullptr) {
                m_hexEditor.setSelectionUnchecked(m_selectionStart.get(newProvider), m_selectionEnd.get(newProvider));
            } else {
                m_hexEditor.setSelectionUnchecked(std::nullopt, std::nullopt);
            }

            if (isSelectionValid()) {
                EventRegionSelected::post(ImHexApi::HexEditor::ProviderRegion{ this->getSelection(), newProvider });
            }
        });

        EventProviderOpened::subscribe(this, [](auto *) {
           ImHexApi::HexEditor::clearSelection();
        });

        EventHighlightingChanged::subscribe(this, [this]{
            auto provider = ImHexApi::Provider::get();
            if (provider == nullptr)
                return;

            m_foregroundHighlights.get(provider).clear();
            m_backgroundHighlights.get(provider).clear();
        });

        ProjectFile::registerPerProviderHandler({
            .basePath = "custom_encoding.tbl",
            .required = false,
            .load = [this](prv::Provider *, const std::fs::path &basePath, const Tar &tar) {
                if (!tar.contains(basePath))
                    return true;

                auto content = tar.readString(basePath);
                if (!content.empty())
                    m_hexEditor.setCustomEncoding(EncodingFile(hex::EncodingFile::Type::Thingy, content));

                return true;
            },
            .store = [this](prv::Provider *, const std::fs::path &basePath, const Tar &tar) {
                if (const auto &encoding = m_hexEditor.getCustomEncoding(); encoding.has_value()) {
                    auto content = encoding->getTableContent();

                    if (!content.empty())
                        tar.writeString(basePath, encoding->getTableContent());
                }

                return true;
            }
        });

        ContentRegistry::Settings::onChange("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.bytes_per_row", [this](const ContentRegistry::Settings::SettingsValue &value) {
            m_hexEditor.setBytesPerRow(value.get<int>(m_hexEditor.getBytesPerRow()));
        });
        ContentRegistry::Settings::onChange("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.highlight_color", [this](const ContentRegistry::Settings::SettingsValue &value) {
            m_hexEditor.setSelectionColor(value.get<int>(0x60C08080));
        });
        ContentRegistry::Settings::onChange("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.sync_scrolling", [this](const ContentRegistry::Settings::SettingsValue &value) {
            m_hexEditor.enableSyncScrolling(value.get<bool>(false));
        });
        ContentRegistry::Settings::onChange("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.byte_padding", [this](const ContentRegistry::Settings::SettingsValue &value) {
            m_hexEditor.setByteCellPadding(value.get<int>(0));
        });
        ContentRegistry::Settings::onChange("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.char_padding", [this](const ContentRegistry::Settings::SettingsValue &value) {
            m_hexEditor.setCharacterCellPadding(value.get<int>(0));
        });

        static bool showSelectionInWindowFooter = true;
        ContentRegistry::Settings::onChange("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.show_selection", [this](const ContentRegistry::Settings::SettingsValue &value) {
            const auto show = value.get<bool>(false);

            m_hexEditor.setShowSelectionInFooter(show);
            showSelectionInWindowFooter = !show;
        });

        ContentRegistry::UserInterface::addFooterItem([] {
            if (!showSelectionInWindowFooter) return;

            if (auto selection = ImHexApi::HexEditor::getSelection(); selection.has_value()) {
                ImGuiExt::TextFormatted("0x{0:02X} - 0x{1:02X} (0x{2:02X} | {2} bytes)",
                    selection->getStartAddress(),
                    selection->getEndAddress(),
                    selection->getSize()
                );
            }
        });
    }

    void ViewHexEditor::registerMenuItems() {
        /* Undo */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.undo" }, ICON_VS_DISCARD, 1000, CTRLCMD + Keys::Z, [] {
            auto provider = ImHexApi::Provider::get();
                provider->undo();
        }, [&] { return ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->canUndo(); },
        this);

        /* Redo */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.redo" }, ICON_VS_REDO, 1050, CTRLCMD + Keys::Y, [] {
            auto provider = ImHexApi::Provider::get();
                provider->redo();
        }, [&] { return ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->canRedo(); },
        this);

        ContentRegistry::UserInterface::addMenuItemSeparator({ "hex.builtin.menu.file" }, 1300);

        /* Save */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.view.hex_editor.menu.file.save" }, ICON_VS_SAVE, 1350,
                                                CTRLCMD + Keys::S,
                                                save,
                                                [] {
                                                    auto provider      = ImHexApi::Provider::get();
                                                    bool providerValid = ImHexApi::Provider::isValid();

                                                    return providerValid && provider->isWritable() && provider->isSavable() && provider->isDirty();
                                                },
                                                this);

        /* Save As */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.view.hex_editor.menu.file.save_as" }, ICON_VS_SAVE_AS, 1375,
                                                CTRLCMD + SHIFT + Keys::S,
                                                saveAs,
                                                [] {
                                                    auto provider      = ImHexApi::Provider::get();
                                                    bool providerValid = ImHexApi::Provider::isValid();

                                                    return providerValid && provider->isDumpable();
                                                },
                                                this);

        /* Load Encoding File */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.import", "hex.builtin.menu.file.import.custom_encoding" }, "あ", 5700, Shortcut::None,
                                                [this]{
                                                    const auto basePaths = paths::Encodings.read();
                                                    std::vector<std::fs::path> paths;
                                                    for (const auto &path : basePaths) {
                                                        std::error_code error;
                                                        for (const auto &entry : std::fs::recursive_directory_iterator(path, error)) {
                                                            if (!entry.is_regular_file()) continue;

                                                            paths.push_back(entry);
                                                        }
                                                    }

                                                    ui::PopupFileChooser::open(basePaths, paths, std::vector<hex::fs::ItemFilter>{ {"Thingy Table File", "tbl"} }, false,
                                                    [this](const auto &path) {
                                                        TaskManager::createTask("hex.builtin.task.loading_encoding_file", 0, [this, path](auto&) {
                                                            auto encoding = EncodingFile(EncodingFile::Type::Thingy, path);
                                                            ImHexApi::Provider::markDirty();

                                                            TaskManager::doLater([this, encoding = std::move(encoding)]() mutable {
                                                                m_hexEditor.setCustomEncoding(std::move(encoding));
                                                            });
                                                        });
                                                    });
                                                },
                                                ImHexApi::Provider::isValid,
                                                this);

        ContentRegistry::UserInterface::addMenuItemSeparator({ "hex.builtin.menu.file" }, 1500, this);

        /* Search */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.view.hex_editor.menu.file.search" }, ICON_VS_SEARCH, 1550,
                                                CTRLCMD + Keys::F,
                                                [this] {
                                                    this->openPopup<PopupFind>(this);
                                                },
                                                ImHexApi::Provider::isValid,
                                                this);

        /* Goto */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.view.hex_editor.menu.file.goto" }, ICON_VS_DEBUG_STEP_INTO, 1600,
                                                CTRLCMD + Keys::G,
                                                [this] {
                                                    this->openPopup<PopupGoto>();
                                                },
                                                ImHexApi::Provider::isValid,
                                                this);

        /* Skip until */
        ContentRegistry::UserInterface::addMenuItemSubMenu({ "hex.builtin.menu.file", "hex.builtin.view.hex_editor.menu.file.skip_until" }, ICON_VS_DEBUG_STEP_OVER, 1610,
                                                []{},
                                                canSearchForDifferingByte,
                                                this);

        /* Skip until previous differing byte */
        ContentRegistry::UserInterface::addMenuItem({
                                                    "hex.builtin.menu.file",
                                                    "hex.builtin.view.hex_editor.menu.file.skip_until",
                                                    "hex.builtin.view.hex_editor.menu.file.skip_until.previous_differing_byte"
                                                },
                                                ICON_VS_DEBUG_STEP_BACK,
                                                1620,
                                                CTRLCMD + Keys::LeftBracket,
                                                [] {
                                                    bool didFindNextValue = false;
                                                    bool didReachBeginning = false;
                                                    u64 foundAddress;

                                                    findNextDifferingByte(
                                                        [] (prv::Provider* provider) -> u64 {
                                                            return provider->getBaseAddress();
                                                        },
                                                        [] (u64 currentAddress, u64 endAddress) -> bool {
                                                            return currentAddress > endAddress;
                                                        },
                                                        [] (u64* currentAddress) {
                                                            (*currentAddress)--;
                                                        },
                                                        &didFindNextValue,
                                                        &didReachBeginning,
                                                        &foundAddress
                                                    );

                                                    if (didFindNextValue) {
                                                        ImHexApi::HexEditor::setSelection(foundAddress, 1);
                                                    }

                                                    if (!didFindNextValue && didReachBeginning) {
                                                        ui::ToastInfo::open("hex.builtin.view.hex_editor.menu.file.skip_until.beginning_reached"_lang);
                                                    }
                                                },
                                                canSearchForDifferingByte,
                                                this);

        /* Skip until next differing byte */
        ContentRegistry::UserInterface::addMenuItem({
                                                    "hex.builtin.menu.file",
                                                    "hex.builtin.view.hex_editor.menu.file.skip_until",
                                                    "hex.builtin.view.hex_editor.menu.file.skip_until.next_differing_byte"
                                                },
                                                ICON_VS_DEBUG_STEP_OVER,
                                                1630,
                                                CTRLCMD + Keys::RightBracket,
                                                [] {
                                                    bool didFindNextValue = false;
                                                    bool didReachEnd = false;
                                                    u64 foundAddress;

                                                    findNextDifferingByte(
                                                        [] (prv::Provider* provider) -> u64 {
                                                            return provider->getBaseAddress() + provider->getActualSize() - 1;
                                                        },
                                                        [] (u64 currentAddress, u64 endAddress) -> bool {
                                                            return currentAddress < endAddress;
                                                        },
                                                        [] (u64* currentAddress) {
                                                            (*currentAddress)++;
                                                        },
                                                        &didFindNextValue,
                                                        &didReachEnd,
                                                        &foundAddress
                                                    );

                                                    if (didFindNextValue) {
                                                        ImHexApi::HexEditor::setSelection(foundAddress, 1);
                                                    }

                                                    if (!didFindNextValue && didReachEnd) {
                                                        ui::ToastInfo::open("hex.builtin.view.hex_editor.menu.file.skip_until.end_reached"_lang);
                                                    }
                                                },
                                                canSearchForDifferingByte,
                                                this);


        ContentRegistry::UserInterface::addMenuItemSeparator({ "hex.builtin.menu.edit" }, 1100, this);

        /* Copy */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.copy" }, ICON_VS_COPY, 1150,
                                                CurrentView + CTRLCMD + Keys::C,
                                                [] {
                                                    auto selection = ImHexApi::HexEditor::getSelection();
                                                    if (selection.has_value() && selection != Region::Invalid())
                                                        copyBytes(*selection);
                                                },
                                                ImHexApi::HexEditor::isSelectionValid,
                                                this);

        ContentRegistry::UserInterface::addMenuItemSubMenu({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.copy_as" }, ICON_VS_PREVIEW, 1190, []{}, ImHexApi::HexEditor::isSelectionValid, this);

        /* Copy As */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.copy_as", "hex.builtin.view.hex_editor.copy.ascii" }, ICON_VS_SYMBOL_KEY, 1200,
                                                CurrentView + CTRLCMD + ALT + Keys::C,
                                                [] {
                                                    auto selection = ImHexApi::HexEditor::getSelection();
                                                    if (selection.has_value() && selection != Region::Invalid())
                                                        copyString(*selection);
                                                },
                                                ImHexApi::HexEditor::isSelectionValid,
                                                this);

        /* Copy address */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.copy_as", "hex.builtin.view.hex_editor.copy.address" }, ICON_VS_LOCATION, 1250,
                                                Shortcut::None,
                                                [] {
                                                    auto selection = ImHexApi::HexEditor::getSelection();
                                                    if (selection.has_value() && selection != Region::Invalid())
                                                        ImGui::SetClipboardText(fmt::format("0x{:08X}", selection->getStartAddress()).c_str());
                                                },
                                                ImHexApi::HexEditor::isSelectionValid,
                                                this);

        /* Copy custom encoding */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.copy_as", "hex.builtin.view.hex_editor.copy.custom_encoding" }, "あ", 1300,
                                                SHIFT + ALT + Keys::C,
                                                [this] {
                                                    auto selection = ImHexApi::HexEditor::getSelection();
                                                    auto customEncoding = m_hexEditor.getCustomEncoding();
                                                    if (customEncoding.has_value() && selection.has_value() && selection != Region::Invalid())
                                                        copyCustomEncoding(*customEncoding, *selection);
                                                },
                                                [this] {
                                                    return ImHexApi::HexEditor::isSelectionValid() && m_hexEditor.getCustomEncoding().has_value();
                                                },
                                                this);

        ContentRegistry::UserInterface::addMenuItemSeparator({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.copy_as" }, 1350, this);

        /* Copy as... */
        ContentRegistry::UserInterface::addMenuItemSubMenu({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.copy_as" }, ICON_VS_FILE_CODE, 1400, []{
            auto selection = ImHexApi::HexEditor::getSelection();
            auto provider  = ImHexApi::Provider::get();

            bool enabled = ImHexApi::HexEditor::isSelectionValid();
            for (const auto &[unlocalizedName, callback] : ContentRegistry::DataFormatter::impl::getExportMenuEntries()) {
                if (menu::menuItem(Lang(unlocalizedName), Shortcut::None, false, enabled)) {
                    ImGui::SetClipboardText(
                            callback(
                                    provider,
                                    selection->getStartAddress(),
                                    selection->size,
                                    false
                            ).c_str()
                    );
                }

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                    const auto previewText = callback(
                        provider,
                        selection->getStartAddress(),
                        std::min<u64>(selection->size, 32),
                        true
                    );

                    if (!previewText.empty()) {
                        if (ImGui::BeginTooltip()) {
                            ImGuiExt::Header("hex.builtin.view.hex_editor.menu.edit.copy_as.preview"_lang, true);
                            ImGui::TextDisabled("%s", previewText.c_str());
                            ImGui::EndTooltip();
                        }
                    }
                }
            }
        },
        [] {
            return ImHexApi::Provider::isValid() && ImHexApi::HexEditor::isSelectionValid();
        }, this);

        /* Paste */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.paste" }, ICON_VS_OUTPUT, 1450, CurrentView + CTRLCMD + Keys::V,
                                                [this] {
                                                    processPasteBehaviour(ImHexApi::HexEditor::getSelection().value_or( ImHexApi::HexEditor::ProviderRegion(Region { .address=0, .size=0 }, ImHexApi::Provider::get())));
                                                },
                                                ImHexApi::HexEditor::isSelectionValid,
                                                this);

        /* Paste... */
        ContentRegistry::UserInterface::addMenuItemSubMenu({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.paste_as" }, ICON_VS_CLIPPY, 1490, []{}, ImHexApi::HexEditor::isSelectionValid, this);

        /* Paste... > Paste all */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.paste_as", "hex.builtin.view.hex_editor.menu.edit.paste_all" }, ICON_VS_CLIPPY, 1500, CurrentView + CTRLCMD + SHIFT + Keys::V,
                                                [] {
                                                    pasteBytes(ImHexApi::HexEditor::getSelection().value_or( ImHexApi::HexEditor::ProviderRegion(Region { .address=0, .size=0 }, ImHexApi::Provider::get())), false, false);
                                                },
                                                ImHexApi::HexEditor::isSelectionValid,
                                                this);

        /* Paste... > Paste all as string */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.paste_as", "hex.builtin.view.hex_editor.menu.edit.paste_all_string" }, ICON_VS_SYMBOL_KEY, 1510,
                                                Shortcut::None,
                                                [] {
                                                    pasteBytes(ImHexApi::HexEditor::getSelection().value_or( ImHexApi::HexEditor::ProviderRegion(Region { .address=0, .size=0 }, ImHexApi::Provider::get())), false, true);
                                                },
                                                ImHexApi::HexEditor::isSelectionValid,
                                                this);

        /* Select */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.select" }, ICON_VS_LIST_SELECTION, 1525,
                                                CTRLCMD + SHIFT + Keys::A,
                                                [this] {
                                                    auto selection = ImHexApi::HexEditor::getSelection().value_or(ImHexApi::HexEditor::ProviderRegion{ { .address=0, .size=1 }, nullptr });
                                                    this->openPopup<PopupSelect>(selection.getStartAddress(), selection.getSize());
                                                },
                                                ImHexApi::Provider::isValid,
                                                this);

        /* Select All */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.select_all" }, ICON_VS_LIST_FLAT, 1550, CurrentView + CTRLCMD + Keys::A,
                                                [] {
                                                    auto provider = ImHexApi::Provider::get();
                                                    ImHexApi::HexEditor::setSelection(provider->getBaseAddress(), provider->getActualSize());
                                                },
                                                ImHexApi::HexEditor::isSelectionValid,
                                                this);


        ContentRegistry::UserInterface::addMenuItemSeparator({ "hex.builtin.menu.edit" }, 1600, this);

        /* Set Base Address */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.set_base" }, ICON_VS_LOCATION, 1650, Shortcut::None,
                                                [this] {
                                                    auto provider = ImHexApi::Provider::get();
                                                    this->openPopup<PopupBaseAddress>(provider->getBaseAddress());
                                                },
                                                [] { return ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->isReadable(); },
                                                this);

        /* Resize */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.resize" }, ICON_VS_ARROW_BOTH, 1700, Shortcut::None,
                                                [this] {
                                                    auto provider = ImHexApi::Provider::get();
                                                    this->openPopup<PopupResize>(provider->getActualSize());
                                                },
                                                [] { return ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->isResizable(); },
                                                this);

        /* Insert */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.insert" }, ICON_VS_INSERT, 1750, Shortcut::None,
                                                [this] {
                                                    auto selection      = ImHexApi::HexEditor::getSelection();

                                                    this->openPopup<PopupInsert>(selection->getStartAddress(), 0x00);
                                                },
                                                [] { return ImHexApi::HexEditor::isSelectionValid() && ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->isResizable(); },
                                                this);

        /* Remove */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.remove" }, ICON_VS_CLEAR_ALL, 1800, Shortcut::None,
                                                [this] {
                                                    auto selection = ImHexApi::HexEditor::getSelection();

                                                    this->openPopup<PopupRemove>(selection->getStartAddress(), selection->getSize());
                                                },
                                                [] { return ImHexApi::HexEditor::isSelectionValid() && ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->isResizable(); },
                                                this);

        /* Fill */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.fill" }, ICON_VS_PAINTCAN, 1810, Shortcut::None,
                                                [this] {
                                                    auto selection = ImHexApi::HexEditor::getSelection();

                                                    this->openPopup<PopupFill>(selection->getStartAddress(), selection->getSize());
                                                },
                                                [] { return ImHexApi::HexEditor::isSelectionValid() && ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->isWritable(); },
                                                this);

        /* Toggle Overwrite/Insert mode */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.insert_mode" }, ICON_VS_EDIT, 1820, Shortcut::None,
                                                [this] {
                                                    if (m_hexEditor.getMode() == ui::HexEditor::Mode::Insert)
                                                        m_hexEditor.setMode(ui::HexEditor::Mode::Overwrite);
                                                    else
                                                        m_hexEditor.setMode(ui::HexEditor::Mode::Insert);
                                                },
                                                [] {
                                                    return ImHexApi::HexEditor::isSelectionValid() &&
                                                           ImHexApi::Provider::isValid() &&
                                                           ImHexApi::Provider::get()->isWritable() &&
                                                           ImHexApi::Provider::get()->isResizable();
                                                },
                                                [this] {
                                                    return m_hexEditor.getMode() == ui::HexEditor::Mode::Insert;
                                                },
                                                this);

        /* Jump to */
        ContentRegistry::UserInterface::addMenuItemSubMenu({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.jump_to" }, ICON_VS_DEBUG_STEP_OUT, 1850,
                                                [] {
                                                    auto provider = ImHexApi::Provider::get();
                                                    if (provider == nullptr)
                                                        return;
                                                    const auto selection  = ImHexApi::HexEditor::getSelection();
                                                    if (!selection.has_value())
                                                        return;
                                                    if (selection->getSize() > sizeof(u64))
                                                        return;

                                                    u64 value = 0;
                                                    provider->read(selection->getStartAddress(), &value, selection->getSize());

                                                    auto littleEndianValue = hex::changeEndianness(value, selection->size, std::endian::little);
                                                    auto bigEndianValue    = hex::changeEndianness(value, selection->size,  std::endian::big);

                                                    auto canJumpTo = [provider](u64 value) {
                                                        return (value >= provider->getBaseAddress()) && (value < (provider->getBaseAddress() + provider->getActualSize()));
                                                    };

                                                    ImGui::PushID(1);
                                                    if (menu::menuItem(fmt::format("{} | 0x{:08X}", "hex.ui.common.little_endian"_lang, littleEndianValue).c_str(), Shortcut::None, false, canJumpTo(littleEndianValue))) {
                                                        ImHexApi::HexEditor::setSelection(littleEndianValue, 1);
                                                    }
                                                    ImGui::PopID();

                                                    ImGui::PushID(2);
                                                    if (menu::menuItem(fmt::format("{} | 0x{:08X}", "hex.ui.common.big_endian"_lang, bigEndianValue).c_str(), Shortcut::None, false, canJumpTo(bigEndianValue))) {
                                                        ImHexApi::HexEditor::setSelection(bigEndianValue, 1);
                                                    }
                                                    ImGui::PopID();

                                                    menu::menuSeparator();

                                                    if (menu::menuItem("hex.builtin.view.hex_editor.menu.edit.jump_to.curr_pattern"_lang, Shortcut::None, false, selection.has_value() && ContentRegistry::PatternLanguage::getRuntime().getCreatedPatternCount() > 0)) {
                                                        auto patterns = ContentRegistry::PatternLanguage::getRuntime().getPatternsAtAddress(selection->getStartAddress());

                                                        if (!patterns.empty())
                                                            RequestJumpToPattern::post(patterns.front());
                                                    }
                                                },
                                                [] { return ImHexApi::Provider::isValid() && ImHexApi::HexEditor::isSelectionValid() && ImHexApi::HexEditor::getSelection()->getSize() <= sizeof(u64); },
                                                this);

        /* Set Page Size */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.set_page_size" }, ICON_VS_BROWSER, 1860, Shortcut::None,
                                                [this] {
                                                    auto provider = ImHexApi::Provider::get();
                                                    this->openPopup<PopupPageSize>(provider->getPageSize());
                                                },
                                                [] { return ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->isReadable(); },
                                                this);

        ContentRegistry::UserInterface::addMenuItemSeparator({ "hex.builtin.menu.edit" }, 1900, this);

        /* Open in new provider */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.open_in_new_provider" }, ICON_VS_GO_TO_FILE, 1950, Shortcut::None,
                                                [] {
                                                    auto selection = ImHexApi::HexEditor::getSelection();

                                                    auto newProvider = ImHexApi::Provider::createProvider("hex.builtin.provider.view", true);
                                                    if (auto *viewProvider = dynamic_cast<ViewProvider*>(newProvider.get()); viewProvider != nullptr) {
                                                        viewProvider->setProvider(selection->getStartAddress(), selection->getSize(), selection->getProvider());
                                                        ImHexApi::Provider::openProvider(newProvider);
                                                    }
                                                },
                                                [] { return ImHexApi::HexEditor::isSelectionValid() && ImHexApi::Provider::isValid(); },
                                                this);

        /* Decode as Text */
        ContentRegistry::UserInterface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.decode_as_text" }, ICON_VS_CHAT_SPARKLE, 1960, Shortcut::None,
                                                [this] {
                                                    const auto selection = ImHexApi::HexEditor::getSelection();

                                                    TaskManager::createTask("", TaskManager::NoProgress, [this, selection] {
                                                        const auto &customEncoding = this->m_hexEditor.getCustomEncoding();
                                                        if (!customEncoding.has_value())
                                                            return;

                                                        std::vector<u8> buffer(selection->getSize());
                                                        selection->getProvider()->read(selection->getStartAddress(), buffer.data(), buffer.size());

                                                        auto decodedString = customEncoding->decodeAll(buffer);
                                                        TaskManager::doLater([this, decodedString = std::move(decodedString)]() mutable {
                                                            this->openPopup<PopupDecodedString>(std::move(decodedString));
                                                        });
                                                    });
                                                },
                                                [this] {
                                                    return  ImHexApi::HexEditor::isSelectionValid() &&
                                                            ImHexApi::Provider::isValid() &&
                                                            this->m_hexEditor.getCustomEncoding().has_value();
                                                },
                                                this);
    }

    void ViewHexEditor::drawHelpText() {
        ImGuiExt::TextFormattedWrapped("This is the main view you'll be working with. It shows the raw data of the currently opened data source together with any highlighting that may be generated by other parts of ImHex.");
        ImGui::NewLine();
        ImGuiExt::TextFormattedWrapped("You can edit cells by simply double clicking them. By default, this will overwrite the existing value of that cell. To insert new bytes instead, toggle the {} option in the {} menu.",
            "hex.builtin.view.hex_editor.menu.edit.insert_mode"_lang, "hex.builtin.menu.edit"_lang
        );
        ImGui::NewLine();
        ImGuiExt::TextFormattedWrapped("Many more options can be found by right clicking anywhere in this view, or by using the toggles and options in the footer bar. This includes options to change the number of bytes shown per row, the encoding used to display text, the minimap and the cell's data format.",
            "hex.builtin.view.hex_editor.menu.edit.insert_mode"_lang, "hex.builtin.menu.edit"_lang
        );
    }

}
