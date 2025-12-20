#include <ui/hex_editor.hpp>

#include <hex/api/content_registry/hex_editor.hpp>
#include <hex/api/localization_manager.hpp>

#include <hex/helpers/encoding_file.hpp>
#include <hex/helpers/utils.hpp>

#include <wolv/utils/guards.hpp>

#include <fonts/vscode_icons.hpp>
#include <hex/providers/buffered_reader.hpp>

#include <algorithm>
#include <fonts/fonts.hpp>

#include <imgui_internal.h>

namespace hex::ui {

    /* Data Visualizer */

    class DataVisualizerAscii : public hex::ContentRegistry::HexEditor::DataVisualizer {
    public:
        DataVisualizerAscii() : DataVisualizer("ASCII", 1, 1) { }

        void draw(u64 address, const u8 *data, size_t size, bool upperCase) override {
            std::ignore = address;
            std::ignore = upperCase;

            if (size == 1) {
                const auto c = static_cast<unsigned char>(data[0]);
                if (std::isprint(c) != 0) {
                    const std::array<char, 2> string = { char(c), 0x00 };
                    ImGui::TextUnformatted(string.data());
                } else if (m_extendedAscii) {
                    if (c <= 0x1F) {
                        constexpr static std::array ControlCharacters = {
                            "\u2400", "\u2401", "\u2402", "\u2403", "\u2404", "\u2405", "\u2406", "\u2407",
                            "\u2408", "\u2409", "\u240A", "\u240B", "\u240C", "\u240D", "\u240E", "\u240F",
                            "\u2410", "\u2411", "\u2412", "\u2413", "\u2414", "\u2415", "\u2416", "\u2417",
                            "\u2418", "\u2419", "\u241A", "\u241B", "\u241C", "\u241D", "\u241E", "\u241F",
                        };

                        ImGui::TextUnformatted(ControlCharacters[c]);
                    } else if (c >= 0x7F) {
                        constexpr static std::array ExtendedAsciiCharacters = {
                            "\u2421",
                            "\u20AC", "\u0081", "\u201A", "\u0192", "\u201E", "\u2026", "\u2020", "\u2021",
                            "\u02C6", "\u2030", "\u0160", "\u2039", "\u0152", "\u008D", "\u017D", "\u008F",
                            "\u0090", "\u2018", "\u2019", "\u201C", "\u201D", "\u2022", "\u2013", "\u2014",
                            "\u02DC", "\u2122", "\u0161", "\u203A", "\u0153", "\u009D", "\u017E", "\u0178",
                            "\u00A0", "\u00A1", "\u00A2", "\u00A3", "\u00A4", "\u00A5", "\u00A6", "\u00A7",
                            "\u00A8", "\u00A9", "\u00AA", "\u00AB", "\u00AC", "\u00AD", "\u00AE", "\u00AF",
                            "\u00B0", "\u00B1", "\u00B2", "\u00B3", "\u00B4", "\u00B5", "\u00B6", "\u00B7",
                            "\u00B8", "\u00B9", "\u00BA", "\u00BB", "\u00BC", "\u00BD", "\u00BE", "\u00BF",
                            "\u00C0", "\u00C1", "\u00C2", "\u00C3", "\u00C4", "\u00C5", "\u00C6", "\u00C7",
                            "\u00C8", "\u00C9", "\u00CA", "\u00CB", "\u00CC", "\u00CD", "\u00CE", "\u00CF",
                            "\u00D0", "\u00D1", "\u00D2", "\u00D3", "\u00D4", "\u00D5", "\u00D6", "\u00D7",
                            "\u00D8", "\u00D9", "\u00DA", "\u00DB", "\u00DC", "\u00DD", "\u00DE", "\u00DF",
                            "\u00E0", "\u00E1", "\u00E2", "\u00E3", "\u00E4", "\u00E5", "\u00E6", "\u00E7",
                            "\u00E8", "\u00E9", "\u00EA", "\u00EB", "\u00EC", "\u00ED", "\u00EE", "\u00EF",
                            "\u00F0", "\u00F1", "\u00F2", "\u00F3", "\u00F4", "\u00F5", "\u00F6", "\u00F7",
                            "\u00F8", "\u00F9", "\u00FA", "\u00FB", "\u00FC", "\u00FD", "\u00FE", "\u00FF",
                        };

                        ImGui::TextUnformatted(ExtendedAsciiCharacters[c - 0x7F]);
                    } else {
                        ImGuiExt::TextFormattedDisabled(".");
                    }
                } else {
                    ImGuiExt::TextFormattedDisabled(".");
                }
            } else {
                ImGuiExt::TextFormattedDisabled(".");
            }
        }

        bool drawEditing(u64 address, u8 *data, size_t size, bool upperCase, bool startedEditing) override {
            std::ignore = address;
            std::ignore = startedEditing;
            std::ignore = upperCase;

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
                ON_SCOPE_EXIT { ImGui::PopID(); };
                std::array<char, 2> buffer = { std::isprint(data[0]) != 0 ? char(data[0]) : '.', 0x00 };
                ImGui::InputText("##editing_input", buffer.data(), buffer.size(), DefaultTextInputFlags() | ImGuiInputTextFlags_CallbackEdit, [](ImGuiInputTextCallbackData *data) -> int {
                    auto &userData = *static_cast<UserData*>(data->UserData);

                    if (data->BufTextLen >= userData.maxChars) {
                        userData.editingDone = true;
                        userData.data[0] = data->Buf[0];
                    }

                    return 0;
                }, &userData);

                return userData.editingDone || ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Escape);
            } else {
                return false;
            }
        }

        void enableExtendedAscii(bool enable) {
            m_extendedAscii = enable;
        }

    private:
        bool m_extendedAscii = false;
    };

    /* Hex Editor */

    HexEditor::HexEditor(prv::Provider *provider) : m_provider(provider) {

    }

    constexpr static u16 getByteColumnSeparatorCount(u16 columnCount) {
        return (columnCount - 1) / 8;
    }

    constexpr static bool isColumnSeparatorColumn(u16 currColumn, u16 columnCount) {
        return currColumn > 0 && (currColumn) < columnCount && ((currColumn) % 8) == 0;
    }

    std::optional<color_t> HexEditor::applySelectionColor(u64 byteAddress, std::optional<color_t> color) {
        if (m_mode == Mode::Overwrite) {
            if (m_frameStartSelectionRegion != Region::Invalid()) {
                auto selection = m_frameStartSelectionRegion;

                if (byteAddress >= selection.getStartAddress() && byteAddress <= selection.getEndAddress()) {
                    if (color.has_value())
                        color = (ImAlphaBlendColors(color.value(), m_selectionColor)) & 0x00FFFFFF;
                    else
                        color = m_selectionColor;
                }
            }
        }

        if (color.has_value())
            color = (*color & 0x00FFFFFF) | (m_selectionColor & 0xFF000000);

        return color;
    }

    std::string HexEditor::formatAddress(u64 address, u32 width, bool prefix) const {
        switch (m_addressFormat) {
            using enum AddressFormat;
            default:
            case Hexadecimal: return fmt::format(fmt::runtime(m_upperCaseHex ? "{0}{1:0{2}X}" : "{0}{1:0{2}x}"), prefix ? "0x" : "", address, width);
            case Decimal:     return fmt::format("{0: >{1}d}", address, width);
            case Octal:       return fmt::format("{0}{1:0{2}o}", prefix ? "0o" : "", address, width);
        }
    }

    struct CustomEncodingData {
        std::string displayValue;
        size_t advance;
        ImColor color;
    };

    static CustomEncodingData queryCustomEncodingData(prv::Provider *provider, const EncodingFile &encodingFile, u64 address) {
        const auto longestSequence = encodingFile.getLongestSequence();

        if (longestSequence == 0) {
            return {
                .displayValue = ".",
                .advance = 1,
                .color = 0xFFFF8000
            };

        }

        size_t size = std::min<size_t>(longestSequence, provider->getActualSize() - address);

        std::vector<u8> buffer(size);
        provider->read(address, buffer.data(), size);

        const auto [decoded, advance] = encodingFile.getEncodingFor(buffer);
        const ImColor color = [&]{
            if (decoded.length() == 1 && std::isalnum(decoded[0]) != 0)
                return ImGuiExt::GetCustomColorU32(ImGuiCustomCol_AdvancedEncodingASCII);
            else if (decoded.length() == 1 && advance == 1)
                return ImGuiExt::GetCustomColorU32(ImGuiCustomCol_AdvancedEncodingSingleChar);
            else if (decoded.length() > 1 && advance == 1)
                return ImGuiExt::GetCustomColorU32(ImGuiCustomCol_AdvancedEncodingMultiChar);
            else if (advance > 1)
                return ImGui::GetColorU32(ImGuiCol_Text);
            else
                return ImGuiExt::GetCustomColorU32(ImGuiCustomCol_ToolbarBlue);
        }();

        return {
            .displayValue = std::string(decoded),
            .advance = advance,
            .color = color
        };
    }

    static auto getCellPosition() {
        return ImGui::GetCursorScreenPos() - ImGui::GetStyle().CellPadding;
    }

    void HexEditor::drawTooltip(u64 address, const u8 *data, size_t size) const {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, scaled(ImVec2(5, 5)));

        m_tooltipCallback(address, data, size);

        ImGui::PopStyleVar();
    }

    void HexEditor::drawScrollbar(ImVec2 characterSize) {
        const auto bytesPerCell = m_currDataVisualizer->getBytesPerCell();
        const auto bytesPerRow  =  m_bytesPerRow / bytesPerCell * bytesPerCell;

        ImS64 numRows = m_provider == nullptr ? 0LLU : (m_provider->getSize() / bytesPerRow) + ((m_provider->getSize() % bytesPerRow) == 0 ? 0LLU : 1LLU);

        auto window = ImGui::GetCurrentWindowRead();
        const auto outerRect = window->Rect();
        const auto innerRect = window->InnerRect;
        const auto borderSize = window->WindowBorderSize;
        const auto scrollbarWidth = ImGui::GetStyle().ScrollbarSize;
        const auto bb = ImRect(ImMax(outerRect.Min.x, outerRect.Max.x - borderSize - scrollbarWidth), innerRect.Min.y, outerRect.Max.x, innerRect.Max.y);

        constexpr auto roundingCorners = ImDrawFlags_RoundCornersTopRight | ImDrawFlags_RoundCornersBottomRight;
        constexpr auto axis = ImGuiAxis_Y;

        if (numRows > 0) {
            ImGui::PushID("MainScrollBar");
            ImGui::ScrollbarEx(
                bb,
                ImGui::GetWindowScrollbarID(window, axis),
                axis,
                &m_scrollPosition.get(),
                static_cast<ImS64>(std::ceil(innerRect.Max.y - innerRect.Min.y) / characterSize.y),
                static_cast<ImS64>(std::nextafterf(static_cast<float>(numRows) + (ImGui::GetWindowSize().y / characterSize.y), std::numeric_limits<float>::max())),
                roundingCorners);
            ImGui::PopID();
        }

        if (m_showMiniMap && m_miniMapVisualizer != nullptr)
            this->drawMinimap(characterSize);

        if (ImGui::IsWindowHovered()) {
            float scrollMultiplier;
            if (ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyShift)
                scrollMultiplier = m_visibleRowCount * 10.0F;
            else if (ImGui::GetIO().KeyCtrl)
                scrollMultiplier = m_visibleRowCount;
            else
                scrollMultiplier = 5;

            m_scrollPosition += ImS64(ImGui::GetIO().MouseWheel * -scrollMultiplier);
        }

        if (m_scrollPosition < 0)
            m_scrollPosition = 0;
        if (m_scrollPosition > (numRows - 1))
            m_scrollPosition = numRows - 1;
    }

    void HexEditor::drawMinimap(ImVec2 characterSize) {
        if (m_provider == nullptr)
            return;

        const auto bytesPerCell    = m_currDataVisualizer->getBytesPerCell();
        const auto bytesPerRow     = m_bytesPerRow / bytesPerCell * bytesPerCell;

        ImS64 numRows = (m_provider->getSize() / bytesPerRow) + ((m_provider->getSize() % bytesPerRow) == 0 ? 0 : 1);

        auto window = ImGui::GetCurrentWindowRead();
        const auto outerRect = window->Rect();
        const auto innerRect = window->InnerRect;
        const auto borderSize = window->WindowBorderSize;
        const auto scrollbarWidth = ImGui::GetStyle().ScrollbarSize;
        const auto bb = ImRect(ImMax(outerRect.Min.x, outerRect.Max.x - borderSize - scrollbarWidth) - scrollbarWidth * (1 + m_miniMapWidth), innerRect.Min.y, outerRect.Max.x - scrollbarWidth, innerRect.Max.y);

        constexpr auto roundingCorners = ImDrawFlags_RoundCornersTopRight | ImDrawFlags_RoundCornersBottomRight;
        constexpr auto axis = ImGuiAxis_Y;

        const auto rowHeight = 4_scaled;
        const u64 rowCount = innerRect.GetSize().y / rowHeight;
        const ImS64 scrollPos = m_scrollPosition.get();
        const auto grabSize = rowHeight * m_visibleRowCount;
        const ImS64 grabPos = (rowCount - m_visibleRowCount) * (double(scrollPos) / numRows);

        auto drawList = ImGui::GetWindowDrawList();

        drawList->ChannelsSplit(2);
        drawList->ChannelsSetCurrent(1);
        if (numRows > 0) {
            ImGui::PushID("MiniMapScrollBar");
            ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, grabSize);
            ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 0);
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImGui::GetColorU32(ImGuiCol_ScrollbarGrab, 0.4F));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabActive, 0.5F));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabHovered, 0.5F));
            ImGui::ScrollbarEx(
                bb,
                ImGui::GetWindowScrollbarID(window, axis),
                axis,
                &m_scrollPosition.get(),
                static_cast<ImS64>(std::ceil(innerRect.Max.y - innerRect.Min.y) / characterSize.y),
                static_cast<ImS64>(std::nextafterf(static_cast<float>(numRows) + (ImGui::GetWindowSize().y / characterSize.y), std::numeric_limits<float>::max())),
                roundingCorners);

            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                ImGui::OpenPopup("MiniMapOptions");
            }

            drawMinimapPopup();

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(3);
            ImGui::PopID();
        }
        drawList->ChannelsSetCurrent(0);

        std::vector<u8> rowData(bytesPerRow);
        std::vector<ImColor> rowColors;
        const auto drawStart = std::max<ImS64>(0, scrollPos - grabPos);
        for (ImS64 y = drawStart; y < std::min<ImS64>(drawStart + rowCount, m_provider->getSize() / bytesPerRow); y += 1) {
            const auto rowStart = bb.Min + ImVec2(0, (y - drawStart) * rowHeight);
            const auto rowEnd = rowStart + ImVec2(bb.GetSize().x, rowHeight);
            const auto rowSize = rowEnd - rowStart;

            const auto address = y * bytesPerRow + m_provider->getBaseAddress() + m_provider->getCurrentPageAddress();
            m_provider->read(address, rowData.data(), rowData.size());

            m_miniMapVisualizer->callback(address, rowData, rowColors);

            const auto cellSize = rowSize / ImVec2(rowColors.size(), 1);
            ImVec2 cellPos = rowStart;
            const auto stride = bytesPerRow / rowColors.size();
            for (u32 i = 0; i < rowColors.size(); i += 1) {
                auto rowColor = rowColors[i];
                if (m_minimapValueBrightness) {
                    rowColor = ImColor(rowColor.Value + ImVec4(0.3F, 0.3F, 0.3F, 0.0F) * ((float(rowData[i * stride]) - 0x7F) / 0xFF));
                }

                drawList->AddRectFilled(cellPos, cellPos + cellSize, rowColor);
                cellPos.x += cellSize.x;
            }
            rowColors.clear();
        }

        drawList->ChannelsMerge();
    }



    void HexEditor::drawCell(u64 address, u8 *data, size_t size, bool hovered, CellType cellType) {
        ImGui::PushID(address + 1);
        ON_SCOPE_EXIT { ImGui::PopID(); };

        static DataVisualizerAscii asciiVisualizer;

        if (m_shouldUpdateEditingValue && address == m_editingAddress) {
            m_shouldUpdateEditingValue = false;

            if (m_editingBytes.size() < size) {
                m_editingBytes.resize(size);
            }

            std::memcpy(m_editingBytes.data(), data, size);
        }

        if (m_editingAddress != address || m_editingCellType != cellType) {
            if (cellType == CellType::Hex) {
                std::array<u8, 32> buffer = {};
                std::memcpy(buffer.data(), data, std::min(size, buffer.size()));

                if (m_dataVisualizerEndianness != std::endian::native)
                    std::reverse(buffer.begin(), buffer.begin() + size);

                m_currDataVisualizer->draw(address, buffer.data(), size, m_upperCaseHex);

            } else {
                asciiVisualizer.enableExtendedAscii(m_showExtendedAscii);
                asciiVisualizer.draw(address, data, size, m_upperCaseHex);
            }

            if (hovered && m_provider->isWritable()) {
                // Enter editing mode when double-clicking a cell
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    setEditingAddress(address);

                    if (m_mode == Mode::Insert) {
                        std::memset(data, 0x00, size);
                    }

                    m_editingCellType = cellType;
                }
            }
        } else {
            std::vector<u8> buffer = m_editingBytes;
            if (m_mode == Mode::Insert)
                std::ranges::fill(buffer, 0x00);

            bool shouldExitEditingMode = true;
            if (cellType == m_editingCellType) {
                switch (cellType) {
                    case CellType::Hex: {
                        if (m_dataVisualizerEndianness != std::endian::native)
                            std::ranges::reverse(buffer);

                        shouldExitEditingMode = m_currDataVisualizer->drawEditing(*m_editingAddress, buffer.data(), buffer.size(), m_upperCaseHex, m_enteredEditingMode);

                        if (m_dataVisualizerEndianness != std::endian::native)
                            std::ranges::reverse(buffer);
                        break;
                    }
                    case CellType::ASCII: {
                        shouldExitEditingMode = asciiVisualizer.drawEditing(*m_editingAddress, buffer.data(), buffer.size(), m_upperCaseHex, m_enteredEditingMode);
                        break;
                    }
                    default:
                        break;
                }

                m_enteredEditingMode = false;
            }

            m_editingBytes = buffer;

            if (ImGui::IsWindowFocused()) {
                ImGui::SetKeyboardFocusHere(-1);
                ImGui::SetNextFrameWantCaptureKeyboard(true);
            }

            const auto anyMouseButtonClicked =
                ImGui::IsMouseClicked(ImGuiMouseButton_Left)   ||
                ImGui::IsMouseClicked(ImGuiMouseButton_Middle) ||
                ImGui::IsMouseClicked(ImGuiMouseButton_Right);

            if (shouldExitEditingMode || m_shouldModifyValue) {
                {
                    std::vector<u8> oldData(m_editingBytes.size());
                    m_provider->read(*m_editingAddress, oldData.data(), oldData.size());

                    size_t writtenBytes = 0;
                    for (size_t i = 0; i < m_editingBytes.size(); i += 1) {
                        if (m_editingBytes[i] != oldData[i]) {
                            m_provider->write(*m_editingAddress + i, &m_editingBytes[i], 1);
                            writtenBytes += 1;
                        }
                    }

                    m_provider->getUndoStack().groupOperations(writtenBytes, "hex.builtin.undo_operation.modification");
                }

                if (!m_selectionChanged && !ImGui::IsMouseDown(ImGuiMouseButton_Left) && !anyMouseButtonClicked && !ImGui::IsKeyDown(ImGuiKey_Escape)) {
                    auto nextEditingAddress = *m_editingAddress + m_currDataVisualizer->getBytesPerCell();
                    this->setSelection(nextEditingAddress, nextEditingAddress);

                    if (nextEditingAddress >= m_provider->getBaseAddress() + m_provider->getCurrentPageAddress() + m_provider->getSize())
                        m_editingAddress = std::nullopt;
                    else {
                        m_editingAddress = nextEditingAddress;
                        m_enteredEditingMode = true;

                        if (m_mode == Mode::Insert) {
                            std::memset(m_editingBytes.data(), 0x00, size);
                            m_provider->insert(nextEditingAddress, size);

                            if (!shouldExitEditingMode)
                                m_provider->getUndoStack().groupOperations(2, "hex.builtin.undo_operation.insert");
                        }
                    }
                } else {
                    if (m_mode == Mode::Insert) {
                        m_provider->undo();
                    }

                    m_editingAddress = std::nullopt;
                }

                m_shouldModifyValue = false;
                m_shouldUpdateEditingValue = true;
            }

            if (anyMouseButtonClicked && !m_enteredEditingMode && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopup)) {
                if (!(ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hovered)) {
                    m_editingAddress = std::nullopt;
                    m_shouldModifyValue = false;
                }
            }

            if (!m_editingAddress.has_value())
                m_editingCellType = CellType::None;
        }
    }

    void HexEditor::drawSeparatorLine(u64 address, bool drawVerticalConnector) {
        if (m_separatorStride == 0) return;

        const auto bytesPerCell = m_currDataVisualizer->getBytesPerCell();
        const u64 regionProgress = address % m_separatorStride;
        const u64 cellsPerRow = m_bytesPerRow / bytesPerCell;
        const auto bytesPerRow = cellsPerRow * bytesPerCell;
        const auto table = ImGui::GetCurrentTable();
        if (regionProgress < cellsPerRow) {
            const auto rect = ImGui::TableGetCellBgRect(table, table->CurrentColumn);

            const auto drawList = ImGui::GetWindowDrawList();

            const auto lineColor = ImGui::GetColorU32(ImGuiCol_SeparatorActive);
            u64 y = (address - m_provider->getBaseAddress() - m_provider->getCurrentPageAddress()) / bytesPerRow;
            if (y != 0)
                drawList->AddLine(rect.Min, ImVec2(rect.Max.x, rect.Min.y), lineColor);
            if (regionProgress == 0 && drawVerticalConnector) {
                drawList->AddLine(ImFloor(rect.Min), ImFloor(ImVec2(rect.Min.x, rect.Max.y)), lineColor);
            }
        }
    }

    void HexEditor::drawBackgroundHighlight(const ImVec2 &cellPos, const ImVec2 &cellSize, const ImColor &backgroundColor) const {
        auto drawList = ImGui::GetWindowDrawList();
        auto window = ImGui::GetCurrentWindowRead();
        drawList->PushClipRect(window->Rect().Min, window->Rect().Max, false);

        drawList->AddRectFilled(cellPos, cellPos + cellSize, backgroundColor);
        drawList->PopClipRect();
    }

    void HexEditor::drawSelection(u32 x, u32 y, Region region, u64 byteAddress, u16 bytesPerCell, const ImVec2 &cellPos, const ImVec2 &cellSize, const ImColor &frameColor) const {
        switch (m_mode) {
            case Mode::Overwrite: this->drawFrame(x, y, region, byteAddress, bytesPerCell, cellPos, cellSize, frameColor); break;
            case Mode::Insert: this->drawInsertCursor(region, byteAddress, cellPos, cellSize, frameColor); break;
        }
    }

    void HexEditor::drawFrame(u32 x, u32 y, Region region, u64 byteAddress, u16 bytesPerCell, const ImVec2 &cellPos, const ImVec2 &cellSize, const ImColor &frameColor) const {
        auto drawList = ImGui::GetWindowDrawList();
        auto window = ImGui::GetCurrentWindowRead();
        drawList->PushClipRect(window->Rect().Min, window->Rect().Max, false);
        ON_SCOPE_EXIT {
            drawList->PopClipRect();
        };

        if (!this->isSelectionValid()) return;

        if (!Region { .address=byteAddress, .size=1 }.isWithin(region))
            return;

        // Draw vertical line at the left of first byte and the start of the line
        if (x == 0 || byteAddress == region.getStartAddress())
            drawList->AddLine(ImTrunc(cellPos), ImTrunc(cellPos + ImVec2(0, cellSize.y)), frameColor, 1_scaled);

        // Draw vertical line at the right of the last byte and the end of the line
        if (x == u16((m_bytesPerRow / bytesPerCell) - 1) || (byteAddress + bytesPerCell) > region.getEndAddress())
            drawList->AddLine(ImTrunc(cellPos + ImVec2(cellSize.x, 0)), ImTrunc(cellPos + cellSize), frameColor, 1_scaled);

        // Draw horizontal line at the top of the bytes
        const auto bytesPerRow  = m_bytesPerRow / bytesPerCell * bytesPerCell;
        if (y == 0 || (byteAddress - bytesPerRow) < region.getStartAddress())
            drawList->AddLine(ImTrunc(cellPos), ImTrunc(cellPos + ImVec2(cellSize.x, 0)), frameColor, 1_scaled);

        // Draw horizontal line at the bottom of the bytes
        if ((byteAddress + bytesPerRow) > region.getEndAddress())
            drawList->AddLine(ImTrunc(cellPos + ImVec2(0, cellSize.y)), ImTrunc(cellPos + cellSize), frameColor, 1_scaled);
    }

    void HexEditor::drawInsertCursor(Region region, u64 byteAddress, const ImVec2 &cellPos, const ImVec2 &cellSize, const ImColor &frameColor) const {
        auto drawList = ImGui::GetWindowDrawList();

        if (!this->isSelectionValid()) return;

        if (!Region { .address=byteAddress, .size=1 }.isWithin(region))
            return;

        bool cursorVisible = (!ImGui::GetIO().ConfigInputTextCursorBlink) || (m_cursorBlinkTimer <= 0.0F) || std::fmod(m_cursorBlinkTimer, 1.20F) <= 0.80F;
        if (cursorVisible && byteAddress == region.getStartAddress()) {
            // Draw vertical line at the left of first byte and the start of the line

            auto window = ImGui::GetCurrentWindowRead();

            drawList->PushClipRect(window->Rect().Min, window->Rect().Max, false);
            drawList->AddLine(ImTrunc(cellPos), ImTrunc(cellPos + ImVec2(0, cellSize.y)), frameColor, 1_scaled);
            drawList->PopClipRect();
        }
    }

    void HexEditor::drawEditor(const ImVec2 &size) {
        const float SeparatorColumWidth   = 6_scaled;
        const auto CharacterSize          = ImGui::CalcTextSize("0");

        if (m_currDataVisualizer == nullptr) {
            if (const auto &visualizer = ContentRegistry::HexEditor::getVisualizerByName("hex.builtin.visualizer.hexadecimal.8bit"); visualizer != nullptr) {
                m_currDataVisualizer = visualizer;
                return;
            }
        }

        if (m_miniMapVisualizer == nullptr) {
            if (const auto &visualizers = ContentRegistry::HexEditor::impl::getMiniMapVisualizers(); !visualizers.empty())
                m_miniMapVisualizer = visualizers.front();
        }

        const auto bytesPerCell    = m_currDataVisualizer->getBytesPerCell();
        const u64 columnCount      = m_bytesPerRow / bytesPerCell;
        const auto bytesPerRow     = columnCount * bytesPerCell;

        auto byteColumnCount       = 2 + columnCount + getByteColumnSeparatorCount(columnCount) + 2 + 2;

        if (byteColumnCount >= IMGUI_TABLE_MAX_COLUMNS) {
            m_bytesPerRow = 64;
            return;
        }

        const auto selection = getSelection();
        m_frameStartSelectionRegion = selection;

        if (m_provider == nullptr || m_provider->getActualSize() == 0) {
            ImGuiExt::TextOverlay("hex.ui.hex_editor.no_bytes"_lang, ImGui::GetWindowPos() + ImGui::GetWindowSize() / 2, ImGui::GetWindowWidth() * 0.7);
        }

        if (!m_editingAddress.has_value() && ImGui::IsKeyPressed(ImGuiKey_Escape))
            m_mode = Mode::Overwrite;

        Region hoveredCell = Region::Invalid();
        ImGui::PushID(bytesPerRow);
        ON_SCOPE_EXIT { ImGui::PopID(); };
        if (ImGui::BeginChild("Hex View", size, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            this->drawScrollbar(CharacterSize);

            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.5, 0));
            if (ImGui::BeginTable("##hex", byteColumnCount, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoKeepColumnsVisible, size)) {
                View::discardNavigationRequests();
                ImGui::TableSetupScrollFreeze(0, 2);

                // Row address column
                ImGui::TableSetupColumn("hex.ui.common.address"_lang, ImGuiTableColumnFlags_WidthFixed,
                    m_provider == nullptr ? 0 :
                    CharacterSize.x * std::max(
                        fmt::formatted_size("{:08X}: ", ((m_scrollPosition + m_visibleRowCount) * bytesPerRow) + m_provider->getBaseAddress() + m_provider->getCurrentPageAddress()),
                        m_separatorStride == 0 ? 0 : fmt::formatted_size("{} {}", "hex.ui.common.segment"_lang, (m_scrollPosition + m_visibleRowCount) * bytesPerRow + m_provider->getBaseAddress() + m_provider->getCurrentPageAddress() / m_separatorStride)
                    )
                );
                ImGui::TableSetupColumn("");

                // Byte columns
                for (u64 i = 0; i < columnCount; i++) {
                    if (isColumnSeparatorColumn(i, columnCount))
                        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, SeparatorColumWidth);

                    ImGui::TableSetupColumn(formatAddress(i * bytesPerCell, m_currDataVisualizer->getMaxCharsPerCell()).c_str(), ImGuiTableColumnFlags_WidthFixed, CharacterSize.x * m_currDataVisualizer->getMaxCharsPerCell() + std::ceil((6 + m_byteCellPadding) * 1_scaled));
                }

                // ASCII column
                ImGui::TableSetupColumn("");

                if (m_showAscii) {
                    ImGui::TableSetupColumn("hex.ui.common.encoding.ascii"_lang, ImGuiTableColumnFlags_WidthFixed, (CharacterSize.x + m_characterCellPadding * 1_scaled) * bytesPerRow);
                } else {
                    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 0);
                }

                ImGui::TableSetupColumn("");
                // Custom encoding column
                {
                    if (m_currCustomEncoding.has_value() && m_showCustomEncoding) {
                        ImGui::TableSetupColumn(m_currCustomEncoding->getName().c_str(), ImGuiTableColumnFlags_WidthStretch);
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

                if (m_provider != nullptr && m_provider->isReadable()) {
                    const auto isCurrRegionValid = [this](u64 address) {
                        auto &[currRegion, currRegionValid] = m_currValidRegion;
                        if (!Region{ .address=address, .size=1 }.isWithin(currRegion)) {
                            m_currValidRegion = m_provider->getRegionValidity(address);
                        }

                        return currRegionValid;
                    };

                    ImS64 numRows = (m_provider->getSize() / bytesPerRow) + ((m_provider->getSize() % bytesPerRow) == 0 ? 0 : 1);

                    if (numRows == 0) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGuiExt::TextFormatted("        ");
                    }


                    m_visibleRowCount = size.y / CharacterSize.y;
                    m_visibleRowCount = std::max<i64>(m_visibleRowCount, 1);

                    // Loop over rows
                    std::vector<u8> bytes(bytesPerRow, 0x00);
                    std::vector<std::tuple<std::optional<color_t>, std::optional<color_t>>> cellColors(bytesPerRow / bytesPerCell);
                    for (ImS64 y = m_scrollPosition; y < (m_scrollPosition + m_visibleRowCount + 5) && y < numRows && numRows != 0; y++) {
                        // Draw address column
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();

                        double addressWidth = ImGui::GetCursorPosX();
                        {
                            const auto rowAddress = y * bytesPerRow + m_provider->getBaseAddress() + m_provider->getCurrentPageAddress();

                            if (m_separatorStride > 0 && rowAddress % m_separatorStride < bytesPerRow && !ImGui::GetIO().KeyShift)
                                ImGuiExt::TextFormattedColored(ImGui::GetStyleColorVec4(ImGuiCol_SeparatorActive), "{} {}", "hex.ui.common.segment"_lang, rowAddress / m_separatorStride);
                            else
                                ImGuiExt::TextFormattedSelectable("{0}: ", formatAddress(rowAddress, 8));
                        }

                        ImGui::TableNextColumn();
                        addressWidth = ImGui::GetCursorPosX() - addressWidth;

                        const u8 validBytes = std::min<u64>(bytesPerRow, m_provider->getSize() - y * bytesPerRow);

                        m_provider->read(y * bytesPerRow + m_provider->getBaseAddress() + m_provider->getCurrentPageAddress(), bytes.data(), validBytes);

                        {
                            for (u64 x = 0; x < std::ceil(float(validBytes) / bytesPerCell); x++) {
                                const u64 byteAddress = y * bytesPerRow + x * bytesPerCell + m_provider->getBaseAddress() + m_provider->getCurrentPageAddress();

                                const auto cellBytes = std::min<u64>(validBytes, bytesPerCell);

                                // Query cell colors
                                if (x < std::ceil(float(validBytes) / bytesPerCell)) {
                                    auto foregroundColor = m_foregroundColorCallback(byteAddress, &bytes[x * cellBytes], cellBytes);
                                    auto backgroundColor = m_backgroundColorCallback(byteAddress, &bytes[x * cellBytes], cellBytes);

                                    if (m_grayOutZero && !foregroundColor.has_value()) {
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

                                    cellColors[x] = {
                                            foregroundColor,
                                            backgroundColor
                                    };
                                } else {
                                    cellColors[x] = {
                                        std::nullopt,
                                        std::nullopt
                                    };
                                }
                            }
                        }

                        // Draw byte columns
                        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, scaled(ImVec2(2.75F, 0.0F)));

                        const auto maxCharsPerCell = m_currDataVisualizer->getMaxCharsPerCell();
                        auto byteCellSize = (CharacterSize * ImVec2(maxCharsPerCell, 1)) + (ImVec2(2, 2) * ImGui::GetStyle().CellPadding) + scaled(ImVec2(1 + m_byteCellPadding, 0));
                        byteCellSize = ImVec2(std::ceil(byteCellSize.x), std::ceil(byteCellSize.y));

                        std::optional<float> prevEndPosX;
                        for (u64 x = 0; x < columnCount; x++) {
                            const u64 byteAddress = y * bytesPerRow + x * bytesPerCell + m_provider->getBaseAddress() + m_provider->getCurrentPageAddress();

                            ImGui::TableNextColumn();
                            drawSeparatorLine(byteAddress, x != 0);
                            if (isColumnSeparatorColumn(x, columnCount)) {
                                ImGui::TableNextColumn();
                                drawSeparatorLine(byteAddress, false);
                            }

                            if (x < std::ceil(float(validBytes) / bytesPerCell)) {
                                auto cellStartPos = getCellPosition();

                                auto [foregroundColor, backgroundColor] = cellColors[x];

                                auto adjustedCellSize = byteCellSize;
                                if (isColumnSeparatorColumn(x + 1, columnCount) && cellColors.size() > x + 1) {
                                    auto separatorAddress = x + y * columnCount;
                                    auto [nextForegroundColor, nextBackgroundColor] = cellColors[x + 1];
                                    if ((isSelectionValid() && getSelection().overlaps({ .address=separatorAddress, .size=1 }) && getSelection().getEndAddress() != separatorAddress) || backgroundColor == nextBackgroundColor)
                                        adjustedCellSize.x += SeparatorColumWidth + 1;
                                }

                                if (y == m_scrollPosition)
                                    adjustedCellSize.y -= (ImGui::GetStyle().CellPadding.y);

                                backgroundColor = applySelectionColor(byteAddress, backgroundColor);

                                if (prevEndPosX.has_value()) {
                                    adjustedCellSize.x += cellStartPos.x - prevEndPosX.value();
                                    cellStartPos.x = prevEndPosX.value();
                                }

                                prevEndPosX = cellStartPos.x + adjustedCellSize.x;

                                // Draw highlights and selection
                                if (backgroundColor.has_value()) {
                                    this->drawBackgroundHighlight(cellStartPos, adjustedCellSize, backgroundColor.value());
                                }

                                // Draw frame around mouse selection
                                this->drawSelection(x, y, selection, byteAddress, bytesPerCell, cellStartPos, adjustedCellSize, ImGui::GetStyleColorVec4(ImGuiCol_Text));

                                const bool cellHovered = ImGui::IsMouseHoveringRect(cellStartPos, cellStartPos + adjustedCellSize, false) && ImGui::IsWindowHovered();

                                this->handleSelection(byteAddress, bytesPerCell, &bytes[x * bytesPerCell], cellHovered);

                                // Set byte foreground color
                                auto popForeground = SCOPE_GUARD { ImGui::PopStyleColor(); };
                                if (foregroundColor.has_value() && m_editingAddress != byteAddress)
                                    ImGui::PushStyleColor(ImGuiCol_Text, *foregroundColor);
                                else
                                    popForeground.release();

                                // Draw cell content
                                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                                ImGui::PushItemWidth((CharacterSize * maxCharsPerCell).x);
                                if (isCurrRegionValid(byteAddress))
                                    this->drawCell(byteAddress, &bytes[x * bytesPerCell], bytesPerCell, cellHovered, CellType::Hex);
                                else
                                    ImGuiExt::TextFormatted("{:?>{}}", "", maxCharsPerCell);

                                if (cellHovered) {
                                    Region newHoveredCell = { .address=byteAddress, .size=bytesPerCell };
                                    if (hoveredCell != newHoveredCell) {
                                        hoveredCell = newHoveredCell;
                                    }
                                }

                                ImGui::PopItemWidth();
                                ImGui::PopStyleVar();
                            }
                        }
                        ImGui::PopStyleVar();

                        ImGui::TableNextColumn();
                        ImGui::TableNextColumn();

                        // Draw ASCII column
                        if (m_showAscii) {
                            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
                            if (ImGui::BeginTable("##ascii_column", bytesPerRow)) {
                                for (u64 x = 0; x < bytesPerRow; x++)
                                    ImGui::TableSetupColumn(fmt::format("##ascii_cell{}", x).c_str(), ImGuiTableColumnFlags_WidthFixed, CharacterSize.x + (m_characterCellPadding * 1_scaled));

                                ImGui::TableNextRow();

                                const auto asciiCellSize = CharacterSize + scaled(ImVec2(m_characterCellPadding, 0));

                                for (u64 x = 0; x < bytesPerRow; x++) {
                                    const u64 byteAddress = y * bytesPerRow + x + m_provider->getBaseAddress() + m_provider->getCurrentPageAddress();

                                    ImGui::TableNextColumn();
                                    drawSeparatorLine(byteAddress, true);

                                    const auto cellStartPos = getCellPosition();

                                    const bool cellHovered = ImGui::IsMouseHoveringRect(cellStartPos, cellStartPos + asciiCellSize, true) && ImGui::IsWindowHovered();

                                    if (x < validBytes) {
                                        this->handleSelection(byteAddress, bytesPerCell, &bytes[x], cellHovered);

                                        auto [foregroundColor, backgroundColor] = cellColors[x / bytesPerCell];

                                        backgroundColor = applySelectionColor(byteAddress, backgroundColor);

                                        // Draw highlights and selection
                                        if (backgroundColor.has_value()) {
                                            this->drawBackgroundHighlight(cellStartPos, asciiCellSize, backgroundColor.value());
                                        }

                                        this->drawSelection(x, y, selection, byteAddress, 1, cellStartPos, asciiCellSize, ImGui::GetStyleColorVec4(ImGuiCol_Text));

                                        // Set cell foreground color
                                        auto popForeground = SCOPE_GUARD { ImGui::PopStyleColor(); };
                                        if (foregroundColor.has_value() && m_editingAddress != byteAddress)
                                            ImGui::PushStyleColor(ImGuiCol_Text, *foregroundColor);
                                        else
                                            popForeground.release();

                                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (m_characterCellPadding * 1_scaled) / 2);
                                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                                        ImGui::PushItemWidth(CharacterSize.x);
                                        if (!isCurrRegionValid(byteAddress))
                                            ImGuiExt::TextFormattedDisabled("{}", m_unknownDataCharacter);
                                        else
                                            this->drawCell(byteAddress, &bytes[x], 1, cellHovered, CellType::ASCII);

                                        if (cellHovered) {
                                            Region newHoveredCell = { .address=byteAddress, .size=bytesPerCell };
                                            if (hoveredCell != newHoveredCell) {
                                                hoveredCell = newHoveredCell;
                                            }
                                        }

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
                        if (m_showCustomEncoding && m_currCustomEncoding.has_value()) {
                            m_maxFittingColumns = 16;

                            if (m_encodingLineStartAddresses.empty()) {
                                m_encodingLineStartAddresses.push_back(0);
                            }

                            const bool singleByteEncoding = m_currCustomEncoding->getLongestSequence() == 1 && m_currCustomEncoding->getShortestSequence() == 1;
                            if (size_t(y) < m_encodingLineStartAddresses.size() || singleByteEncoding) {
                                std::vector<std::pair<u64, CustomEncodingData>> encodingData;

                                if (singleByteEncoding) {
                                    u64 offset = 0;
                                    do {
                                        const u64 address = y * bytesPerRow + offset + m_provider->getBaseAddress() + m_provider->getCurrentPageAddress();

                                        auto result = queryCustomEncodingData(m_provider, *m_currCustomEncoding, address);

                                        offset += result.advance;
                                        encodingData.emplace_back(address, result);
                                    } while (offset < bytesPerRow);
                                } else {
                                    if (m_encodingLineStartAddresses[y] >= bytesPerRow) {
                                        encodingData.emplace_back(y * bytesPerRow + m_provider->getBaseAddress() + m_provider->getCurrentPageAddress(), CustomEncodingData(".", 1, ImGuiExt::GetCustomColorU32(ImGuiCustomCol_AdvancedEncodingUnknown)));
                                        m_encodingLineStartAddresses.push_back(0);
                                    } else {
                                        u64 offset = m_encodingLineStartAddresses[y];
                                        do {
                                            const u64 address = y * bytesPerRow + offset + m_provider->getBaseAddress() + m_provider->getCurrentPageAddress();

                                            auto result = queryCustomEncodingData(m_provider, *m_currCustomEncoding, address);

                                            offset += result.advance;
                                            encodingData.emplace_back(address, result);
                                        } while (offset < bytesPerRow);

                                        m_encodingLineStartAddresses.push_back(offset - bytesPerRow);
                                    }
                                }

                                ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
                                ImGui::PushID(y);
                                ON_SCOPE_EXIT { ImGui::PopID(); };
                                if (ImGui::BeginTable("##encoding_cell", encodingData.size(), ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoKeepColumnsVisible)) {
                                    ImGui::TableNextRow();

                                    for (const auto &[address, data] : encodingData) {
                                        ImGui::TableNextColumn();

                                        const auto cellStartPos = getCellPosition();
                                        const auto cellSize = ImGui::CalcTextSize(data.displayValue.c_str()) * ImVec2(1, 0) + ImVec2(m_characterCellPadding * 1_scaled, CharacterSize.y);
                                        const bool cellHovered = ImGui::IsMouseHoveringRect(cellStartPos, cellStartPos + cellSize, true) && ImGui::IsWindowHovered();

                                        const auto x = address % bytesPerRow;
                                        if (x < validBytes && isCurrRegionValid(address)) {
                                            auto [foregroundColor, backgroundColor] = cellColors[x / bytesPerCell];

                                            backgroundColor = applySelectionColor(address, backgroundColor);

                                            // Draw highlights and selection
                                            if (backgroundColor.has_value()) {
                                                this->drawBackgroundHighlight(cellStartPos, cellSize, backgroundColor.value());
                                            }

                                            this->drawSelection(x, y, selection, address, 1, cellStartPos, cellSize, ImGui::GetStyleColorVec4(ImGuiCol_Text));

                                            auto startPos = ImGui::GetCursorPos();
                                            ImGuiExt::TextFormattedColored(data.color, "{}", data.displayValue);
                                            ImGui::SetCursorPosX(startPos.x + cellSize.x);
                                            ImGui::SameLine(0, 0);
                                            ImGui::Dummy({ 0, 0 });

                                            this->handleSelection(address, data.advance, &bytes[address % bytesPerRow], cellHovered);

                                            if (cellHovered) {
                                                Region newHoveredCell = { .address=address, .size=data.advance };
                                                if (hoveredCell != newHoveredCell) {
                                                    hoveredCell = newHoveredCell;
                                                }
                                            }
                                        }
                                    }

                                    ImGui::EndTable();
                                }
                                ImGui::PopStyleVar();
                            }
                        } else {
                            m_maxFittingColumns = std::floor((size.x - addressWidth) / (byteCellSize.x + CharacterSize.x + m_characterCellPadding * 1_scaled));
                            m_maxFittingColumns *= 0.95;
                            m_maxFittingColumns = std::clamp<i32>(m_maxFittingColumns, 1, 128 / this->getBytesPerCell());
                        }

                        // Scroll to the cursor if it's either at the top or bottom edge of the screen
                        if (m_shouldScrollToSelection && isSelectionValid()) {
                            // Make sure simply clicking on a byte at the edge of the screen won't cause scrolling
                            if ((ImGui::IsMouseDragging(ImGuiMouseButton_Left))) {
                                if ((*m_selectionStart >= (*m_selectionEnd + bytesPerRow)) && y == (m_scrollPosition + 1)) {
                                    if (i128(m_selectionEnd.value() - m_provider->getBaseAddress() - m_provider->getCurrentPageAddress()) <= (ImS64(m_scrollPosition + 1) * bytesPerRow)) {
                                        m_shouldScrollToSelection = false;
                                        m_scrollPosition -= 3;
                                    }
                                } else if ((*m_selectionStart <= (*m_selectionEnd - bytesPerRow)) && y == ((m_scrollPosition + m_visibleRowCount) - 1)) {
                                    if (i128(m_selectionEnd.value() - m_provider->getBaseAddress() - m_provider->getCurrentPageAddress()) >= (ImS64((m_scrollPosition + m_visibleRowCount) - 2) * bytesPerRow)) {
                                        m_shouldScrollToSelection = false;
                                        m_scrollPosition += 3;
                                    }
                                }
                            }

                            // If the cursor is off-screen, directly jump to the byte
                            if (m_shouldJumpWhenOffScreen) {
                                m_shouldJumpWhenOffScreen = false;

                                const auto pageAddress = m_provider->getCurrentPageAddress() + m_provider->getBaseAddress();
                                auto newSelection = getSelection();
                                newSelection.address -= pageAddress;

                                if ((newSelection.getStartAddress()) < u64(m_scrollPosition * bytesPerRow))
                                    this->jumpToSelection(0.0F);
                                if ((newSelection.getEndAddress()) > u64((m_scrollPosition + m_visibleRowCount) * bytesPerRow))
                                    this->jumpToSelection(1.0F);
                            }
                        }
                    }

                    // Handle jumping to selection
                    if (m_shouldJumpToSelection) {
                        const auto jumpAddress = this->getCursorPosition().value_or(0);

                        m_provider->setCurrentPage(m_provider->getPageOfAddress(jumpAddress).value_or(0));

                        const auto pageAddress = m_provider->getCurrentPageAddress() + m_provider->getBaseAddress();
                        const auto targetRowNumber = (jumpAddress - pageAddress) / bytesPerRow;

                        // Calculate the current top and bottom row numbers of the viewport
                        ImS64 currentTopRow = m_scrollPosition;
                        ImS64 currentBottomRow = std::max<ImS64>(m_scrollPosition + m_visibleRowCount - 3, 0);

                        // Check if the targetRowNumber is outside the current visible range
                        if (ImS64(targetRowNumber) < currentTopRow) {
                            // If target is above the current view, scroll just enough to bring it into view at the top
                            m_scrollPosition = targetRowNumber + m_visibleRowCount * m_jumpPivot - 3;
                        } else if (ImS64(targetRowNumber) > currentBottomRow) {
                            // If target is below the current view, scroll just enough to bring it into view at the bottom
                            m_scrollPosition = targetRowNumber - 3;
                        }

                        m_jumpPivot = 0.0F;
                        m_shouldJumpToSelection = false;
                    }

                }

                ImGui::EndTable();
                ImGui::PopStyleVar();
            }
        }
        ImGui::EndChild();

        ImHexApi::HexEditor::impl::setHoveredRegion(m_provider, hoveredCell);

        if (m_hoveredRegion != hoveredCell) {
            m_hoveredRegion = hoveredCell;
            m_hoverChangedCallback(m_hoveredRegion.address, m_hoveredRegion.size);
        }

        m_shouldScrollToSelection = false;
    }

    void HexEditor::drawMinimapPopup() {
        if (ImGui::BeginPopup("MiniMapOptions")) {
            ImGui::SliderInt("hex.ui.hex_editor.minimap.width"_lang, &m_miniMapWidth, 1, 25, "%d", ImGuiSliderFlags_AlwaysClamp);

            if (ImGui::BeginCombo("##minimap_visualizer", Lang(m_miniMapVisualizer->unlocalizedName))) {

                for (const auto &visualizer : ContentRegistry::HexEditor::impl::getMiniMapVisualizers()) {
                    if (ImGui::Selectable(Lang(visualizer->unlocalizedName))) {
                        m_miniMapVisualizer = visualizer;
                    }
                }

                ImGui::EndCombo();
            }

            ImGui::Checkbox("hex.ui.hex_editor.minimap.value_brightness"_lang, &m_minimapValueBrightness);

            ImGui::EndPopup();
        }
    }

    void HexEditor::drawFooter(const ImVec2 &size) {
        const auto windowEndPos = ImGui::GetWindowPos() + size - ImGui::GetStyle().WindowPadding;
        ImGui::GetWindowDrawList()->AddLine(windowEndPos - ImVec2(0, size.y - 1_scaled), windowEndPos - size + ImVec2(0, 1_scaled), ImGui::GetColorU32(ImGuiCol_Separator), 2.0_scaled);

        if (ImGui::BeginChild("##footer", size, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 8_scaled);
            ImGui::Dummy({});
            if (ImGui::BeginTable("##footer_table", 3, ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthStretch, 0.5F);
                ImGui::TableSetupColumn("Center", ImGuiTableColumnFlags_WidthFixed, 20_scaled);
                ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthStretch, 0.5F);
                ImGui::TableNextRow();

                if (m_provider != nullptr && m_provider->isReadable()) {
                    const auto pageCount = std::max<u32>(1, m_provider->getPageCount());
                    constexpr static u32 MinPage = 1;

                    const auto pageAddress = m_provider->getCurrentPageAddress();
                    const auto pageSize    = m_provider->getSize();

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    {
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 2_scaled);

                        // Upper/lower case hex
                        ImGuiExt::DimmedIconToggle(ICON_VS_CASE_SENSITIVE, &m_upperCaseHex);
                        ImGuiExt::InfoTooltip("hex.ui.hex_editor.uppercase_hex"_lang);

                        ImGui::SameLine();

                        // Grayed out zeros
                        ImGuiExt::DimmedIconToggle(ICON_VS_LIGHTBULB, &m_grayOutZero);
                        ImGuiExt::InfoTooltip("hex.ui.hex_editor.gray_out_zero"_lang);

                        ImGui::SameLine();

                        // ASCII view
                        ImGuiExt::DimmedIconToggle(ICON_VS_SYMBOL_KEY, &m_showAscii);
                        ImGuiExt::InfoTooltip("hex.ui.hex_editor.ascii_view"_lang);

                        ImGui::SameLine(0, 1_scaled);

                        // Extended ASCII
                        ImGui::BeginDisabled(!m_showAscii);
                        ImGuiExt::DimmedIconToggle(ICON_VS_WHITESPACE, &m_showExtendedAscii);
                        ImGuiExt::InfoTooltip("hex.ui.hex_editor.extended_ascii"_lang);
                        ImGui::EndDisabled();

                        ImGui::SameLine(0, 1_scaled);

                        // Custom encoding view
                        ImGui::BeginDisabled(!m_currCustomEncoding.has_value());
                        ImGuiExt::DimmedIconToggle(ICON_VS_CODE_REVIEW, &m_showCustomEncoding);
                        ImGuiExt::InfoTooltip("hex.ui.hex_editor.custom_encoding_view"_lang);
                        ImGui::EndDisabled();

                        ImGui::SameLine(0, 1_scaled);

                        // Minimap
                        ImGuiExt::DimmedIconToggle(ICON_VS_MAP, &m_showMiniMap);
                        ImGuiExt::InfoTooltip("hex.ui.hex_editor.minimap"_lang);
                        if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && m_miniMapVisualizer != nullptr)
                            ImGui::OpenPopup("MiniMapOptions");

                        drawMinimapPopup();

                        ImGui::SameLine(0, 1_scaled);

                        // Data Cell configuration
                        if (ImGuiExt::DimmedIconButton(ICON_VS_TABLE, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                            ImGui::OpenPopup("DataCellOptions");
                        }
                        ImGuiExt::InfoTooltip("hex.ui.hex_editor.data_cell_options"_lang);

                        if (ImGui::BeginPopup("DataCellOptions")) {

                            if (ImGui::BeginCombo("##visualizer", Lang(m_currDataVisualizer->getUnlocalizedName()))) {
                                for (const auto &visualizer : ContentRegistry::HexEditor::impl::getVisualizers()) {
                                    if (ImGui::Selectable(Lang(visualizer->getUnlocalizedName()))) {
                                        m_currDataVisualizer = visualizer;
                                        m_encodingLineStartAddresses.clear();

                                        m_bytesPerRow = std::max(m_bytesPerRow, visualizer->getBytesPerCell());
                                    }
                                }

                                ImGui::EndCombo();
                            }

                            {
                                bool hasEndianness = m_currDataVisualizer->getBytesPerCell() > 1;

                                if (!hasEndianness)
                                    m_dataVisualizerEndianness = std::endian::native;

                                ImGui::BeginDisabled(!hasEndianness);
                                {
                                    int sliderPos = m_dataVisualizerEndianness == std::endian::little ? 0 : 1;
                                    ImGui::SliderInt("##visualizer_endianness", &sliderPos, 0, 1, sliderPos == 0 ? "hex.ui.common.little"_lang : "hex.ui.common.big"_lang);
                                    m_dataVisualizerEndianness = sliderPos == 0 ? std::endian::little : std::endian::big;
                                }
                                ImGui::EndDisabled();
                            }

                            ImGui::NewLine();

                            int byteColumnCount = m_autoFitColumns ? 0 : m_bytesPerRow / this->getBytesPerCell();
                            if (ImGui::SliderInt("##byte_column_count", &byteColumnCount, 0, 128 / this->getBytesPerCell(), m_autoFitColumns ? "hex.ui.hex_editor.fit_columns"_lang : fmt::format("{} {}", byteColumnCount * this->getBytesPerCell(), "hex.ui.hex_editor.columns"_lang).c_str())) {
                                m_bytesPerRow = byteColumnCount * this->getBytesPerCell();
                                m_encodingLineStartAddresses.clear();
                            }
                            m_autoFitColumns = byteColumnCount == 0;

                            {
                                const u64 min = 0;
                                const u64 max = m_provider->getActualSize();
                                ImGui::SliderScalar("##separator_stride", ImGuiDataType_U64, &m_separatorStride, &min, &max, m_separatorStride == 0 ? "hex.ui.hex_editor.no_separator"_lang : fmt::format("hex.ui.hex_editor.separator_stride"_lang, m_separatorStride).c_str());
                            }
                            {
                                int selection = [this] {
                                    switch (m_addressFormat) {
                                        default:
                                        case AddressFormat::Hexadecimal:
                                            return 0;
                                        case AddressFormat::Decimal:
                                            return 1;
                                        case AddressFormat::Octal:
                                            return 2;
                                    }
                                }();

                                std::array options = {
                                    fmt::format("{}:  {}", "hex.ui.common.number_format"_lang, "hex.ui.common.hexadecimal"_lang),
                                    fmt::format("{}:  {}", "hex.ui.common.number_format"_lang, "hex.ui.common.decimal"_lang),
                                    fmt::format("{}:  {}", "hex.ui.common.number_format"_lang, "hex.ui.common.octal"_lang)
                                };

                                if (ImGui::SliderInt("##format", &selection, 0, options.size() - 1, options[selection].c_str(), ImGuiSliderFlags_NoInput)) {
                                    switch (selection) {
                                        default:
                                        case 0:
                                            m_addressFormat = AddressFormat::Hexadecimal;
                                        break;
                                        case 1:
                                            m_addressFormat = AddressFormat::Decimal;
                                        break;
                                        case 2:
                                            m_addressFormat = AddressFormat::Octal;
                                        break;
                                    }
                                }
                            }
                            ImGui::EndPopup();
                        }
                    }

                    ImGui::SameLine(0, 15_scaled);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2_scaled);
                    if (m_mode == Mode::Insert) {
                        ImGui::TextUnformatted("[ INSERT ]");
                    } else {
                        ImGui::Dummy({});
                    }

                    // Collapse button
                    ImGui::TableNextColumn();
                    {
                        if (ImGuiExt::DimmedIconButton(m_footerCollapsed ? ICON_VS_FOLD_UP : ICON_VS_FOLD_DOWN, ImGui::GetStyleColorVec4(ImGuiCol_Text)))
                            m_footerCollapsed = !m_footerCollapsed;
                    }

                    ImGui::TableNextColumn();

                    if (m_showSelectionInFooter && this->isSelectionValid()) {
                        const auto selection = this->getSelection();

                        ImGui::SameLine(0, 15_scaled);
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2_scaled);
                        ImGuiExt::TextFormattedSelectable("{0} - {1} ({2} | {3} bytes)",
                            formatAddress(selection.getStartAddress(), 2, true),
                            formatAddress(selection.getEndAddress(), 2, true),
                            formatAddress(selection.getSize(), 2, true),
                            selection.getSize()
                        );
                    }

                    if (!m_footerCollapsed) {
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3_scaled);
                        ImGui::Dummy({});
                        ImGui::TableNextRow();

                        // Page slider
                        ImGui::TableNextColumn();
                        {
                            u32 page = m_provider->getCurrentPage() + 1;

                            ImGui::BeginDisabled(pageCount <= 1);
                            {
                                ImGui::PushItemWidth(-1);
                                if (ImGui::SliderScalar("##page_selection", ImGuiDataType_U32, &page, &MinPage, &pageCount,
                                    fmt::format("%llu / {0}  [{1} - {2}]",
                                        pageCount,
                                        formatAddress(pageAddress, 4, true),
                                        formatAddress(pageSize == 0 ? 0 : (pageAddress + pageSize - 1), 4, true)
                                        ).c_str()))
                                    m_provider->setCurrentPage(page - 1);
                                ImGui::PopItemWidth();
                            }
                            ImGui::EndDisabled();
                        }

                        ImGui::TableNextColumn();

                        // Loaded data size
                        ImGui::TableNextColumn();
                        {
                            ImGuiExt::TextFormattedSelectable("{0} ({1} | {2})",
                                                           formatAddress(m_provider->getBaseAddress(), 8, true),
                                                           formatAddress(m_provider->getBaseAddress() + m_provider->getActualSize(), 1, true),
                                                           ImGui::GetIO().KeyCtrl
                                                               ? fmt::format("{}", m_provider->getActualSize())
                                                               : hex::toByteString(m_provider->getActualSize())
                            );
                            ImGui::SetItemTooltip("%s", "hex.ui.hex_editor.data_size"_lang.get());
                        }
                    }
                }

                ImGui::EndTable();
            }
        }
        ImGui::EndChild();

        if (m_autoFitColumns) {
            m_bytesPerRow = m_maxFittingColumns * this->getBytesPerCell();
        }
    }

    void HexEditor::handleSelection(u64 address, u32 bytesPerCell, const u8 *data, bool cellHovered) {
        if (ImGui::IsWindowHovered() && cellHovered) {
            drawTooltip(address, data, bytesPerCell);

            auto endAddress = address + bytesPerCell - 1;
            auto &selectionStart = m_selectionStart;

            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                this->setSelection(selectionStart.value_or(address), endAddress);
                this->scrollToSelection();
            }
            else if (ImGui::IsMouseDown(ImGuiMouseButton_Left) || (ImGui::IsMouseDown(ImGuiMouseButton_Right) && (address < std::min(m_selectionStart, m_selectionEnd) || address > std::max(m_selectionStart, m_selectionEnd)))) {
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

        auto footerSize = ImVec2(width, 0);
        if (!m_footerCollapsed)
            footerSize.y = ImGui::GetTextLineHeightWithSpacing() * 4.0F;
        else
            footerSize.y = ImGui::GetTextLineHeightWithSpacing() * 2.4F;

        auto tableSize  = ImVec2(width, height - footerSize.y);
        if (tableSize.y <= 0)
            tableSize.y = height;

        fonts::HexEditor().push();
        this->drawEditor(tableSize);
        fonts::HexEditor().pop();

        if (tableSize.y > 0)
            this->drawFooter(footerSize);

        m_selectionChanged = false;

        m_cursorBlinkTimer += ImGui::GetIO().DeltaTime;
    }

}
