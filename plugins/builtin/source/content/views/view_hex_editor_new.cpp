#include "content/views/view_hex_editor_new.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/helpers/project_file_handler.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/providers/buffered_reader.hpp>
#include <hex/helpers/crypto.hpp>

#include "math_evaluator.hpp"

#include <imgui_internal.h>
#include <nlohmann/json.hpp>

#include <thread>

namespace hex::plugin::builtin {

    class PopupGoto : public ViewHexEditorNew::Popup {
    public:

        void draw(ViewHexEditorNew *editor) override {
            ImGui::TextUnformatted("Goto...");
            if (ImGui::BeginTabBar("##goto_tabs")) {
                if (ImGui::BeginTabItem("Absolute")) {
                    this->m_mode = Mode::Absolute;
                    ImGui::EndTabItem();
                }

                ImGui::BeginDisabled(!editor->isSelectionValid());
                if (ImGui::BeginTabItem("Relative")) {
                    this->m_mode = Mode::Relative;
                    ImGui::EndTabItem();
                }
                ImGui::EndDisabled();

                if (ImGui::BeginTabItem("End")) {
                    this->m_mode = Mode::End;
                    ImGui::EndTabItem();
                }

                ImGui::SetKeyboardFocusHere();
                ImGui::CaptureKeyboardFromApp(true);
                if (ImGui::InputText("##input", this->m_input, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                    if (auto result = this->m_evaluator.evaluate(this->m_input); result.has_value()) {
                        const auto inputResult = result.value();
                        u64 newAddress = 0x00;

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
                            case Mode::End: {
                                    newAddress = ImHexApi::Provider::get()->getActualSize() - inputResult;
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
            End
        };

        Mode m_mode = Mode::Absolute;

        std::string m_input;
        MathEvaluator<i128> m_evaluator;
    };

    class PopupFind : public ViewHexEditorNew::Popup {
    public:
        void draw(ViewHexEditorNew *editor) override {
            const auto ButtonSize = ImVec2(ImGui::CalcTextSize(ICON_VS_SEARCH).x, ImGui::GetTextLineHeight()) + ImGui::GetStyle().CellPadding * 2;
            const auto ButtonColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);

            std::vector<u8> searchSequence;
            bool shouldSearch = false;
            bool backwards = false;

            ImGui::TextUnformatted("Find...");
            if (ImGui::BeginTabBar("##find_tabs")) {
                if (ImGui::BeginTabItem("Hex")) {
                    ImGui::SetKeyboardFocusHere();
                    ImGui::CaptureKeyboardFromApp(true);

                    if (ImGui::InputText("##input", this->m_input, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CharsHexadecimal)) {
                        shouldSearch = true;
                        backwards = false;
                    }

                    ImGui::SameLine();
                    if (ImGui::IconButton(ICON_VS_SEARCH, ButtonColor, ButtonSize)) {
                        shouldSearch = true;
                        backwards = false;
                        this->m_lastFind.reset();
                    }

                    if (ImGui::IconButton(ICON_VS_ARROW_UP, ButtonColor, ButtonSize)) {
                        shouldSearch = true;
                        backwards = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::IconButton(ICON_VS_ARROW_DOWN, ButtonColor, ButtonSize)) {
                        shouldSearch = true;
                        backwards = false;
                    }

                    if (shouldSearch) {
                        searchSequence = decodeByteString(this->m_input);
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("String")) {
                    ImGui::SetKeyboardFocusHere();
                    ImGui::CaptureKeyboardFromApp(true);

                    if (ImGui::InputText("##input", this->m_input, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                        shouldSearch = false;
                    }

                    ImGui::SameLine();
                    if (ImGui::IconButton(ICON_VS_SEARCH, ButtonColor, ButtonSize)) {
                        shouldSearch = true;
                        backwards = false;
                        this->m_lastFind.reset();
                    }

                    if (ImGui::IconButton(ICON_VS_ARROW_UP, ButtonColor, ButtonSize)) {
                        shouldSearch = true;
                        backwards = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::IconButton(ICON_VS_ARROW_DOWN, ButtonColor, ButtonSize)) {
                        shouldSearch = true;
                        backwards = false;
                    }

                    if (shouldSearch) {
                        searchSequence.clear();
                        std::copy(this->m_input.begin(), this->m_input.end(), std::back_inserter(searchSequence));

                        // Remove null termination
                        searchSequence.pop_back();
                    }

                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }


            if (!searchSequence.empty() && shouldSearch) {
                if (auto region = this->findSequence(searchSequence, backwards); region.has_value()) {
                    editor->setSelection(region->getStartAddress(), region->getEndAddress());
                    editor->jumpToSelection();
                }
            }
        }

        std::optional<Region> findSequence(const std::vector<u8> &sequence, bool backwards) {
            hex::prv::BufferedReader reader(ImHexApi::Provider::get());

            reader.seek(this->m_lastFind.value_or(0x00));

            constexpr static auto searchFunction = [](const auto &haystackBegin, const auto &haystackEnd, const auto &needleBegin, const auto &needleEnd) {
                #if defined(OS_MACOS)
                    return std::search(haystackBegin, haystackEnd, needleBegin, needleEnd);
                #else
                    return std::search(haystackBegin, haystackEnd, std::boyer_moore_horspool_searcher(needleBegin, needleEnd));
                #endif
            };

            if (!backwards) {
                auto occurrence = searchFunction(reader.begin(), reader.end(), sequence.begin(), sequence.end());
                if (occurrence != reader.end()) {
                    this->m_lastFind = occurrence.getAddress();
                    return Region { *this->m_lastFind, *this->m_lastFind + sequence.size() - 1 };
                }
            } else {
                auto occurrence = searchFunction(reader.rbegin(), reader.rend(), sequence.begin(), sequence.end());
                if (occurrence != reader.rend()) {
                    this->m_lastFind = occurrence.getAddress();
                    return Region { *this->m_lastFind, *this->m_lastFind + sequence.size() - 1 };
                }
            }


            return std::nullopt;
        }

        std::string m_input;
        std::optional<u64> m_lastFind;
    };

    class PopupBaseAddress : public ViewHexEditorNew::Popup {
    public:
        explicit PopupBaseAddress(u64 baseAddress) : m_baseAddress(baseAddress) {

        }

        void draw(ViewHexEditorNew *editor) override {
            ImGui::TextUnformatted("Base Address");

            if (ImGui::InputHexadecimal("##base_address", &this->m_baseAddress, ImGuiInputTextFlags_EnterReturnsTrue)) {
                this->setBaseAddress(this->m_baseAddress);
                editor->closePopup();
            }

            View::confirmButtons("hex.builtin.common.set"_lang, "hex.builtin.common.cancel"_lang,
                [&, this]{
                    this->setBaseAddress(this->m_baseAddress);
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

    class PopupResize : public ViewHexEditorNew::Popup {
    public:
        explicit PopupResize(u64 currSize) : m_size(currSize) {}

        void draw(ViewHexEditorNew *editor) override {
            ImGui::TextUnformatted("Resize");

            if (ImGui::InputHexadecimal("##resize", &this->m_size, ImGuiInputTextFlags_EnterReturnsTrue)) {
                this->resize(static_cast<size_t>(this->m_size));
            }

            View::confirmButtons("hex.builtin.common.set"_lang, "hex.builtin.common.cancel"_lang,
                [&, this]{
                    this->resize(static_cast<size_t>(this->m_size));
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

    class PopupInsert : public ViewHexEditorNew::Popup {
    public:
        PopupInsert(u64 address, size_t size) : m_address(address), m_size(size) {}

        void draw(ViewHexEditorNew *editor) override {
            ImGui::TextUnformatted("Insert Bytes");

            if (ImGui::InputHexadecimal("Address", &this->m_address, ImGuiInputTextFlags_EnterReturnsTrue)) {
                this->insert(this->m_address, static_cast<size_t>(this->m_size));
            }
            if (ImGui::InputHexadecimal("Size", &this->m_size, ImGuiInputTextFlags_EnterReturnsTrue)) {
                this->insert(this->m_address, static_cast<size_t>(this->m_size));
            }

            View::confirmButtons("hex.builtin.common.set"_lang, "hex.builtin.common.cancel"_lang,
                [&, this]{
                    this->insert(this->m_address, static_cast<size_t>(this->m_size));
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

    ViewHexEditorNew::ViewHexEditorNew() : View("Hex Editor New") {
        this->m_currDataVisualizer = ContentRegistry::HexEditor::impl::getVisualizers()["hex.builtin.visualizer.hexadecimal.8bit"];

        this->registerShortcuts();
        this->registerEvents();
        this->registerMenuItems();

        ImHexApi::HexEditor::addForegroundHighlightingProvider([this](u64 address, const u8 *data, size_t size) -> std::optional<color_t> {
            hex::unused(address);

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

    void ViewHexEditorNew::drawCell(u64 address, u8 *data, size_t size, bool hovered) {
        if (this->m_editingAddress != address) {
            this->m_currDataVisualizer->draw(address, data, size, this->m_upperCaseHex);

            if (hovered) {
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
            ImGui::CaptureKeyboardFromApp(true);

            if (this->m_currDataVisualizer->drawEditing(address, this->m_editingBytes.data(), this->m_editingBytes.size(), this->m_upperCaseHex, this->m_enteredEditingMode) || this->m_shouldModifyValue) {

                auto provider = ImHexApi::Provider::get();

                provider->write(address, this->m_editingBytes.data(), this->m_editingBytes.size());

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
            }

            this->m_enteredEditingMode = false;
        }
    }

    static std::optional<color_t> queryBackgroundColor(u64 address, const u8 *data, size_t size) {
        for (const auto &[id, callback] : ImHexApi::HexEditor::impl::getBackgroundHighlightingFunctions()) {
            if (auto color = callback(address, data, size); color.has_value())
                return color.value();
        }

        for (const auto &[id, highlighting] : ImHexApi::HexEditor::impl::getBackgroundHighlights()) {
            if (highlighting.getRegion().overlaps({ address, size }))
                return highlighting.getColor();
        }

        return std::nullopt;
    }

    static std::optional<color_t> queryForegroundColor(u64 address, const u8 *data, size_t size) {
        for (const auto &[id, callback] : ImHexApi::HexEditor::impl::getForegroundHighlightingFunctions()) {
            if (auto color = callback(address, data, size); color.has_value())
                return color.value();
        }

        for (const auto &[id, highlighting] : ImHexApi::HexEditor::impl::getForegroundHighlights()) {
            if (highlighting.getRegion().overlaps({ address, size }))
                return highlighting.getColor();
        }

        return std::nullopt;
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
                if (ImGui::BeginTable("##tooltips", 1, ImGuiTableFlags_RowBg | ImGuiTableFlags_NoClip)) {
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

    void ViewHexEditorNew::drawSelectionFrame(u32 x, u32 y, u64 byteAddress, u16 bytesPerCell, const ImVec2 &cellPos, const ImVec2 &cellSize) {
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
        if (x == u16((this->m_bytesPerRow / bytesPerCell) - 1) || (byteAddress + this->m_currDataVisualizer->getBytesPerCell()) > selection.getEndAddress())
            drawList->AddLine(cellPos + ImVec2(cellSize.x, -1), cellPos + cellSize, ImColor(SelectionFrameColor), 1.0F);

        // Draw horizontal line at the top of the bytes
        if (y == 0 || (byteAddress - this->m_bytesPerRow) < selection.getStartAddress())
            drawList->AddLine(cellPos, cellPos + ImVec2(cellSize.x + 1, 0), ImColor(SelectionFrameColor), 1.0F);

        // Draw horizontal line at the bottom of the bytes
        if ((byteAddress + this->m_bytesPerRow) > selection.getEndAddress())
            drawList->AddLine(cellPos + ImVec2(0, cellSize.y), cellPos + cellSize + ImVec2(1, 0), ImColor(SelectionFrameColor), 1.0F);
    }

    void ViewHexEditorNew::drawPopup() {
        // Popup windows
        if (this->m_shouldOpenPopup) {
            this->m_shouldOpenPopup = false;
            ImGui::OpenPopup("##hex_editor_popup");
        }

        ImGui::SetNextWindowPos(ImGui::GetWindowPos() + ImGui::GetWindowContentRegionMin() - ImGui::GetStyle().WindowPadding);
        if (ImGui::BeginPopup("##hex_editor_popup", ImGuiWindowFlags_NoTitleBar)) {

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
            this->drawEditMenu();
            ImGui::EndPopup();
        }
    }

    void ViewHexEditorNew::drawContent() {
        if (ImGui::Begin(View::toWindowName(this->getUnlocalizedName()).c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {

            const float SeparatorColumWidth   = 6_scaled;
            const auto CharacterSize          = ImGui::CalcTextSize("0");

            const color_t SelectionColor = 0x60C08080;

            const auto FooterSize = ImVec2(ImGui::GetContentRegionAvailWidth(), ImGui::GetTextLineHeightWithSpacing() * 3);
            const auto TableSize = ImGui::GetContentRegionAvail() - ImVec2(0, FooterSize.y);

            const auto bytesPerCell = this->m_currDataVisualizer->getBytesPerCell();
            const u16 columnCount = this->m_bytesPerRow / bytesPerCell;
            const auto byteColumnCount = columnCount + getByteColumnSeparatorCount(columnCount);

            const auto selectionMin = std::min(this->m_selectionStart, this->m_selectionEnd);
            const auto selectionMax = std::max(this->m_selectionStart, this->m_selectionEnd);

            this->drawPopup();

            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.5, 0));
            if (ImGui::BeginTable("##hex", 1 + 1 + byteColumnCount + 1 + 1, ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoKeepColumnsVisible, TableSize)) {
                View::discardNavigationRequests();
                ImGui::TableSetupScrollFreeze(0, 2);

                // Row address column
                ImGui::TableSetupColumn("Address");
                ImGui::TableSetupColumn("");

                // Byte columns
                for (u16 i = 0; i < columnCount; i++) {
                    if (isColumnSeparatorColumn(i, columnCount))
                        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, SeparatorColumWidth);

                    ImGui::TableSetupColumn(hex::format(this->m_upperCaseHex ? "{:0{}X}" : "{:0{}x}", i * bytesPerCell, this->m_currDataVisualizer->getMaxCharsPerCell()).c_str(), ImGuiTableColumnFlags_WidthFixed, CharacterSize.x * this->m_currDataVisualizer->getMaxCharsPerCell() + 6);
                }
                ImGui::TableSetupColumn("");

                // ASCII columns
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, CharacterSize.x * this->m_bytesPerRow);

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

                    ImGuiListClipper clipper(std::ceil(provider->getSize() / (long double)(this->m_bytesPerRow)), CharacterSize.y);

                    while (clipper.Step()) {
                        this->m_visibleRowCount = clipper.DisplayEnd - clipper.DisplayStart;

                        // Loop over rows
                        for (i128 y = clipper.DisplayStart; y < u64(clipper.DisplayEnd); y++) {

                            // Draw address column
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::TextFormatted(this->m_upperCaseHex ? "{:08X}: " : "{:08x}: ", y * this->m_bytesPerRow + provider->getBaseAddress() + provider->getCurrentPageAddress());
                            ImGui::TableNextColumn();

                            const u8 validBytes = std::min<u64>(this->m_bytesPerRow, provider->getSize() - y * this->m_bytesPerRow);

                            std::vector<u8> bytes(validBytes);
                            provider->read(y * this->m_bytesPerRow + provider->getCurrentPageAddress(), bytes.data(), validBytes);

                            std::vector<std::tuple<std::optional<color_t>, std::optional<color_t>>> cellColors;
                            {
                                for (u64 x = 0; x < columnCount; x++) {
                                    const u64 byteAddress = y * this->m_bytesPerRow + x * bytesPerCell + provider->getBaseAddress() + provider->getCurrentPageAddress();

                                    const auto cellBytes = std::min<u64>(validBytes, bytesPerCell);

                                    // Query cell colors
                                    const auto foregroundColor = queryForegroundColor(byteAddress, &bytes[x], cellBytes);
                                    const auto backgroundColor = [&]{
                                        auto color = queryBackgroundColor(byteAddress, &bytes[x], cellBytes);

                                        if ((byteAddress >= selectionMin && byteAddress <= selectionMax)) {
                                            if (color.has_value())
                                                color = ImAlphaBlendColors(color.value(), SelectionColor);
                                            else
                                                color = SelectionColor;

                                            *color = (*color & 0x00FFFFFF) | (this->m_highlightAlpha << 24);
                                        }

                                        return color;
                                    }();

                                    cellColors.emplace_back(
                                        foregroundColor,
                                        backgroundColor
                                    );
                                }
                            }

                            // Draw byte columns
                            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(3, 0));

                            bool shouldScroll = false;
                            for (u64 x = 0; x < columnCount; x++) {
                                const u64 byteAddress = y * this->m_bytesPerRow + x * bytesPerCell + provider->getBaseAddress() + provider->getCurrentPageAddress();

                                ImGui::TableNextColumn();
                                if (isColumnSeparatorColumn(x, columnCount))
                                    ImGui::TableNextColumn();

                                if (x < validBytes) {
                                    const auto cellStartPos  = (ImGui::GetWindowPos() + ImGui::GetCursorPos()) - ImGui::GetStyle().CellPadding - ImVec2(ImGui::GetScrollX(), ImGui::GetScrollY());
                                    const auto cellSize = (CharacterSize * ImVec2(this->m_currDataVisualizer->getMaxCharsPerCell(), 1) + (ImVec2(3, 2) * ImGui::GetStyle().CellPadding) - ImVec2(1, 0) * ImGui::GetStyle().CellPadding) + ImVec2(1, 0);

                                    auto [foregroundColor, backgroundColor] = cellColors[x];

                                    auto adjustedCellSize = cellSize;
                                    if (isColumnSeparatorColumn(x + 1, columnCount) && selectionMax != x + y * columnCount)
                                        adjustedCellSize.x += SeparatorColumWidth + 1;
                                    if (y == clipper.DisplayStart)
                                        adjustedCellSize.y -= (ImGui::GetStyle().CellPadding.y + 1);

                                    // Draw highlights and selection
                                    if (backgroundColor.has_value()) {
                                        auto drawList = ImGui::GetWindowDrawList();

                                        // Draw background color
                                        drawList->AddRectFilled(cellStartPos, cellStartPos + adjustedCellSize, backgroundColor.value());

                                        // Draw frame around mouse selection
                                        this->drawSelectionFrame(x, y, byteAddress, bytesPerCell, cellStartPos, adjustedCellSize);
                                    }


                                    const bool cellHovered = ImGui::IsMouseHoveringRect(cellStartPos, cellStartPos + adjustedCellSize, false);

                                    // Handle selection
                                    {
                                        if (ImGui::IsWindowHovered() && cellHovered && ImGui::IsMouseHoveringRect(ImGui::GetWindowPos(), ImGui::GetWindowPos() + TableSize)) {
                                            drawTooltip(byteAddress, &bytes[x], bytesPerCell);

                                            auto endAddress = byteAddress + bytesPerCell - 1;
                                            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                                                this->setSelection(this->m_selectionStart, endAddress);
                                                shouldScroll = true;
                                            }
                                            else if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                                                if (ImGui::GetIO().KeyShift)
                                                    this->setSelection(this->m_selectionStart, endAddress);
                                                else
                                                    this->setSelection(byteAddress, endAddress);

                                                shouldScroll = true;
                                            }
                                        }

                                    }

                                    // Get byte foreground color
                                    if (foregroundColor.has_value())
                                        ImGui::PushStyleColor(ImGuiCol_Text, *foregroundColor);

                                    // Draw cell content
                                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                                    ImGui::PushItemWidth((CharacterSize * this->m_currDataVisualizer->getMaxCharsPerCell()).x);
                                    this->drawCell(byteAddress, &bytes[x], bytesPerCell, cellHovered);
                                    ImGui::PopItemWidth();
                                    ImGui::PopStyleVar();

                                    if (foregroundColor.has_value())
                                        ImGui::PopStyleColor();
                                }
                            }
                            ImGui::PopStyleVar();

                            // Scroll to the cursor if it's either at the top or bottom edge of the screen
                            if (shouldScroll && this->m_selectionEnd != InvalidSelection && this->m_selectionStart != this->m_selectionEnd) {
                                if (y == (clipper.DisplayStart + 2)) {
                                    if (i128(this->m_selectionEnd - provider->getBaseAddress() - provider->getCurrentPageAddress()) <= (i64(clipper.DisplayStart + 4) * this->m_bytesPerRow))
                                        ImGui::SetScrollHereY(0.1);
                                } else if (y == (clipper.DisplayEnd - 1)) {
                                    if (i128(this->m_selectionEnd - provider->getBaseAddress() - provider->getCurrentPageAddress()) >= (i64(clipper.DisplayEnd - 2) * this->m_bytesPerRow))
                                        ImGui::SetScrollHereY(0.95);
                                }
                            }

                            ImGui::TableNextColumn();
                            ImGui::TableNextColumn();

                            // Draw ASCII column
                            if (this->m_showAscii) {
                                ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
                                if (ImGui::BeginTable("##ascii_cell", this->m_bytesPerRow)) {
                                    ImGui::TableNextRow();

                                    for (u64 x = 0; x < this->m_bytesPerRow; x++) {
                                        ImGui::TableNextColumn();

                                        const u64 byteAddress = y * this->m_bytesPerRow + x + provider->getBaseAddress() + provider->getCurrentPageAddress();

                                        const auto cellStartPos  = (ImGui::GetWindowPos() + ImGui::GetCursorPos()) - ImGui::GetStyle().CellPadding - ImVec2(ImGui::GetScrollX(), ImGui::GetScrollY());
                                        const auto cellSize = CharacterSize;

                                        const bool cellHovered = ImGui::IsMouseHoveringRect(cellStartPos, cellStartPos + cellSize, false);

                                        // Handle selection
                                        {
                                            if (ImGui::IsWindowHovered() && cellHovered && ImGui::IsMouseHoveringRect(ImGui::GetWindowPos(), ImGui::GetWindowPos() + TableSize)) {
                                                drawTooltip(byteAddress, &bytes[x], bytesPerCell);
                                                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                                                    this->setSelection(this->m_selectionStart, byteAddress);
                                                }
                                                else if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                                                    if (ImGui::GetIO().KeyShift)
                                                        this->setSelection(this->m_selectionStart, byteAddress);
                                                    else
                                                        this->setSelection(byteAddress, byteAddress);
                                                }
                                            }
                                        }

                                        auto [foregroundColor, backgroundColor] = cellColors[x / bytesPerCell];

                                        // Draw highlights and selection
                                        if (backgroundColor.has_value()) {
                                            auto drawList = ImGui::GetWindowDrawList();

                                            // Draw background color
                                            drawList->AddRectFilled(cellStartPos, cellStartPos + cellSize, backgroundColor.value());

                                            this->drawSelectionFrame(x, y, byteAddress, 1, cellStartPos, cellSize);
                                        }

                                        if (x < validBytes) {
                                            if (std::isprint(bytes[x]))
                                                ImGui::TextFormatted("{:c}", bytes[x]);
                                            else
                                                ImGui::TextDisabled(".");
                                        }
                                    }

                                    ImGui::EndTable();
                                }
                                ImGui::PopStyleVar();
                            }
                        }
                    }
                } else {
                    ImGui::TextFormattedCentered("No bytes available");
                }

                ImGui::EndTable();
            }
            ImGui::PopStyleVar();

            if (ImHexApi::Provider::isValid()) {
                auto provider = ImHexApi::Provider::get();
                const auto pageCount = provider->getPageCount();
                constexpr static u32 MinPage = 1;

                const auto windowEndPos = ImGui::GetWindowPos() + ImGui::GetWindowSize() - ImGui::GetStyle().WindowPadding;
                ImGui::GetWindowDrawList()->AddLine(windowEndPos - ImVec2(0, FooterSize.y - 1_scaled), windowEndPos - FooterSize + ImVec2(0, 1_scaled), ImGui::GetColorU32(ImGuiCol_Separator), 2.0_scaled);

                if (ImGui::BeginChild("##footer", FooterSize, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                    if (ImGui::BeginTable("##footer_table", 2)) {
                        ImGui::TableNextRow();

                        // Page slider
                        ImGui::TableNextColumn();
                        {
                            u32 page = provider->getCurrentPage() + 1;
                            ImGui::BeginDisabled(pageCount <= 1);
                            if (ImGui::SliderScalar("Page", ImGuiDataType_U32, &page, &MinPage, &pageCount, hex::format("%d / {}", pageCount).c_str()))
                                provider->setCurrentPage(page - 1);

                            ImGui::EndDisabled();
                        }

                        // Page Address
                        ImGui::TableNextColumn();
                        {
                            ImGui::TextFormatted("Region: 0x{0:08X} - 0x{1:08X} ({0} - {1})", provider->getCurrentPageAddress(), provider->getSize());
                        }

                        ImGui::TableNextRow();

                        // Selection
                        ImGui::TableNextColumn();
                        {
                            if (selectionMin == InvalidSelection || selectionMax == InvalidSelection)
                                ImGui::TextUnformatted("Selection: None");
                            else
                                ImGui::TextFormatted("Selection: 0x{0:08X} - 0x{1:08X} ({0} - {1})", selectionMin, selectionMax);
                        }

                        // Loaded data size
                        ImGui::TableNextColumn();
                        {
                            ImGui::TextFormatted("Data size: 0x{0:08X} ({1})", provider->getActualSize(), hex::toByteString(provider->getActualSize()));
                        }

                        ImGui::EndTable();
                    }

                    ImGui::EndChild();
                }
            }


            // Handle jumping to selection
            if (this->m_shouldScrollToSelection) {
                this->m_shouldScrollToSelection = false;
                ImGui::BeginChild("##hex");
                ImGui::SetScrollFromPosY(ImGui::GetCursorStartPos().y + (float(this->m_selectionStart) / this->m_bytesPerRow) * CharacterSize.y, 0.0);
                ImGui::EndChild();
            }

        }
        ImGui::End();

        this->m_selectionChanged = false;
    }

    void ViewHexEditorNew::drawAlwaysVisible() {

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
        auto provider = ImHexApi::Provider::get();

        std::vector<u8> buffer(selection.size, 0x00);
        provider->read(selection.getStartAddress() + provider->getBaseAddress() + provider->getCurrentPageAddress(), buffer.data(), buffer.size());

        std::string str;
        for (const auto &byte : buffer)
            str += hex::format("{0:02X} ", byte);
        str.pop_back();

        ImGui::SetClipboardText(str.c_str());
    }

    static void pasteBytes(const Region &selection) {
        auto provider = ImHexApi::Provider::get();

        std::string clipboard = ImGui::GetClipboardText();

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
        std::vector<u8> buffer(clipboard.length() / 2, 0x00);
        u32 stringIndex = 0;
        for (u8 &byte : buffer) {
            for (u8 i = 0; i < 2; i++) {
                byte <<= 4;

                char c = clipboard[stringIndex];

                if (c >= '0' && c <= '9') byte |= (c - '0');
                else if (c >= 'a' && c <= 'f') byte |= (c - 'a') + 0xA;
                else if (c >= 'A' && c <= 'F') byte |= (c - 'A') + 0xA;

                stringIndex++;
            }
        }

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

    void ViewHexEditorNew::registerShortcuts() {

        ShortcutManager::addShortcut(this, CTRL + Keys::S, [] {
            save();
        });
        ShortcutManager::addShortcut(this, CTRL + SHIFT + Keys::S, [] {
            saveAs();
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::A, [this] {
            if (ImHexApi::Provider::isValid())
                this->setSelection(size_t(0), ImHexApi::Provider::get()->getActualSize());
        });

        ShortcutManager::addShortcut(this, Keys::Escape, [this] {
            this->m_selectionStart = InvalidSelection;
            this->m_selectionEnd = InvalidSelection;
            EventManager::post<EventRegionSelected>(this->getSelection());
        });

        ShortcutManager::addShortcut(this, Keys::Up, [this] {
            if (this->m_selectionStart >= this->m_bytesPerRow) {
                auto pos = this->m_selectionStart - this->m_bytesPerRow;
                this->setSelection(pos, pos);
            }
        });
        ShortcutManager::addShortcut(this, Keys::Down, [this] {
            auto pos = this->m_selectionStart + this->m_bytesPerRow;
            this->setSelection(pos, pos);
        });
        ShortcutManager::addShortcut(this, Keys::Left, [this] {
            if (this->m_selectionStart > 0) {
                auto pos = this->m_selectionStart - 1;
                this->setSelection(pos, pos);
            }
        });
        ShortcutManager::addShortcut(this, Keys::Right, [this] {
            auto pos = this->m_selectionStart + 1;
            this->setSelection(pos, pos);
        });

        ShortcutManager::addShortcut(this, Keys::PageUp, [this] {
            u64 visibleByteCount = this->m_bytesPerRow * this->m_visibleRowCount;
            if (this->m_selectionStart >= visibleByteCount) {
                auto pos = this->m_selectionStart - visibleByteCount;
                this->setSelection(pos, pos);
                this->jumpToSelection();
            }
        });
        ShortcutManager::addShortcut(this, Keys::PageDown, [this] {
            auto pos = this->m_selectionStart + (this->m_bytesPerRow * this->m_visibleRowCount);
            this->setSelection(pos, pos);
            this->jumpToSelection();
        });

        ShortcutManager::addShortcut(this, SHIFT + Keys::Up, [this] {
            this->setSelection(this->m_selectionStart - this->m_bytesPerRow, this->m_selectionEnd);
        });
        ShortcutManager::addShortcut(this, SHIFT + Keys::Down, [this] {
            this->setSelection(this->m_selectionStart + this->m_bytesPerRow, this->m_selectionEnd);

        });
        ShortcutManager::addShortcut(this, SHIFT + Keys::Left, [this] {
            this->setSelection(this->m_selectionStart - 1, this->m_selectionEnd);

        });
        ShortcutManager::addShortcut(this, SHIFT + Keys::Right, [this] {
            this->setSelection(this->m_selectionStart + 1, this->m_selectionEnd);
        });

        ShortcutManager::addShortcut(this, CTRL + Keys::G, [this] {
            if (!ImHexApi::Provider::isValid()) return;

            this->openPopup<PopupGoto>();
        });
        ShortcutManager::addShortcut(this, CTRL + Keys::F, [this] {
            if (!ImHexApi::Provider::isValid()) return;

            this->openPopup<PopupFind>();
        });
    }

    void ViewHexEditorNew::registerEvents() {
        EventManager::subscribe<EventRegionSelected>(this, [this](auto) {
            this->m_shouldModifyValue = true;
        });

        EventManager::subscribe<RequestSelectionChange>(this, [this](Region region) {
            auto provider = ImHexApi::Provider::get();
            auto page     = provider->getPageOfAddress(region.getStartAddress());

            if (!page.has_value())
                return;

            if (region.size != 0) {
                provider->setCurrentPage(page.value());
                region.address -= provider->getBaseAddress() + provider->getCurrentPageAddress();
                this->setSelection(region);
            }
        });

        EventManager::subscribe<QuerySelection>(this, [this](auto &region) {
            region = this->getSelection();
        });

        EventManager::subscribe<EventSettingsChanged>(this, [this] {
            {
                auto alpha = ContentRegistry::Settings::getSetting("hex.builtin.setting.interface", "hex.builtin.setting.interface.highlight_alpha");

                if (alpha.is_number())
                    this->m_highlightAlpha = alpha;
            }

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
                auto &visualizers = ContentRegistry::HexEditor::impl::getVisualizers();
                auto selectedVisualizer = ContentRegistry::Settings::getSetting("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.visualizer");

                if (selectedVisualizer.is_string() && visualizers.contains(selectedVisualizer))
                    this->m_currDataVisualizer = visualizers[selectedVisualizer];
                else
                    this->m_currDataVisualizer = visualizers["hex.builtin.visualizer.hexadecimal.8bit"];
            }

        });
    }

    void ViewHexEditorNew::registerMenuItems() {
        /* Basic operations */

        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.file", 1100, [&] {
            auto provider      = ImHexApi::Provider::get();
            bool providerValid = ImHexApi::Provider::isValid();


            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.save"_lang, "CTRL + S", false, providerValid && provider->isWritable())) {
                save();
            }

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.save_as"_lang, "CTRL + SHIFT + S", false, providerValid && provider->isWritable())) {
                saveAs();
            }

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.close"_lang, "", false, providerValid)) {
                EventManager::post<EventFileUnloaded>();
                ImHexApi::Provider::remove(ImHexApi::Provider::get());
                providerValid = false;
            }

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.quit"_lang, "", false)) {
                ImHexApi::Common::closeImHex();
            }
        });


        /* Metadata save/load */
        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.file", 1200, [&, this] {
            auto provider      = ImHexApi::Provider::get();
            bool providerValid = ImHexApi::Provider::isValid();

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.open_project"_lang, "")) {
                fs::openFileBrowser(fs::DialogMode::Open, { {"Project File", "hexproj"}
                },
                    [](const auto &path) {
                        ProjectFile::load(path);
                    });
            }

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.save_project"_lang, "", false, providerValid && provider->isWritable())) {
                if (ProjectFile::getProjectFilePath() == "") {
                    fs::openFileBrowser(fs::DialogMode::Save, { {"Project File", "hexproj"}
                    },
                        [](std::fs::path path) {
                            if (path.extension() != ".hexproj") {
                                path.replace_extension(".hexproj");
                            }

                            ProjectFile::store(path);
                        });
                } else
                    ProjectFile::store();
            }

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


        /* Import / Export */
        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.file", 1300, [&] {
            auto provider      = ImHexApi::Provider::get();
            bool providerValid = ImHexApi::Provider::isValid();

            /* Import */
            if (ImGui::BeginMenu("hex.builtin.view.hex_editor.menu.file.import"_lang)) {
                if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.import.base64"_lang)) {

                    fs::openFileBrowser(fs::DialogMode::Open, {}, [](const auto &path) {
                        fs::File inputFile(path, fs::File::Mode::Read);
                        if (!inputFile.isValid()) {
                            View::showErrorPopup("hex.builtin.view.hex_editor.error.open"_lang);
                            return;
                        }

                        auto base64 = inputFile.readBytes();

                        if (!base64.empty()) {
                            auto data = crypt::decode64(base64);

                            if (data.empty())
                                View::showErrorPopup("hex.builtin.view.hex_editor.base64.import_error"_lang);
                            else {
                                fs::openFileBrowser(fs::DialogMode::Save, {}, [&data](const std::fs::path &path) {
                                    fs::File outputFile(path, fs::File::Mode::Create);

                                    if (!outputFile.isValid())
                                        View::showErrorPopup("hex.builtin.view.hex_editor.base64.import_error"_lang);

                                    outputFile.write(data);
                                });
                            }
                        } else {
                            View::showErrorPopup("hex.builtin.view.hex_editor.file_open_error"_lang);
                        }
                    });
                }

                ImGui::Separator();

                if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.import.ips"_lang, nullptr, false)) {

                    fs::openFileBrowser(fs::DialogMode::Open, {}, [](const auto &path) {
                        std::thread([path] {
                            auto task = ImHexApi::Tasks::createTask("hex.builtin.view.hex_editor.processing", 0);

                            auto patchData = fs::File(path, fs::File::Mode::Read).readBytes();
                            auto patch     = hex::loadIPSPatch(patchData);

                            task.setMaxValue(patch.size());

                            auto provider = ImHexApi::Provider::get();

                            u64 progress = 0;
                            for (auto &[address, value] : patch) {
                                provider->addPatch(address, &value, 1);
                                progress++;
                                task.update(progress);
                            }

                            provider->createUndoPoint();
                        }).detach();
                    });
                }

                if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.import.ips32"_lang, nullptr, false)) {
                    fs::openFileBrowser(fs::DialogMode::Open, {}, [](const auto &path) {
                        std::thread([path] {
                            auto task = ImHexApi::Tasks::createTask("hex.builtin.view.hex_editor.processing", 0);

                            auto patchData = fs::File(path, fs::File::Mode::Read).readBytes();
                            auto patch     = hex::loadIPS32Patch(patchData);

                            task.setMaxValue(patch.size());

                            auto provider = ImHexApi::Provider::get();

                            u64 progress = 0;
                            for (auto &[address, value] : patch) {
                                provider->addPatch(address, &value, 1);
                                progress++;
                                task.update(progress);
                            }

                            provider->createUndoPoint();
                        }).detach();
                    });
                }

                ImGui::EndMenu();
            }


            /* Export */
            if (ImGui::BeginMenu("hex.builtin.view.hex_editor.menu.file.export"_lang, providerValid && provider->isWritable())) {
                if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.export.ips"_lang, nullptr, false)) {
                    Patches patches = provider->getPatches();
                    if (!patches.contains(0x00454F45) && patches.contains(0x00454F46)) {
                        u8 value = 0;
                        provider->read(0x00454F45, &value, sizeof(u8));
                        patches[0x00454F45] = value;
                    }

                    std::thread([patches] {
                        auto task = ImHexApi::Tasks::createTask("hex.builtin.view.hex_editor.processing", 0);

                        auto data = generateIPSPatch(patches);

                        ImHexApi::Tasks::doLater([data] {
                            fs::openFileBrowser(fs::DialogMode::Save, {}, [&data](const auto &path) {
                                auto file = fs::File(path, fs::File::Mode::Create);
                                if (!file.isValid()) {
                                    View::showErrorPopup("hex.builtin.view.hex_editor.error.create"_lang);
                                    return;
                                }

                                file.write(data);
                            });
                        });
                    }).detach();
                }

                if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.export.ips32"_lang, nullptr, false)) {
                    Patches patches = provider->getPatches();
                    if (!patches.contains(0x00454F45) && patches.contains(0x45454F46)) {
                        u8 value = 0;
                        provider->read(0x45454F45, &value, sizeof(u8));
                        patches[0x45454F45] = value;
                    }

                    std::thread([patches] {
                        auto task = ImHexApi::Tasks::createTask("hex.builtin.view.hex_editor.processing", 0);

                        auto data = generateIPS32Patch(patches);

                        ImHexApi::Tasks::doLater([data] {
                            fs::openFileBrowser(fs::DialogMode::Save, {}, [&data](const auto &path) {
                                auto file = fs::File(path, fs::File::Mode::Create);
                                if (!file.isValid()) {
                                    View::showErrorPopup("hex.builtin.view.hex_editor.error.create"_lang);
                                    return;
                                }

                                file.write(data);
                            });
                        });
                    }).detach();
                }

                ImGui::EndMenu();
            }
        });


        /* Search / Goto */
        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.file", 1400, [&, this] {
            bool providerValid = ImHexApi::Provider::isValid();

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.search"_lang, "CTRL + F", false, providerValid)) {
                this->openPopup<PopupFind>();
            }

            if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.file.goto"_lang, "CTRL + G", false, providerValid)) {
                this->openPopup<PopupGoto>();
            }
        });


        /* Edit menu */
        ContentRegistry::Interface::addMenuItem("hex.builtin.menu.edit", 1000, [&, this] {
            this->drawEditMenu();
        });
    }

    void ViewHexEditorNew::drawEditMenu() {
        auto provider      = ImHexApi::Provider::get();
        bool providerValid = ImHexApi::Provider::isValid();
        if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.undo"_lang, "CTRL + Z", false, providerValid && provider->canUndo()))
            provider->undo();
        if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.redo"_lang, "CTRL + Y", false, providerValid && provider->canRedo()))
            provider->redo();

        ImGui::Separator();

        const bool bytesSelected = this->isSelectionValid();
        const auto selection = this->getSelection();

        if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.copy"_lang, "CTRL + C", false, bytesSelected))
            copyBytes(selection);

        if (ImGui::BeginMenu("hex.builtin.view.hex_editor.menu.edit.copy_as"_lang, bytesSelected)) {
            if (ImGui::MenuItem("hex.builtin.view.hex_editor.copy.hex"_lang, "CTRL + SHIFT + C"))
                copyString(selection);

            ImGui::Separator();

            for (const auto &[unlocalizedName, callback] : ContentRegistry::DataFormatter::getEntries()) {
                if (ImGui::MenuItem(LangEntry(unlocalizedName))) {
                    ImGui::SetClipboardText(
                        callback(
                            provider,
                            selection.getStartAddress() + provider->getBaseAddress() + provider->getCurrentPageAddress(),
                            selection.size
                        ).c_str()
                    );
                }
            }

            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.paste"_lang, "CTRL + V", false, bytesSelected))
            pasteBytes(selection);

        if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.select_all"_lang, "CTRL + A", false, providerValid))
            ImHexApi::HexEditor::setSelection(provider->getBaseAddress(), provider->getActualSize());

        ImGui::Separator();

        if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.bookmark"_lang, nullptr, false, bytesSelected)) {
            auto base = provider->getBaseAddress();

            ImHexApi::Bookmarks::add(base + selection.getStartAddress(), selection.getEndAddress(), {}, {});
        }

        if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.set_base"_lang, nullptr, false, providerValid && provider->isReadable())) {
            this->openPopup<PopupBaseAddress>(provider->getBaseAddress());
        }

        if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.resize"_lang, nullptr, false, providerValid && provider->isResizable())) {
            this->openPopup<PopupResize>(provider->getActualSize());
        }

        if (ImGui::MenuItem("hex.builtin.view.hex_editor.menu.edit.insert"_lang, nullptr, false, providerValid && provider->isResizable())) {
            this->openPopup<PopupInsert>(this->getSelection().getStartAddress(), 0x00);
        }
    }

}