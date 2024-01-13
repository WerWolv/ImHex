#include "content/views/view_hex_editor.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/api/shortcut_manager.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/api/achievement_manager.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/crypto.hpp>

#include <hex/providers/buffered_reader.hpp>

#include <wolv/math_eval/math_evaluator.hpp>

#include <content/providers/view_provider.hpp>
#include <popups/popup_file_chooser.hpp>

#include <imgui_internal.h>
#include <content/popups/popup_blocking_task.hpp>

#include <codecvt>

using namespace std::literals::string_literals;

namespace hex::plugin::builtin {

    /* Popups */

    class PopupGoto : public ViewHexEditor::Popup {
    public:
        void draw(ViewHexEditor *editor) override {
            ImGui::TextUnformatted("hex.builtin.view.hex_editor.menu.file.goto"_lang);
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
                if (ImGuiExt::InputTextIcon("##input", ICON_VS_SYMBOL_OPERATOR, m_input, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                    if (auto result = m_evaluator.evaluate(m_input); result.has_value()) {
                        const auto inputResult = result.value();
                        u64 newAddress = 0x00;

                        auto provider = ImHexApi::Provider::get();

                        switch (m_mode) {
                            case Mode::Absolute: {
                                newAddress = inputResult;
                            }
                                break;
                            case Mode::Relative: {
                                const auto selection = editor->getSelection();
                                newAddress = selection.getStartAddress() + inputResult;
                            }
                                break;
                            case Mode::Begin: {
                                newAddress = provider->getBaseAddress() + provider->getCurrentPageAddress() + inputResult;
                            }
                                break;
                            case Mode::End: {
                                newAddress = provider->getActualSize() - inputResult;
                            }
                                break;
                        }

                        editor->setSelection(newAddress, newAddress);
                        editor->jumpToSelection();
                    }
                }

                ImGui::EndTabBar();
            }
        }

    private:
        enum class Mode : u8 {
            Absolute,
            Relative,
            Begin,
            End
        };

        Mode m_mode = Mode::Absolute;

        bool m_requestFocus = true;
        std::string m_input;
        wolv::math_eval::MathEvaluator<i128> m_evaluator;
    };

    class PopupSelect : public ViewHexEditor::Popup {
    public:
        
        PopupSelect(u64 address, size_t size): m_region({address, size}) {}

        void draw(ViewHexEditor *editor) override {
            ImGui::TextUnformatted("hex.builtin.view.hex_editor.menu.file.select"_lang);
            if (ImGui::BeginTabBar("select_tabs")) {
                if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.select.offset.region"_lang)) {
                    u64 inputA = m_region.getStartAddress();
                    u64 inputB = m_region.getEndAddress();
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
                    ImGuiExt::InputHexadecimal("hex.builtin.view.hex_editor.select.offset.begin"_lang, &inputA, ImGuiInputTextFlags_AutoSelectAll);
                    ImGuiExt::InputHexadecimal("hex.builtin.view.hex_editor.select.offset.size"_lang, &inputB, ImGuiInputTextFlags_AutoSelectAll);

                    if (inputB <= 0)
                        inputB = 1;

                    m_region = { inputA, inputB };
                    ImGui::EndTabItem();
                }

                if (ImGui::Button("hex.builtin.view.hex_editor.select.select"_lang) || (ImGui::IsItemFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Enter)))) {
                    editor->setSelection(m_region.getStartAddress(), m_region.getEndAddress());
                    editor->jumpToSelection();
                }

                ImGui::EndTabBar();
            }
        }

    private:
        Region m_region = { 0, 1 };
    };

    class PopupFind : public ViewHexEditor::Popup {
    public:
        PopupFind() {
            EventRegionSelected::subscribe(this, [this](Region region) {
                m_searchPosition = m_nextSearchPosition.value_or(region.getStartAddress());
                m_nextSearchPosition.reset();
            });
        }

        ~PopupFind() override {
            EventRegionSelected::unsubscribe(this);
        }

        void draw(ViewHexEditor *editor) override {
            std::vector<u8> searchSequence;

            ImGui::TextUnformatted("hex.builtin.view.hex_editor.menu.file.search"_lang);
            if (ImGui::BeginTabBar("##find_tabs")) {
                if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.search.hex"_lang)) {
                    if (ImGuiExt::InputTextIcon("##input", ICON_VS_SYMBOL_NUMERIC, m_input, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CharsHexadecimal)) {
                        if (!m_input.empty()) {
                            m_shouldSearch = true;
                            m_backwards = false;
                        }
                    }

                    this->drawButtons();

                    if (m_shouldSearch) {
                        searchSequence = crypt::decode16(m_input);
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.search.string"_lang)) {
                    if (ImGuiExt::InputTextIcon("##input", ICON_VS_SYMBOL_KEY, m_input, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                        if (!m_input.empty()) {
                            m_shouldSearch = true;
                            m_backwards = false;
                        }
                    }

                    this->drawButtons();

                    constexpr static std::array StringModeEncodingNames = {
                        "UTF-8",
                        "UTF-16",
                        "UTF-32"
                    };
                    
                    if (ImGui::BeginCombo("hex.builtin.view.hex_editor.search.string.encoding"_lang, 
                        StringModeEncodingNames[static_cast<u8>(m_stringModeEncoding.load())])) {
                        if (ImGui::Selectable(StringModeEncodingNames[0], m_stringModeEncoding == StringModeEncoding::UTF8))
                            m_stringModeEncoding = StringModeEncoding::UTF8;
                        if (ImGui::Selectable(StringModeEncodingNames[1], m_stringModeEncoding == StringModeEncoding::UTF16))
                            m_stringModeEncoding = StringModeEncoding::UTF16;
                        if (ImGui::Selectable(StringModeEncodingNames[2], m_stringModeEncoding == StringModeEncoding::UTF32))
                            m_stringModeEncoding = StringModeEncoding::UTF32;
                        ImGui::EndCombo();
                    }

                    constexpr static std::array StringModeEndianessNames = {
                        "Little endian",
                        "Big endian"
                    };

                    if (ImGui::BeginCombo("hex.builtin.view.hex_editor.search.string.endianess"_lang, 
                        StringModeEndianessNames[static_cast<u8>(m_stringModeEndianess.load())])) {
                        if (ImGui::Selectable(StringModeEndianessNames[0], m_stringModeEndianess == StringModeEndianness::Little))
                            m_stringModeEndianess = StringModeEndianness::Little;
                        if (ImGui::Selectable(StringModeEndianessNames[1], m_stringModeEndianess == StringModeEndianness::Big))
                            m_stringModeEndianess = StringModeEndianness::Big;
                        ImGui::EndCombo();
                    }

                    if (m_shouldSearch) {
                        searchSequence.clear();

                        std::endian nativeEndianess = std::endian::native;
                        if (m_stringModeEndianess == StringModeEndianness::Big)
                            nativeEndianess = std::endian::big;
                        else if (m_stringModeEndianess == StringModeEndianness::Little)
                            nativeEndianess = std::endian::little;
                        
                        auto swapEndianess = [](auto &value, std::endian endian, StringModeEncoding encoding) {
                            if (encoding == StringModeEncoding::UTF16 || encoding == StringModeEncoding::UTF32) {
                                if ((endian == std::endian::big && std::endian::big != std::endian::native) ||
                                    (endian == std::endian::little && std::endian::little != std::endian::native)) {
                                    if constexpr (sizeof(value) == 2)
                                        value = __builtin_bswap16(value);
                                    else if constexpr (sizeof(value) == 4)
                                        value = __builtin_bswap32(value);
                                }
                            }
                        };
                                            
                        switch (m_stringModeEncoding.load()) {
                            case StringModeEncoding::UTF8: {
                                std::copy(m_input.begin(), m_input.end(), std::back_inserter(searchSequence));
                                log::info("UTF-8: {}", crypt::encode16(searchSequence));
                            }
                                break;
                            case StringModeEncoding::UTF16: {
                                std::wstring_convert<std::codecvt_utf8<char16_t>, char16_t> convert;
                                std::u16string utf16 = convert.from_bytes(m_input);
                                for (auto &c : utf16)
                                    swapEndianess(c, nativeEndianess, StringModeEncoding::UTF16);
                                std::copy(reinterpret_cast<const u8 *>(utf16.data()), reinterpret_cast<const u8 *>(utf16.data() + utf16.size()), std::back_inserter(searchSequence));
                                log::info("UTF-16: {}", crypt::encode16(searchSequence));
                            }
                                break;
                            case StringModeEncoding::UTF32: {
                                std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> convert;
                                std::u32string utf32 = convert.from_bytes(m_input);
                                for (auto &c : utf32)
                                    swapEndianess(c, nativeEndianess, StringModeEncoding::UTF32);
                                std::copy(reinterpret_cast<const u8 *>(utf32.data()), reinterpret_cast<const u8 *>(utf32.data() + utf32.size()), std::back_inserter(searchSequence));
                                log::info("UTF-32: {}", crypt::encode16(searchSequence));
                            }
                                break;
                        }

                        if (!searchSequence.empty() && searchSequence.back() == 0x00)
                            searchSequence.pop_back();
                    }

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            if (!m_searchTask.isRunning() && !searchSequence.empty() && m_shouldSearch) {
                m_searchTask = TaskManager::createTask("hex.ui.common.processing", ImHexApi::Provider::get()->getActualSize(), [this, editor, searchSequence](auto &) {
                    for (u8 retry = 0; retry < 2; retry++) {
                        auto region = this->findSequence(searchSequence, m_backwards);

                        if (region.has_value()) {
                            if (editor->getSelection() == region) {
                                if (m_nextSearchPosition.has_value())
                                    m_searchPosition = m_nextSearchPosition.value();
                                m_nextSearchPosition.reset();
                            } else {
                                TaskManager::doLater([editor, region]{
                                    editor->setSelection(region->getStartAddress(), region->getEndAddress());
                                    editor->jumpToSelection();
                                });

                                break;
                            }
                        } else {
                            m_reachedEnd = true;
                        }
                    }

                    m_shouldSearch = false;
                    m_requestFocus = true;
                });
            }
        }

    private:
        void drawButtons() {
            const auto ButtonSize = ImVec2(ImGui::CalcTextSize(ICON_VS_SEARCH).x, ImGui::GetTextLineHeight()) + ImGui::GetStyle().CellPadding * 2;
            const auto ButtonColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);

            if (m_requestFocus) {
                ImGui::SetKeyboardFocusHere(-1);
                m_requestFocus = false;
            }

            ImGui::BeginDisabled(m_searchTask.isRunning());
            {
                ImGui::SameLine();
                if (ImGuiExt::IconButton(ICON_VS_SEARCH "##search", ButtonColor, ButtonSize)) {
                    m_shouldSearch = true;
                    m_backwards = false;
                    m_reachedEnd = false;
                    m_searchPosition.reset();
                    m_nextSearchPosition.reset();
                }

                ImGui::BeginDisabled(!m_searchPosition.has_value());
                {
                    ImGui::BeginDisabled(m_reachedEnd && m_backwards);
                    {
                        if (ImGuiExt::IconButton(ICON_VS_ARROW_UP "##up", ButtonColor, ButtonSize)) {
                            m_shouldSearch = true;
                            m_backwards = true;
                            m_reachedEnd = false;
                        }
                    }
                    ImGui::EndDisabled();

                    ImGui::SameLine();

                    ImGui::BeginDisabled(m_reachedEnd && !m_backwards);
                    {
                        if (ImGuiExt::IconButton(ICON_VS_ARROW_DOWN "##down", ButtonColor, ButtonSize)) {
                            m_shouldSearch = true;
                            m_backwards = false;
                            m_reachedEnd = false;
                        }
                    }
                    ImGui::EndDisabled();
                }

                ImGui::EndDisabled();
            }
            ImGui::EndDisabled();
        }

        std::optional<Region> findSequence(const std::vector<u8> &sequence, bool backwards) {
            auto provider = ImHexApi::Provider::get();

            prv::ProviderReader reader(provider);

            reader.seek(m_searchPosition.value_or(provider->getBaseAddress()));

            constexpr static auto searchFunction = [](const auto &haystackBegin, const auto &haystackEnd, const auto &needleBegin, const auto &needleEnd) {
                return std::search(haystackBegin, haystackEnd, std::boyer_moore_horspool_searcher(needleBegin, needleEnd));
            };

            if (!backwards) {
                auto occurrence = searchFunction(reader.begin(), reader.end(), sequence.begin(), sequence.end());
                if (occurrence != reader.end()) {
                    m_nextSearchPosition = occurrence.getAddress() + sequence.size();
                    return Region { occurrence.getAddress(), sequence.size() };
                }
            } else {
                auto occurrence = searchFunction(reader.rbegin(), reader.rend(), sequence.rbegin(), sequence.rend());
                if (occurrence != reader.rend()) {
                    if (occurrence.getAddress() < sequence.size())
                        m_nextSearchPosition = 0x00;
                    else
                        m_nextSearchPosition = occurrence.getAddress() - sequence.size();

                    return Region { occurrence.getAddress() - (sequence.size() - 1), sequence.size() };
                }
            }

            return std::nullopt;
        }

        enum class StringModeEncoding : u8 {
            UTF8,
            UTF16,
            UTF32
        };

        enum class StringModeEndianness : u8 {
            Little,
            Big
        };

        std::string m_input;
        std::optional<u64> m_searchPosition, m_nextSearchPosition;

        bool m_requestFocus = true;
        std::atomic<bool> m_shouldSearch = false;
        std::atomic<bool> m_backwards    = false;
        std::atomic<bool> m_reachedEnd   = false;

        std::atomic<StringModeEncoding> m_stringModeEncoding = StringModeEncoding::UTF8;
        std::atomic<StringModeEndianness> m_stringModeEndianess = StringModeEndianness::Little;

        TaskHolder m_searchTask;
    };

    class PopupBaseAddress : public ViewHexEditor::Popup {
    public:
        explicit PopupBaseAddress(u64 baseAddress) : m_baseAddress(baseAddress) { }

        void draw(ViewHexEditor *editor) override {
            ImGui::TextUnformatted("hex.builtin.view.hex_editor.menu.edit.set_base"_lang);

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
            ImGui::TextUnformatted("hex.builtin.view.hex_editor.menu.edit.set_page_size"_lang);

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
            ImGui::TextUnformatted("hex.builtin.view.hex_editor.menu.edit.resize"_lang);

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
            ImGui::TextUnformatted("hex.builtin.view.hex_editor.menu.edit.insert"_lang);

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
            ImGui::TextUnformatted("hex.builtin.view.hex_editor.menu.edit.remove"_lang);

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
            ImGui::TextUnformatted("hex.builtin.view.hex_editor.menu.edit.fill"_lang);

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
            for (u64 i = 0; i < size; i += bytes.size()) {
                auto remainingSize = std::min<size_t>(size - i, bytes.size());
                provider->write(provider->getBaseAddress() + address + i, bytes.data(), remainingSize);
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

        m_hexEditor.setBackgroundHighlightCallback([this](u64 address, const u8 *data, size_t size) -> std::optional<color_t> {
            if (auto highlight = m_backgroundHighlights->find(address); highlight != m_backgroundHighlights->end())
                return highlight->second;

            std::optional<color_t> result;
            for (const auto &[id, callback] : ImHexApi::HexEditor::impl::getBackgroundHighlightingFunctions()) {
                if (auto color = callback(address, data, size, result.has_value()); color.has_value())
                    result = color;
            }

            if (!result.has_value()) {
                for (const auto &[id, highlighting] : ImHexApi::HexEditor::impl::getBackgroundHighlights()) {
                    if (highlighting.getRegion().overlaps({ address, size }))
                        return highlighting.getColor();
                }
            }

            if (result.has_value())
                m_backgroundHighlights->insert({ address, result.value() });

            return result;
        });

        m_hexEditor.setTooltipCallback([](u64 address, const u8 *data, size_t size) {
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
        RequestSelectionChange::unsubscribe(this);
        EventProviderChanged::unsubscribe(this);
        EventProviderOpened::unsubscribe(this);
        EventHighlightingChanged::unsubscribe(this);
        EventSettingsChanged::unsubscribe(this);

        ContentRegistry::Settings::write("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.bytes_per_row", m_hexEditor.getBytesPerRow());
    }

    void ViewHexEditor::drawPopup() {
        // Popup windows
        if (m_shouldOpenPopup) {
            m_shouldOpenPopup = false;
            ImGui::OpenPopup("##hex_editor_popup");
        }

        static bool justOpened = true;

        ImGui::SetNextWindowPos(ImGui::GetWindowPos() + ImGui::GetWindowContentRegionMin() - ImGui::GetStyle().WindowPadding, ImGuiCond_Appearing);
        if (ImGui::BeginPopup("##hex_editor_popup", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |ImGuiWindowFlags_NoTitleBar)) {
            // Force close the popup when user is editing an input
            if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape))){
                ImGui::CloseCurrentPopup();
            }

            if (justOpened) {
                ImGui::SetKeyboardFocusHere();
                justOpened = false;
            }

            if (m_currPopup != nullptr)
                m_currPopup->draw(this);
            else
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        } else {
            this->closePopup();
            justOpened = true;
        }

        // Right click menu
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
            RequestOpenPopup::post("hex.builtin.menu.edit");
    }

    void ViewHexEditor::drawContent() {
        m_hexEditor.setProvider(ImHexApi::Provider::get());

        m_hexEditor.draw();

        this->drawPopup();
    }

    static void save() {
        ImHexApi::Provider::get()->save();
    }

    static void saveAs() {
        fs::openFileBrowser(fs::DialogMode::Save, {}, [](const auto &path) {
            auto provider = ImHexApi::Provider::get();
            PopupBlockingTask::open(TaskManager::createTask("Saving...", TaskManager::NoProgress, [=](Task &){
                provider->saveAs(path);
            }));
        });
    }

    static void copyBytes(const Region &selection) {
        constexpr static auto Format = "{0:02X} ";

        auto provider = ImHexApi::Provider::get();

        auto reader = prv::ProviderReader (provider);
        reader.seek(selection.getStartAddress());
        reader.setEndAddress(selection.getEndAddress());

        std::string result;
        result.reserve(fmt::format(Format, 0x00).size() * selection.getSize());

        for (const auto &byte : reader)
            result += fmt::format(Format, byte);
        result.pop_back();

        ImGui::SetClipboardText(result.c_str());
    }

    static void pasteBytes(const Region &selection, bool selectionCheck) {
        auto provider = ImHexApi::Provider::get();

        auto clipboard = ImGui::GetClipboardText();
        if (clipboard == nullptr)
            return;

        auto buffer = parseHexString(clipboard);

        if (!selectionCheck) {
            if (selection.getStartAddress() + buffer.size() >= provider->getActualSize())
                provider->resize(selection.getStartAddress() + buffer.size());
        }

        // Write bytes
        auto size = selectionCheck ? std::min(buffer.size(), selection.getSize()) : buffer.size();
        provider->write(selection.getStartAddress(), buffer.data(), size);
    }

    static void copyString(const Region &selection) {
        auto provider = ImHexApi::Provider::get();

        std::string buffer(selection.size, 0x00);
        buffer.reserve(selection.size);
        provider->read(selection.getStartAddress(), buffer.data(), selection.size);

        ImGui::SetClipboardText(buffer.c_str());
    }

    static void copyCustomEncoding(const EncodingFile &customEncoding, const Region &selection) {
        auto provider = ImHexApi::Provider::get();

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
                m_hexEditor.scrollToSelection();
                m_hexEditor.jumpIfOffScreen();
            }
        });
        ShortcutManager::addShortcut(this, Keys::Down, "hex.builtin.view.hex_editor.shortcut.cursor_down", [this] {
            auto selection = getSelection();
            auto cursor = m_hexEditor.getCursorPosition().value_or(selection.getEndAddress());

            auto pos = cursor + m_hexEditor.getBytesPerRow();
            this->setSelection(pos, pos);
            m_hexEditor.scrollToSelection();
            m_hexEditor.jumpIfOffScreen();
        });
        ShortcutManager::addShortcut(this, Keys::Left, "hex.builtin.view.hex_editor.shortcut.cursor_left", [this] {
            auto selection = getSelection();
            auto cursor = m_hexEditor.getCursorPosition().value_or(selection.getEndAddress());

            if (cursor > 0) {
                auto pos = cursor - m_hexEditor.getBytesPerCell();
                this->setSelection(pos, pos);
                m_hexEditor.scrollToSelection();
                m_hexEditor.jumpIfOffScreen();
            }
        });
        ShortcutManager::addShortcut(this, Keys::Right, "hex.builtin.view.hex_editor.shortcut.cursor_right", [this] {
            auto selection = getSelection();
            auto cursor = m_hexEditor.getCursorPosition().value_or(selection.getEndAddress());

            auto pos = cursor + m_hexEditor.getBytesPerCell();
            this->setSelection(pos, pos);
            m_hexEditor.scrollToSelection();
            m_hexEditor.jumpIfOffScreen();
        });

        ShortcutManager::addShortcut(this, Keys::PageUp, "hex.builtin.view.hex_editor.shortcut.cursor_page_up", [this] {
            auto selection = getSelection();
            auto cursor = m_hexEditor.getCursorPosition().value_or(selection.getEndAddress());

            u64 visibleByteCount = m_hexEditor.getBytesPerRow() * m_hexEditor.getVisibleRowCount();
            if (cursor >= visibleByteCount) {
                auto pos = cursor - visibleByteCount;
                this->setSelection(pos, (pos + m_hexEditor.getBytesPerCell()) - 1);
                m_hexEditor.scrollToSelection();
                m_hexEditor.jumpIfOffScreen();
            }
        });
        ShortcutManager::addShortcut(this, Keys::PageDown, "hex.builtin.view.hex_editor.shortcut.cursor_page_down", [this] {
            auto selection = getSelection();
            auto cursor = m_hexEditor.getCursorPosition().value_or(selection.getEndAddress());

            auto pos = cursor + (m_hexEditor.getBytesPerRow() * m_hexEditor.getVisibleRowCount());
            this->setSelection(pos, (pos + m_hexEditor.getBytesPerCell()) - 1);
            m_hexEditor.scrollToSelection();
            m_hexEditor.jumpIfOffScreen();
        });

        ShortcutManager::addShortcut(this, Keys::Home, "hex.builtin.view.hex_editor.shortcut.cursor_start", [this] {
            auto selection = getSelection();
            auto cursor = m_hexEditor.getCursorPosition().value_or(selection.getEndAddress());

            auto pos = cursor - cursor % m_hexEditor.getBytesPerRow();
            this->setSelection(pos, (pos + m_hexEditor.getBytesPerCell()) - 1);
            m_hexEditor.scrollToSelection();
            m_hexEditor.jumpIfOffScreen();
        });

        ShortcutManager::addShortcut(this, Keys::End, "hex.builtin.view.hex_editor.shortcut.cursor_end", [this] {
            auto selection = getSelection();
            auto cursor = m_hexEditor.getCursorPosition().value_or(selection.getEndAddress());

            auto pos = cursor - cursor % m_hexEditor.getBytesPerRow() + m_hexEditor.getBytesPerRow() - m_hexEditor.getBytesPerCell();
            this->setSelection(pos, (pos + m_hexEditor.getBytesPerCell()) - 1);
            m_hexEditor.scrollToSelection();
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

            m_hexEditor.scrollToSelection();
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

            m_hexEditor.scrollToSelection();
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

            m_hexEditor.scrollToSelection();
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

            m_hexEditor.scrollToSelection();
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

            m_hexEditor.scrollToSelection();
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

            m_hexEditor.scrollToSelection();
            m_hexEditor.jumpIfOffScreen();
        });

    }

    void ViewHexEditor::registerEvents() {
        RequestSelectionChange::subscribe(this, [this](Region region) {
            auto provider = ImHexApi::Provider::get();

            if (region == Region::Invalid()) {
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
                this->jumpToSelection();
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
                ImHexApi::HexEditor::clearSelection();
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

        m_hexEditor.setBytesPerRow(ContentRegistry::Settings::read("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.bytes_per_row", m_hexEditor.getBytesPerRow()));
        EventSettingsChanged::subscribe(this, [this] {
            m_hexEditor.setSelectionColor(ContentRegistry::Settings::read("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.highlight_color", 0x60C08080));
            m_hexEditor.enableSyncScrolling(ContentRegistry::Settings::read("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.sync_scrolling", false));
            m_hexEditor.setByteCellPadding(ContentRegistry::Settings::read("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.byte_padding", 0));
            m_hexEditor.setCharacterCellPadding(ContentRegistry::Settings::read("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.char_padding", 0));
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
                                                    const auto basePaths = fs::getDefaultPaths(fs::ImHexPath::Encodings);
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
                                                        TaskManager::createTask("Loading encoding file", 0, [this, path](auto&) {
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
                                                    this->openPopup<PopupFind>();
                                                },
                                                ImHexApi::Provider::isValid);

        /* Goto */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.view.hex_editor.menu.file.goto" }, ICON_VS_DEBUG_STEP_INTO, 1600,
                                                CTRLCMD + Keys::G,
                                                [this] {
                                                    this->openPopup<PopupGoto>();
                                                },
                                                ImHexApi::Provider::isValid);

        /* Select */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.file", "hex.builtin.view.hex_editor.menu.file.select" }, ICON_VS_SELECTION, 1650,
                                                CTRLCMD + SHIFT + Keys::A,
                                                [this] {
                                                    auto selection = ImHexApi::HexEditor::getSelection().value_or(ImHexApi::HexEditor::ProviderRegion{ { 0, 1 }, nullptr });
                                                    this->openPopup<PopupSelect>(selection.getStartAddress(), selection.getSize());
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
                                                CurrentView + CTRLCMD + SHIFT + Keys::C,
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
                                                Shortcut::None,
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
            for (const auto &[unlocalizedName, callback] : ContentRegistry::DataFormatter::impl::getEntries()) {
                if (ImGui::MenuItem(Lang(unlocalizedName), nullptr, false, enabled)) {
                    ImGui::SetClipboardText(
                            callback(
                                    provider,
                                    selection->getStartAddress() + provider->getBaseAddress() + provider->getCurrentPageAddress(),
                                    selection->size
                            ).c_str()
                    );
                }
            }
        },
        [] {
            return ImHexApi::Provider::isValid() && ImHexApi::HexEditor::isSelectionValid();
        });

        /* Paste */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.paste" }, ICON_VS_OUTPUT, 1450, CurrentView + CTRLCMD + Keys::V,
                                                [] {
                                                    pasteBytes(ImHexApi::HexEditor::getSelection().value_or( ImHexApi::HexEditor::ProviderRegion(Region { 0, 0 }, ImHexApi::Provider::get())), true);
                                                },
                                                ImHexApi::HexEditor::isSelectionValid,
                                                this);

        /* Paste All */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.paste_all" }, ICON_VS_CLIPPY, 1500, CurrentView + CTRLCMD + SHIFT + Keys::V,
                                                [] {
                                                    pasteBytes(ImHexApi::HexEditor::getSelection().value_or( ImHexApi::HexEditor::ProviderRegion(Region { 0, 0 }, ImHexApi::Provider::get())), false);
                                                },
                                                ImHexApi::HexEditor::isSelectionValid,
                                                this);

        /* Select All */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.select_all" }, ICON_VS_SELECTION, 1550, CurrentView + CTRLCMD + Keys::A,
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
                                                    this->openPopup<PopupResize>(provider->getBaseAddress());
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
                                                    auto selection      = ImHexApi::HexEditor::getSelection();

                                                    this->openPopup<PopupRemove>(selection->getStartAddress(), selection->getSize());
                                                },
                                                [] { return ImHexApi::HexEditor::isSelectionValid() && ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->isResizable(); });

        /* Fill */
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.fill" }, ICON_VS_PAINTCAN, 1810, Shortcut::None,
                                                [this] {
                                                    auto selection      = ImHexApi::HexEditor::getSelection();

                                                    this->openPopup<PopupFill>(selection->getStartAddress(), selection->getSize());
                                                },
                                                [] { return ImHexApi::HexEditor::isSelectionValid() && ImHexApi::Provider::isValid() && ImHexApi::Provider::get()->isWritable(); });

        /* Jump to */
        ContentRegistry::Interface::addMenuItemSubMenu({ "hex.builtin.menu.edit", "hex.builtin.view.hex_editor.menu.edit.jump_to" }, ICON_VS_DEBUG_STEP_OUT, 1850,
                                                [] {
                                                    auto provider   = ImHexApi::Provider::get();
                                                    auto selection  = ImHexApi::HexEditor::getSelection();

                                                    u64 value = 0;
                                                    provider->read(selection->getStartAddress(), &value, selection->getSize());

                                                    auto littleEndianValue = hex::changeEndianess(value, selection->size, std::endian::little);
                                                    auto bigEndianValue    = hex::changeEndianess(value, selection->size,  std::endian::big);

                                                    auto canJumpTo = [provider](u64 value) {
                                                        return (value >= provider->getBaseAddress()) && (value < (provider->getBaseAddress() + provider->getActualSize()));
                                                    };

                                                    if (ImGui::MenuItem(hex::format("0x{:08X}", littleEndianValue).c_str(), "hex.ui.common.little_endian"_lang, false, canJumpTo(littleEndianValue))) {
                                                        ImHexApi::HexEditor::setSelection(littleEndianValue, 1);
                                                    }

                                                    if (ImGui::MenuItem(hex::format("0x{:08X}", bigEndianValue).c_str(), "hex.ui.common.big_endian"_lang, false, canJumpTo(bigEndianValue))) {
                                                        ImHexApi::HexEditor::setSelection(bigEndianValue, 1);
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
