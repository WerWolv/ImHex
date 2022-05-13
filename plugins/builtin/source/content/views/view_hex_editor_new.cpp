#include "content/views/view_hex_editor_new.hpp"
#include "content/providers/file_provider.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/helpers/project_file_handler.hpp>
#include <hex/helpers/logger.hpp>

#include <imgui_internal.h>
#include <nlohmann/json.hpp>

namespace hex::plugin::builtin {

    ViewHexEditorNew::ViewHexEditorNew() : View("Hex Editor New") {
        this->m_currDataVisualizer = ContentRegistry::HexEditor::impl::getVisualizers().begin()->second;

        ShortcutManager::addShortcut(this, Keys::Escape, [this] {
            this->m_selectionStart = this->m_selectionEnd = InvalidSelection;
        });

        ShortcutManager::addShortcut(this, Keys::Up, [this] {
            const u64 bytesPerRow = this->m_currDataVisualizer->getBytesPerCell() * this->m_columnCount;
            if (this->m_selectionStart >= bytesPerRow) {
                auto pos = this->m_selectionStart - bytesPerRow;
                this->setSelection(pos, pos);
            }
        });
        ShortcutManager::addShortcut(this, Keys::Down, [this] {
            const u64 bytesPerRow = this->m_currDataVisualizer->getBytesPerCell() * this->m_columnCount;
            auto pos = this->m_selectionStart + bytesPerRow;
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

        ShortcutManager::addShortcut(this, SHIFT + Keys::Up, [this] {
            const u64 bytesPerRow = this->m_currDataVisualizer->getBytesPerCell() * this->m_columnCount;
            this->setSelection(this->m_selectionStart - bytesPerRow, this->m_selectionEnd);
        });
        ShortcutManager::addShortcut(this, SHIFT + Keys::Down, [this] {
            const u64 bytesPerRow = this->m_currDataVisualizer->getBytesPerCell() * this->m_columnCount;
            this->setSelection(this->m_selectionStart + bytesPerRow, this->m_selectionEnd);

        });
        ShortcutManager::addShortcut(this, SHIFT + Keys::Left, [this] {
            this->setSelection(this->m_selectionStart - 1, this->m_selectionEnd);

        });
        ShortcutManager::addShortcut(this, SHIFT + Keys::Right, [this] {
            this->setSelection(this->m_selectionStart + 1, this->m_selectionEnd);
        });


        EventManager::subscribe<RequestOpenFile>(this, [this](const auto &path) { this->openFile(path); });

        EventManager::subscribe<EventSettingsChanged>(this, [this] {
            {
                auto alpha = ContentRegistry::Settings::getSetting("hex.builtin.setting.interface", "hex.builtin.setting.interface.highlight_alpha");

                if (alpha.is_number())
                    this->m_highlightAlpha = alpha;
            }

            {
                auto columnCount = ContentRegistry::Settings::getSetting("hex.builtin.setting.hex_editor", "hex.builtin.setting.hex_editor.column_count");

                if (columnCount.is_number())
                    this->m_columnCount = static_cast<int>(columnCount);
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

        });

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

    void ViewHexEditorNew::drawCell(u64 address, u8 *data, size_t size) {
        if (this->m_editingAddress != address)
            this->m_currDataVisualizer->draw(address, data, size, this->m_upperCaseHex);
        else {
            ImGui::SetKeyboardFocusHere();
            ImGui::CaptureKeyboardFromApp(true);

            if (this->m_currDataVisualizer->drawEditing(address, data, size, this->m_upperCaseHex)) {
                ImHexApi::Provider::get()->write(this->m_editingAddress.value(), data, size);
                (*this->m_editingAddress)++;
                this->setSelection(this->m_selectionStart + 1, this->m_selectionStart + 1);

                if (this->m_editingAddress > ImHexApi::Provider::get()->getSize())
                    this->m_editingAddress = std::nullopt;
            }
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            this->m_editingAddress = address;
        }

        if (this->m_editingAddress.has_value() && this->m_editingAddress != address && ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            ImHexApi::Provider::get()->write(this->m_editingAddress.value(), data, size);
            this->m_editingAddress = std::nullopt;
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

    void ViewHexEditorNew::drawContent() {
        if (ImGui::Begin(View::toWindowName(this->getUnlocalizedName()).c_str(), &this->getWindowOpenState(), ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNavInputs)) {

            const float SeparatorColumWidth   = 6_scaled;
            const auto CharacterSize          = ImGui::CalcTextSize("0");
            const color_t SelectionFrameColor = ImGui::GetColorU32(ImGuiCol_Text);

            const auto byteColumnCount = this->m_columnCount + getByteColumnSeparatorCount(this->m_columnCount);
            const u16 bytesPerRow = this->m_columnCount * this->m_currDataVisualizer->getBytesPerCell();


            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.5, 0));
            if (ImGui::BeginTable("##hex", 2 + byteColumnCount + 1 + 1, ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoKeepColumnsVisible)) {
                View::discardNavigationRequests();
                ImGui::TableSetupScrollFreeze(0, 2);

                // Row address column
                ImGui::TableSetupColumn("Address");
                ImGui::TableSetupColumn("");

                // Byte columns
                for (u16 i = 0; i < this->m_columnCount; i++) {
                    if (isColumnSeparatorColumn(i, this->m_columnCount))
                        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, SeparatorColumWidth);

                    ImGui::TableSetupColumn(hex::format(this->m_upperCaseHex ? "{:0{}X}" : "{:0{}x}", i, this->m_currDataVisualizer->getMaxCharsPerCell()).c_str(), ImGuiTableColumnFlags_WidthFixed, CharacterSize.x * this->m_currDataVisualizer->getMaxCharsPerCell() + 6);
                }
                ImGui::TableSetupColumn("");

                // ASCII columns
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, CharacterSize.x * this->m_columnCount);

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

                    ImGuiListClipper clipper(std::ceil(provider->getSize() / double(bytesPerRow)), CharacterSize.y);
                    while (clipper.Step()) {
                        // Loop over rows
                        for (i128 y = clipper.DisplayStart; y < u64(clipper.DisplayEnd); y++) {

                            // Scroll to the cursor if it's at the top of bottom edge of the screen
                            if (clipper.DisplayEnd > 1 && this->m_selectionChanged && this->m_selectionStart != InvalidSelection && this->m_selectionEnd != InvalidSelection) {
                                if (y < clipper.DisplayStart + 2 && i128(this->m_selectionStart) <= i64((clipper.DisplayStart + 1) * bytesPerRow)) {
                                    this->m_selectionChanged = false;
                                    ImGui::SetScrollHereY(0.05);
                                }

                                if (y > u64(clipper.DisplayEnd - 2) && i128(this->m_selectionEnd) >= i64((clipper.DisplayEnd - 2) * bytesPerRow)) {
                                    ImGui::SetScrollHereY(0.95);
                                    this->m_selectionChanged = false;
                                }

                            }

                            // Draw address column
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::TextFormatted(this->m_upperCaseHex ? "{:08X}: " : "{:08x}: ", y * bytesPerRow);
                            ImGui::TableNextColumn();

                            const u8 validBytes = std::min<u64>(bytesPerRow, provider->getSize() - y * bytesPerRow);

                            std::vector<u8> bytes(validBytes);
                            provider->read(y * bytesPerRow, bytes.data(), validBytes);

                            // Draw byte columns
                            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(3, 0));
                            for (u64 x = 0; x < this->m_columnCount; x++) {
                                const u64 byteAddress = y * bytesPerRow + x;

                                ImGui::TableNextColumn();
                                if (isColumnSeparatorColumn(x, this->m_columnCount))
                                    ImGui::TableNextColumn();

                                if (x < validBytes) {
                                    const auto cellStartPos  = (ImGui::GetWindowPos() + ImGui::GetCursorPos()) - ImGui::GetStyle().CellPadding - ImVec2(ImGui::GetScrollX(), ImGui::GetScrollY());
                                    const auto cellSize = (CharacterSize * ImVec2(this->m_currDataVisualizer->getMaxCharsPerCell(), 1) + (ImVec2(3, 2) * ImGui::GetStyle().CellPadding) - ImVec2(1, 0) * ImGui::GetStyle().CellPadding) + ImVec2(1, 0);

                                    auto backgroundColor = queryBackgroundColor(byteAddress, &bytes[x], this->m_currDataVisualizer->getBytesPerCell());

                                    // Calculate background color by blending highlighting and selection color together
                                    const color_t selectionColor = (ImAlphaBlendColors(0x32FFFFFF, 0x60C08080) & 0x00FFFFFF) | (this->m_highlightAlpha << 24);
                                    if ((byteAddress >= this->m_selectionStart && byteAddress <= this->m_selectionEnd)) {
                                        if (backgroundColor.has_value())
                                            backgroundColor = ImAlphaBlendColors(backgroundColor.value(), selectionColor);
                                        else
                                            backgroundColor = selectionColor;
                                    }

                                    // Draw highlights and selection
                                    if (backgroundColor.has_value()) {
                                        auto drawList = ImGui::GetWindowDrawList();

                                        auto adjustedCellSize = cellSize;
                                        if (isColumnSeparatorColumn(x + 1, this->m_columnCount) && this->m_selectionEnd != x + y * this->m_columnCount)
                                            adjustedCellSize.x += SeparatorColumWidth + 1;

                                        // Draw background color
                                        drawList->AddRectFilled(cellStartPos, cellStartPos + adjustedCellSize, backgroundColor.value());

                                        // Draw frame around mouse selection
                                        {
                                            // Draw vertical line at the left of first byte and the start of the line
                                            if (x == 0 || byteAddress == this->m_selectionStart)
                                                drawList->AddLine(cellStartPos, cellStartPos + ImVec2(0, adjustedCellSize.y), ImColor(SelectionFrameColor), 1.0F);

                                            // Draw vertical line at the right of the last byte and the end of the line
                                            if (x == u16(this->m_columnCount - 1) || byteAddress == this->m_selectionEnd)
                                                drawList->AddLine(cellStartPos + ImVec2(adjustedCellSize.x, -1), cellStartPos + adjustedCellSize, ImColor(SelectionFrameColor), 1.0F);

                                            // Draw horizontal line at the top of the bytes
                                            if ((byteAddress - this->m_columnCount) < this->m_selectionStart)
                                                drawList->AddLine(cellStartPos, cellStartPos + ImVec2(adjustedCellSize.x + 1, 0), ImColor(SelectionFrameColor), 1.0F);

                                            // Draw horizontal line at the bottom of the bytes
                                            if ((byteAddress + this->m_columnCount) > this->m_selectionEnd)
                                                drawList->AddLine(cellStartPos + ImVec2(0, adjustedCellSize.y), cellStartPos + adjustedCellSize + ImVec2(1, 0), ImColor(SelectionFrameColor), 1.0F);
                                        }
                                    }

                                    // Get byte foreground color
                                    auto foregroundColor = queryForegroundColor(byteAddress, &bytes[x], this->m_currDataVisualizer->getBytesPerCell());
                                    if (foregroundColor.has_value())
                                        ImGui::PushStyleColor(ImGuiCol_Text, *foregroundColor);

                                    // Draw cell content
                                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                                    ImGui::PushItemWidth((CharacterSize * this->m_currDataVisualizer->getMaxCharsPerCell()).x);
                                    this->drawCell(byteAddress, &bytes[x], this->m_currDataVisualizer->getBytesPerCell());
                                    ImGui::PopItemWidth();
                                    ImGui::PopStyleVar();

                                    if (foregroundColor.has_value())
                                        ImGui::PopStyleColor();

                                    // Handle selection
                                    {
                                        if (ImGui::IsMouseHoveringRect(cellStartPos, cellStartPos + cellSize + ImVec2(1, 0))) {
                                            drawTooltip(byteAddress, &bytes[x], this->m_currDataVisualizer->getBytesPerCell());
                                            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                                                if (byteAddress <= this->m_selectionStart)
                                                    this->setSelection(byteAddress, this->m_selectionEnd);
                                                else
                                                    this->setSelection(this->m_selectionStart, byteAddress);
                                            }
                                            else if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
                                                this->setSelection(byteAddress, byteAddress);
                                        }
                                    }
                                }
                            }
                            ImGui::PopStyleVar();
                            ImGui::TableNextColumn();
                            ImGui::TableNextColumn();

                            // Draw ASCII column
                            if (this->m_showAscii) {
                                ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
                                if (ImGui::BeginTable("##ascii_cell", this->m_columnCount)) {
                                    ImGui::TableNextRow();

                                    for (u64 x = 0; x < bytesPerRow; x++) {
                                        ImGui::TableNextColumn();

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
        }
        ImGui::End();
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

}