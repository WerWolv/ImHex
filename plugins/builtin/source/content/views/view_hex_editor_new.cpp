#include "content/views/view_hex_editor_new.hpp"
#include "content/providers/file_provider.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/helpers/project_file_handler.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/providers/buffered_reader.hpp>
#include "math_evaluator.hpp"

#include <imgui_internal.h>
#include <nlohmann/json.hpp>

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
        } m_mode;

        std::string m_input;
        MathEvaluator<i128> m_evaluator;
    };

    class PopupFind : public ViewHexEditorNew::Popup {
    public:
        void draw(ViewHexEditorNew *editor) override {
            const auto ButtonSize = ImVec2(ImGui::GetTextLineHeightWithSpacing(), ImGui::GetTextLineHeightWithSpacing());
            const auto ButtonColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);

            std::vector<u8> searchSequence;

            ImGui::TextUnformatted("Find...");
            if (ImGui::BeginTabBar("##find_tabs")) {
                if (ImGui::BeginTabItem("Hex")) {
                    ImGui::SetKeyboardFocusHere();
                    ImGui::CaptureKeyboardFromApp(true);

                    if (ImGui::InputText("##input", this->m_input, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CharsHexadecimal)) {
                        searchSequence = decodeByteString(this->m_input);
                    }

                    ImGui::SameLine();
                    if (ImGui::IconButton(ICON_VS_SEARCH, ButtonColor, ButtonSize)) {
                        this->m_shouldSearch = true;
                        this->m_backwards = false;
                        this->m_lastFind.reset();
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("String")) {
                    ImGui::SetKeyboardFocusHere();
                    ImGui::CaptureKeyboardFromApp(true);

                    if (ImGui::InputText("##input", this->m_input, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                        searchSequence.clear();
                        std::copy(this->m_input.begin(), this->m_input.end(), std::back_inserter(searchSequence));

                        // Remove null termination
                        searchSequence.pop_back();
                    }

                    ImGui::SameLine();
                    if (ImGui::IconButton(ICON_VS_SEARCH, ButtonColor, ButtonSize)) {
                        this->m_shouldSearch = true;
                        this->m_backwards = false;
                    }

                    ImGui::EndTabItem();
                }

                ImGui::NewLine();
                if (ImGui::IconButton(ICON_VS_ARROW_UP, ButtonColor, ButtonSize)) {
                    this->m_shouldSearch = true;
                    this->m_backwards = true;
                }
                ImGui::SameLine();
                if (ImGui::IconButton(ICON_VS_ARROW_UP, ButtonColor, ButtonSize)) {
                    this->m_shouldSearch = true;
                    this->m_backwards = false;
                }

                ImGui::EndTabBar();
            }



            if (!searchSequence.empty()) {
                if (auto region = this->findSequence(searchSequence, this->m_backwards); region.has_value()) {
                    editor->setSelection(region->getStartAddress(), region->getEndAddress());
                    editor->jumpToSelection();
                }
            }
        }

        std::optional<Region> findSequence(const std::vector<u8> &sequence, bool backwards) {
            hex::prv::BufferedReader reader(ImHexApi::Provider::get());

            reader.seek(this->m_lastFind.value_or(0x00));

            if (!backwards) {
                auto occurrence = std::search(reader.begin(), reader.end(), std::boyer_moore_horspool_searcher(sequence.begin(), sequence.end()));
                if (occurrence != reader.end()) {
                    this->m_lastFind = occurrence.getAddress();
                    return Region { *this->m_lastFind, *this->m_lastFind + sequence.size() - 1 };
                }
            } else {
                auto occurrence = std::search(reader.rbegin(), reader.rend(), std::boyer_moore_horspool_searcher(sequence.begin(), sequence.end()));
                if (occurrence != reader.rend()) {
                    this->m_lastFind = occurrence.getAddress();
                    return Region { *this->m_lastFind, *this->m_lastFind + sequence.size() - 1 };
                }
            }


            return std::nullopt;
        }

        bool m_shouldSearch = false;
        bool m_backwards = false;

        std::string m_input;
        std::optional<u64> m_lastFind;
    };

    ViewHexEditorNew::ViewHexEditorNew() : View("Hex Editor New") {
        this->m_currDataVisualizer = ContentRegistry::HexEditor::impl::getVisualizers()["hex.builtin.visualizer.hexadecimal.8bit"];

        this->registerShortcuts();
        this->registerEvents();

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

                    this->m_editingBytes.resize(size);
                    std::memcpy(this->m_editingBytes.data(), data, size);
                }
            }
        }
        else {
            ImGui::SetKeyboardFocusHere();
            ImGui::CaptureKeyboardFromApp(true);

            this->m_shouldModifyValue = this->m_currDataVisualizer->drawEditing(address, this->m_editingBytes.data(), this->m_editingBytes.size(), this->m_upperCaseHex);
            this->m_shouldModifyValue = this->m_shouldModifyValue || this->m_selectionChanged;
        }

        if (this->m_shouldModifyValue) {
            this->m_shouldModifyValue = false;

            log::info("Changes written");

            if (!this->m_selectionChanged) {
                auto nextEditingAddress = *this->m_editingAddress + this->m_currDataVisualizer->getBytesPerCell();
                this->setSelection(nextEditingAddress, nextEditingAddress);

                if (nextEditingAddress >= ImHexApi::Provider::get()->getSize())
                    this->m_editingAddress = std::nullopt;
                else
                    this->m_editingAddress = nextEditingAddress;
            } else {
                this->m_editingAddress = std::nullopt;
            }
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
        for (const auto &[id, callback] : ImHexApi::HexEditor::impl::getTooltipFunctions()) {
            callback(address, data, size);
        }

        for (const auto &[id, tooltip] : ImHexApi::HexEditor::impl::getTooltips()) {
            if (tooltip.getRegion().overlaps({ address, size })) {
                ImGui::BeginTooltip();
                ImGui::ColorButton(tooltip.getValue().c_str(), ImColor(tooltip.getColor()));
                ImGui::SameLine(0, 10);
                ImGui::TextUnformatted(tooltip.getValue().c_str());
                ImGui::EndTooltip();
            }
        }
    }

    void ViewHexEditorNew::drawSelectionFrame(u32 x, u32 y, u64 byteAddress, const ImVec2 &cellPos, const ImVec2 &cellSize) {
        if (!this->isSelectionValid()) return;

        const color_t SelectionFrameColor = ImGui::GetColorU32(ImGuiCol_Text);
        const auto selection = this->getSelection();

        auto drawList = ImGui::GetWindowDrawList();

        // Draw vertical line at the left of first byte and the start of the line
        if (x == 0 || byteAddress == selection.getStartAddress())
            drawList->AddLine(cellPos, cellPos + ImVec2(0, cellSize.y), ImColor(SelectionFrameColor), 1.0F);

        // Draw vertical line at the right of the last byte and the end of the line
        if (x == u16((this->m_bytesPerRow / this->m_currDataVisualizer->getBytesPerCell()) - 1) || byteAddress == selection.getEndAddress())
            drawList->AddLine(cellPos + ImVec2(cellSize.x, -1), cellPos + cellSize, ImColor(SelectionFrameColor), 1.0F);

        // Draw horizontal line at the top of the bytes
        if (y == 0 || (byteAddress - this->m_bytesPerRow) < selection.getStartAddress())
            drawList->AddLine(cellPos, cellPos + ImVec2(cellSize.x + 1, 0), ImColor(SelectionFrameColor), 1.0F);

        // Draw horizontal line at the bottom of the bytes
        if ((byteAddress + this->m_bytesPerRow) > selection.getEndAddress())
            drawList->AddLine(cellPos + ImVec2(0, cellSize.y), cellPos + cellSize + ImVec2(1, 0), ImColor(SelectionFrameColor), 1.0F);
    }

    void ViewHexEditorNew::drawPopup() {
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
    }

    void ViewHexEditorNew::drawContent() {
        if (ImGui::Begin(View::toWindowName(this->getUnlocalizedName()).c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {

            const float SeparatorColumWidth   = 6_scaled;
            const auto CharacterSize          = ImGui::CalcTextSize("0");

            const color_t SelectionColor = (ImAlphaBlendColors(0x32FFFFFF, 0x60C08080) & 0x00FFFFFF) | (this->m_highlightAlpha << 24);


            const auto FooterSize = ImVec2(ImGui::GetContentRegionAvailWidth(), ImGui::GetTextLineHeightWithSpacing() * 3);
            const auto TableSize = ImGui::GetContentRegionAvail() - ImVec2(0, FooterSize.y);

            const u16 columnCount = this->m_bytesPerRow / this->m_currDataVisualizer->getBytesPerCell();
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

                    ImGui::TableSetupColumn(hex::format(this->m_upperCaseHex ? "{:0{}X}" : "{:0{}x}", i * this->m_currDataVisualizer->getBytesPerCell(), this->m_currDataVisualizer->getMaxCharsPerCell()).c_str(), ImGuiTableColumnFlags_WidthFixed, CharacterSize.x * this->m_currDataVisualizer->getMaxCharsPerCell() + 6);
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
                                    const u64 byteAddress = y * this->m_bytesPerRow + x;

                                    const auto cellBytes = std::min<u64>(validBytes, this->m_currDataVisualizer->getBytesPerCell());

                                    // Query cell colors
                                    const auto foregroundColor = queryForegroundColor(byteAddress, &bytes[x], cellBytes);
                                    const auto backgroundColor = [&]{
                                        auto color = queryBackgroundColor(byteAddress, &bytes[x], cellBytes);

                                        if ((byteAddress >= selectionMin && byteAddress <= selectionMax)) {
                                            if (color.has_value())
                                                color = ImAlphaBlendColors(color.value(), SelectionColor);
                                            else
                                                color = SelectionColor;
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
                            for (u64 x = 0; x < columnCount; x++) {
                                const u64 byteAddress = y * this->m_bytesPerRow + x;

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
                                        this->drawSelectionFrame(x, y, byteAddress, cellStartPos, adjustedCellSize);
                                    }


                                    const bool cellHovered = ImGui::IsMouseHoveringRect(cellStartPos, cellStartPos + adjustedCellSize, false);

                                    // Handle selection
                                    {
                                        if (ImGui::IsWindowHovered() && cellHovered && ImGui::IsMouseHoveringRect(ImGui::GetWindowPos(), ImGui::GetWindowPos() + TableSize)) {
                                            drawTooltip(byteAddress, &bytes[x], this->m_currDataVisualizer->getBytesPerCell());
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

                                    // Get byte foreground color
                                    if (foregroundColor.has_value())
                                        ImGui::PushStyleColor(ImGuiCol_Text, *foregroundColor);

                                    // Draw cell content
                                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                                    ImGui::PushItemWidth((CharacterSize * this->m_currDataVisualizer->getMaxCharsPerCell()).x);
                                    this->drawCell(byteAddress, &bytes[x], this->m_currDataVisualizer->getBytesPerCell(), cellHovered);
                                    ImGui::PopItemWidth();
                                    ImGui::PopStyleVar();

                                    if (foregroundColor.has_value())
                                        ImGui::PopStyleColor();
                                }
                            }
                            ImGui::PopStyleVar();

                            // Scroll to the cursor if it's either at the top or bottom edge of the screen
                            if (this->m_selectionChanged && this->m_selectionEnd != InvalidSelection && this->m_selectionStart != this->m_selectionEnd) {
                                if (y == clipper.DisplayStart) {
                                    if (i128(this->m_selectionEnd) <= (i64(clipper.DisplayStart + 2) * this->m_bytesPerRow))
                                        ImGui::SetScrollHereY(0.1);
                                } else if (y == (clipper.DisplayEnd - 1)) {
                                    if (i128(this->m_selectionEnd) >= (i64(clipper.DisplayEnd - 2) * this->m_bytesPerRow))
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

                                        const u64 byteAddress = y * this->m_bytesPerRow + x;

                                        const auto cellStartPos  = (ImGui::GetWindowPos() + ImGui::GetCursorPos()) - ImGui::GetStyle().CellPadding - ImVec2(ImGui::GetScrollX(), ImGui::GetScrollY());
                                        const auto cellSize = CharacterSize;

                                        const bool cellHovered = ImGui::IsMouseHoveringRect(cellStartPos, cellStartPos + cellSize, false);

                                        // Handle selection
                                        {
                                            if (ImGui::IsWindowHovered() && cellHovered && ImGui::IsMouseHoveringRect(ImGui::GetWindowPos(), ImGui::GetWindowPos() + TableSize)) {
                                                drawTooltip(byteAddress, &bytes[x], this->m_currDataVisualizer->getBytesPerCell());
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

                                        auto [foregroundColor, backgroundColor] = cellColors[x];

                                        // Draw highlights and selection
                                        if (backgroundColor.has_value()) {
                                            auto drawList = ImGui::GetWindowDrawList();

                                            // Draw background color
                                            drawList->AddRectFilled(cellStartPos, cellStartPos + cellSize, backgroundColor.value());

                                            this->drawSelectionFrame(x, y, byteAddress, cellStartPos, cellSize);
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
                    if (ImGui::BeginTable("##footer_table", 3)) {
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
                            ImGui::TextFormatted("Base Address: 0x{0:08X}", provider->getBaseAddress());
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

    void ViewHexEditorNew::openFile(const std::fs::path &path) {
        hex::prv::Provider *provider = nullptr;
        EventManager::post<RequestCreateProvider>("hex.builtin.provider.file", &provider);

        if (auto fileProvider = dynamic_cast<prv::FileProvider *>(provider)) {
            fileProvider->setPath(path);
            if (!fileProvider->open()) {
                View::showErrorPopup("hex.builtin.view.hex_editor.error.open"_lang);
                ImHexApi::Provider::remove(provider);

                return;
            }
        }

        if (!provider->isWritable()) {
            View::showErrorPopup("hex.builtin.view.hex_editor.error.read_only"_lang);
        }

        if (!provider->isAvailable()) {
            View::showErrorPopup("hex.builtin.view.hex_editor.error.open"_lang);
            ImHexApi::Provider::remove(provider);

            return;
        }

        ProjectFile::setFilePath(path);

        this->getWindowOpenState() = true;

        EventManager::post<EventFileLoaded>(path);
        EventManager::post<EventDataChanged>();
        EventManager::post<EventHighlightingChanged>();
    }

    void ViewHexEditorNew::registerShortcuts() {
        ShortcutManager::addShortcut(this, Keys::Escape, [this] {
            this->setSelection(InvalidSelection, InvalidSelection);
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
        EventManager::subscribe<RequestOpenFile>(this, [this](const auto &path) { this->openFile(path); });

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

}