#include "content/views/view_hex_editor.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/api/shortcut_manager.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/api/achievement_manager.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/crypto.hpp>
#include <hex/helpers/default_paths.hpp>

#include <hex/providers/buffered_reader.hpp>

#include <wolv/math_eval/math_evaluator.hpp>

#include <content/providers/view_provider.hpp>

#include <fonts/vscode_icons.hpp>

#include <popups/popup_file_chooser.hpp>
#include <content/popups/popup_blocking_task.hpp>
#include <content/popups/hex_editor/popup_hex_editor_find.hpp>
#include <pl/patterns/pattern.hpp>
#include <ui/menu_items.hpp>
#include <wolv/literals.hpp>

using namespace std::literals::string_literals;
using namespace wolv::literals;

namespace hex::plugin::builtin {

    /* Popups */

    class PopupGoto : public ViewHexEditor::Popup {
    public:
        void draw(ViewHexEditor *editor) override {
            if (ImGui::BeginTabBar("goto_tabs")) {
                if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.goto.offset.absolute"_lang)) {
                    m_mode = Mode::Absolute;
                    ImGui::EndTabItem();
                }

                ImGui::BeginDisabled(!editor->isSelectionValid());
                if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.goto.offset.relative"_lang)) {
                    m_mode = Mode::Relative;
                    ImGui::EndTabItem();
                }
                ImGui::EndDisabled();

                if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.goto.offset.begin"_lang)) {
                    m_mode = Mode::Begin;
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.goto.offset.end"_lang)) {
                    m_mode = Mode::End;
                    ImGui::EndTabItem();
                }

                if (m_requestFocus){
                    ImGui::SetKeyboardFocusHere();
                    m_requestFocus = false;
                }
                if (ImGuiExt::InputTextIcon("##input", ICON_VS_SYMBOL_OPERATOR, m_input)) {
                    if (auto result = m_evaluator.evaluate(m_input); result.has_value()) {
                        const auto inputResult = result.value();
                        auto provider = ImHexApi::Provider::get();

                        switch (m_mode) {
                            case Mode::Absolute: {
                                m_newAddress = inputResult;
                            }
                                break;
                            case Mode::Relative: {
                                const auto selection = editor->getSelection();
                                m_newAddress = selection.getStartAddress() + inputResult;
                            }
                                break;
                            case Mode::Begin: {
                                m_newAddress = provider->getBaseAddress() + provider->getCurrentPageAddress() + inputResult;
                            }
                                break;
                            case Mode::End: {
                                m_newAddress = provider->getActualSize() - inputResult;
                            }
                                break;
                        }
                    } else {
                        m_newAddress.reset();
                    }
                }

                bool isOffsetValid = m_newAddress <= ImHexApi::Provider::get()->getActualSize();

                bool executeGoto = false;

                if (ImGui::IsWindowFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) {
                    executeGoto = true;
                }

                ImGui::BeginDisabled(!m_newAddress.has_value() || !isOffsetValid);
                {
                    const auto label = hex::format("{} {}", "hex.builtin.view.hex_editor.menu.file.goto"_lang, m_newAddress.has_value() ? hex::format("0x{:08X}", *m_newAddress) : "???");
                    const auto buttonWidth = ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2;
                    if (ImGuiExt::DimmedButton(label.c_str(), ImVec2(buttonWidth, 0))) {
                        executeGoto = true;
                    }
                }
                ImGui::EndDisabled();

                if (executeGoto && m_newAddress.has_value()) {
                    editor->setSelection(*m_newAddress, *m_newAddress);
                    editor->jumpToSelection();

                    if (!this->isPinned())
                        editor->closePopup();
                }

                ImGui::EndTabBar();
            }
        }

        [[nodiscard]] UnlocalizedString getTitle() const override {
            return "hex.builtin.view.hex_editor.menu.file.goto";
        }

        bool canBePinned() const override {
            return true;
        }

    private:
        enum class Mode : u8 {
            Absolute,
            Relative,
            Begin,
            End
        };

        Mode m_mode = Mode::Absolute;
        std::optional<u64> m_newAddress;

        bool m_requestFocus = true;
        std::string m_input = "0x";
        wolv::math_eval::MathEvaluator<i128> m_evaluator;
    };

    class PopupSelect : public ViewHexEditor::Popup {
    public:
        
        PopupSelect(u64 address, size_t size): m_region({address, size}) {}

        void draw(ViewHexEditor *editor) override {
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

                    m_region = { inputA, (inputB - inputA) + 1 };

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

                    m_region = { inputA, inputB };
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

        [[nodiscard]] UnlocalizedString getTitle() const override {
            return "hex.builtin.view.hex_editor.menu.edit.select";
        }

        [[nodiscard]] bool canBePinned() const override {
            return true;
        }

    private:
        Region m_region = { 0, 1 };
        bool m_justOpened = true;
    };

    class PopupBaseAddress : public ViewHexEditor::Popup {
    public:
        explicit PopupBaseAddress(u64 baseAddress) : m_baseAddress(baseAddress) { }

        void draw(ViewHexEditor *editor) override {
            ImGuiExt::InputHexadecimal("##base_address", &m_baseAddress);
            if (ImGui::IsItemFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) {
                setBaseAddress(m_baseAddress);
                editor->closePopup();
            }

            ImGuiExt::ConfirmButtons("hex.ui.common.set"_lang, "hex.ui.common.cancel"_lang,
                [&, this]{
                    setBaseAddress(m_baseAddress);
                    editor->closePopup();
                },
                [&]{
                    editor->closePopup();
                }
            );
        }

        [[nodiscard]] UnlocalizedString getTitle() const override {
            return "hex.builtin.view.hex_editor.menu.edit.set_base";
        }

    private:
        static void setBaseAddress(u64 baseAddress) {
            if (ImHexApi::Provider::isValid())
                ImHexApi::Provider::get()->setBaseAddress(baseAddress);
        }

    private:
        u64 m_baseAddress;
    };

    class PopupPageSize : public ViewHexEditor::Popup {
    public:
        explicit PopupPageSize(u64 pageSize) : m_pageSize(pageSize) { }

        void draw(ViewHexEditor *editor) override {
            ImGuiExt::InputHexadecimal("##page_size", &m_pageSize);
            if (ImGui::IsItemFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) {
                setPageSize(m_pageSize);
                editor->closePopup();
            }

            ImGuiExt::ConfirmButtons("hex.ui.common.set"_lang, "hex.ui.common.cancel"_lang,
                [&, this]{
                    setPageSize(m_pageSize);
                    editor->closePopup();
                },
                [&]{
                    editor->closePopup();
                }
            );
        }

        [[nodiscard]] UnlocalizedString getTitle() const override {
            return "hex.builtin.view.hex_editor.menu.edit.set_page_size";
        }

    private:
        static void setPageSize(u64 pageSize) {
            if (ImHexApi::Provider::isValid()) {
                auto provider = ImHexApi::Provider::get();

                provider->setPageSize(pageSize);
                provider->setCurrentPage(0);
            }
        }

    private:
        u64 m_pageSize;
    };

    class PopupResize : public ViewHexEditor::Popup {
    public:
        explicit PopupResize(u64 currSize) : m_size(currSize) {}

        void draw(ViewHexEditor *editor) override {
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

        [[nodiscard]] UnlocalizedString getTitle() const override {
            return "hex.builtin.view.hex_editor.menu.edit.resize";
        }

    private:
        static void resize(size_t newSize) {
            if (ImHexApi::Provider::isValid())
                ImHexApi::Provider::get()->resize(newSize);
        }

    private:
        u64 m_size;
    };

    class PopupInsert : public ViewHexEditor::Popup {
    public:
        PopupInsert(u64 address, size_t size) : m_address(address), m_size(size) {}

        void draw(ViewHexEditor *editor) override {
            ImGuiExt::InputHexadecimal("hex.ui.common.address"_lang, &m_address);
            ImGuiExt::InputHexadecimal("hex.ui.common.size"_lang, &m_size);

            ImGuiExt::ConfirmButtons("hex.ui.common.set"_lang, "hex.ui.common.cancel"_lang,
                [&, this]{
                    insert(m_address, m_size);
                    editor->closePopup();
                },
                [&]{
                    editor->closePopup();
                });

            if (ImGui::IsWindowFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) {
                insert(m_address, m_size);
                editor->closePopup();
            }
        }

        [[nodiscard]] UnlocalizedString getTitle() const override {
            return "hex.builtin.view.hex_editor.menu.edit.insert";
        }

    private:
        static void insert(u64 address, size_t size) {
            if (ImHexApi::Provider::isValid())
                ImHexApi::Provider::get()->insert(address, size);
        }

    private:
        u64 m_address;
        u64 m_size;
    };

    class PopupRemove : public ViewHexEditor::Popup {
    public:
        PopupRemove(u64 address, size_t size) : m_address(address), m_size(size) {}

        void draw(ViewHexEditor *editor) override {
            ImGuiExt::InputHexadecimal("hex.ui.common.address"_lang, &m_address);
            ImGuiExt::InputHexadecimal("hex.ui.common.size"_lang, &m_size);

            ImGuiExt::ConfirmButtons("hex.ui.common.set"_lang, "hex.ui.common.cancel"_lang,
                [&, this]{
                    remove(m_address, m_size);
                    editor->closePopup();
                },
                [&]{
                    editor->closePopup();
                });

            if (ImGui::IsWindowFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) {
                remove(m_address, m_size);
                editor->closePopup();
            }
        }

        [[nodiscard]] UnlocalizedString getTitle() const override {
            return "hex.builtin.view.hex_editor.menu.edit.remove";
        }

    private:
        static void remove(u64 address, size_t size) {
            if (ImHexApi::Provider::isValid())
                ImHexApi::Provider::get()->remove(address, size);
        }

    private:
        u64 m_address;
        u64 m_size;
    };

    class PopupFill : public ViewHexEditor::Popup {
    public:
        PopupFill(u64 address, size_t size) : m_address(address), m_size(size) {}

        void draw(ViewHexEditor *editor) override {
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

        [[nodiscard]] UnlocalizedString getTitle() const override {
            return "hex.builtin.view.hex_editor.menu.edit.fill";
        }

    private:
        static void fill(u64 address, size_t size, std::string input) {
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

    private:
        u64 m_address;
        u64 m_size;

        std::string m_input;
    };

    class PopupPasteBehaviour final : public ViewHexEditor::Popup {
    public:
        explicit PopupPasteBehaviour(const Region &selection, const auto &pasteCallback) : m_selection(), m_pasteCallback(pasteCallback) {
            m_selection = Region { .address=selection.getStartAddress(), .size=selection.getSize() };
        }

        void draw(ViewHexEditor *editor) override {
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

        [[nodiscard]] UnlocalizedString getTitle() const override {
            return "hex.builtin.view.hex_editor.menu.edit.paste.popup.title"_lang;
        }

    private:
        Region m_selection;
        std::function<void(const Region &selection, bool selectionCheck)> m_pasteCallback;
    };

    /* Hex Editor */

    ViewHexEditor::ViewHexEditor() : View::Window("hex.builtin.view.hex_editor.name", ICON_VS_FILE_BINARY) {
        m_hexEditor.setForegroundHighlightCallback([this](u64 address, const u8 *data, size_t size) -> std::optional<color_t> {
            if (auto highlight = m_foregroundHighlights->find(address); highlight != m_foregroundHighlights->end())
                return highlight->second;

            std::optional<color_t> result;
            for (const auto &[id, callback] : ImHexApi::HexEditor::impl::getForegroundHighlightingFunctions()) {
                if (auto color = callback(address, data, size, result.has_value()); color.has_value())
                    result = color;
            }

            if (!result.has_value()) {
                for (const auto &[id, highlighting] : ImHexApi::HexEditor::impl::getForegroundHighlights()) {
                    if (highlighting.getRegion().overlaps({ address, size }))
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
                    if (highlighting.getRegion().overlaps({ address, size })) {
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
                if (tooltip.getRegion().overlaps({ address, size })) {
                    ImGui::BeginTooltip();
                    if (ImGui::BeginTable("##tooltips", 1, ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoClip)) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        ImGui::ColorButton(tooltip.getValue().c_str(), ImColor(tooltip.getColor()));
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

        ContentRegistry::Settings::write<int>("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.bytes_per_row", m_hexEditor.getBytesPerRow());
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
            if (ImGui::Begin(hex::format("##{}", m_currPopup->getTitle().get()).c_str(), &open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking)) {
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
        if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && !ImGui::IsAnyItemHovered() && !ImGui::IsMouseDragging(ImGuiMouseButton_Right))
            RequestOpenPopup::post("hex.builtin.menu.edit");
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
            PopupBlockingTask::open(TaskManager::createTask("hex.builtin.task.saving_data"_lang, TaskManager::NoProgress, [=](Task &){
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
        RequestHexEditorSelectionChange::subscribe(this, [this](Region region) {
            auto provider = ImHexApi::Provider::get();

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
           m_foregroundHighlights->clear();
           m_backgroundHighlights->clear();
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

        m_hexEditor.setBytesPerRow(ContentRegistry::Settings::read<int>("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.bytes_per_row", m_hexEditor.getBytesPerRow()));
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

        ContentRegistry::Interface::addFooterItem([] {
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

        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.file" }, 1300);

        /* Save */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.view.hex_editor.menu.file.save" }, ICON_VS_SAVE, 1350,
                                                CTRLCMD + Keys::S,
                                                save,
                                                [] {
                                                    auto provider      = ImHexApi::Provider::get();
                                                    bool providerValid = ImHexApi::Provider::isValid();

                                                    return providerValid && provider->isWritable() && provider->isSavable() && provider->isDirty();
                                                });

        /* Save As */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.view.hex_editor.menu.file.save_as" }, ICON_VS_SAVE_AS, 1375,
                                                CTRLCMD + SHIFT + Keys::S,
                                                saveAs,
                                                [] {
                                                    auto provider      = ImHexApi::Provider::get();
                                                    bool providerValid = ImHexApi::Provider::isValid();

                                                    return providerValid && provider->isDumpable();
                                                });

        /* Load Encoding File */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.menu.file.import", "hex.builtin.menu.file.import.custom_encoding" }, "あ", 5050, Shortcut::None,
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
                                                        TaskManager::createTask("hex.builtin.task.loading_encoding_file"_lang, 0, [this, path](auto&) {
                                                            auto encoding = EncodingFile(EncodingFile::Type::Thingy, path);
                                                            ImHexApi::Provider::markDirty();

                                                            TaskManager::doLater([this, encoding = std::move(encoding)] mutable {
                                                                m_hexEditor.setCustomEncoding(std::move(encoding));
                                                            });
                                                        });
                                                    });
                                                },
                                                ImHexApi::Provider::isValid);

        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.file" }, 1500);

        /* Search */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.view.hex_editor.menu.file.search" }, ICON_VS_SEARCH, 1550,
                                                CTRLCMD + Keys::F,
                                                [this] {
                                                    this->openPopup<PopupFind>(this);
                                                },
                                                ImHexApi::Provider::isValid);

        /* Goto */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.view.hex_editor.menu.file.goto" }, ICON_VS_DEBUG_STEP_INTO, 1600,
                                                CTRLCMD + Keys::G,
                                                [this] {
                                                    this->openPopup<PopupGoto>();
                                                },
                                                ImHexApi::Provider::isValid);



        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.edit" }, 1100);

        /* Copy */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.copy" }, ICON_VS_COPY, 1150,
                                                CurrentView + CTRLCMD + Keys::C,
                                                [] {
                                                    auto selection = ImHexApi::HexEditor::getSelection();
                                                    if (selection.has_value() && selection != Region::Invalid())
                                                        copyBytes(*selection);
                                                },
                                                ImHexApi::HexEditor::isSelectionValid,
                                                this);

        ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.copy_as" }, ICON_VS_PREVIEW, 1190, []{}, ImHexApi::HexEditor::isSelectionValid);

        /* Copy As */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.copy_as", "hex.builtin.view.hex_editor.copy.ascii" }, ICON_VS_SYMBOL_TEXT, 1200,
                                                CurrentView + ALT + Keys::C,
                                                [] {
                                                    auto selection = ImHexApi::HexEditor::getSelection();
                                                    if (selection.has_value() && selection != Region::Invalid())
                                                        copyString(*selection);
                                                },
                                                ImHexApi::HexEditor::isSelectionValid,
                                                this);

        /* Copy address */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.copy_as", "hex.builtin.view.hex_editor.copy.address" }, ICON_VS_LOCATION, 1250,
                                                Shortcut::None,
                                                [] {
                                                    auto selection = ImHexApi::HexEditor::getSelection();
                                                    if (selection.has_value() && selection != Region::Invalid())
                                                        ImGui::SetClipboardText(hex::format("0x{:08X}", selection->getStartAddress()).c_str());
                                                },
                                                ImHexApi::HexEditor::isSelectionValid);

        /* Copy custom encoding */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.copy_as", "hex.builtin.view.hex_editor.copy.custom_encoding" }, "あ", 1300,
                                                SHIFT + ALT + Keys::C,
                                                [this] {
                                                    auto selection = ImHexApi::HexEditor::getSelection();
                                                    auto customEncoding = m_hexEditor.getCustomEncoding();
                                                    if (customEncoding.has_value() && selection.has_value() && selection != Region::Invalid())
                                                        copyCustomEncoding(*customEncoding, *selection);
                                                },
                                                [this] {
                                                    return ImHexApi::HexEditor::isSelectionValid() && m_hexEditor.getCustomEncoding().has_value();
                                                });

        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.copy_as" }, 1350);

        /* Copy as... */
        ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.copy_as" }, ICON_VS_FILE_CODE, 1400, []{
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
        });

        /* Paste */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.paste" }, ICON_VS_OUTPUT, 1450, CurrentView + CTRLCMD + Keys::V,
                                                [this] {
                                                    processPasteBehaviour(ImHexApi::HexEditor::getSelection().value_or( ImHexApi::HexEditor::ProviderRegion(Region { 0, 0 }, ImHexApi::Provider::get())));
                                                },
                                                ImHexApi::HexEditor::isSelectionValid,
                                                this);

        /* Paste... */
        ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.paste_as" }, ICON_VS_CLIPPY, 1490, []{}, ImHexApi::HexEditor::isSelectionValid);

        /* Paste... > Paste all */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.paste_as", "hex.builtin.view.hex_editor.menu.edit.paste_all" }, ICON_VS_CLIPPY, 1500, CurrentView + CTRLCMD + SHIFT + Keys::V,
                                                [] {
                                                    pasteBytes(ImHexApi::HexEditor::getSelection().value_or( ImHexApi::HexEditor::ProviderRegion(Region { 0, 0 }, ImHexApi::Provider::get())), false, false);
                                                },
                                                ImHexApi::HexEditor::isSelectionValid,
                                                this);

        /* Paste... > Paste all as string */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.paste_as", "hex.builtin.view.hex_editor.menu.edit.paste_all_string" }, ICON_VS_SYMBOL_TEXT, 1510,
                                                Shortcut::None,
                                                [] {
                                                    pasteBytes(ImHexApi::HexEditor::getSelection().value_or( ImHexApi::HexEditor::ProviderRegion(Region { 0, 0 }, ImHexApi::Provider::get())), false, true);
                                                },
                                                ImHexApi::HexEditor::isSelectionValid,
                                                this);

        /* Select */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.select" }, ICON_VS_SELECTION, 1525,
                                                CTRLCMD + SHIFT + Keys::A,
                                                [this] {
                                                    auto selection = ImHexApi::HexEditor::getSelection().value_or(ImHexApi::HexEditor::ProviderRegion{ { 0, 1 }, nullptr });
                                                    this->openPopup<PopupSelect>(selection.getStartAddress(), selection.getSize());
                                                },
                                                ImHexApi::Provider::isValid);

        /* Select All */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.select_all" }, ICON_VS_LIST_FLAT, 1550, CurrentView + CTRLCMD + Keys::A,
                                                [] {
                                                    auto provider = ImHexApi::Provider::get();
                                                    ImHexApi::HexEditor::setSelection(provider->getBaseAddress(), provider->getActualSize());
                                                },
                                                ImHexApi::HexEditor::isSelectionValid,
                                                this);


        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.edit" }, 1600);

        /* Set Base Address */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.set_base" }, ICON_VS_LOCATION, 1650, Shortcut::None,
                                                [this] {
                                                    auto provider = ImHexApi::Provider::get();
                                                    this->openPopup<PopupBaseAddress>(provider->getBaseAddress());
                                                },
                                                [] { return ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->isReadable(); });

        /* Resize */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.resize" }, ICON_VS_ARROW_BOTH, 1700, Shortcut::None,
                                                [this] {
                                                    auto provider = ImHexApi::Provider::get();
                                                    this->openPopup<PopupResize>(provider->getActualSize());
                                                },
                                                [] { return ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->isResizable(); });

        /* Insert */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.insert" }, ICON_VS_INSERT, 1750, Shortcut::None,
                                                [this] {
                                                    auto selection      = ImHexApi::HexEditor::getSelection();

                                                    this->openPopup<PopupInsert>(selection->getStartAddress(), 0x00);
                                                },
                                                [] { return ImHexApi::HexEditor::isSelectionValid() && ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->isResizable(); });

        /* Remove */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.remove" }, ICON_VS_CLEAR_ALL, 1800, Shortcut::None,
                                                [this] {
                                                    auto selection = ImHexApi::HexEditor::getSelection();

                                                    this->openPopup<PopupRemove>(selection->getStartAddress(), selection->getSize());
                                                },
                                                [] { return ImHexApi::HexEditor::isSelectionValid() && ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->isResizable(); });

        /* Fill */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.fill" }, ICON_VS_PAINTCAN, 1810, Shortcut::None,
                                                [this] {
                                                    auto selection = ImHexApi::HexEditor::getSelection();

                                                    this->openPopup<PopupFill>(selection->getStartAddress(), selection->getSize());
                                                },
                                                [] { return ImHexApi::HexEditor::isSelectionValid() && ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->isWritable(); });

        /* Toggle Overwrite/Insert mode */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.insert_mode" }, ICON_VS_PENCIL, 1820, Shortcut::None,
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
                                                });

        /* Jump to */
        ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.jump_to" }, ICON_VS_DEBUG_STEP_OUT, 1850,
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
                                                    if (menu::menuItem(hex::format("{} | 0x{:08X}", "hex.ui.common.little_endian"_lang, littleEndianValue).c_str(), Shortcut::None, false, canJumpTo(littleEndianValue))) {
                                                        ImHexApi::HexEditor::setSelection(littleEndianValue, 1);
                                                    }
                                                    ImGui::PopID();

                                                    ImGui::PushID(2);
                                                    if (menu::menuItem(hex::format("{} | 0x{:08X}", "hex.ui.common.big_endian"_lang, bigEndianValue).c_str(), Shortcut::None, false, canJumpTo(bigEndianValue))) {
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
                                                [] { return ImHexApi::Provider::isValid() && ImHexApi::HexEditor::isSelectionValid() && ImHexApi::HexEditor::getSelection()->getSize() <= sizeof(u64); });

        /* Set Page Size */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.set_page_size" }, ICON_VS_BROWSER, 1860, Shortcut::None,
                                                [this] {
                                                    auto provider = ImHexApi::Provider::get();
                                                    this->openPopup<PopupPageSize>(provider->getPageSize());
                                                },
                                                [] { return ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->isReadable(); });

        ContentRegistry::Interface::addMenuItemSeparator({ "hex.builtin.menu.edit" }, 1900);

        /* Open in new provider */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.open_in_new_provider" }, ICON_VS_GO_TO_FILE, 1950, Shortcut::None,
                                                [] {
                                                    auto selection = ImHexApi::HexEditor::getSelection();

                                                    auto newProvider = ImHexApi::Provider::createProvider("hex.builtin.provider.view", true);
                                                    if (auto *viewProvider = dynamic_cast<ViewProvider*>(newProvider); viewProvider != nullptr) {
                                                        viewProvider->setProvider(selection->getStartAddress(), selection->getSize(), selection->getProvider());
                                                        if (viewProvider->open())
                                                            EventProviderOpened::post(viewProvider);
                                                    }
                                                },
                                                [] { return ImHexApi::HexEditor::isSelectionValid() && ImHexApi::Provider::isValid(); });
    }

}
