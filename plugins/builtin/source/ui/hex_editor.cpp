#include <ui/hex_editor.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/api/localization.hpp>

#include <hex/helpers/encoding_file.hpp>
#include <hex/helpers/utils.hpp>

namespace hex::plugin::builtin::ui {

    /* Data Visualizer */

    class DataVisualizerAscii : public hex::ContentRegistry::HexEditor::DataVisualizer {
    public:
        DataVisualizerAscii() : DataVisualizer("ASCII", 1, 1) { }

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

    HexEditor::HexEditor(prv::Provider *provider) : m_provider(provider) {
        this->m_currDataVisualizer = ContentRegistry::HexEditor::getVisualizerByName("hex.builtin.visualizer.hexadecimal.8bit");

        EventManager::subscribe<EventSettingsChanged>(this, [this] {
            this->m_selectionColor = ContentRegistry::Settings::read("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.highlight_color", 0x60C08080);
            this->m_syncScrolling = ContentRegistry::Settings::read("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.sync_scrolling", 0);
            this->m_byteCellPadding = ContentRegistry::Settings::read("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.byte_padding", 0);
            this->m_characterCellPadding = ContentRegistry::Settings::read("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.char_padding", 0);
        });
    }

    HexEditor::~HexEditor() {
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

        size_t size = std::min<size_t>(longestSequence, provider->getActualSize() - address);

        std::vector<u8> buffer(size);
        provider->read(address, buffer.data(), size);

        const auto [decoded, advance] = encodingFile.getEncodingFor(buffer);
        const ImColor color = [&]{
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

    void HexEditor::drawCell(u64 address, u8 *data, size_t size, bool hovered, CellType cellType) {
        static DataVisualizerAscii asciiVisualizer;

        if (this->m_shouldUpdateEditingValue) {
            this->m_shouldUpdateEditingValue = false;
            this->m_editingBytes.resize(size);
            std::memcpy(this->m_editingBytes.data(), data, size);
        }

        if (this->m_editingAddress != address || this->m_editingCellType != cellType) {
            if (cellType == CellType::Hex) {
                std::vector<u8> buffer(size);
                std::memcpy(buffer.data(), data, size);

                if (this->m_dataVisualizerEndianness != std::endian::native)
                    std::reverse(buffer.begin(), buffer.end());

                this->m_currDataVisualizer->draw(address, buffer.data(), buffer.size(), this->m_upperCaseHex);
            } else {
                asciiVisualizer.draw(address, data, size, this->m_upperCaseHex);
            }

            if (hovered && this->m_provider->isWritable()) {
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
            if (cellType == this->m_editingCellType && cellType == CellType::Hex) {
                std::vector<u8> buffer = this->m_editingBytes;

                if (this->m_dataVisualizerEndianness != std::endian::native)
                    std::reverse(buffer.begin(), buffer.end());

                shouldExitEditingMode = this->m_currDataVisualizer->drawEditing(*this->m_editingAddress, buffer.data(), buffer.size(), this->m_upperCaseHex, this->m_enteredEditingMode);

                if (this->m_dataVisualizerEndianness != std::endian::native)
                    std::reverse(buffer.begin(), buffer.end());

                this->m_editingBytes = buffer;
            } else if (cellType == this->m_editingCellType && cellType == CellType::ASCII) {
                shouldExitEditingMode = asciiVisualizer.drawEditing(*this->m_editingAddress, this->m_editingBytes.data(), this->m_editingBytes.size(), this->m_upperCaseHex, this->m_enteredEditingMode);
            }

            if (shouldExitEditingMode || this->m_shouldModifyValue) {
                this->m_provider->write(*this->m_editingAddress, this->m_editingBytes.data(), this->m_editingBytes.size());

                if (!this->m_selectionChanged && !ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    auto nextEditingAddress = *this->m_editingAddress + this->m_currDataVisualizer->getBytesPerCell();
                    this->setSelection(nextEditingAddress, nextEditingAddress);

                    if (nextEditingAddress >= this->m_provider->getBaseAddress() + this->m_provider->getCurrentPageAddress() + this->m_provider->getSize())
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

            this->m_enteredEditingMode = false;
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
            drawList->AddLine(cellPos, cellPos + ImVec2(0, cellSize.y), ImColor(SelectionFrameColor), 1_scaled);

        // Draw vertical line at the right of the last byte and the end of the line
        if (x == u16((this->m_bytesPerRow / bytesPerCell) - 1) || (byteAddress + bytesPerCell) > selection.getEndAddress())
            drawList->AddLine(cellPos + ImVec2(cellSize.x, -1), cellPos + cellSize, ImColor(SelectionFrameColor), 1_scaled);

        // Draw horizontal line at the top of the bytes
        if (y == 0 || (byteAddress - this->m_bytesPerRow) < selection.getStartAddress())
            drawList->AddLine(cellPos, cellPos + ImVec2(cellSize.x + 1, 0), ImColor(SelectionFrameColor), 1_scaled);

        // Draw horizontal line at the bottom of the bytes
        if ((byteAddress + this->m_bytesPerRow) > selection.getEndAddress())
            drawList->AddLine(cellPos + ImVec2(0, cellSize.y), cellPos + cellSize + ImVec2(1, 0), ImColor(SelectionFrameColor), 1_scaled);
    }

    void HexEditor::drawEditor(const ImVec2 &size) {
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

                ImGui::TableSetupColumn(hex::format(this->m_upperCaseHex ? "{:0{}X}" : "{:0{}x}", i * bytesPerCell, this->m_currDataVisualizer->getMaxCharsPerCell()).c_str(), ImGuiTableColumnFlags_WidthFixed, CharacterSize.x * this->m_currDataVisualizer->getMaxCharsPerCell() + (6 + this->m_byteCellPadding) * 1_scaled);
            }

            // ASCII column
            ImGui::TableSetupColumn("");

            if (this->m_showAscii) {
                ImGui::TableSetupColumn("hex.builtin.common.encoding.ascii"_lang, ImGuiTableColumnFlags_WidthFixed, (CharacterSize.x + this->m_characterCellPadding * 1_scaled) * this->m_bytesPerRow);
            }
            else
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 0);

            ImGui::TableSetupColumn("");
            // Custom encoding column
            {
                if (this->m_currCustomEncoding.has_value() && this->m_showCustomEncoding) {
                    ImGui::TableSetupColumn(this->m_currCustomEncoding->getName().c_str(), ImGuiTableColumnFlags_WidthStretch);
                } else {
                    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 0);
                }
            }

            ImGui::TableNextRow();
            for (i32 i = 0; i < ImGui::TableGetColumnCount(); i++) {
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(ImGui::TableGetColumnName(i));
                ImGui::Dummy(ImVec2(0, CharacterSize.y / 2));
            }

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            if (this->m_provider != nullptr && this->m_provider->isReadable()) {
                const auto isCurrRegionValid = [this](u64 address) {
                    auto &[currRegion, currRegionValid] = this->m_currValidRegion;
                    if (!Region{ address, 1 }.isWithin(currRegion)) {
                        this->m_currValidRegion = this->m_provider->getRegionValidity(address);
                    }

                    return currRegionValid;
                };

                ImGuiListClipper clipper;

                u64 numRows = std::ceil(this->m_provider->getSize() / (long double)(this->m_bytesPerRow));
                clipper.Begin(numRows + size.y / CharacterSize.y - 3, CharacterSize.y);
                while (clipper.Step()) {
                    this->m_visibleRowCount = clipper.DisplayEnd - clipper.DisplayStart;

                    // Loop over rows
                    for (u64 y = u64(clipper.DisplayStart); y < std::min(numRows, u64(clipper.DisplayEnd)); y++) {
                        // Draw address column
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextFormatted(this->m_upperCaseHex ? "{:08X}: " : "{:08x}: ", y * this->m_bytesPerRow + this->m_provider->getBaseAddress() + this->m_provider->getCurrentPageAddress());
                        ImGui::TableNextColumn();

                        const u8 validBytes = std::min<u64>(this->m_bytesPerRow, this->m_provider->getSize() - y * this->m_bytesPerRow);

                        std::vector<u8> bytes(this->m_bytesPerRow, 0x00);
                        this->m_provider->read(y * this->m_bytesPerRow + this->m_provider->getBaseAddress() + this->m_provider->getCurrentPageAddress(), bytes.data(), validBytes);

                        std::vector<std::tuple<std::optional<color_t>, std::optional<color_t>>> cellColors;
                        {
                            for (u64 x = 0; x <  std::ceil(float(validBytes) / bytesPerCell); x++) {
                                const u64 byteAddress = y * this->m_bytesPerRow + x * bytesPerCell + this->m_provider->getBaseAddress() + this->m_provider->getCurrentPageAddress();

                                const auto cellBytes = std::min<u64>(validBytes, bytesPerCell);

                                // Query cell colors
                                if (x < std::ceil(float(validBytes) / bytesPerCell)) {
                                    auto foregroundColor = this->m_foregroundColorCallback(byteAddress, &bytes[x * cellBytes], cellBytes);
                                    auto backgroundColor = this->m_backgroundColorCallback(byteAddress, &bytes[x * cellBytes], cellBytes);

                                    if (this->m_grayOutZero && !foregroundColor.has_value()) {
                                        bool allZero = true;
                                        for (u64 i = 0; i < cellBytes && (x * cellBytes + i) < bytes.size(); i++) {
                                            if (bytes[x * cellBytes + i] != 0x00) {
                                                allZero = false;
                                                break;
                                            }
                                        }

                                        if (allZero)
                                            foregroundColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
                                    }

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
                        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, scaled(ImVec2(2.75F, 0.0F)));

                        for (u64 x = 0; x < columnCount; x++) {
                            const u64 byteAddress = y * this->m_bytesPerRow + x * bytesPerCell + this->m_provider->getBaseAddress() + this->m_provider->getCurrentPageAddress();

                            ImGui::TableNextColumn();
                            if (isColumnSeparatorColumn(x, columnCount))
                                ImGui::TableNextColumn();

                            if (x < std::ceil(float(validBytes) / bytesPerCell)) {
                                auto cellStartPos = getCellPosition();
                                auto cellSize = (CharacterSize * ImVec2(this->m_currDataVisualizer->getMaxCharsPerCell(), 1)) + (ImVec2(2, 2) * ImGui::GetStyle().CellPadding) + scaled(ImVec2(1 + this->m_byteCellPadding, 0));
                                auto maxCharsPerCell = this->m_currDataVisualizer->getMaxCharsPerCell();

                                auto [foregroundColor, backgroundColor] = cellColors[x];

                                if (isColumnSeparatorColumn(x + 1, columnCount) && cellColors.size() > x + 1) {
                                    auto separatorAddress = x + y * columnCount;
                                    auto [nextForegroundColor, nextBackgroundColor] = cellColors[x + 1];
                                    if ((isSelectionValid() && getSelection().overlaps({ separatorAddress, 1 }) && getSelection().getEndAddress() != separatorAddress) || backgroundColor == nextBackgroundColor)
                                        cellSize.x += SeparatorColumWidth + 1;
                                }

                                if (y == u64(clipper.DisplayStart))
                                    cellSize.y -= (ImGui::GetStyle().CellPadding.y);

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
                                    this->drawCell(byteAddress, &bytes[x * bytesPerCell], bytesPerCell, cellHovered, CellType::Hex);
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
                                    ImGui::TableSetupColumn(hex::format("##ascii_cell{}", x).c_str(), ImGuiTableColumnFlags_WidthFixed, CharacterSize.x + this->m_characterCellPadding * 1_scaled);

                                ImGui::TableNextRow();

                                for (u64 x = 0; x < this->m_bytesPerRow; x++) {
                                    ImGui::TableNextColumn();

                                    const u64 byteAddress = y * this->m_bytesPerRow + x + this->m_provider->getBaseAddress() + this->m_provider->getCurrentPageAddress();

                                    const auto cellStartPos = getCellPosition();
                                    const auto cellSize = CharacterSize + scaled(ImVec2(this->m_characterCellPadding, 0));

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

                                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (this->m_characterCellPadding * 1_scaled) / 2);
                                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                                        ImGui::PushItemWidth(CharacterSize.x);
                                        if (!isCurrRegionValid(byteAddress))
                                            ImGui::TextFormattedDisabled("{}", this->m_unknownDataCharacter);
                                        else
                                            this->drawCell(byteAddress, &bytes[x], 1, cellHovered, CellType::ASCII);
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
                        if (this->m_showCustomEncoding && this->m_currCustomEncoding.has_value()) {
                            std::vector<std::pair<u64, CustomEncodingData>> encodingData;

                            if (this->m_encodingLineStartAddresses.empty()) {
                                this->m_encodingLineStartAddresses.push_back(0);
                            }

                            if (y < this->m_encodingLineStartAddresses.size()) {
                                if (this->m_encodingLineStartAddresses[y] >= this->m_bytesPerRow) {
                                    encodingData.emplace_back(y * this->m_bytesPerRow + this->m_provider->getBaseAddress() + this->m_provider->getCurrentPageAddress(), CustomEncodingData(".", 1, ImGui::GetCustomColorU32(ImGuiCustomCol_ToolbarRed)));
                                    this->m_encodingLineStartAddresses.push_back(0);
                                } else {
                                    u32 offset = this->m_encodingLineStartAddresses[y];
                                    do {
                                        const u64 address = y * this->m_bytesPerRow + offset + this->m_provider->getBaseAddress() + this->m_provider->getCurrentPageAddress();

                                        auto result = queryCustomEncodingData(this->m_provider, *this->m_currCustomEncoding, address);

                                        offset += result.advance;
                                        encodingData.emplace_back(address, result);
                                    } while (offset < this->m_bytesPerRow);

                                    this->m_encodingLineStartAddresses.push_back(offset - this->m_bytesPerRow);
                                }

                                ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
                                ImGui::PushID(y);
                                if (ImGui::BeginTable("##encoding_cell", encodingData.size(), ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoKeepColumnsVisible)) {
                                    ImGui::TableNextRow();

                                    for (const auto &[address, data] : encodingData) {
                                        ImGui::TableNextColumn();

                                        const auto cellStartPos = getCellPosition();
                                        const auto cellSize = ImGui::CalcTextSize(data.displayValue.c_str()) * ImVec2(1, 0) + ImVec2(this->m_characterCellPadding * 1_scaled, CharacterSize.y);
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

                                            auto startPos = ImGui::GetCursorPos();
                                            ImGui::TextFormattedColored(data.color, "{}", data.displayValue);
                                            ImGui::SetCursorPosX(startPos.x + cellSize.x);
                                            ImGui::SameLine(0, 0);
                                            ImGui::Dummy({ 0, 0 });

                                            this->handleSelection(address, data.advance, &bytes[address % this->m_bytesPerRow], cellHovered);
                                        }
                                    }

                                    ImGui::EndTable();
                                }
                                ImGui::PopStyleVar();
                                ImGui::PopID();
                            }
                        }

                        // Scroll to the cursor if it's either at the top or bottom edge of the screen
                        if (this->m_shouldScrollToSelection && isSelectionValid()) {
                            // Make sure simply clicking on a byte at the edge of the screen won't cause scrolling
                            if ((ImGui::IsMouseDragging(ImGuiMouseButton_Left) && *this->m_selectionStart != *this->m_selectionEnd)) {
                                auto fractionPerLine = 1.0 / (this->m_visibleRowCount + 1);

                                if (y == (u64(clipper.DisplayStart) + 3)) {
                                    if (i128(this->m_selectionEnd.value() - this->m_provider->getBaseAddress() - this->m_provider->getCurrentPageAddress()) <= (i64(clipper.DisplayStart + 3) * this->m_bytesPerRow)) {
                                        this->m_shouldScrollToSelection = false;
                                        ImGui::SetScrollHereY(fractionPerLine * 5);
                                    }
                                } else if (y == (u64(clipper.DisplayEnd) - 1)) {
                                    if (i128(this->m_selectionEnd.value() - this->m_provider->getBaseAddress() - this->m_provider->getCurrentPageAddress()) >= (i64(clipper.DisplayEnd - 2) * this->m_bytesPerRow)) {
                                        this->m_shouldScrollToSelection = false;
                                        ImGui::SetScrollHereY(fractionPerLine * (this->m_visibleRowCount));
                                    }
                                }
                            }

                            // If the cursor is off-screen, directly jump to the byte
                            if (this->m_shouldJumpWhenOffScreen) {
                                this->m_shouldJumpWhenOffScreen = false;

                                const auto pageAddress = this->m_provider->getCurrentPageAddress() + this->m_provider->getBaseAddress();
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
                    this->m_provider->setCurrentPage(this->m_provider->getPageOfAddress(newSelection.address).value_or(0));

                    const auto pageAddress = this->m_provider->getCurrentPageAddress() + this->m_provider->getBaseAddress();
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


            }

            ImGui::EndTable();
        }
        ImGui::PopStyleVar();

        this->m_shouldScrollToSelection = false;
    }

    void HexEditor::drawFooter(const ImVec2 &size) {
        if (this->m_provider != nullptr && this->m_provider->isReadable()) {
            const auto pageCount = std::max<u32>(1, this->m_provider->getPageCount());
            constexpr static u32 MinPage = 1;

            const auto windowEndPos = ImGui::GetWindowPos() + size - ImGui::GetStyle().WindowPadding;
            ImGui::GetWindowDrawList()->AddLine(windowEndPos - ImVec2(0, size.y - 1_scaled), windowEndPos - size + ImVec2(0, 1_scaled), ImGui::GetColorU32(ImGuiCol_Separator), 2.0_scaled);

            if (ImGui::BeginChild("##footer", size, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                if (ImGui::BeginTable("##footer_table", 2)) {
                    ImGui::TableNextRow();

                    // Page slider
                    ImGui::TableNextColumn();
                    {
                        u32 page = this->m_provider->getCurrentPage() + 1;

                        ImGui::TextFormatted("{}: ", "hex.builtin.hex_editor.page"_lang);
                        ImGui::SameLine();

                        ImGui::BeginDisabled(pageCount <= 1);
                        {
                            if (ImGui::SliderScalar("##page_selection", ImGuiDataType_U32, &page, &MinPage, &pageCount, hex::format("0x%02llX / 0x{:02X}", pageCount).c_str()))
                                this->m_provider->setCurrentPage(page - 1);
                        }
                        ImGui::EndDisabled();
                    }

                    // Page Address
                    ImGui::TableNextColumn();
                    {
                        auto pageAddress = this->m_provider->getCurrentPageAddress();
                        ImGui::TextFormatted("{0}: 0x{1:08X} - 0x{2:08X} ({1} - {2})", "hex.builtin.hex_editor.region"_lang, pageAddress, pageAddress + this->m_provider->getSize() - 1);
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
                            value = std::string("hex.builtin.hex_editor.selection.none"_lang);

                        ImGui::TextFormatted("{0}: {1}", "hex.builtin.hex_editor.selection"_lang, value);
                    }

                    // Loaded data size
                    ImGui::TableNextColumn();
                    {
                        ImGui::TextFormatted("{0}: 0x{1:08X} (0x{2:X} | {3})", "hex.builtin.hex_editor.data_size"_lang,
                                             this->m_provider->getActualSize(),
                                             this->m_provider->getActualSize(),
                                             hex::toByteString(this->m_provider->getActualSize())
                        );
                    }

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 2_scaled);

                    // Upper/lower case hex
                    ImGui::DimmedIconToggle(ICON_VS_CASE_SENSITIVE, &this->m_upperCaseHex);
                    ImGui::InfoTooltip("hex.builtin.hex_editor.uppercase_hex"_lang);

                    ImGui::SameLine();

                    // Grayed out zeros
                    ImGui::DimmedIconToggle(ICON_VS_LIGHTBULB, &this->m_grayOutZero);
                    ImGui::InfoTooltip("hex.builtin.hex_editor.gray_out_zero"_lang);

                    ImGui::SameLine();

                    // ASCII view
                    ImGui::DimmedIconToggle(ICON_VS_SYMBOL_KEY, &this->m_showAscii);
                    ImGui::InfoTooltip("hex.builtin.hex_editor.ascii_view"_lang);

                    ImGui::SameLine(0, 1_scaled);

                    // Custom encoding view
                    ImGui::BeginDisabled(!this->m_currCustomEncoding.has_value());
                    ImGui::DimmedIconToggle(ICON_VS_WHITESPACE, &this->m_showCustomEncoding);
                    ImGui::EndDisabled();

                    ImGui::InfoTooltip("hex.builtin.hex_editor.custom_encoding_view"_lang);

                    ImGui::TableNextColumn();

                    // Visualizer
                    {
                        auto &visualizers = ContentRegistry::HexEditor::impl::getVisualizers();

                        ImGui::TextFormatted("{}: ", "hex.builtin.hex_editor.visualizer"_lang);

                        ImGui::SameLine(0, 0);

                        {
                            bool hasEndianess = this->m_currDataVisualizer->getBytesPerCell() > 1;

                            if (!hasEndianess)
                                this->m_dataVisualizerEndianness = std::endian::native;

                            ImGui::BeginDisabled(!hasEndianess);
                            {
                                int sliderPos = this->m_dataVisualizerEndianness == std::endian::little ? 0 : 1;
                                ImGui::PushItemWidth(60_scaled);
                                ImGui::SliderInt("##visualizer_endianness", &sliderPos, 0, 1, sliderPos == 0 ? "hex.builtin.common.little"_lang : "hex.builtin.common.big"_lang);
                                ImGui::PopItemWidth();
                                this->m_dataVisualizerEndianness = sliderPos == 0 ? std::endian::little : std::endian::big;
                            }
                            ImGui::EndDisabled();
                        }

                        ImGui::SameLine(0, 2_scaled);
                        ImGui::PushItemWidth((ImGui::GetContentRegionAvail().x / 3) * 2);
                        if (ImGui::BeginCombo("##visualizer", LangEntry(this->m_currDataVisualizer->getUnlocalizedName()))) {

                            for (const auto &visualizer : visualizers) {
                                if (ImGui::Selectable(LangEntry(visualizer->getUnlocalizedName()))) {
                                    this->m_currDataVisualizer = visualizer;
                                    this->m_encodingLineStartAddresses.clear();

                                    if (this->m_bytesPerRow < visualizer->getBytesPerCell())
                                        this->m_bytesPerRow = visualizer->getBytesPerCell();
                                }
                            }

                            ImGui::EndCombo();
                        }
                        ImGui::PopItemWidth();

                        ImGui::SameLine(0, 2_scaled);
                        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
                        int bytesPerRow = this->m_bytesPerRow / this->getBytesPerCell();
                        if (ImGui::SliderInt("##row_size", &bytesPerRow, 1, 32 / this->getBytesPerCell(), hex::format("{}", bytesPerRow * this->getBytesPerCell()).c_str())) {
                            this->m_bytesPerRow = bytesPerRow * this->getBytesPerCell();
                            this->m_encodingLineStartAddresses.clear();
                        }
                        ImGui::PopItemWidth();
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
            else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (ImGui::GetIO().KeyShift)
                    this->setSelection(selectionStart.value_or(address), endAddress);
                else
                    this->setSelection(address, endAddress);

                this->scrollToSelection();
            }
        }
    }

    void HexEditor::draw(float height) {
        const auto width = ImGui::GetContentRegionAvail().x;

        auto FooterSize = ImVec2(width, ImGui::GetTextLineHeightWithSpacing() * 3.6F);
        auto TableSize  = ImVec2(width, height - FooterSize.y);

        if (TableSize.y <= 0)
            TableSize.y = height;

        this->drawEditor(TableSize);

        if (TableSize.y > 0)
            this->drawFooter(FooterSize);

        this->m_selectionChanged = false;
    }

}