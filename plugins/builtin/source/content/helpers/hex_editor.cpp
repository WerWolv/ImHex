#include <content/helpers/hex_editor.hpp>

#include <hex/api/imhex_api.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/localization.hpp>

#include <hex/helpers/encoding_file.hpp>
#include <hex/helpers/utils.hpp>

namespace hex {

    /* Data Visualizer */

    class DataVisualizerAscii : public hex::ContentRegistry::HexEditor::DataVisualizer {
    public:
        DataVisualizerAscii() : DataVisualizer(1, 1) { }

        void draw(u64 address, const u8 *data, size_t size, bool upperCase) override {
            hex::unused(address, upperCase);

            if (size == 1) {
                const u8 c = data[0];
                if (std::isprint(c))
                    ImGui::Text("%c", c);
                else
                    ImGui::TextDisabled(".");
            }
            else
                ImGui::TextDisabled(".");
        }

        bool drawEditing(u64 address, u8 *data, size_t size, bool upperCase, bool startedEditing) override {
            hex::unused(address, startedEditing, upperCase);

            if (size == 1) {
                struct UserData {
                    u8 *data;
                    i32 maxChars;

                    bool editingDone;
                };

                UserData userData = {
                        .data = data,
                        .maxChars = this->getMaxCharsPerCell(),

                        .editingDone = false
                };

                ImGui::PushID(reinterpret_cast<void*>(address));
                char buffer[2] = { std::isprint(data[0]) ? char(data[0]) : '.', 0x00 };
                ImGui::InputText("##editing_input", buffer, 2, TextInputFlags | ImGuiInputTextFlags_CallbackEdit, [](ImGuiInputTextCallbackData *data) -> int {
                    auto &userData = *reinterpret_cast<UserData*>(data->UserData);

                    if (data->BufTextLen >= userData.maxChars) {
                        userData.editingDone = true;
                        userData.data[0] = data->Buf[0];
                    }

                    return 0;
                }, &userData);
                ImGui::PopID();

                return userData.editingDone || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Escape);
            }
            else
                return false;
        }
    };

    /* Hex Editor */

    HexEditor::HexEditor() {
        this->m_currDataVisualizer = ContentRegistry::HexEditor::impl::getVisualizers()["hex.builtin.visualizer.hexadecimal.8bit"];

        this->m_grayZeroHighlighter = ImHexApi::HexEditor::addForegroundHighlightingProvider([this](u64 address, const u8 *data, size_t size, bool hasColor) -> std::optional<color_t> {
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

    HexEditor::~HexEditor() {
        ImHexApi::HexEditor::removeForegroundHighlightingProvider(this->m_grayZeroHighlighter);
        EventManager::unsubscribe<EventSettingsChanged>(this);
    }

    constexpr static u16 getByteColumnSeparatorCount(u16 columnCount) {
        return (columnCount - 1) / 8;
    }

    constexpr static bool isColumnSeparatorColumn(u16 currColumn, u16 columnCount) {
        return currColumn > 0 && (currColumn) < columnCount && ((currColumn) % 8) == 0;
    }

    std::optional<color_t> HexEditor::applySelectionColor(u64 byteAddress, std::optional<color_t> color) {
        if (isSelectionValid()) {
            auto selection = getSelection();

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
    }

    struct CustomEncodingData {
        std::string displayValue;
        size_t advance;
        ImColor color;
    };

    static CustomEncodingData queryCustomEncodingData(prv::Provider *provider, const EncodingFile &encodingFile, u64 address) {
        const auto longestSequence = encodingFile.getLongestSequence();

        if (longestSequence == 0)
            return { ".", 1, 0xFFFF8000 };

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

    void HexEditor::drawTooltip(u64 address, const u8 *data, size_t size) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, scaled(ImVec2(5, 5)));

        this->m_tooltipCallback(address, data, size);

        ImGui::PopStyleVar();
    }

    void HexEditor::drawCell(prv::Provider *provider, u64 address, u8 *data, size_t size, bool hovered, CellType cellType) {
        static DataVisualizerAscii asciiVisualizer;

        if (this->m_shouldUpdateEditingValue) {
            this->m_shouldUpdateEditingValue = false;

            this->m_editingBytes.resize(size);
            std::memcpy(this->m_editingBytes.data(), data, size);
        }

        if (this->m_editingAddress != address || this->m_editingCellType != cellType) {
            if (cellType == CellType::Hex)
                this->m_currDataVisualizer->draw(address, data, size, this->m_upperCaseHex);
            else
                asciiVisualizer.draw(address, data, size, this->m_upperCaseHex);

            if (hovered && provider->isWritable()) {
                // Enter editing mode when double-clicking a cell
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    this->m_editingAddress = address;
                    this->m_shouldModifyValue = false;
                    this->m_enteredEditingMode = true;

                    this->m_editingBytes.resize(size);
                    std::memcpy(this->m_editingBytes.data(), data, size);
                    this->m_editingCellType = cellType;
                }
            }
        }
        else {
            ImGui::SetKeyboardFocusHere();
            ImGui::SetNextFrameWantCaptureKeyboard(true);

            bool shouldExitEditingMode = true;
            if (cellType == this->m_editingCellType && cellType == CellType::Hex)
                shouldExitEditingMode = this->m_currDataVisualizer->drawEditing(*this->m_editingAddress, this->m_editingBytes.data(), this->m_editingBytes.size(), this->m_upperCaseHex, this->m_enteredEditingMode);
            else if (cellType == this->m_editingCellType && cellType == CellType::ASCII)
                shouldExitEditingMode = asciiVisualizer.drawEditing(*this->m_editingAddress, this->m_editingBytes.data(), this->m_editingBytes.size(), this->m_upperCaseHex, this->m_enteredEditingMode);

            if (shouldExitEditingMode || this->m_shouldModifyValue) {

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

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !hovered && !this->m_enteredEditingMode) {
                this->m_editingAddress = std::nullopt;
                this->m_shouldModifyValue = false;
            }

            if (!this->m_editingAddress.has_value())
                this->m_editingCellType = CellType::None;
        }
    }

    void HexEditor::drawSelectionFrame(u32 x, u32 y, u64 byteAddress, u16 bytesPerCell, const ImVec2 &cellPos, const ImVec2 &cellSize) const {
        if (!this->isSelectionValid()) return;

        const auto selection = getSelection();
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

    void HexEditor::drawEditor(prv::Provider *provider, const ImVec2 &size) {
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

            if (provider != nullptr && provider->isReadable()) {

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
                                    const auto foregroundColor = this->m_foregroundColorCallback(byteAddress, &bytes[x * cellBytes], cellBytes);
                                    const auto backgroundColor = this->m_backgroundColorCallback(byteAddress, &bytes[x * cellBytes], cellBytes);

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

                                if (isColumnSeparatorColumn(x + 1, columnCount) && cellColors.size() > x + 1) {
                                    auto separatorAddress = x + y * columnCount;
                                    auto [nextForegroundColor, nextBackgroundColor] = cellColors[x + 1];
                                    if ((isSelectionValid() && getSelection().overlaps({ separatorAddress, 1 }) && getSelection().getEndAddress() != separatorAddress) || backgroundColor == nextBackgroundColor)
                                        cellSize.x += SeparatorColumWidth + 1;
                                }

                                if (y == u64(clipper.DisplayStart))
                                    cellSize.y -= (ImGui::GetStyle().CellPadding.y + 1);

                                backgroundColor = applySelectionColor(byteAddress, backgroundColor);

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
                                    this->drawCell(provider, byteAddress, &bytes[x * bytesPerCell], bytesPerCell, cellHovered, CellType::Hex);
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

                                        backgroundColor = applySelectionColor(byteAddress, backgroundColor);

                                        // Draw highlights and selection
                                        if (backgroundColor.has_value()) {
                                            auto drawList = ImGui::GetWindowDrawList();

                                            // Draw background color
                                            drawList->AddRectFilled(cellStartPos, cellStartPos + cellSize, backgroundColor.value());

                                            this->drawSelectionFrame(x, y, byteAddress, 1, cellStartPos, cellSize);
                                        }

                                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + this->m_characterCellPadding / 2);
                                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                                        ImGui::PushItemWidth(CharacterSize.x);
                                        if (!isCurrRegionValid(byteAddress))
                                            ImGui::TextFormatted("?");
                                        else
                                            this->drawCell(provider, byteAddress, &bytes[x], 1, cellHovered, CellType::ASCII);
                                        ImGui::PopItemWidth();
                                        ImGui::PopStyleVar();
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

                                auto result = queryCustomEncodingData(provider, *this->m_currCustomEncoding, address);
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

                                        backgroundColor = applySelectionColor(address, backgroundColor);

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
                        if (this->m_shouldScrollToSelection && isSelectionValid()) {
                            // Make sure simply clicking on a byte at the edge of the screen won't cause scrolling
                            if ((ImGui::IsMouseDown(ImGuiMouseButton_Left) && *this->m_selectionStart != *this->m_selectionEnd)) {
                                auto fractionPerLine = 1.0 / (this->m_visibleRowCount + 1);

                                if (y == (u64(clipper.DisplayStart) + 3)) {
                                    if (i128(this->m_selectionEnd.value() - provider->getBaseAddress() - provider->getCurrentPageAddress()) <= (i64(clipper.DisplayStart + 3) * this->m_bytesPerRow)) {
                                        this->m_shouldScrollToSelection = false;
                                        ImGui::SetScrollHereY(fractionPerLine * 5);

                                    }
                                } else if (y == (u64(clipper.DisplayEnd) - 1)) {
                                    if (i128(this->m_selectionEnd.value() - provider->getBaseAddress() - provider->getCurrentPageAddress()) >= (i64(clipper.DisplayEnd - 2) * this->m_bytesPerRow)) {
                                        this->m_shouldScrollToSelection = false;
                                        ImGui::SetScrollHereY(fractionPerLine * (this->m_visibleRowCount));
                                    }
                                }
                            }

                            // If the cursor is off-screen, directly jump to the byte
                            if (this->m_shouldJumpWhenOffScreen) {
                                this->m_shouldJumpWhenOffScreen = false;

                                const auto pageAddress = provider->getCurrentPageAddress() + provider->getBaseAddress();
                                auto newSelection = getSelection();
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

                    auto newSelection = getSelection();
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
                        ImGui::SetScrollY(this->m_scrollPosition);
                    } else {
                        this->m_scrollPosition = ImGui::GetScrollY();
                    }
                }


            } else {
                ImGui::TextFormattedCentered("hex.builtin.view.hex_editor.no_bytes"_lang);
            }

            ImGui::EndTable();
        }
        ImGui::PopStyleVar();

        this->m_enteredEditingMode = false;
    }

    void HexEditor::drawFooter(prv::Provider *provider, const ImVec2 &size) {
        if (provider != nullptr && provider->isReadable()) {
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
                        auto selection = getSelection();
                        std::string value;
                        if (isSelectionValid()) {
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

    void HexEditor::handleSelection(u64 address, u32 bytesPerCell, const u8 *data, bool cellHovered) {
        if (ImGui::IsWindowHovered() && cellHovered) {
            drawTooltip(address, data, bytesPerCell);

            auto endAddress = address + bytesPerCell - 1;
            auto &selectionStart = this->m_selectionStart;

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

    void HexEditor::draw(prv::Provider *provider, float height) {
        const auto width = ImGui::GetContentRegionAvail().x;

        const auto FooterSize = ImVec2(width, ImGui::GetTextLineHeightWithSpacing() * 2.3F);
        const auto TableSize  = ImVec2(width, height - FooterSize.y);

        this->drawEditor(provider, TableSize);
        this->drawFooter(provider, FooterSize);

        this->m_selectionChanged = false;
    }

}