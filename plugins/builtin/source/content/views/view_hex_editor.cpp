#include "content/views/view_hex_editor.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/providers/buffered_reader.hpp>
#include <hex/helpers/crypto.hpp>

#include <content/helpers/math_evaluator.hpp>

#include <imgui_internal.h>
#include <nlohmann/json.hpp>

#include <thread>

namespace hex::plugin::builtin {

    class PopupGoto : public ViewHexEditor::Popup {
    public:

        void draw(ViewHexEditor *editor) override {
            ImGui::TextUnformatted("hex.builtin.view.hex_editor.menu.file.goto"_lang);
            if (ImGui::BeginTabBar("goto_tabs")) {
                if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.goto.offset.absolute"_lang)) {
                    this->m_mode = Mode::Absolute;
                    ImGui::EndTabItem();
                }

                ImGui::BeginDisabled(!editor->isSelectionValid());
                if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.goto.offset.relative"_lang)) {
                    this->m_mode = Mode::Relative;
                    ImGui::EndTabItem();
                }
                ImGui::EndDisabled();

                if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.goto.offset.begin"_lang)) {
                    this->m_mode = Mode::Begin;
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.goto.offset.end"_lang)) {
                    this->m_mode = Mode::End;
                    ImGui::EndTabItem();
                }

                ImGui::SetKeyboardFocusHere();
                ImGui::SetNextFrameWantCaptureKeyboard(true);
                if (ImGui::InputTextIcon("##input", ICON_VS_SYMBOL_OPERATOR, this->m_input, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                    if (auto result = this->m_evaluator.evaluate(this->m_input); result.has_value()) {
                        const auto inputResult = result.value();
                        u64 newAddress = 0x00;

                        auto provider = ImHexApi::Provider::get();

                        switch (this->m_mode) {
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

        std::string m_input;
        MathEvaluator<i128> m_evaluator;
    };

    class PopupSelect : public ViewHexEditor::Popup {
    public:

        void draw(ViewHexEditor *editor) override {
            ImGui::TextUnformatted("hex.builtin.view.hex_editor.menu.file.select"_lang);
            if (ImGui::BeginTabBar("select_tabs")) {

                if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.select.offset.region"_lang)) {
                    u64 inputA = this->m_region.getStartAddress();
                    u64 inputB = this->m_region.getEndAddress();
                    ImGui::InputHexadecimal("hex.builtin.view.hex_editor.select.offset.begin"_lang, &inputA, ImGuiInputTextFlags_AutoSelectAll);
                    ImGui::InputHexadecimal("hex.builtin.view.hex_editor.select.offset.end"_lang, &inputB, ImGuiInputTextFlags_AutoSelectAll);

                    if (inputB < inputA)
                        inputB = inputA;

                    this->m_region = { inputA, (inputB - inputA) + 1 };

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.select.offset.size"_lang)) {
                    u64 inputA = this->m_region.getStartAddress();
                    u64 inputB = this->m_region.getSize();
                    ImGui::InputHexadecimal("hex.builtin.view.hex_editor.select.offset.begin"_lang, &inputA, ImGuiInputTextFlags_AutoSelectAll);
                    ImGui::InputHexadecimal("hex.builtin.view.hex_editor.select.offset.size"_lang, &inputB, ImGuiInputTextFlags_AutoSelectAll);

                    if (inputB <= 0)
                        inputB = 1;

                    this->m_region = { inputA, inputB };
                    ImGui::EndTabItem();
                }

                if (ImGui::Button("hex.builtin.view.hex_editor.select.select"_lang) || (ImGui::IsItemFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Enter)))) {
                    editor->setSelection(this->m_region.getStartAddress(), this->m_region.getEndAddress());
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
            EventManager::subscribe<EventRegionSelected>(this, [this](Region region) {
                this->m_searchPosition = this->m_nextSearchPosition.value_or(region.getStartAddress());
                this->m_nextSearchPosition.reset();
            });
        }

        ~PopupFind() override {
            EventManager::unsubscribe<EventRegionSelected>(this);
        }

        void draw(ViewHexEditor *editor) override {
            std::vector<u8> searchSequence;

            ImGui::TextUnformatted("hex.builtin.view.hex_editor.menu.file.search"_lang);
            if (ImGui::BeginTabBar("##find_tabs")) {
                if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.search.hex"_lang)) {
                    if (ImGui::InputTextIcon("##input", ICON_VS_SYMBOL_NUMERIC, this->m_input, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CharsHexadecimal)) {
                        if (!this->m_input.empty()) {
                            this->m_shouldSearch = true;
                            this->m_backwards = false;
                        }
                    }

                    this->drawButtons();

                    if (this->m_shouldSearch) {
                        searchSequence = crypt::decode16(this->m_input);
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("hex.builtin.view.hex_editor.search.string"_lang)) {
                    if (ImGui::InputTextIcon("##input", ICON_VS_SYMBOL_KEY, this->m_input, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                       if (!this->m_input.empty()) {
                            this->m_shouldSearch = true;
                            this->m_backwards = false;
                       }
                    }

                    this->drawButtons();

                    if (this->m_shouldSearch) {
                        searchSequence.clear();
                        std::copy(this->m_input.begin(), this->m_input.end(), std::back_inserter(searchSequence));

                        if (!searchSequence.empty() && searchSequence.back() == 0x00)
                            searchSequence.pop_back();
                    }

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            if (!this->m_searchTask.isRunning() && !searchSequence.empty() && this->m_shouldSearch) {
                this->m_searchTask = TaskManager::createTask("hex.builtin.common.processing", ImHexApi::Provider::get()->getActualSize(), [this, editor, searchSequence](auto &) {
                    for (u8 retry = 0; retry < 2; retry++) {
                        auto region = this->findSequence(searchSequence, this->m_backwards);

                        if (region.has_value()) {
                            if (editor->getSelection() == region) {
                                if (this->m_nextSearchPosition.has_value())
                                    this->m_searchPosition = this->m_nextSearchPosition.value();
                                this->m_nextSearchPosition.reset();

                                continue;
                            } else {
                                TaskManager::doLater([editor, region]{
                                    editor->setSelection(region->getStartAddress(), region->getEndAddress());
                                    editor->jumpToSelection();
                                });

                                break;
                            }
                        } else {
                            this->m_reachedEnd = true;
                        }
                    }

                    this->m_shouldSearch = false;
                    this->m_requestFocus = true;
                });
            }
        }

    private:
        void drawButtons() {
            const auto ButtonSize = ImVec2(ImGui::CalcTextSize(ICON_VS_SEARCH).x, ImGui::GetTextLineHeight()) + ImGui::GetStyle().CellPadding * 2;
            const auto ButtonColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);

            if (this->m_requestFocus) {
                ImGui::SetKeyboardFocusHere(-1);
                this->m_requestFocus = false;
            }

            ImGui::BeginDisabled(this->m_searchTask.isRunning());
            {
                ImGui::SameLine();
                if (ImGui::IconButton(ICON_VS_SEARCH "##search", ButtonColor, ButtonSize)) {
                    this->m_shouldSearch = true;
                    this->m_backwards = false;
                    this->m_reachedEnd = false;
                    this->m_searchPosition.reset();
                    this->m_nextSearchPosition.reset();
                }

                ImGui::BeginDisabled(!this->m_searchPosition.has_value());
                {
                    ImGui::BeginDisabled(this->m_reachedEnd && this->m_backwards);
                    {
                        if (ImGui::IconButton(ICON_VS_ARROW_UP "##up", ButtonColor, ButtonSize)) {
                            this->m_shouldSearch = true;
                            this->m_backwards = true;
                            this->m_reachedEnd = false;
                        }
                    }
                    ImGui::EndDisabled();

                    ImGui::SameLine();

                    ImGui::BeginDisabled(this->m_reachedEnd && !this->m_backwards);
                    {
                        if (ImGui::IconButton(ICON_VS_ARROW_DOWN "##down", ButtonColor, ButtonSize)) {
                            this->m_shouldSearch = true;
                            this->m_backwards = false;
                            this->m_reachedEnd = false;
                        }
                    }
                    ImGui::EndDisabled();
                }

                ImGui::EndDisabled();
            }
            ImGui::EndDisabled();
        }

        std::optional<Region> findSequence(const std::vector<u8> &sequence, bool backwards) {
            hex::prv::BufferedReader reader(ImHexApi::Provider::get());

            reader.seek(this->m_searchPosition.value_or(0x00));

            constexpr static auto searchFunction = [](const auto &haystackBegin, const auto &haystackEnd, const auto &needleBegin, const auto &needleEnd) {
                return std::search(haystackBegin, haystackEnd, std::boyer_moore_horspool_searcher(needleBegin, needleEnd));
            };

            if (!backwards) {
                auto occurrence = searchFunction(reader.begin(), reader.end(), sequence.begin(), sequence.end());
                if (occurrence != reader.end()) {
                    this->m_nextSearchPosition = occurrence.getAddress() + sequence.size();
                    return Region { occurrence.getAddress(), sequence.size() };
                }
            } else {
                auto occurrence = searchFunction(reader.rbegin(), reader.rend(), sequence.rbegin(), sequence.rend());
                if (occurrence != reader.rend()) {
                    if (occurrence.getAddress() < sequence.size())
                        this->m_nextSearchPosition = 0x00;
                    else
                        this->m_nextSearchPosition = occurrence.getAddress() - sequence.size();

                    return Region { occurrence.getAddress() - (sequence.size() - 1), sequence.size() };
                }
            }

            return std::nullopt;
        }

        std::string m_input;
        std::optional<u64> m_searchPosition, m_nextSearchPosition;

        bool m_requestFocus = true;
        std::atomic<bool> m_shouldSearch = false;
        std::atomic<bool> m_backwards    = false;
        std::atomic<bool> m_reachedEnd   = false;

        TaskHolder m_searchTask;
    };

    class PopupBaseAddress : public ViewHexEditor::Popup {
    public:
        explicit PopupBaseAddress(u64 baseAddress) : m_baseAddress(baseAddress) {

        }

        void draw(ViewHexEditor *editor) override {
            ImGui::TextUnformatted("hex.builtin.view.hex_editor.menu.edit.set_base"_lang);

            ImGui::InputHexadecimal("##base_address", &this->m_baseAddress);
            if (ImGui::IsItemFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Enter))) {
                setBaseAddress(this->m_baseAddress);
                editor->closePopup();
            }

            View::confirmButtons("hex.builtin.common.set"_lang, "hex.builtin.common.cancel"_lang,
                [&, this]{
                    setBaseAddress(this->m_baseAddress);
                    editor->closePopup();
                },
                [&]{
                    editor->closePopup();
                });
        }

    private:
        static void setBaseAddress(u64 baseAddress) {
            if (ImHexApi::Provider::isValid())
                ImHexApi::Provider::get()->setBaseAddress(baseAddress);
        }

    private:
        u64 m_baseAddress;
    };

    class PopupResize : public ViewHexEditor::Popup {
    public:
        explicit PopupResize(u64 currSize) : m_size(currSize) {}

        void draw(ViewHexEditor *editor) override {
            ImGui::TextUnformatted("hex.builtin.view.hex_editor.menu.edit.resize"_lang);

            ImGui::InputHexadecimal("##resize", &this->m_size);
            if (ImGui::IsItemFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Enter))) {
                resize(static_cast<size_t>(this->m_size));
                editor->closePopup();
            }

            View::confirmButtons("hex.builtin.common.set"_lang, "hex.builtin.common.cancel"_lang,
                [&, this]{
                    resize(static_cast<size_t>(this->m_size));
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

            ImGui::InputHexadecimal("hex.builtin.common.address"_lang, &this->m_address);
            ImGui::InputHexadecimal("hex.builtin.common.size"_lang, &this->m_size);

            View::confirmButtons("hex.builtin.common.set"_lang, "hex.builtin.common.cancel"_lang,
                [&, this]{
                    insert(this->m_address, static_cast<size_t>(this->m_size));
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

            ImGui::InputHexadecimal("hex.builtin.common.address"_lang, &this->m_address);
            ImGui::InputHexadecimal("hex.builtin.common.size"_lang, &this->m_size);

            View::confirmButtons("hex.builtin.common.set"_lang, "hex.builtin.common.cancel"_lang,
                [&, this]{
                    remove(this->m_address, static_cast<size_t>(this->m_size));
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

    ViewHexEditor::ViewHexEditor() : View("hex.builtin.view.hex_editor.name") {
        this->m_currDataVisualizer = ContentRegistry::HexEditor::impl::getVisualizers()["hex.builtin.visualizer.hexadecimal.8bit"];

        this->registerShortcuts();
        this->registerEvents();
        this->registerMenuItems();

        ImHexApi::HexEditor::addForegroundHighlightingProvider([this](u64 address, const u8 *data, size_t size, bool hasColor) -> std::optional<color_t> {
            hex::unused(address);

            if (hasColor)
                return std::nullopt;

            if (!this->m_grayOutZero)
                return std::nullopt;

            for (u32 i = 0; i < size; i++)
                if (data[i] != 0x00)
                    return std::nullopt;

            return ImGui::GetColorU32(ImGuiCol_TextDisabled);
        });

    }

    constexpr static u16 getByteColumnSeparatorCount(u16 columnCount) {
        return (columnCount - 1) / 8;
    }

    constexpr static bool isColumnSeparatorColumn(u16 currColumn, u16 columnCount) {
        return currColumn > 0 && (currColumn) < columnCount && ((currColumn) % 8) == 0;
    }

    static std::optional<color_t> queryBackgroundColor(u64 address, const u8 *data, size_t size) {
        std::optional<color_t> result;
        for (const auto &[id, callback] : ImHexApi::HexEditor::impl::getBackgroundHighlightingFunctions()) {
            if (auto color = callback(address, data, size, result.has_value()); color.has_value())
                return color.value();
        }

        if (result.has_value())
            return result;

        for (const auto &[id, highlighting] : ImHexApi::HexEditor::impl::getBackgroundHighlights()) {
            if (highlighting.getRegion().overlaps({ address, size }))
                return highlighting.getColor();
        }

        return std::nullopt;
    }

    static std::optional<color_t> queryForegroundColor(u64 address, const u8 *data, size_t size) {
        std::optional<color_t> result;
        for (const auto &[id, callback] : ImHexApi::HexEditor::impl::getForegroundHighlightingFunctions()) {
            if (auto color = callback(address, data, size, result.has_value()); color.has_value())
                result = color;
        }

        if (result.has_value())
            return result;

        for (const auto &[id, highlighting] : ImHexApi::HexEditor::impl::getForegroundHighlights()) {
            if (highlighting.getRegion().overlaps({ address, size }))
                return highlighting.getColor();
        }

        return std::nullopt;
    }

    struct CustomEncodingData {
        std::string displayValue;
        size_t advance;
        ImColor color;
    };

    static CustomEncodingData queryCustomEncodingData(const EncodingFile &encodingFile, u64 address) {
        const auto longestSequence = encodingFile.getLongestSequence();

        if (longestSequence == 0)
            return { ".", 1, 0xFFFF8000 };

        auto provider = ImHexApi::Provider::get();
        size_t size   = std::min<size_t>(longestSequence, provider->getActualSize() - address);

        std::vector<u8> buffer(size);
        provider->read(address + provider->getBaseAddress() + provider->getCurrentPageAddress(), buffer.data(), size);

        const auto [decoded, advance] = encodingFile.getEncodingFor(buffer);
        const ImColor color = [&decoded = decoded, &advance = advance]{
            if (decoded.length() == 1 && std::isalnum(decoded[0]))
                return ImGui::GetCustomColorU32(ImGuiCustomCol_ToolbarBlue);
            else if (decoded.length() == 1 && advance == 1)
                return ImGui::GetCustomColorU32(ImGuiCustomCol_ToolbarRed);
            else if (decoded.length() > 1 && advance == 1)
                return ImGui::GetCustomColorU32(ImGuiCustomCol_ToolbarYellow);
            else if (advance > 1)
                return ImGui::GetColorU32(ImGuiCol_Text);
            else
                return ImGui::GetCustomColorU32(ImGuiCustomCol_ToolbarBlue);
        }();

        return { std::string(decoded), advance, color };
    }

    static auto getCellPosition() {
        return ImGui::GetCursorScreenPos() - ImGui::GetStyle().CellPadding;
    }

    static void drawTooltip(u64 address, const u8 *data, size_t size) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, scaled(ImVec2(5, 5)));

        for (const auto &[id, callback] : ImHexApi::HexEditor::impl::getTooltipFunctions()) {
            callback(address, data, size);
        }

        const auto &tooltips = ImHexApi::HexEditor::impl::getTooltips();
        if (!tooltips.empty()) {
            ImGui::BeginTooltip();

            for (const auto &[id, tooltip] : tooltips) {
                if (ImGui::BeginTable("##tooltips", 1, ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoClip)) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    if (tooltip.getRegion().overlaps({ address, size })) {
                        ImGui::ColorButton(tooltip.getValue().c_str(), ImColor(tooltip.getColor()));
                        ImGui::SameLine(0, 10);
                        ImGui::TextUnformatted(tooltip.getValue().c_str());
                    }

                    ImGui::PushStyleColor(ImGuiCol_TableRowBg, tooltip.getColor());
                    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, tooltip.getColor());
                    ImGui::EndTable();
                    ImGui::PopStyleColor(2);
                }
            }

            ImGui::EndTooltip();
        }

        ImGui::PopStyleVar();
    }

    void ViewHexEditor::drawCell(u64 address, u8 *data, size_t size, bool hovered) {
        auto provider = ImHexApi::Provider::get();

        if (this->m_shouldUpdateEditingValue) {
            this->m_shouldUpdateEditingValue = false;

            this->m_editingBytes.resize(size);
            std::memcpy(this->m_editingBytes.data(), data, size);
        }

        if (this->m_editingAddress != address) {
            this->m_currDataVisualizer->draw(address, data, size, this->m_upperCaseHex);

            if (hovered && provider->isWritable()) {
                // Enter editing mode when double-clicking a cell
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    this->m_editingAddress = address;
                    this->m_shouldModifyValue = false;
                    this->m_enteredEditingMode = true;

                    this->m_editingBytes.resize(size);
                    std::memcpy(this->m_editingBytes.data(), data, size);
                }
            }
        }
        else {
            ImGui::SetKeyboardFocusHere();
            ImGui::SetNextFrameWantCaptureKeyboard(true);

            if (this->m_currDataVisualizer->drawEditing(*this->m_editingAddress, this->m_editingBytes.data(), this->m_editingBytes.size(), this->m_upperCaseHex, this->m_enteredEditingMode) || this->m_shouldModifyValue) {

                provider->write(*this->m_editingAddress, this->m_editingBytes.data(), this->m_editingBytes.size());

                if (!this->m_selectionChanged && !ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    auto nextEditingAddress = *this->m_editingAddress + this->m_currDataVisualizer->getBytesPerCell();
                    this->setSelection(nextEditingAddress, nextEditingAddress);

                    if (nextEditingAddress >= provider->getSize())
                        this->m_editingAddress = std::nullopt;
                    else
                        this->m_editingAddress = nextEditingAddress;
                } else {
                    this->m_editingAddress = std::nullopt;
                }

                this->m_shouldModifyValue = false;
                this->m_shouldUpdateEditingValue = true;
            }

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !hovered) {
                this->m_editingAddress = std::nullopt;
                this->m_shouldModifyValue = false;
            }

            this->m_enteredEditingMode = false;
        }
    }

    void ViewHexEditor::drawSelectionFrame(u32 x, u32 y, u64 byteAddress, u16 bytesPerCell, const ImVec2 &cellPos, const ImVec2 &cellSize) {
        if (!this->isSelectionValid()) return;

        const auto selection = this->getSelection();
        if (!Region { byteAddress, 1 }.isWithin(selection))
            return;

        const color_t SelectionFrameColor = ImGui::GetColorU32(ImGuiCol_Text);

        auto drawList = ImGui::GetWindowDrawList();

        // Draw vertical line at the left of first byte and the start of the line
        if (x == 0 || byteAddress == selection.getStartAddress())
            drawList->AddLine(cellPos, cellPos + ImVec2(0, cellSize.y), ImColor(SelectionFrameColor), 1.0F);

        // Draw vertical line at the right of the last byte and the end of the line
        if (x == u16((this->m_bytesPerRow / bytesPerCell) - 1) || (byteAddress + bytesPerCell) > selection.getEndAddress())
            drawList->AddLine(cellPos + ImVec2(cellSize.x, -1), cellPos + cellSize, ImColor(SelectionFrameColor), 1.0F);

        // Draw horizontal line at the top of the bytes
        if (y == 0 || (byteAddress - this->m_bytesPerRow) < selection.getStartAddress())
            drawList->AddLine(cellPos, cellPos + ImVec2(cellSize.x + 1, 0), ImColor(SelectionFrameColor), 1.0F);

        // Draw horizontal line at the bottom of the bytes
        if ((byteAddress + this->m_bytesPerRow) > selection.getEndAddress())
            drawList->AddLine(cellPos + ImVec2(0, cellSize.y), cellPos + cellSize + ImVec2(1, 0), ImColor(SelectionFrameColor), 1.0F);
    }

    void ViewHexEditor::drawPopup() {
        // Popup windows
        if (this->m_shouldOpenPopup) {
            this->m_shouldOpenPopup = false;
            ImGui::OpenPopup("##hex_editor_popup");
        }

        ImGui::SetNextWindowPos(ImGui::GetWindowPos() + ImGui::GetWindowContentRegionMin() - ImGui::GetStyle().WindowPadding, ImGuiCond_Appearing);
        if (ImGui::BeginPopup("##hex_editor_popup", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |ImGuiWindowFlags_NoTitleBar)) {

            if (this->m_currPopup != nullptr)
                this->m_currPopup->draw(this);
            else
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        } else {
            this->closePopup();
        }

        // Right click menu
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
            ImGui::OpenPopup("hex.builtin.menu.edit"_lang);

        if (ImGui::BeginPopup("hex.builtin.menu.edit"_lang)) {

            bool needsSeparator = false;
            for (auto &[priority, menuItem] : ContentRegistry::Interface::getMenuItems()) {
                if (menuItem.unlocalizedName != "hex.builtin.menu.edit")
                    continue;

                if (needsSeparator)
                    ImGui::Separator();

                menuItem.callback();
                needsSeparator = true;
            }

            ImGui::EndPopup();
        }
    }

    void ViewHexEditor::drawEditor(const ImVec2 &size) {
        const float SeparatorColumWidth   = 6_scaled;
        const auto CharacterSize          = ImGui::CalcTextSize("0");

        const auto bytesPerCell    = this->m_currDataVisualizer->getBytesPerCell();
        const u16 columnCount      = this->m_bytesPerRow / bytesPerCell;
        const auto byteColumnCount = columnCount + getByteColumnSeparatorCount(columnCount);

        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.5, 0));
        if (ImGui::BeginTable("##hex", 2 + byteColumnCount + 2 + 2 , ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoKeepColumnsVisible, size)) {
            View::discardNavigationRequests();
            ImGui::TableSetupScrollFreeze(0, 2);

            // Row address column
            ImGui::TableSetupColumn("hex.builtin.common.address"_lang);
            ImGui::TableSetupColumn("");

            // Byte columns
            for (u16 i = 0; i < columnCount; i++) {
                if (isColumnSeparatorColumn(i, columnCount))
                    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, SeparatorColumWidth);

                ImGui::TableSetupColumn(hex::format(this->m_upperCaseHex ? "{:0{}X}" : "{:0{}x}", i * bytesPerCell, this->m_currDataVisualizer->getMaxCharsPerCell()).c_str(), ImGuiTableColumnFlags_WidthFixed, CharacterSize.x * this->m_currDataVisualizer->getMaxCharsPerCell() + 6 + this->m_byteCellPadding);
            }

            // ASCII column
            ImGui::TableSetupColumn("");
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, (CharacterSize.x + this->m_characterCellPadding) * this->m_bytesPerRow);

            // Custom encoding column
            ImGui::TableSetupColumn("");
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);

            ImGui::TableNextRow();
            for (i32 i = 0; i < ImGui::TableGetColumnCount(); i++) {
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(ImGui::TableGetColumnName(i));
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + CharacterSize.y / 2);
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            if (ImHexApi::Provider::isValid()) {
                auto provider = ImHexApi::Provider::get();
                auto &providerData = ProviderExtraData::get(provider).editor;

                std::pair<Region, bool> validRegion = { Region::Invalid(), false };
                const auto isCurrRegionValid = [&validRegion, &provider](u64 address){
                    auto &[currRegion, currRegionValid] = validRegion;
                    if (!Region{ address, 1 }.isWithin(currRegion)) {
                        validRegion = provider->getRegionValidity(address);
                    }

                    return currRegionValid;
                };

                ImGuiListClipper clipper;

                clipper.Begin(std::ceil(provider->getSize() / (long double)(this->m_bytesPerRow)), CharacterSize.y);
                while (clipper.Step()) {
                    this->m_visibleRowCount = clipper.DisplayEnd - clipper.DisplayStart;

                    // Loop over rows
                    for (u64 y = u64(clipper.DisplayStart); y < u64(clipper.DisplayEnd); y++) {

                        // Draw address column
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextFormatted(this->m_upperCaseHex ? "{:08X}: " : "{:08x}: ", y * this->m_bytesPerRow + provider->getBaseAddress() + provider->getCurrentPageAddress());
                        ImGui::TableNextColumn();

                        const u8 validBytes = std::min<u64>(this->m_bytesPerRow, provider->getSize() - y * this->m_bytesPerRow);

                        std::vector<u8> bytes(this->m_bytesPerRow, 0x00);
                        provider->read(y * this->m_bytesPerRow + provider->getBaseAddress() + provider->getCurrentPageAddress(), bytes.data(), validBytes);

                        std::vector<std::tuple<std::optional<color_t>, std::optional<color_t>>> cellColors;
                        {
                            for (u64 x = 0; x <  std::ceil(float(validBytes) / bytesPerCell); x++) {
                                const u64 byteAddress = y * this->m_bytesPerRow + x * bytesPerCell + provider->getBaseAddress() + provider->getCurrentPageAddress();

                                const auto cellBytes = std::min<u64>(validBytes, bytesPerCell);

                                // Query cell colors
                                if (x < std::ceil(float(validBytes) / bytesPerCell)) {
                                    const auto foregroundColor = queryForegroundColor(byteAddress, &bytes[x * cellBytes], cellBytes);
                                    const auto backgroundColor = [&]{
                                        auto color = queryBackgroundColor(byteAddress, &bytes[x * cellBytes], cellBytes);

                                        if (this->isSelectionValid()) {
                                            auto selection = this->getSelection();

                                            if (byteAddress >= selection.getStartAddress() && byteAddress <= selection.getEndAddress()) {
                                                if (color.has_value())
                                                    color = (ImAlphaBlendColors(color.value(), this->m_selectionColor)) & 0x00FFFFFF;
                                                else
                                                    color = this->m_selectionColor;
                                            }
                                        }

                                        if (color.has_value())
                                            color = (*color & 0x00FFFFFF) | (this->m_selectionColor & 0xFF000000);

                                        return color;
                                    }();

                                    cellColors.emplace_back(
                                        foregroundColor,
                                        backgroundColor
                                    );
                                } else {
                                    cellColors.emplace_back(
                                        std::nullopt,
                                        std::nullopt
                                    );
                                }
                            }
                        }

                        // Draw byte columns
                        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(3, 0));

                        for (u64 x = 0; x < columnCount; x++) {
                            const u64 byteAddress = y * this->m_bytesPerRow + x * bytesPerCell + provider->getBaseAddress() + provider->getCurrentPageAddress();

                            ImGui::TableNextColumn();
                            if (isColumnSeparatorColumn(x, columnCount))
                                ImGui::TableNextColumn();

                            if (x < std::ceil(float(validBytes) / bytesPerCell)) {
                                auto cellStartPos = getCellPosition();
                                auto cellSize = (CharacterSize * ImVec2(this->m_currDataVisualizer->getMaxCharsPerCell(), 1) + (ImVec2(3, 2) * ImGui::GetStyle().CellPadding) - ImVec2(1, 0) * ImGui::GetStyle().CellPadding) + ImVec2(1 + this->m_byteCellPadding, 0);
                                auto maxCharsPerCell = this->m_currDataVisualizer->getMaxCharsPerCell();

                                auto [foregroundColor, backgroundColor] = cellColors[x];

                                if (isColumnSeparatorColumn(x + 1, columnCount)) {
                                    if (this->isSelectionValid() && this->getSelection().getEndAddress() != x + y * columnCount)
                                    cellSize.x += SeparatorColumWidth + 1;
                                }

                                if (y == u64(clipper.DisplayStart))
                                    cellSize.y -= (ImGui::GetStyle().CellPadding.y + 1);

                                // Draw highlights and selection
                                if (backgroundColor.has_value()) {
                                    auto drawList = ImGui::GetWindowDrawList();

                                    // Draw background color
                                    drawList->AddRectFilled(cellStartPos, cellStartPos + cellSize, backgroundColor.value());

                                    // Draw frame around mouse selection
                                    this->drawSelectionFrame(x, y, byteAddress, bytesPerCell, cellStartPos, cellSize);
                                }

                                const bool cellHovered = ImGui::IsMouseHoveringRect(cellStartPos, cellStartPos + cellSize, false);

                                this->handleSelection(byteAddress, bytesPerCell, &bytes[x * bytesPerCell], cellHovered);

                                // Get byte foreground color
                                if (foregroundColor.has_value())
                                    ImGui::PushStyleColor(ImGuiCol_Text, *foregroundColor);

                                // Draw cell content
                                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                                ImGui::PushItemWidth((CharacterSize * maxCharsPerCell).x);
                                if (isCurrRegionValid(byteAddress))
                                    this->drawCell(byteAddress, &bytes[x * bytesPerCell], bytesPerCell, cellHovered);
                                else
                                    ImGui::TextFormatted("{}", std::string(maxCharsPerCell, '?'));
                                ImGui::PopItemWidth();
                                ImGui::PopStyleVar();

                                if (foregroundColor.has_value())
                                    ImGui::PopStyleColor();
                            }
                        }
                        ImGui::PopStyleVar();

                        ImGui::TableNextColumn();
                        ImGui::TableNextColumn();

                        // Draw ASCII column
                        if (this->m_showAscii) {
                            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
                            if (ImGui::BeginTable("##ascii_column", this->m_bytesPerRow)) {
                                for (u64 x = 0; x < this->m_bytesPerRow; x++)
                                    ImGui::TableSetupColumn(hex::format("##ascii_cell{}", x).c_str(), ImGuiTableColumnFlags_WidthFixed, CharacterSize.x + this->m_characterCellPadding);

                                ImGui::TableNextRow();

                                for (u64 x = 0; x < this->m_bytesPerRow; x++) {
                                    ImGui::TableNextColumn();

                                    const u64 byteAddress = y * this->m_bytesPerRow + x + provider->getBaseAddress() + provider->getCurrentPageAddress();

                                    const auto cellStartPos = getCellPosition();
                                    const auto cellSize = CharacterSize + ImVec2(this->m_characterCellPadding, 0);

                                    const bool cellHovered = ImGui::IsMouseHoveringRect(cellStartPos, cellStartPos + cellSize, true);

                                    if (x < validBytes) {
                                        this->handleSelection(byteAddress, bytesPerCell, &bytes[x], cellHovered);

                                        auto [foregroundColor, backgroundColor] = cellColors[x / bytesPerCell];

                                        // Draw highlights and selection
                                        if (backgroundColor.has_value()) {
                                            auto drawList = ImGui::GetWindowDrawList();

                                            // Draw background color
                                            drawList->AddRectFilled(cellStartPos, cellStartPos + cellSize, backgroundColor.value());

                                            this->drawSelectionFrame(x, y, byteAddress, 1, cellStartPos, cellSize);
                                        }

                                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + this->m_characterCellPadding / 2);
                                        if (!isCurrRegionValid(byteAddress))
                                            ImGui::TextFormatted("?");
                                        else if (std::isprint(bytes[x]))
                                            ImGui::TextFormatted("{:c}", bytes[x]);
                                        else
                                            ImGui::TextDisabled(".");
                                    }
                                }

                                ImGui::EndTable();
                            }
                            ImGui::PopStyleVar();
                        }

                        ImGui::TableNextColumn();
                        ImGui::TableNextColumn();

                        // Draw Custom encoding column
                        if (this->m_currCustomEncoding.has_value()) {
                            std::vector<std::pair<u64, CustomEncodingData>> encodingData;
                            u32 offset = 0;
                            do {
                                const u64 address = y * this->m_bytesPerRow + offset + provider->getBaseAddress() + provider->getCurrentPageAddress();

                                auto result = queryCustomEncodingData(*this->m_currCustomEncoding, address);
                                offset += std::max<size_t>(1, result.advance);

                                encodingData.emplace_back(address, result);
                            } while (offset < this->m_bytesPerRow);

                            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
                            ImGui::PushID(y);
                            if (ImGui::BeginTable("##encoding_cell", encodingData.size(), ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoKeepColumnsVisible)) {
                                ImGui::TableNextRow();

                                for (const auto &[address, data] : encodingData) {
                                    ImGui::TableNextColumn();

                                    const auto cellStartPos = getCellPosition();
                                    const auto cellSize = ImGui::CalcTextSize(data.displayValue.c_str()) * ImVec2(1, 0) + ImVec2(0, CharacterSize.y);
                                    const bool cellHovered = ImGui::IsMouseHoveringRect(cellStartPos, cellStartPos + cellSize, true);


                                    const auto x = address % this->m_bytesPerRow;
                                    if (x < validBytes && isCurrRegionValid(address)) {
                                        auto [foregroundColor, backgroundColor] = cellColors[x / bytesPerCell];

                                        // Draw highlights and selection
                                        if (backgroundColor.has_value()) {
                                            auto drawList = ImGui::GetWindowDrawList();

                                            // Draw background color
                                            drawList->AddRectFilled(cellStartPos, cellStartPos + cellSize, backgroundColor.value());

                                            this->drawSelectionFrame(x, y, address, 1, cellStartPos, cellSize);
                                        }

                                        ImGui::PushItemWidth(cellSize.x);
                                        ImGui::TextFormattedColored(data.color, "{}", data.displayValue);
                                        ImGui::PopItemWidth();

                                        this->handleSelection(address, data.advance, &bytes[address % this->m_bytesPerRow], cellHovered);
                                    }
                                }

                                ImGui::EndTable();
                            }
                            ImGui::PopStyleVar();
                            ImGui::PopID();

                        }

                        // Scroll to the cursor if it's either at the top or bottom edge of the screen
                        if (this->m_shouldScrollToSelection && this->isSelectionValid()) {
                            // Make sure simply clicking on a byte at the edge of the screen won't cause scrolling
                            if ((ImGui::IsMouseDown(ImGuiMouseButton_Left) && providerData.selectionStart != providerData.selectionEnd)) {
                                auto fractionPerLine = 1.0 / (this->m_visibleRowCount + 1);

                                if (y == (u64(clipper.DisplayStart) + 3)) {
                                    if (i128(*providerData.selectionEnd - provider->getBaseAddress() - provider->getCurrentPageAddress()) <= (i64(clipper.DisplayStart + 3) * this->m_bytesPerRow)) {
                                        this->m_shouldScrollToSelection = false;
                                        ImGui::SetScrollHereY(fractionPerLine * 5);

                                    }
                                } else if (y == (u64(clipper.DisplayEnd) - 1)) {
                                    if (i128(*providerData.selectionEnd - provider->getBaseAddress() - provider->getCurrentPageAddress()) >= (i64(clipper.DisplayEnd - 2) * this->m_bytesPerRow)) {
                                        this->m_shouldScrollToSelection = false;
                                        ImGui::SetScrollHereY(fractionPerLine * (this->m_visibleRowCount));
                                    }
                                }
                            }

                            // If the cursor is off-screen, directly jump to the byte
                            if (this->m_shouldJumpWhenOffScreen) {
                                this->m_shouldJumpWhenOffScreen = false;

                                const auto pageAddress = provider->getCurrentPageAddress() + provider->getBaseAddress();
                                auto newSelection = this->getSelection();
                                newSelection.address -= pageAddress;

                                if ((newSelection.getStartAddress()) < u64(clipper.DisplayStart * this->m_bytesPerRow))
                                    this->jumpToSelection(false);
                                if ((newSelection.getEndAddress()) > u64(clipper.DisplayEnd * this->m_bytesPerRow))
                                    this->jumpToSelection(false);

                            }
                        }
                    }
                }

                // Handle jumping to selection
                if (this->m_shouldJumpToSelection) {
                    this->m_shouldJumpToSelection = false;

                    auto newSelection = this->getSelection();
                    provider->setCurrentPage(provider->getPageOfAddress(newSelection.address).value_or(0));

                    const auto pageAddress = provider->getCurrentPageAddress() + provider->getBaseAddress();
                    auto scrollPos = (static_cast<long double>(newSelection.getStartAddress() - pageAddress) / this->m_bytesPerRow) * CharacterSize.y;
                    bool scrollUpwards = scrollPos < ImGui::GetScrollY();
                    auto scrollFraction = scrollUpwards ? 0.0F : (1.0F - ((1.0F / this->m_visibleRowCount) * 2));

                    if (this->m_centerOnJump) {
                        scrollFraction = 0.5F;
                        this->m_centerOnJump = false;
                    }

                    ImGui::SetScrollFromPosY(ImGui::GetCursorStartPos().y + scrollPos, scrollFraction);
                }

                if (!this->m_syncScrolling) {
                    if (this->m_shouldUpdateScrollPosition) {
                        this->m_shouldUpdateScrollPosition = false;
                        ImGui::SetScrollY(providerData.scrollPosition);
                    } else {
                        providerData.scrollPosition = ImGui::GetScrollY();
                    }
                }


            } else {
                ImGui::TextFormattedCentered("hex.builtin.view.hex_editor.no_bytes"_lang);
            }

            ImGui::EndTable();
        }
        ImGui::PopStyleVar();
    }

    void ViewHexEditor::drawFooter(const ImVec2 &size) {
        if (ImHexApi::Provider::isValid()) {
            auto provider = ImHexApi::Provider::get();
            const auto pageCount = provider->getPageCount();
            constexpr static u32 MinPage = 1;

            const auto windowEndPos = ImGui::GetWindowPos() + ImGui::GetWindowSize() - ImGui::GetStyle().WindowPadding;
            ImGui::GetWindowDrawList()->AddLine(windowEndPos - ImVec2(0, size.y - 1_scaled), windowEndPos - size + ImVec2(0, 1_scaled), ImGui::GetColorU32(ImGuiCol_Separator), 2.0_scaled);

            if (ImGui::BeginChild("##footer", size, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                if (ImGui::BeginTable("##footer_table", 2)) {
                    ImGui::TableNextRow();

                    // Page slider
                    ImGui::TableNextColumn();
                    {
                        u32 page = provider->getCurrentPage() + 1;

                        ImGui::TextFormatted("{}: ", "hex.builtin.view.hex_editor.page"_lang);
                        ImGui::SameLine();

                        ImGui::BeginDisabled(pageCount <= 1);
                        {
                            if (ImGui::SliderScalar("##page_selection", ImGuiDataType_U32, &page, &MinPage, &pageCount, hex::format("%d / {}", pageCount).c_str()))
                                provider->setCurrentPage(page - 1);
                        }
                        ImGui::EndDisabled();
                    }

                    // Page Address
                    ImGui::TableNextColumn();
                    {
                        ImGui::TextFormatted("{0}: 0x{1:08X} - 0x{2:08X} ({1} - {2})", "hex.builtin.view.hex_editor.region"_lang, provider->getCurrentPageAddress(), provider->getSize());
                    }

                    ImGui::TableNextRow();

                    // Selection
                    ImGui::TableNextColumn();
                    {
                        auto selection = this->getSelection();
                        std::string value;
                        if (this->isSelectionValid()) {
                            value = hex::format("0x{0:08X} - 0x{1:08X} (0x{2:X} | {3})",
                                                selection.getStartAddress(),
                                                selection.getEndAddress(),
                                                selection.getSize(),
                                                hex::toByteString(selection.getSize())
                            );
                        }
                        else
                            value = std::string("hex.builtin.view.hex_editor.selection.none"_lang);

                        ImGui::TextFormatted("{0}: {1}", "hex.builtin.view.hex_editor.selection"_lang, value);
                    }

                    // Loaded data size
                    ImGui::TableNextColumn();
                    {
                        ImGui::TextFormatted("{0}: 0x{1:08X} (0x{2:X} | {3})", "hex.builtin.view.hex_editor.data_size"_lang,
                                             provider->getActualSize(),
                                             provider->getActualSize(),
                                             hex::toByteString(provider->getActualSize())
                                             );
                    }

                    ImGui::EndTable();
                }

            }
            ImGui::EndChild();
        }
    }

    void ViewHexEditor::handleSelection(u64 address, u32 bytesPerCell, const u8 *data, bool cellHovered) {
        if (ImGui::IsWindowHovered() && cellHovered) {
            drawTooltip(address, data, bytesPerCell);

            auto endAddress = address + bytesPerCell - 1;
            auto &selectionStart = ProviderExtraData::getCurrent().editor.selectionStart;

            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                this->setSelection(selectionStart.value_or(address), endAddress);
                this->scrollToSelection();
            }
            else if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                if (ImGui::GetIO().KeyShift)
                    this->setSelection(selectionStart.value_or(address), endAddress);
                else
                    this->setSelection(address, endAddress);

                this->scrollToSelection();
            }
        }
    }

    void ViewHexEditor::drawContent() {

        if (ImGui::Begin(View::toWindowName(this->getUnlocalizedName()).c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            const auto FooterSize = ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeightWithSpacing() * 2.3);
            const auto TableSize = ImGui::GetContentRegionAvail() - ImVec2(0, FooterSize.y);

            this->drawPopup();
            this->drawEditor(TableSize);
            this->drawFooter(FooterSize);
        }
        ImGui::End();

        this->m_selectionChanged = false;
    }

    static void save() {
        ImHexApi::Provider::get()->save();
    }

    static void saveAs() {
        fs::openFileBrowser(fs::DialogMode::Save, {}, [](const auto &path) {
            ImHexApi::Provider::get()->saveAs(path);
        });
    }

    static void copyBytes(const Region &selection) {
        constexpr static auto Format = "{0:02X} ";

        auto provider = ImHexApi::Provider::get();

        auto reader = prv::BufferedReader(provider);
        reader.seek(selection.getStartAddress() + provider->getBaseAddress() + provider->getCurrentPageAddress());
        reader.setEndAddress(selection.getEndAddress() + provider->getBaseAddress() + provider->getCurrentPageAddress());

        std::string result;
        result.reserve(fmt::format(Format, 0x00).size() * selection.getSize());

        for (const auto &byte : reader)
            result += fmt::format(Format, byte);
        result.pop_back();

        ImGui::SetClipboardText(result.c_str());
    }

    static void pasteBytes(const Region &selection) {
        auto provider = ImHexApi::Provider::get();

        std::string clipboard = ImGui::GetClipboardText();
        if (clipboard.empty())
            return;

        // Check for non-hex characters
        bool isValidHexString = std::find_if(clipboard.begin(), clipboard.end(), [](char c) {
            return !std::isxdigit(c) && !std::isspace(c);
        }) == clipboard.end();

        if (!isValidHexString) return;

        // Remove all whitespace
        clipboard.erase(std::remove_if(clipboard.begin(), clipboard.end(), [](char c) { return std::isspace(c); }), clipboard.end());

        // Only paste whole bytes
        if (clipboard.length() % 2 != 0) return;

        // Convert hex string to bytes
        std::vector<u8> buffer = crypt::decode16(clipboard);

        // Write bytes
        provider->write(selection.getStartAddress() + provider->getBaseAddress() + provider->getCurrentPageAddress(), buffer.data(), std::min(selection.size, buffer.size()));
    }

    static void copyString(const Region &selection) {
        auto provider = ImHexApi::Provider::get();

        std::string buffer(selection.size, 0x00);
        buffer.reserve(selection.size);
        provider->read(selection.getStartAddress() + provider->getBaseAddress() + provider->getCurrentPageAddress(), buffer.data(), selection.size);

        ImGui::SetClipboardText(buffer.c_str());
    }

    void ViewHexEditor::registerShortcuts() {

        // Save operations
        ShortcutManager::addShortcut(this, CTRL + Keys::S, [] {
            save();
        });
        ShortcutManager::addShortcut(this, CTRL + SHIFT + Keys::S, [] {
            saveAs();
        });

        // Select All
        ShortcutManager::addShortcut(this, CTRL + Keys::A, [this] {
            if (ImHexApi::Provider::isValid())
                this->setSelection(size_t(0), ImHexApi::Provider::get()->getActualSize());
        });

        // Select range
        ShortcutManager::addShortcut(this, CTRL + SHIFT + Keys::A, [this] {
            if (ImHexApi::Provider::isValid())
                this->openPopup<PopupSelect>();
        });

        // Remove selection
        ShortcutManager::addShortcut(this, Keys::Escape, [this] {
            auto &data = ProviderExtraData::getCurrent().editor;

            data.selectionStart.reset();
            data.selectionEnd.reset();
            EventManager::post<EventRegionSelected>(this->getSelection());
        });

        // Move cursor around
        ShortcutManager::addShortcut(this, Keys::Up, [this] {
            auto selection = this->getSelection();

            if (selection.getEndAddress() >= this->m_bytesPerRow) {
                auto pos = selection.getEndAddress() - this->m_bytesPerRow;
                this->setSelection(pos, pos);
                this->scrollToSelection();
                this->jumpIfOffScreen();
            }
        });
        ShortcutManager::addShortcut(this, Keys::Down, [this] {
            auto selection = this->getSelection();

            auto pos = selection.getEndAddress() + this->m_bytesPerRow;
            this->setSelection(pos, pos);
            this->scrollToSelection();
            this->jumpIfOffScreen();
        });
        ShortcutManager::addShortcut(this, Keys::Left, [this] {
            auto selection = this->getSelection();

            if (selection.getEndAddress() > 0) {
                auto pos = selection.getEndAddress() - 1;
                this->setSelection(pos, pos);
                this->scrollToSelection();
                this->jumpIfOffScreen();
            }
        });
        ShortcutManager::addShortcut(this, Keys::Right, [this] {
            auto selection = this->getSelection();

            auto pos = selection.getEndAddress() + 1;
            this->setSelection(pos, pos);
            this->scrollToSelection();
            this->jumpIfOffScreen();
        });

        ShortcutManager::addShortcut(this, Keys::PageUp, [this] {
            auto selection = this->getSelection();

            u64 visibleByteCount = this->m_bytesPerRow * this->m_visibleRowCount;
            if (selection.getEndAddress() >= visibleByteCount) {
                auto pos = selection.getEndAddress() - visibleByteCount;
                this->setSelection(pos, pos);
                this->scrollToSelection();
                this->jumpIfOffScreen();
            }
        });
        ShortcutManager::addShortcut(this, Keys::PageDown, [this] {
            auto selection = this->getSelection();

            auto pos = selection.getEndAddress() + (this->m_bytesPerRow * this->m_visibleRowCount);
            this->setSelection(pos, pos);
            this->scrollToSelection();
            this->jumpIfOffScreen();
        });

        // Move selection around
        ShortcutManager::addShortcut(this, SHIFT + Keys::Up, [this] {
            auto selection = this->getSelection();

            this->setSelection(std::max<u64>(selection.getStartAddress(), this->m_bytesPerRow) - this->m_bytesPerRow, selection.getEndAddress());
            this->scrollToSelection();
            this->jumpIfOffScreen();
        });
        ShortcutManager::addShortcut(this, SHIFT + Keys::Down, [this] {
            auto selection = this->getSelection();

            this->setSelection(selection.getStartAddress() + this->m_bytesPerRow, selection.getEndAddress());
            this->scrollToSelection();
            this->jumpIfOffScreen();

        });
        ShortcutManager::addShortcut(this, SHIFT + Keys::Left, [this] {
            auto selection = this->getSelection();

            this->setSelection(std::max<u64>(selection.getStartAddress(), 1) - 1, selection.getEndAddress());
            this->scrollToSelection();
            this->jumpIfOffScreen();
        });
        ShortcutManager::addShortcut(this, SHIFT + Keys::Right, [this] {
            auto selection = this->getSelection();

            this->setSelection(selection.getStartAddress() + 1, selection.getEndAddress());
            this->scrollToSelection();
            this->jumpIfOffScreen();
        });
        ShortcutManager::addShortcut(this, Keys::PageUp, [this] {
            auto selection = this->getSelection();
            u64 visibleByteCount = this->m_bytesPerRow * this->m_visibleRowCount;

            if (selection.getEndAddress() >= visibleByteCount) {
                auto pos = selection.getEndAddress() - visibleByteCount;
                this->setSelection(pos, selection.getEndAddress());
                this->scrollToSelection();
                this->jumpIfOffScreen();
            }
        });
        ShortcutManager::addShortcut(this, Keys::PageDown, [this] {
            auto selection = this->getSelection();
            auto pos = selection.getEndAddress() + (this->m_bytesPerRow * this->m_visibleRowCount);

            this->setSelection(pos, selection.getEndAddress());
            this->scrollToSelection();
            this->jumpIfOffScreen();
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::G, [this] {
            if (!ImHexApi::Provider::isValid()) return;

            this->openPopup<PopupGoto>();
        });
        ShortcutManager::addShortcut(this, CTRL + Keys::F, [this] {
            if (!ImHexApi::Provider::isValid()) return;

            this->openPopup<PopupFind>();
        });

        // Copy
        ShortcutManager::addShortcut(this, CTRL + Keys::C, [this] {
            const auto selection = this->getSelection();
            copyBytes(selection);
        });
        ShortcutManager::addShortcut(this, CTRL + SHIFT + Keys::C, [this] {
            const auto selection = this->getSelection();
            copyString(selection);
        });

        // Paste
        ShortcutManager::addShortcut(this, CTRL + Keys::V, [this] {
            const auto selection = this->getSelection();
            pasteBytes(selection);
        });

        // Undo / Redo
        ShortcutManager::addShortcut(this, CTRL + Keys::Z, [] {
            if (ImHexApi::Provider::isValid())
                ImHexApi::Provider::get()->undo();
        });
        ShortcutManager::addShortcut(this, CTRL + Keys::Y, [] {
            if (ImHexApi::Provider::isValid())
                ImHexApi::Provider::get()->redo();
        });
    }

    void ViewHexEditor::registerEvents() {
        EventManager::subscribe<EventRegionSelected>(this, [this](auto) {
            this->m_shouldModifyValue = true;
        });

        EventManager::subscribe<RequestSelectionChange>(this, [this](Region region) {
            auto provider = ImHexApi::Provider::get();

            if (region == Region::Invalid()) {
                auto &providerData = ProviderExtraData::get(provider).editor;

                providerData.selectionStart.reset();
                providerData.selectionEnd.reset();

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

        EventManager::subscribe<QuerySelection>(this, [this](auto &region) {
            if (this->isSelectionValid())
                region = this->getSelection();
        });

        EventManager::subscribe<EventProviderChanged>(this, [this](auto, auto) {
            this->m_shouldUpdateScrollPosition = true;

            if (this->isSelectionValid())
                EventManager::post<EventRegionSelected>(this->getSelection());
        });

        EventManager::subscribe<EventSettingsChanged>(this, [this] {
            {
                auto bytesPerRow = ContentRegistry::Settings::getSetting("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.bytes_per_row");

                if (bytesPerRow.is_number())
                    this->m_bytesPerRow = static_cast<int>(bytesPerRow);
            }

            {
                auto ascii = ContentRegistry::Settings::getSetting("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.ascii");

                if (ascii.is_number())
                    this->m_showAscii = static_cast<int>(ascii);
            }

            {
                auto greyOutZeros = ContentRegistry::Settings::getSetting("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.grey_zeros");

                if (greyOutZeros.is_number())
                    this->m_grayOutZero = static_cast<int>(greyOutZeros);
            }

            {
                auto upperCaseHex = ContentRegistry::Settings::getSetting("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.uppercase_hex");

                if (upperCaseHex.is_number())
                    this->m_upperCaseHex = static_cast<int>(upperCaseHex);
            }

            {
                auto selectionColor = ContentRegistry::Settings::getSetting("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.highlight_color");

                if (selectionColor.is_number())
                    this->m_selectionColor = static_cast<color_t>(selectionColor);
            }

            {
                auto &visualizers = ContentRegistry::HexEditor::impl::getVisualizers();
                auto selectedVisualizer = ContentRegistry::Settings::getSetting("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.visualizer");

                if (selectedVisualizer.is_string() && visualizers.contains(selectedVisualizer))
                    this->m_currDataVisualizer = visualizers[selectedVisualizer];
                else
                    this->m_currDataVisualizer = visualizers["hex.builtin.visualizer.hexadecimal.8bit"];
            }

            {
                auto syncScrolling = ContentRegistry::Settings::getSetting("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.sync_scrolling");

                if (syncScrolling.is_number())
                    this->m_syncScrolling = static_cast<int>(syncScrolling);
            }

            {
                auto padding = ContentRegistry::Settings::getSetting("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.byte_padding");

                if (padding.is_number())
                    this->m_byteCellPadding = static_cast<int>(padding);
            }

            {
                auto padding = ContentRegistry::Settings::getSetting("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.char_padding");

                if (padding.is_number())
                    this->m_characterCellPadding = static_cast<int>(padding);
            }

        });
    }

    void ViewHexEditor::registerMenuItems() {
        // Basic operations
        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.file", 1100, [&] {
            auto provider      = ImHexApi::Provider::get();
            bool providerValid = ImHexApi::Provider::isValid();

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.save"_lang, "CTRL + S", false, providerValid && provider->isWritable())) {
                save();
            }

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.save_as"_lang, "CTRL + SHIFT + S", false, providerValid && provider->isWritable())) {
                saveAs();
            }
        });


        // Metadata save/load
        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.file", 1200, [&, this] {
            bool providerValid = ImHexApi::Provider::isValid();

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.load_encoding_file"_lang, nullptr, false, providerValid)) {
                std::vector<std::fs::path> paths;
                for (const auto &path : fs::getDefaultPaths(fs::ImHexPath::Encodings)) {
                    std::error_code error;
                    for (const auto &entry : std::fs::recursive_directory_iterator(path, error)) {
                        if (!entry.is_regular_file()) continue;

                        paths.push_back(entry);
                    }
                }

                View::showFileChooserPopup(paths, { {"Thingy Table File", "tbl"} },
                    [this](const auto &path) {
                        this->m_currCustomEncoding = EncodingFile(EncodingFile::Type::Thingy, path);
                    });
            }
        });


        // Search / Goto
        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.file", 1400, [&, this] {
            bool providerValid = ImHexApi::Provider::isValid();

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.search"_lang, "CTRL + F", false, providerValid)) {
                this->openPopup<PopupFind>();
            }

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.goto"_lang, "CTRL + G", false, providerValid)) {
                this->openPopup<PopupGoto>();
            }

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.select"_lang, "CTRL + SHIFT + A", false, providerValid)) {
                this->openPopup<PopupSelect>();
            }
        });


        // Copy / Paste
        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.edit", 1100, [&] {
            auto provider      = ImHexApi::Provider::get();
            bool providerValid = ImHexApi::Provider::isValid();
            auto selection     = ImHexApi::HexEditor::getSelection();

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.copy"_lang, "CTRL + C", false, selection.has_value()))
                copyBytes(*selection);

            if (ImGui::BeginMenu("hex.builtin.view.hex_editor.menu.edit.copy_as"_lang, selection.has_value() && providerValid)) {
                if (ImGui::MenuItem("hex.builtin.view.hex_editor.copy.hex"_lang, "CTRL + SHIFT + C"))
                    copyString(*selection);
                if (ImGui::MenuItem("hex.builtin.view.hex_editor.copy.address"_lang))
                    ImGui::SetClipboardText(hex::format("0x{:08X}", selection->getStartAddress()).c_str());

                ImGui::Separator();

                for (const auto &[unlocalizedName, callback] : ContentRegistry::DataFormatter::getEntries()) {
                    if (ImGui::MenuItem(LangEntry(unlocalizedName))) {
                        ImGui::SetClipboardText(
                            callback(
                                provider,
                                selection->getStartAddress() + provider->getBaseAddress() + provider->getCurrentPageAddress(),
                                selection->size
                                ).c_str()
                        );
                    }
                }

                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.paste"_lang, "CTRL + V", false, selection.has_value()))
                pasteBytes(*selection);

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.select_all"_lang, "CTRL + A", false, selection.has_value() && providerValid))
                ImHexApi::HexEditor::setSelection(provider->getBaseAddress(), provider->getActualSize());
        });

        // Popups
        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.edit", 1200, [&] {
            auto provider       = ImHexApi::Provider::get();
            bool providerValid  = ImHexApi::Provider::isValid();
            auto selection      = ImHexApi::HexEditor::getSelection();

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.set_base"_lang, nullptr, false, providerValid && provider->isReadable())) {
                this->openPopup<PopupBaseAddress>(provider->getBaseAddress());
            }

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.resize"_lang, nullptr, false, providerValid && provider->isResizable())) {
                this->openPopup<PopupResize>(provider->getActualSize());
            }

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.insert"_lang, nullptr, false, providerValid && provider->isResizable() && selection.has_value())) {
                this->openPopup<PopupInsert>(selection->getStartAddress(), 0x00);
            }

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.remove"_lang, nullptr, false, providerValid && provider->isResizable() && selection.has_value())) {
                this->openPopup<PopupRemove>(selection->getStartAddress(), selection->getSize());
            }

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.jump_to"_lang, nullptr, false, providerValid && provider->isResizable() && selection.has_value())) {
                if (selection->getSize() <= sizeof(u64)) {
                    u64 value = 0;
                    provider->read(selection->getStartAddress(), &value, selection->getSize());

                    if (value < provider->getBaseAddress() + provider->getActualSize()) {
                        ImHexApi::HexEditor::setSelection(value, 1);
                    }
                }

            }
        });
    }

}