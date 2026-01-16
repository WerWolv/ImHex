#include "imgui.h"
#include "fonts/fonts.hpp"
#include <ui/text_editor.hpp>
#include <hex/helpers/scaling.hpp>
#include <wolv/utils/string.hpp>
#include <algorithm>


namespace hex::ui {
    TextEditor::Palette s_paletteBase = TextEditor::getDarkPalette();
    using Keys = TextEditor::Keys;

    inline void TextUnformattedColored(const ImU32 &color, const char *text) {
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
    }

    inline void TextUnformattedColoredAt(const ImVec2 &pos, const ImU32 &color, const char *text) {
        ImGui::SetCursorScreenPos(pos);
        TextUnformattedColored(color, text);
    }

    void TextEditor::Line::print(i32 lineIndex, i32 maxLineIndex, std::optional<ImVec2> position) {
        u32 idx = 0;
        std::string lineNumberStr = std::to_string(lineIndex + 1);
        auto padding = std::string(std::to_string(maxLineIndex + 1).size() - lineNumberStr.size(),' ');
        lineNumberStr = " " + lineNumberStr + padding + " ";
        auto drawList = ImGui::GetCurrentWindow()->DrawList;
        drawList->AddRectFilled(ImGui::GetCursorScreenPos(),ImVec2(ImGui::GetCursorScreenPos().x + ImGui::CalcTextSize(lineNumberStr.c_str()).x,ImGui::GetCursorScreenPos().y + ImGui::GetTextLineHeightWithSpacing()),ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg]));
        drawList->AddLine(ImVec2(ImGui::GetCursorScreenPos().x + ImGui::CalcTextSize(lineNumberStr.c_str()).x,ImGui::GetCursorScreenPos().y),ImVec2(ImGui::GetCursorScreenPos().x + ImGui::CalcTextSize(lineNumberStr.c_str()).x,ImGui::GetCursorScreenPos().y + ImGui::GetTextLineHeightWithSpacing()),ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_Border]));
        if (position.has_value()) {
            TextUnformattedColoredAt(*position, TextEditor::m_palette[(i32) PaletteIndex::LineNumber], (lineNumberStr+" ").c_str());
            position->x += ImGui::CalcTextSize(lineNumberStr.c_str()).x;
        } else {
            TextUnformattedColored(TextEditor::m_palette[(i32) PaletteIndex::LineNumber], (lineNumberStr+ " ").c_str());
            ImGui::SameLine();
        }
        while (idx < m_chars.size()) {
            u8 color =  idx < m_colors.size() ? (u8) m_colors[idx] : 0;
            auto colorIdx = m_colors.find_first_not_of((char) color, idx);
            u32 wordSize;
            if (colorIdx == std::string::npos)
                 wordSize = m_colors.size() - idx;
            else
                wordSize = colorIdx - idx;
            if (position.has_value()) {
                TextUnformattedColoredAt(*position, TextEditor::m_palette[color], substr(idx, wordSize).c_str());
                position->x += ImGui::CalcTextSize(substr(idx, wordSize).c_str()).x;
            } else {
                TextUnformattedColored(TextEditor::m_palette[color], substr(idx, wordSize).c_str());
                ImGui::SameLine();
            }
            idx += wordSize;
            if (wordSize == 0 && !m_colorized)
                break;
            else if (wordSize == 0)
                idx++;
        }
    }

    void TextEditor::setTopMarginChanged(i32 newMargin) {
        m_newTopMargin = newMargin;
        m_topMarginChanged = true;
    }

    void TextEditor::clearErrorMarkers() {
        m_lines.clearErrorMarkers();
    }

    void TextEditor::Lines::clearErrorMarkers() {
        m_errorMarkers.clear();
        m_errorHoverBoxes.clear();
    }

    void TextEditor::Lines::clearCodeFolds() {
        m_codeFolds.clear();
        m_codeFoldKeys.clear();
    }

    void TextEditor::Lines::clearActionables() {
        clearErrorMarkers();
        clearGotoBoxes();
        clearCursorBoxes();
    }

    bool TextEditor::Lines::lineNeedsDelimiter(i32 lineIndex) {
        auto row = lineIndexToRow(lineIndex);
        if (row == -1.0 || !m_foldedLines.contains(row)) {
            if (lineIndex >= (i64)m_unfoldedLines.size() || lineIndex < 0)
                return false;
            auto line = m_unfoldedLines[lineIndex].m_chars;
            if (line.empty())
                return false;

            for (auto keys : m_codeFoldKeys) {
                if (keys.m_start.m_line == lineIndex && m_codeFoldDelimiters.contains(keys)) {
                      auto delimiter = m_codeFoldDelimiters[keys].first;
                      if (!delimiter || (delimiter != '(' && delimiter != '[' && delimiter != '{' && delimiter != '<'))
                            return false;
                      return !line.contains(delimiter);
                }
            }
            return !line.ends_with('{');
        }
        return m_foldedLines.at(row).firstLineNeedsDelimiter();
    }

    bool TextEditor::FoldedLine::firstLineNeedsDelimiter() {
        return ((u8)m_type & (u8)FoldedLine::FoldType::FirstLineNeedsDelimiter);
    }

    bool TextEditor::FoldedLine::addsLastLineToFold() {
        return (u8)m_type & (u8)FoldedLine::FoldType::AddsLastLine;
    }

    bool TextEditor::FoldedLine::addsFullFirstLineToFold() {
        return (u8)m_type & (u8)FoldedLine::FoldType::AddsFirstLine;
    }

    ImVec2 TextEditor::underWavesAt(ImVec2 pos, i32 nChars, ImColor color, const ImVec2 &size_arg) {
        ImGui::GetStyle().AntiAliasedLines = false;
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        window->DC.CursorPos = pos;
        const ImVec2 label_size = ImGui::CalcTextSize("W", nullptr, true);
        ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x, label_size.y);
        float lineWidth = size.x / 3.0f + 0.5f;
        constexpr i32 segmentEndCount = 4;
        constexpr i32 segmentCount = segmentEndCount - 1;
        constexpr float signMultiplier = -1.0f;
        ImVec2 segment[segmentEndCount];

        for (i32 i = 0; i < nChars; i++) {
            pos = window->DC.CursorPos;
            float lineY = pos.y + size.y;
            auto sign = signMultiplier;
            for (i32 j = 0; j < segmentEndCount; j++) {
                sign *= signMultiplier;
                segment[j] = ImVec2(pos.x + j * lineWidth, lineY + sign * lineWidth / 2.0f);
            }

            for (i32 j = 0; j < segmentCount; j++) {
                ImGui::GetWindowDrawList()->AddLine(segment[j], segment[j + 1], ImU32(color), 0.4f);
            }

            window->DC.CursorPos = ImVec2(pos.x + size.x, pos.y);
        }
        auto result = window->DC.CursorPos;
        result.y += size.y;
        return result;
    }

    void TextEditor::setTabSize(i32 value) {
        m_tabSize = std::max(0, std::min(32, value));
    }

    float TextEditor::getPageSize() const {
        return ImGui::GetCurrentWindow()->InnerClipRect.GetHeight() / m_lines.m_charAdvance.y;
    }

    bool TextEditor::Lines::isEndOfLine()  {
        return isEndOfLine(m_state.m_cursorPosition);
    }

    bool TextEditor::Lines::isStartOfLine() const {
        return m_state.m_cursorPosition.m_column == 0;
    }

    bool TextEditor::Line::isEndOfLine(i32 column) {
        return column >= maxColumn();
    }

    bool TextEditor::Lines::isEndOfLine(const Coordinates &coordinates) {
        if (coordinates.m_line < size())
            return m_unfoldedLines[coordinates.m_line].isEndOfLine(coordinates.m_column);

        return true;
    }

    bool TextEditor::Lines::isEndOfFile(const Coordinates &coordinates) {
        if (coordinates.m_line < size())
            return isLastLine(coordinates.m_line) && isEndOfLine(coordinates);

        return true;
    }

    bool TextEditor::Lines::isLastLine() {
        return isLastLine(m_state.m_cursorPosition.m_line);
    }

    bool TextEditor::Lines::isLastLine(i32 lineIndex) {
        auto row = lineIndexToRow(lineIndex);
        return row == getMaxDisplayedRow();
    }

    void TextEditor::Lines::setFirstRow() {
        if (!m_withinRender) {
            m_setTopRow = true;
            return;
        } else {
            m_setTopRow = false;
            ImGui::SetScrollY(m_topRow * m_charAdvance.y);
        }
    }

    float TextEditor::Lines::getMaxDisplayedRow() {
        auto maxRow = getGlobalRowMax();
        if (maxRow - m_topRow < m_numberOfLinesDisplayed)
            return maxRow;
        return m_topRow + m_numberOfLinesDisplayed;
    }

    float TextEditor::Lines::getGlobalRowMax() {
        float maxRow = size() - 1.0f;
        if (m_codeFoldsDisabled || m_foldedLines.empty() || m_codeFoldKeys.empty())
            return std::floor(maxRow);

        if (m_globalRowMaxChanged) {
            Keys spanningIntervals;

            for (auto key1 = m_codeFoldKeys.begin(); key1 != m_codeFoldKeys.end(); ++key1) {
                auto key2 = std::next(key1);
                while (key2 != m_codeFoldKeys.end() && (!key2->contains(*key1) || (m_codeFoldState.contains(*key2) && m_codeFoldState[*key2]))) {
                    ++key2;
                }
                if (key2 == m_codeFoldKeys.end())
                    spanningIntervals.push_back(*key1);
            }

            for (auto key: spanningIntervals) {
                if (m_codeFoldState.contains(key) && !m_codeFoldState[key]) {
                    if ((key.m_end.m_line - key.m_start.m_line - 1) < maxRow) {
                        maxRow -= ((key.m_end.m_line - key.m_start.m_line));
                    } else
                        break;
                } else
                    m_codeFoldState[key] = true;
            }
            m_cachedGlobalRowMax = std::floor(maxRow);
            m_globalRowMaxChanged = false;
        }
        return m_cachedGlobalRowMax;
    }

    float TextEditor::getMaxLineNumber() {
        float maxLineNumber = std::min(m_lines.size() - 2.0f, m_lines.m_topRow + m_lines.m_numberOfLinesDisplayed);

        if (maxLineNumber == m_lines.size() - 2.0f || m_lines.m_codeFoldKeys.empty())
            return std::floor(maxLineNumber + 1.0f);
        float currentLineNumber = m_topLineNumber;
        for (auto range: m_lines.m_codeFoldKeys) {

            if (m_lines.m_codeFoldState.contains(range) && !m_lines.m_codeFoldState[range]) {

                if ((range.m_start.m_line - currentLineNumber) < maxLineNumber)
                    maxLineNumber += ((range.m_end.m_line - range.m_start.m_line));
                else
                    break;
            } else
                m_lines.m_codeFoldState[range] = true;
           currentLineNumber = range.m_end.m_line + 1;
        }
        return maxLineNumber + 1.0f;
    }

    float TextEditor::getTopLineNumber() {
        float result = m_lines.m_topRow;
        for (auto interval: m_lines.m_codeFoldKeys) {

            if (interval.m_start.m_line > result)
                break;
            if (m_lines.m_codeFoldState.contains(interval) && !m_lines.m_codeFoldState[interval])
                result += (interval.m_end.m_line- interval.m_start.m_line);
            else
                m_lines.m_codeFoldState[interval] = true;
        }
        return result;
    }

    void TextEditor::render(const char *title, const ImVec2 &size, bool border) {
        m_lines.m_title = title;
        if (m_lines.m_unfoldedLines.capacity() < 2u * m_lines.size())
            m_lines.m_unfoldedLines.reserve(2 * m_lines.size());

        auto scrollBg = ImGui::GetStyleColorVec4(ImGuiCol_ScrollbarBg);
        scrollBg.w = 0.0f;
        auto scrollBarSize = ImGui::GetStyle().ScrollbarSize;

        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImGui::ColorConvertFloat4ToU32(scrollBg));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, scrollBarSize);

        m_lines.m_lineNumbersStartPos = ImGui::GetCursorScreenPos();
        if (m_showLineNumbers) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg]);
            std::string lineNumberStr = std::to_string(m_lines.size()) + " ";
            m_lines.m_lineNumberFieldWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, lineNumberStr.c_str(),nullptr, nullptr).x + 2 * m_lines.m_charAdvance.x;
            ImGui::SetNextWindowPos(m_lines.m_lineNumbersStartPos);
            ImGui::SetCursorScreenPos(m_lines.m_lineNumbersStartPos);
            auto lineNumberSize = ImVec2(m_lines.m_lineNumberFieldWidth, size.y);
            if (!m_lines.m_ignoreImGuiChild) {
                ImGui::BeginChild("##lineNumbers", lineNumberSize, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                ImGui::EndChild();
            }
            ImGui::PopStyleColor();
        } else {
            m_lines.m_lineNumberFieldWidth = 0;
        }

        ImVec2 textEditorSize = size;
        textEditorSize.x -= m_lines.m_lineNumberFieldWidth;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(m_palette[(i32) PaletteIndex::Background]));
        bool scroll_x = m_longestDrawnLineLength * m_lines.m_charAdvance.x >= textEditorSize.x;
        bool scroll_y = m_lines.size() > 1;
        ImGui::SetCursorScreenPos(ImVec2(m_lines.m_lineNumbersStartPos.x + m_lines.m_lineNumberFieldWidth, m_lines.m_lineNumbersStartPos.y));
        ImGuiChildFlags childFlags = border ? ImGuiChildFlags_Borders : ImGuiChildFlags_None;
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove;
        if (!m_lines.m_ignoreImGuiChild)
            ImGui::BeginChild(title, textEditorSize, childFlags, windowFlags);
        auto window = ImGui::GetCurrentWindow();
        window->ScrollbarSizes = ImVec2(scrollBarSize * scroll_x, scrollBarSize * scroll_y);
        ImGui::GetCurrentWindowRead()->ScrollbarSizes = ImVec2(scrollBarSize * scroll_y, scrollBarSize * scroll_x);
        if (scroll_y) {
            ImGui::GetCurrentWindow()->ScrollbarY = true;
            ImGui::Scrollbar(ImGuiAxis_Y);
            ImGui::GetCurrentWindow()->ScrollbarY = false;
        }
        if (scroll_x) {
            ImGui::GetCurrentWindow()->ScrollbarX = true;
            ImGui::Scrollbar(ImGuiAxis_X);
            ImGui::GetCurrentWindow()->ScrollbarX = false;
        }
        ImGui::PopStyleColor();
        if (m_handleKeyboardInputs) {
            handleKeyboardInputs();
        }

        if (m_handleMouseInputs)
            handleMouseInputs();

        m_lines.colorizeInternal();
        renderText(textEditorSize);

        if (!m_lines.m_ignoreImGuiChild)
            ImGui::EndChild();

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(m_lines.m_lineNumbersStartPos.x, m_lines.m_lineNumbersStartPos.y + size.y - 1));
        ImGui::Dummy({});
    }

    void TextEditor::Lines::ensureSelectionNotFolded() {
        auto selectionStart = m_state.m_selection.m_start;
        auto selectionEnd = m_state.m_selection.m_end;
        auto foldedSelectionStart = unfoldedToFoldedCoords(selectionStart);
        auto foldedSelectionEnd = unfoldedToFoldedCoords(selectionEnd);
        for (auto &foldedLine: m_foldedLines) {
            auto keyCount = foldedLine.second.m_keys.size();
            for (u32 i = 0; i < keyCount; i++) {
                auto ellipsisIndex = foldedLine.second.m_ellipsisIndices[i];
                Range ellipsisRange = Range(
                    Coordinates(rowToLineIndex(foldedLine.first), ellipsisIndex),
                    Coordinates(rowToLineIndex(foldedLine.first), ellipsisIndex + 3)
                );
                if (Range(foldedSelectionStart,foldedSelectionEnd).overlaps(ellipsisRange)) {
                    openCodeFold(foldedLine.second.m_keys[i]);
                }
            }
        }
    }

    void TextEditor::Lines::ensureCursorVisible() {
        auto pos = lineCoordinates(m_state.m_cursorPosition);
        auto row = lineIndexToRow(pos.m_line);

        if (m_unfoldIfNeeded && m_foldedLines.contains(row) && !m_codeFoldsDisabled) {
            auto foldedLine = m_foldedLines.at(row);
            auto foldedCoords = unfoldedToFoldedCoords(pos);
            auto keyCount = foldedLine.m_keys.size();
            for (u32 i = 0; i < keyCount; i++) {
                if (foldedCoords.m_column >= foldedLine.m_ellipsisIndices[i] && foldedCoords.m_column <= foldedLine.m_ellipsisIndices[i] + 3) {
                    openCodeFold(m_foldedLines.at(row).m_keys[i]);
                    break;
                }
            }
        }
        m_unfoldIfNeeded = false;


        if (!m_withinRender) {
            m_scrollToCursor = true;
            return;
        }

        auto scrollBarSize = ImGui::GetStyle().ScrollbarSize;
        float scrollX = ImGui::GetScrollX();
        float scrollY = ImGui::GetScrollY();

        auto windowPadding = ImGui::GetStyle().FramePadding * 2.0f;

        auto height = ImGui::GetWindowHeight() - m_topMargin - scrollBarSize;
        auto width = ImGui::GetWindowWidth() - windowPadding.x - scrollBarSize;

        auto top = m_topMargin > scrollY ? m_topMargin - scrollY : scrollY;
        auto topRow = (i32) rint(top / m_charAdvance.y);
        auto bottomRow = (i32) rint((top + height) / m_charAdvance.y);

        auto leftColumnIndex = (i32) rint(scrollX / m_charAdvance.x);
        auto rightColumnIndex = (i32) rint((scrollX + width) / m_charAdvance.x);

        pos = lineCoordinates(unfoldedToFoldedCoords(m_state.m_cursorPosition));

        auto posColumnIndex = (i32) rint(textDistanceToLineStart(pos) / m_charAdvance.x);
        auto posRow = lineIndexToRow(pos.m_line);
        bool scrollToCursorX = true;
        bool scrollToCursorY = true;

        if ((posRow > topRow && posRow < bottomRow) ||
            (posRow == topRow && topRow == top  && scrollY == ImGui::GetScrollMaxY()))
            scrollToCursorY = false;

        if ((posColumnIndex >= leftColumnIndex) && (posColumnIndex <= rightColumnIndex))
            scrollToCursorX = false;

        if ((!scrollToCursorX && !scrollToCursorY && m_oldTopMargin == m_topMargin) || pos.m_line < 0) {
            m_scrollToCursor = false;
            return;
        }

        if (scrollToCursorY) {
            if (posRow <= topRow) {
                if (posRow <= 0) {
                    ImGui::SetScrollY(0.0f);
                    m_scrollToCursor = false;
                    return;
                }
                ImGui::SetScrollY((posRow - 1) * m_charAdvance.y);
                m_scrollToCursor = true;
            }
            if (posRow >= bottomRow) {
                ImGui::SetScrollY((posRow + 1) * m_charAdvance.y - height);
                m_scrollToCursor = true;
            }
        }
        if (scrollToCursorX) {
            if (posColumnIndex < leftColumnIndex) {
                ImGui::SetScrollX(std::max(0.0f, posColumnIndex * m_charAdvance.x));
                m_scrollToCursor = true;
            }
            if (posColumnIndex > rightColumnIndex) {
                ImGui::SetScrollX(std::max(0.0f, posColumnIndex * m_charAdvance.x - width));
                m_scrollToCursor = true;
            }
        }
        m_oldTopMargin = m_topMargin;
    }

    float TextEditor::screenPosToRow(const ImVec2 &position) const {
        if (position.y < m_lines.m_cursorScreenPosition.y + m_lines.m_topMargin)
            return -1.0f;
        return (position.y - m_lines.m_cursorScreenPosition.y - m_lines.m_topMargin) / m_lines.m_charAdvance.y;
    }

    float TextEditor::rowToLineIndex(i32 row) {
        return m_lines.rowToLineIndex(row);
    }

    float TextEditor::lineIndexToRow(i32 lineIndex) {
        return m_lines.lineIndexToRow(lineIndex);
    }

    float TextEditor::Lines::rowToLineIndex(i32 row) {
        if (m_codeFoldsDisabled || m_foldedLines.empty() || m_rowToLineIndex.empty())
            return row;
        if (m_rowToLineIndex.contains(row))
            return m_rowToLineIndex.at(row);
        return -1.0f;
    }

    float TextEditor::Lines::lineIndexToRow(i32 lineIndex) {
        if (m_codeFoldsDisabled || m_foldedLines.empty() || m_lineIndexToRow.empty())
            return lineIndex;
        if (m_lineIndexToRow.contains(lineIndex))
            return m_lineIndexToRow.at(lineIndex);
        return -1.0f;
    }

    void TextEditor::Lines::resetCursorBlinkTime() {
        m_startTime = ImGui::GetTime() * 1000 - s_cursorBlinkOnTime;
    }

    bool TextEditor::CodeFold::trigger() {
        lines->m_codeFoldHighlighted = NoCodeFoldSelected;
       if (!isOpen() && startHovered()) {
            lines->m_codeFoldHighlighted = key;
            codeFoldStartCursorBox.callback();
       } else {
           auto &rowFoldSymbols = lines->m_rowToFoldSymbol;
           auto row = lines->lineIndexToRow(key.m_end.m_line);
           if (isOpen() && endHovered()  && rowFoldSymbols.contains(row) && rowFoldSymbols[row] != FoldSymbol::Square) {
               lines->m_codeFoldHighlighted = key;
               codeFoldEndCursorBox.callback();
           }
           row = lines->lineIndexToRow(key.m_start.m_line);
           if (startHovered() && rowFoldSymbols.contains(row) && rowFoldSymbols[row] != FoldSymbol::Square) {
               lines->m_codeFoldHighlighted = key;
               codeFoldStartCursorBox.callback();
           }
       }

        bool result = TextEditor::ActionableBox::trigger();
        if (isOpen())
            result = result || codeFoldEndActionBox.trigger();
        bool clicked = ImGui::IsMouseClicked(0);
        result = result && clicked;
        return result;
    }

    ImVec2 TextEditor::coordsToScreen(Coordinates coordinates) {
        return m_lines.foldedCoordsToScreen(coordinates);
    }

    ImVec2 TextEditor::Lines::foldedCoordsToScreen(Coordinates coordinates) {
        ImVec2 lineStartScreenPos = getLineStartScreenPos(0, lineIndexToRow(coordinates.m_line));
        auto line = operator[](coordinates.m_line);
        auto text = line.m_chars.substr(0, line.indexColumn(coordinates.m_column));
        return lineStartScreenPos + ImVec2(line.stringTextSize(text), 0);
    }

    void TextEditor::Lines::initializeCodeFolds() {

        m_codeFoldKeyMap.clear();
        m_codeFoldKeyLineMap.clear();
        m_codeFoldValueMap.clear();
        m_codeFoldValueLineMap.clear();
        m_codeFolds.clear();
        m_rowToFoldSymbol.clear();

        for (auto [key, isOpen]: m_codeFoldState){
            auto index = m_codeFoldKeys.find(key);
            if (index->m_start != key.m_start || index->m_end != key.m_end)
                m_codeFoldState.erase(key);
        }

        for (auto key : m_codeFoldKeys) {
            if (key.m_start >= key.m_end) {
                m_codeFoldKeys.erase(key);
                continue;
            }
            auto rowStart = lineIndexToRow(key.m_start.m_line);
            auto rowEnd = lineIndexToRow(key.m_end.m_line);

            if (!m_rowToFoldSymbol.contains(rowStart))
                m_rowToFoldSymbol[rowStart] = FoldSymbol::Down;
            m_rowToFoldSymbol[rowEnd] = FoldSymbol::Up;
            const ImRect &rect1 = getBoxForRow(rowStart);
            const ImRect &rect2 = getBoxForRow(rowEnd);

            CodeFold fold = CodeFold(this, key, rect1, rect2);
            m_codeFolds.insert({key,fold});

            auto index = m_codeFoldKeys.find(key);
            if (index->m_start != key.m_start || index->m_end != key.m_end)
                m_codeFoldState[key] = true;

            if (!m_codeFoldKeyMap.contains(key.m_start))
                m_codeFoldKeyMap[key.m_start] = key.m_end;

            if (!m_codeFoldValueMap.contains(key.m_end))
                m_codeFoldValueMap[key.m_end] = key.m_start;
            m_codeFoldKeyLineMap.insert(std::make_pair(key.m_start.m_line, key.m_start));
            m_codeFoldValueLineMap.insert(std::make_pair(key.m_end.m_line, key.m_end));
        }

        for (auto &[row, foldedLine]: m_foldedLines) {
            for (auto key = foldedLine.m_keys.begin(); key != foldedLine.m_keys.end(); key++) {
                auto index = m_codeFoldKeys.find(*key);
                if (index->m_start != key->m_start || index->m_end != key->m_end)
                    foldedLine.m_keysToRemove.push_back(*key);
            }
        }

        m_lineIndexToScreen.clear();
        m_leadingLineSpaces.clear();
        m_leadingLineSpaces.resize(size(), 0);
        for (i32 i = 0; i < size(); i++) {
            m_lineIndexToScreen[i] = foldedCoordsToScreen(lineCoordinates(i, 0));
            m_leadingLineSpaces[i] = skipSpaces(lineCoordinates( i, 0));
        }


        if (m_useSavedFoldStatesRequested) {
            applyCodeFoldStates();
            m_useSavedFoldStatesRequested = false;
        } else if (m_saveCodeFoldStateRequested) {
            saveCodeFoldStates();
            m_saveCodeFoldStateRequested = false;
        }

        m_foldedLines.clear();
        Keys closedFolds = removeEmbeddedFolds();
        for (auto closedFold : closedFolds) {
            closeCodeFold(closedFold, false);
            auto row = lineIndexToRow(closedFold.m_start.m_line);
            m_rowToFoldSymbol[row] = FoldSymbol::Square;
        }

        removeKeys();
        m_initializedCodeFolds = true;
    }

    void TextEditor::Lines::setRowToLineIndexMap() {
        m_rowToLineIndex.clear();
        m_lineIndexToRow.clear();

        i32 lineIndex = 0;
        auto maxRow = getGlobalRowMax();
        if (maxRow < 0)
            return;

        for (u32 i = 0; i <= maxRow; i++) {

            if (m_codeFoldKeyLineMap.contains(lineIndex)) {
                auto values = m_codeFoldKeyLineMap.equal_range(lineIndex);
                for (auto value = values.first; value != values.second; value++) {
                    Range key(value->second, m_codeFoldKeyMap[value->second]);

                    auto newKey = key;

                    while (m_codeFolds.contains(newKey) && !m_codeFolds[newKey].isOpen() && m_codeFoldKeyLineMap.contains(newKey.m_end.m_line)) {
                        auto endLine = newKey.m_end.m_line;

                        auto range = m_codeFoldKeyLineMap.equal_range(endLine);
                        bool found = false;
                        for (auto it = range.first; it != range.second; ++it) {
                            Range testKey(it->second, m_codeFoldKeyMap[it->second]);
                            if (m_codeFolds.contains(testKey) && !m_codeFolds[testKey].isOpen()) {
                                newKey = testKey;
                                found = true;
                                break;
                            }
                        }
                        if (!found)
                            break;
                    }
                    if (!m_rowToLineIndex.contains(i)) {
                        m_rowToLineIndex[i] = key.m_start.m_line;
                        m_lineIndexToRow[key.m_start.m_line] = i;
                    }
                    if (!m_codeFolds[key].isOpen())
                        lineIndex = newKey.m_end.m_line;
                }
            } else {
                if (!m_rowToLineIndex.contains(i)) {
                    m_rowToLineIndex[i] = lineIndex;
                    m_lineIndexToRow[lineIndex] = i;
                }
            }
            lineIndex += 1;
        }
        for (lineIndex = 1; lineIndex < size(); lineIndex++) {
           if (!m_lineIndexToRow.contains(lineIndex))
               m_lineIndexToRow[lineIndex] = m_lineIndexToRow[lineIndex - 1];
        }
    }

    bool TextEditor::Lines::updateCodeFolds() {

        bool triggered = false;
        std::map<Range, CodeFold> detectedFolds;
        for (auto key : std::ranges::views::reverse(m_codeFoldKeys)) {
            if (m_codeFolds[key].trigger())
                triggered = true;
            if (m_codeFolds[key].isDetected())
                detectedFolds.insert({key, m_codeFolds[key]});
        }
        if (detectedFolds.empty()) {
            m_codeFoldHighlighted = NoCodeFoldSelected;
            return false;
        }
        auto detectedFoldRIter = detectedFolds.rbegin();
        while (detectedFoldRIter != detectedFolds.rend()) {
            if (!detectedFoldRIter->second.isOpen()) {
                m_codeFoldHighlighted = detectedFoldRIter->first;
                if (triggered) {
                    detectedFoldRIter->second.callback();
                    return true;
                }
            }
            detectedFoldRIter++;
        }
        auto detectedFoldIter = detectedFolds.begin();
        m_codeFoldHighlighted = detectedFoldIter->first;
        if (triggered) {
            detectedFoldIter->second.callback();
            return true;
        }
        return false;
    }

    Keys TextEditor::Lines::removeEmbeddedFolds() {
        Keys closedFolds;
        for (auto key : m_codeFoldKeys) {
            if (m_codeFoldState.contains(key) && !m_codeFoldState[key]) {
                bool replace = false;
                Keys keysToErase;
                for (auto closedFold : closedFolds) {
                    if (key.contains(closedFold)) {
                        replace = true;
                        keysToErase.push_back(closedFold);
                    }
                }
                if (replace) {
                    closedFolds.erase(std::remove_if(closedFolds.begin(), closedFolds.end(),[&keysToErase](
                            const Range &interval
                    ) {
                        return std::find(keysToErase.begin(), keysToErase.end(), interval) != keysToErase.end();
                    }), closedFolds.end());
                }
                closedFolds.push_back(key);
                keysToErase.clear();
            }
        }
        return closedFolds;
    }

    void TextEditor::Lines::getRowSegments() {
        m_rowToFoldSegments.clear();
        m_multiLinesToRow.clear();
        m_rowCodeFoldTooltips.clear();
        Keys closedFolds = removeEmbeddedFolds();

        for (auto key : closedFolds) {
            auto row = lineIndexToRow(key.m_start.m_line);
            if (m_rowToFoldSegments.contains(row) || !m_foldedLines.contains(row))
                continue;
            for (i32 i = key.m_start.m_line; i < key.m_end.m_line; i++)
                m_multiLinesToRow[i] = row;
            auto lineIndex = rowToLineIndex(row);
            auto foldedLine = m_foldedLines.at(row);
            i32 count = foldedLine.m_keys.size();
            if (count == 0)
                continue;
            for (i32 i = 0; i < count; i++) {
                Interval sgm = {foldedLine.m_foldedSegments[2 * i].m_column, foldedLine.m_foldedSegments[2 * i + 1].m_column};
                m_rowToFoldSegments[row].push_back({foldedLine.m_keys[i].m_start, indexScreenPosition(lineIndex, sgm)});
                ImVec2 screenPosEnd = indexCoordsToScreen(lineCoordinates(lineIndex, foldedLine.m_ellipsisIndices[i]));
                m_rowCodeFoldTooltips[row].emplace_back(this, foldedLine.m_keys[i],ImRect(screenPosEnd, screenPosEnd + ImVec2(Ellipsis.lineTextSize(), m_charAdvance.y)));
            }
            Interval sgm = {foldedLine.m_foldedSegments[2 * count].m_column, foldedLine.m_foldedSegments[2 * count + 1].m_column};
            m_rowToFoldSegments[row].push_back({foldedLine.m_keys[count - 1].m_end, indexScreenPosition(lineIndex, sgm)});
        }
    }

    TextEditor::Interval TextEditor::Lines::indexScreenPosition(i32 lineIndex, Interval stringIndices) {
        return Interval( lineIndexToScreen(lineIndex, stringIndices));
    }

     ImVec2 TextEditor::Line::intervalToScreen(Interval stringIndices) const {
        return {(float) textSize(stringIndices.m_start), (float) textSize(stringIndices.m_end)};
    }

    ImVec2 TextEditor::Lines::lineIndexToScreen(i32 lineIndex, Interval stringIndices) {
        auto &line = operator[](lineIndex);
        auto startPos = m_lineIndexToScreen[lineIndex].x;
        auto increments = line.intervalToScreen(stringIndices);
        return {startPos + increments.x, startPos + increments.y};
    }

    ImVec2 TextEditor::Lines::indexCoordsToScreen(Coordinates indexCoords) {
       return foldedCoordsToScreen(lineIndexCoords(indexCoords.m_line + 1, indexCoords.m_column));
    }

    bool TextEditor::CodeFoldTooltip::trigger() {
        bool hovered = ActionableBox::trigger();

        if (hovered && ImGui::IsMouseClicked(0)) {
            auto codeFoldKeys = m_lines->m_codeFoldKeys;
            auto codeFoldState = m_lines->m_codeFoldState;
            Keys keysToOpen;
            auto key = codeFoldKeys.begin();
            for (; key != codeFoldKeys.end(); key++) {
                if (*key == m_key)
                    break;
            }
            keysToOpen.push_back(*key);
            auto prevKey = key;
            key++;
            while (key->m_start == prevKey->m_end && codeFoldState.contains(*key) && !codeFoldState[*key]) {
                keysToOpen.push_back(*key);
                prevKey = key;
                key++;
            }
            while (key != codeFoldKeys.end()) {
                if (key->contains(m_key) && codeFoldState.contains(*key) && !codeFoldState[*key])
                    keysToOpen.push_back(*key);
                key++;
            }
            for (auto openKey : std::ranges::views::reverse(keysToOpen))
                m_lines->openCodeFold(openKey);

            return true;
        }

        return hovered;
    }

    void TextEditor::CodeFoldTooltip::callback()  {
        ImGui::BeginChild("##lineNumbers");
        ImGui::BeginTooltip();
        i32 textWindowWidth = (m_lines->operator[](m_key.m_start.m_line)).lineTextSize();
        for (auto lineIndex = m_key.m_start.m_line + 1; lineIndex <= m_key.m_end.m_line; lineIndex++) {
            textWindowWidth = std::max(textWindowWidth,(m_lines->operator[](lineIndex)).lineTextSize());
        }

        auto textEditorSize = ImVec2(textWindowWidth + m_lines->m_lineNumberFieldWidth, (m_key.m_end.m_line - m_key.m_start.m_line + 1) * m_lines->m_charAdvance.y);

        if (!m_lines->m_ignoreImGuiChild) {
            std::string textTitle = m_lines->m_title + popupText;
            ImGui::BeginChild(textTitle.c_str(), textEditorSize, false, ImGuiWindowFlags_NoScrollbar);
        }
        m_lines->printCodeFold(m_key);
        ImGui::EndChild();
        ImGui::EndTooltip();
        ImGui::EndChild();
    }

    void TextEditor::Lines::printCodeFold(const Range &key) {
        auto maxLineIndex = key.m_end.m_line;
        auto lineIndex = key.m_start.m_line;
        m_unfoldedLines[lineIndex].print(lineIndex, maxLineIndex);
        ImGui::NewLine();
        lineIndex = std::floor(lineIndex + 1.0F);
        while (lineIndex <= maxLineIndex) {
            m_unfoldedLines[lineIndex].print(lineIndex, maxLineIndex);
            ImGui::NewLine();
            lineIndex = std::floor(lineIndex + 1.0F);
        }
    }

    TextEditor::FoldedLine::FoldedLine(Lines *lines) : m_lines(lines) {
        m_row = -1;
        m_full = Range(Invalid,Invalid);
        m_cursorPosition = Invalid;
        m_selection = Range(Invalid, Invalid);
        m_ellipsisIndices.clear();
        m_keys.clear();
        m_built = false;
        m_foldedLine = Line();
    }

    template<typename T> T operator+(const T &lhs, const T &rhs) {
        return (T)((i64) lhs + (i64)rhs);
    }

    void TextEditor::FoldedLine::insertKey(const Range& key) {
        m_type = (FoldType)0;
        std::pair<char,char> delimiters = {' ',' '};
        if (m_lines->m_codeFoldDelimiters.contains(key))
            delimiters = m_lines->m_codeFoldDelimiters[key];
        Line lineStart = m_lines->m_unfoldedLines[key.m_start.m_line];
        Line lineEnd = m_lines->m_unfoldedLines[key.m_end.m_line];

        std::string lineStartFirstNonSpace = lineStart.m_chars.substr(m_lines->m_leadingLineSpaces[key.m_start.m_line], 2);
        bool isSingleLineComment = lineStartFirstNonSpace == "//";

        bool isIfDef = false;
        if (key.m_start.m_line > 0 && key.m_end.m_line < m_lines->size()) {
            std::string prevLine = m_lines->m_unfoldedLines[key.m_start.m_line - 1].m_chars;
            if (prevLine.starts_with("#ifdef") || prevLine.starts_with("#ifndef"))
                isIfDef = true;
        }

        bool appendClosingLine = true;
        Line bracket;
        Range delimiterCoordinates = findDelimiterCoordinates(key);
        if (delimiterCoordinates.m_start == Invalid || delimiterCoordinates.m_end == Invalid)
            return;

        if (m_row == -1) {
            m_keys.push_back(key);
            m_full = key;

            if (lineStart.m_chars.starts_with("import")) {
                appendClosingLine = false;
                m_foldedLine = lineStart.subLine(0, 7);
                m_type = FoldType::AddsFirstLine;
            } else if (lineStart.m_chars.starts_with("#include")) {
                appendClosingLine = false;
                m_foldedLine = lineStart.subLine(0, 9);
                m_type = FoldType::AddsFirstLine;
            } else if (isSingleLineComment) {
                appendClosingLine = false;
                m_foldedLine = lineStart.subLine(m_lines->m_leadingLineSpaces[key.m_start.m_line], 1);
                m_type = FoldType::AddsFirstLine;
            }else if (isIfDef) {
                appendClosingLine = false;
                m_type = FoldType::NoDelimiters;
            } else {
                m_type = FoldType::AddsFirstLine + FoldType::HasOpenDelimiter + FoldType::AddsLastLine + FoldType::HasCloseDelimiter;
                if (delimiterCoordinates.m_start.m_line == key.m_start.m_line) {
                    m_foldedLine = lineStart.subLine(0, delimiterCoordinates.m_start.m_column + 1).trim(TrimMode::TrimEnd);
                } else {
                    m_foldedLine = lineStart.trim(TrimMode::TrimEnd);
                    bracket = m_lines->m_unfoldedLines[delimiterCoordinates.m_start.m_line].subLine(delimiterCoordinates.m_start.m_column, 1);
                    m_type = m_type + FoldType::FirstLineNeedsDelimiter;
                    m_foldedLine.append(" ");
                    m_foldedLine.append(bracket);
                }
            }

            i32 newIndex = m_foldedLine.size();
            auto const newPos = std::lower_bound(m_ellipsisIndices.begin(), m_ellipsisIndices.end(), newIndex);
            m_ellipsisIndices.insert(newPos, newIndex);
            m_foldedLine.append(Ellipsis);
            if (appendClosingLine) {
                if (delimiterCoordinates.m_end.m_line == key.m_end.m_line) {
                    auto line = lineEnd.subLine(delimiterCoordinates.m_end.m_column, -1).trim(TrimMode::TrimBoth);
                    m_foldedLine.append(line);
                } else {
                    auto line = lineEnd.trim(TrimMode::TrimBoth);
                    m_foldedLine.append(line);
                }
            }
        } else if (key.m_end.m_line == m_full.m_start.m_line) {
            Line line = lineStart.trim(TrimMode::TrimEnd);
            m_type = FoldType::AddsFirstLine + FoldType::HasOpenDelimiter + FoldType::AddsLastLine + FoldType::HasCloseDelimiter;
            if (delimiterCoordinates.m_start.m_line != key.m_start.m_line) {
                bracket = m_lines->m_unfoldedLines[delimiterCoordinates.m_start.m_line].subLine(delimiterCoordinates.m_start.m_column, 1);
                m_type = m_type + FoldType::FirstLineNeedsDelimiter;
                line.append(" ");
                line.append(bracket);
            }

            i32 newIndex = line.size();
            std::transform(m_ellipsisIndices.begin(), m_ellipsisIndices.end(), m_ellipsisIndices.begin(), [newIndex, this]
                (i32 index) {
                    return index + newIndex + 3 - m_lines->m_leadingLineSpaces[m_row];
                }
            );
            auto const newPos = std::lower_bound(m_ellipsisIndices.begin(), m_ellipsisIndices.end(), newIndex);
            m_ellipsisIndices.insert(newPos, newIndex);
            line.append(Ellipsis);
            auto trimmedLine = m_foldedLine.trim(TrimMode::TrimBoth);
            line.append(trimmedLine);
            m_foldedLine = line;
            m_keys.insert(m_keys.begin(), key);
            m_full.m_start = key.m_start;
        } else if (key.m_start.m_line == m_full.m_end.m_line) {
            m_type = FoldType::AddsFirstLine + FoldType::HasOpenDelimiter + FoldType::AddsLastLine + FoldType::HasCloseDelimiter;
            if (lineStart.size() > (u32) (delimiterCoordinates.m_start.m_column + 1)) {
                u32 extra = lineStart.size() - (delimiterCoordinates.m_start.m_column + 1);
                m_foldedLine = m_foldedLine.subLine(0, m_foldedLine.size() - extra);
            }
            i32 newIndex = m_foldedLine.size();
            if (delimiterCoordinates.m_start.m_line != key.m_start.m_line) {
                bracket = m_lines->m_unfoldedLines[delimiterCoordinates.m_start.m_line].subLine(delimiterCoordinates.m_start.m_column, 1);
                m_type = m_type + FoldType::FirstLineNeedsDelimiter;
                m_foldedLine.append(" ");
                newIndex += 1;
                m_foldedLine.append(bracket);
                newIndex += bracket.size();
            }
            auto const newPos = std::lower_bound(m_ellipsisIndices.begin(), m_ellipsisIndices.end(), newIndex);
            m_ellipsisIndices.insert(newPos, newIndex);
            m_foldedLine.append(Ellipsis);
            auto line = lineEnd.subLine(delimiterCoordinates.m_end.m_column, -1).trim(TrimMode::TrimBoth);
            m_foldedLine.append(line);
            m_keys.push_back(key);
            m_full.m_end = key.m_end;
        }
        m_row = m_lines->lineIndexToRow(key.m_start.m_line);
        m_built = appendClosingLine;
    }

    TextEditor::Range TextEditor::FoldedLine::findDelimiterCoordinates(Range key) {
        std::pair<char,char> delimiterPair = {' ',' '};
        if (m_lines->m_codeFoldDelimiters.contains(key))
            delimiterPair = m_lines->m_codeFoldDelimiters[key];
        std::string delimiters;
        delimiters += delimiterPair.first;
        delimiters += delimiterPair.second;
        if (delimiters.empty() || (delimiters != "{}" && delimiters != "[]" && delimiters != "()"  && delimiters != "<>")) {
            auto lineStart = m_lines->m_unfoldedLines[key.m_start.m_line].m_chars;
            if (lineStart.starts_with("import") || lineStart.starts_with("#include")) {
                auto lineEnd = m_lines->m_unfoldedLines[key.m_end.m_line];
                auto columnStart = lineStart.find(' ');
                return {m_lines->lineCoordinates(key.m_start.m_line, columnStart), m_lines->lineCoordinates(key.m_end.m_line, lineEnd.maxColumn())};
            }
            return key;
        }
        if (delimiters.size() < 2)
            return key;

        auto lineIndex = key.m_start.m_line;
        Coordinates openDelimiterCoordinates = m_lines->find(delimiters.substr(0,1), Coordinates(lineIndex, 0));
        Coordinates closeDelimiterCoordinates;
        Line openDelimiterLine = m_lines->m_unfoldedLines[openDelimiterCoordinates.m_line];
        i32 columnIndex = 0;
        bool found = false;

        while (true) {
            Coordinates nextCoordinates = m_lines->lineCoordinates(openDelimiterCoordinates.m_line, openDelimiterCoordinates.m_column + columnIndex);
            if (openDelimiterCoordinates.m_column < openDelimiterLine.maxColumn() && openDelimiterLine[(u64) nextCoordinates.m_column] == delimiters[0]) {
                if (m_lines->m_matchedDelimiter.coordinatesNearDelimiter(m_lines, nextCoordinates)) {
                    auto result = m_lines->m_matchedDelimiter.findMatchingDelimiter(m_lines, nextCoordinates, false);
                    if (result.m_line == key.m_end.m_line) {
                        found = true;
                        closeDelimiterCoordinates = result;
                        break;
                    }
                } else
                    break;
                openDelimiterCoordinates.m_column += 1;
            } else
                break;
            columnIndex++;
        }
        if (!found)
            closeDelimiterCoordinates = m_lines->rfind(delimiters.substr(1,1), m_lines->lineCoordinates(key.m_end.m_line, -1));
        return {openDelimiterCoordinates, closeDelimiterCoordinates};
    }

    void TextEditor::FoldedLine::loadSegments() {
        m_foldedSegments.clear();
        m_unfoldedSegments.clear();
        i32 keyCount = (i32)m_keys.size();
        m_foldedSegments.resize(2 * keyCount + 2);
        m_unfoldedSegments.resize(2 * keyCount + 2);

        auto foldedLineIndex = m_lines->rowToLineIndex(m_row);
        auto lineIndex = m_keys[0].m_start.m_line;
        if (!addsFullFirstLineToFold() && !addsLastLineToFold()) {
            Range key = m_keys[0];
            m_foldedSegments[0]   = m_lines->lineCoordinates( foldedLineIndex, 0);
            m_foldedSegments[1]   = m_lines->lineCoordinates( foldedLineIndex, 1);
            m_unfoldedSegments[0] = m_lines->lineCoordinates( lineIndex      , 0);
            m_unfoldedSegments[1] = m_lines->lineCoordinates( lineIndex      , 1);

            lineIndex = key.m_end.m_line;
            m_foldedSegments[2]   = m_lines->lineCoordinates( foldedLineIndex, m_ellipsisIndices[0] + Ellipsis.size() - 1);
            m_foldedSegments[3]   = m_lines->lineCoordinates( foldedLineIndex, m_ellipsisIndices[0] + Ellipsis.size());
            m_unfoldedSegments[2] = m_lines->lineCoordinates( lineIndex      , m_lines->m_unfoldedLines[lineIndex].maxColumn() - 1);
            m_unfoldedSegments[3] = m_lines->lineCoordinates( lineIndex      , m_lines->m_unfoldedLines[lineIndex].maxColumn());
            return;
        }

        Range delimiterCoordinates = findDelimiterCoordinates(m_keys[0]);

        m_foldedSegments[0] = m_lines->lineCoordinates( foldedLineIndex, 0);
        m_foldedSegments[1] = m_lines->lineCoordinates( foldedLineIndex, m_ellipsisIndices[0]);

        m_unfoldedSegments[0] = m_lines->lineCoordinates( lineIndex, 0);
        m_unfoldedSegments[1] = m_lines->lineCoordinates( delimiterCoordinates.m_start.m_line, delimiterCoordinates.m_start.m_column + 1);

        for (i32 i = 0; i < keyCount - 1; i++) {
            auto closeDelimiterCoordinates = delimiterCoordinates.m_end;
            delimiterCoordinates = findDelimiterCoordinates(m_keys[i + 1]);

            lineIndex = m_keys[i].m_end.m_line;
            m_foldedSegments[2 * i + 2] = m_lines->lineCoordinates( foldedLineIndex, m_ellipsisIndices[i] + 3);
            m_foldedSegments[2 * i + 3] = m_lines->lineCoordinates( foldedLineIndex, m_ellipsisIndices[i + 1]);
            if (firstLineNeedsDelimiter())
                m_foldedSegments[2 * i + 3].m_column -= 2;

            m_unfoldedSegments[2 * i + 2] = m_lines->lineCoordinates( lineIndex, closeDelimiterCoordinates.m_column);
            m_unfoldedSegments[2 * i + 3] = m_lines->lineCoordinates( lineIndex, delimiterCoordinates.m_start.m_column + 1);
        }

        lineIndex = m_keys.back().m_end.m_line;
        m_foldedSegments[2 * keyCount] = m_lines->lineCoordinates(foldedLineIndex, m_ellipsisIndices.back() + 3);
        m_foldedSegments[2 * keyCount + 1] = m_lines->lineCoordinates(foldedLineIndex, m_foldedLine.maxColumn());
        m_unfoldedSegments[2 * keyCount] = m_lines->lineCoordinates( lineIndex, delimiterCoordinates.m_end.m_column);
        m_unfoldedSegments[2 * keyCount + 1] = m_lines->lineCoordinates( lineIndex, m_lines->m_unfoldedLines[lineIndex].maxColumn());
    }

    void TextEditor::Lines::removeKeys() {
        for (auto &[row,foldedLine] : m_foldedLines) {
            for (auto i : std::ranges::views::reverse(foldedLine.m_keysToRemove)) {
                openCodeFold(i);
            }
            foldedLine.m_keysToRemove.clear();
            if (foldedLine.m_keys.empty()) {
                m_foldedLines.erase(row);
            }
        }
    }

    void TextEditor::FoldedLine::removeKey(const Range& key) {

        if (m_lines->rowToLineIndex(m_row) == key.m_start.m_line) {
            m_foldedLine = m_foldedLine.subLine(m_ellipsisIndices[0], m_foldedLine.size() - m_ellipsisIndices[0]);
            m_row = m_lines->lineIndexToRow(key.m_end.m_line);
            m_keys.erase(m_keys.begin());
            m_ellipsisIndices.erase(m_ellipsisIndices.begin());
        } else {
            i32 index = 0;
            for (i32 i = 1; i < (i32)m_keys.size(); i++) {
                if (m_keys[i] == key) {
                    index = i;
                    break;
                }
            }
            m_foldedLine = m_foldedLine.subLine(0, m_ellipsisIndices[index]);

            for (i32 i = index + 1; i < (i32)m_keys.size() ;  i++) {
                m_keysToRemove.push_back(m_keys[i]);
            }
            m_ellipsisIndices.erase(m_ellipsisIndices.begin() + index);
            m_keys.erase(m_keys.begin() + index);
        }

        if (!m_keys.empty()) {
            m_full.m_start = m_keys.front().m_start;
            m_full.m_end = m_keys.back().m_end;
            m_built = true;
        } else {
            m_full = Range(Invalid, Invalid);
            m_row = -1;
            m_cursorPosition = Invalid;
            m_foldedLine = Line();
            m_ellipsisIndices.clear();
            m_built = false;
        }
    }

    void TextEditor::renderText(const ImVec2 &textEditorSize) {
        m_lines.m_withinRender = true;
        preRender();
        auto drawList = ImGui::GetWindowDrawList();
        m_lines.m_cursorScreenPosition = ImGui::GetCursorScreenPos();
        float scrollY;

        if (m_setScroll) {
            setScroll(m_scroll);
            scrollY = m_scroll.y;
        } else {
            scrollY = ImGui::GetScrollY();
            float scrollX = ImGui::GetScrollX();
            m_scroll = ImVec2(scrollX, scrollY);
        }

        if (m_lines.m_setTopRow)
            m_lines.setFirstRow();
        else
            m_lines.m_topRow = std::max<float>(0.0F, (scrollY - m_lines.m_topMargin) / m_lines.m_charAdvance.y);
        m_topLineNumber = getTopLineNumber();
        float maxDisplayedRow = m_lines.getMaxDisplayedRow();
        float lineIndex = m_topLineNumber;
        float row = m_lines.m_topRow;
        m_longestDrawnLineLength = m_longestLineLength;
        if (!m_lines.isEmpty()) {
            if (!m_lines.m_codeFoldsDisabled) {
                m_lines.initializeCodeFolds();
                if (m_lines.updateCodeFolds()) {
                    m_lines.setFocusAtCoords(m_lines.m_state.m_cursorPosition, false);
                }
                m_lines.setRowToLineIndexMap();
                m_lines.getRowSegments();
            }

            bool focused = ImGui::IsWindowFocused();
            while (std::floor(row) <= maxDisplayedRow) {
                if (!focused && m_lines.m_updateFocus) {
                    m_lines.m_state.m_cursorPosition = m_lines.m_focusAtCoords;
                    m_lines.resetCursorBlinkTime();
                    if (m_lines.m_scrollToCursor)
                        m_lines.ensureCursorVisible();

                    if (!this->m_lines.m_readOnly)
                        ImGui::SetKeyboardFocusHere(0);
                    m_lines.m_updateFocus = false;
                }

                lineIndex = rowToLineIndex((i32) row);
                if (lineIndex >= m_lines.size() || lineIndex < 0)
                    break;

                if (m_showLineNumbers) {
                    if (!m_lines.m_ignoreImGuiChild)
                        ImGui::EndChild();

                    drawBreakpoints(lineIndex, textEditorSize, drawList, "##lineNumbers");
                    drawLineNumbers(lineIndex);
                    if (!m_lines.m_codeFoldsDisabled)
                        drawCodeFolds(lineIndex, drawList);

                    if (!m_lines.m_ignoreImGuiChild)
                        ImGui::BeginChild(m_lines.m_title.c_str());
                }

                drawSelection(lineIndex, drawList);
                drawButtons(lineIndex);

                if (m_showCursor)
                    drawCursor(lineIndex, textEditorSize, focused, drawList);

                u64 currentLineLength = drawColoredText(lineIndex, textEditorSize);
                if (currentLineLength > m_longestDrawnLineLength)
                    m_longestDrawnLineLength = currentLineLength;

                row = row + 1.0F;
            }
        } else {
            m_lines.m_rowToLineIndex[0] = 1;
            m_topLineNumber = 1;
            lineIndex = 0;
            if (m_lines.m_unfoldedLines.empty())
                m_lines.m_unfoldedLines.emplace_back();
            m_lines.m_state.m_cursorPosition = lineCoordinates( 0, 0);
            if (m_showLineNumbers) {
                if (!m_lines.m_ignoreImGuiChild)
                    ImGui::EndChild();
                drawLineNumbers(0);
                if (!m_lines.m_ignoreImGuiChild)
                    ImGui::BeginChild(m_lines.m_title.c_str());
            }
            if (m_showCursor)
                drawCursor(0,textEditorSize, true, drawList);
            ImGui::Dummy(m_lines.m_charAdvance);
        }

        if (m_lines.m_scrollToCursor)
            m_lines.ensureCursorVisible();
        m_lines.m_withinRender = false;
        postRender(lineIndex, "##lineNumbers");
    }

    i64 TextEditor::drawColoredText(i32 lineIndex, const ImVec2 &textEditorSize) {
        auto line = m_lines[lineIndex];

        if (line.empty()) {
            ImGui::Dummy(m_lines.m_charAdvance);

            auto lineStart = m_lines.lineCoordinates(lineIndex, 0);
            drawText(lineStart, 0, 0);
            return 0;
        }

        auto colors = line.m_colors;
        auto lineSize = line.lineTextSize();

        i64 visibleSize = std::min((u64) textEditorSize.x, (u64) lineSize);
        i64 start = ImGui::GetScrollX();
        Coordinates head = Coordinates(lineIndex, start / m_lines.m_charAdvance.x);
        i64 textSize = m_lines.textDistanceToLineStart(head);
        auto maxColumn = line.indexColumn(line.size());
        if (textSize < start) {
            while (textSize < start && head.m_column < maxColumn) {
                head.m_column += 1;
                textSize = m_lines.textDistanceToLineStart(head);
            }
        } else {
            while (textSize > start && head.m_column > 0) {
                head.m_column -= 1;
                textSize = m_lines.textDistanceToLineStart(head);
            }
        }
        Coordinates current = Coordinates(lineIndex, (start + visibleSize) / m_lines.m_charAdvance.x);//line.substr(0, (start + visibleSize) / m_charAdvance.x);
        textSize = m_lines.textDistanceToLineStart(current);
        if (textSize < start + visibleSize) {
            while (textSize < start + visibleSize && current.m_column < maxColumn) {
                current.m_column += 1;
                textSize = m_lines.textDistanceToLineStart(current);
            }
        } else {
            while (textSize > start + visibleSize && current.m_column > 0) {
                current.m_column -= 1;
                textSize = m_lines.textDistanceToLineStart(current);
            }
        }

        u64 i = line.columnIndex(head.m_column);
        u64 maxI = line.columnIndex(current.m_column);
        while (i < maxI) {
            char color = std::clamp(colors[i], (char) PaletteIndex::Default, (char) ((u8) PaletteIndex::Max - 1));
            auto index = colors.find_first_not_of(color, i);
            if (index == std::string::npos)
                index = maxI;
            else
                index -= i;

            u32 tokenLength = std::clamp((u64) index, (u64) 1, maxI - i);
            i32 columnCoordinate = line.indexColumn(i);
            auto lineStart = m_lines.lineCoordinates(lineIndex, columnCoordinate);

            drawText(lineStart, tokenLength, color);

            i += tokenLength;
        }
        auto result = line.size();
        return result;
    }

    bool TextEditor::Lines::isMultiLineRow(i32 row) {
        return (m_foldedLines.contains(row) &&  !m_foldedLines.at(row).m_keys.empty());
    }

    void TextEditor::preRender() {
        m_lines.m_charAdvance = calculateCharAdvance();
        m_lines.m_leftMargin = m_lines.m_charAdvance.x;
        for (i32 i = 0; i < (i32) PaletteIndex::Max; ++i) {
            auto color = ImGui::ColorConvertU32ToFloat4(s_paletteBase[i]);
            color.w *= ImGui::GetStyle().Alpha;
            m_palette[i] = ImGui::ColorConvertFloat4ToU32(color);
        }
        m_lines.m_numberOfLinesDisplayed = getPageSize();
    }

    void TextEditor::drawSelection(float lineIndex, ImDrawList *drawList) {
        auto row = m_lines.lineIndexToRow(lineIndex);
        auto lineStartScreenPos = m_lines.getLineStartScreenPos(0,row);
        Range lineCoords;
        if (m_lines.isMultiLineRow(row)) {
            lineCoords.m_start = m_lines.lineCoordinates(m_lines.m_foldedLines.at(row).m_full.m_start.m_line, 0);
            lineCoords.m_end = m_lines.lineCoordinates(m_lines.m_foldedLines.at(row).m_full.m_end.m_line, -1);
        } else {
            lineCoords = Range(m_lines.lineCoordinates(lineIndex, 0), m_lines.lineCoordinates(lineIndex, -1));
        }

        if (m_lines.m_state.m_selection.m_start <= lineCoords.m_end && m_lines.m_state.m_selection.m_end > lineCoords.m_start) {
            auto start = m_lines.unfoldedToFoldedCoords(std::max(m_lines.m_state.m_selection.m_start, lineCoords.m_start));
            auto end = m_lines.unfoldedToFoldedCoords(std::min(m_lines.m_state.m_selection.m_end, lineCoords.m_end));
            float selectionStart = m_lines.textDistanceToLineStart(start);//coordsToScreen(start).x;
            float selectionEnd = m_lines.textDistanceToLineStart(end);//)coordsToScreen(end).x;

            if (selectionStart < selectionEnd) {
                ImVec2 rectStart = ImVec2(lineStartScreenPos.x + selectionStart, lineStartScreenPos.y);
                ImVec2 rectEnd = ImVec2(lineStartScreenPos.x + selectionEnd, lineStartScreenPos.y + m_lines.m_charAdvance.y);
                drawList->AddRectFilled(rectStart, rectEnd, m_palette[(i32) PaletteIndex::Selection]);
            }
        }
    }

    ImVec2 TextEditor::Lines::getLineStartScreenPos(float leftMargin, float row) {
        return m_cursorScreenPosition + ImVec2(m_leftMargin + leftMargin, m_topMargin + std::floor(row) * m_charAdvance.y);
    }

    void TextEditor::drawBreakpoints(float lineIndex, const ImVec2 &contentSize, ImDrawList *drawList, std::string title) {
        if (!m_lines.m_ignoreImGuiChild)
            ImGui::BeginChild(title.c_str());
        auto row = m_lines.lineIndexToRow(lineIndex);
        auto lineStartScreenPos = m_lines.getLineStartScreenPos(0, row);
        ImVec2 lineNumberStartScreenPos = ImVec2(m_lines.m_lineNumbersStartPos.x, lineStartScreenPos.y);
        auto start = lineStartScreenPos;
        ImVec2 end = lineStartScreenPos + ImVec2(m_lines.m_lineNumberFieldWidth + contentSize.x, m_lines.m_charAdvance.y);
        auto center = lineNumberStartScreenPos + ImVec2(m_lines.m_lineNumberFieldWidth - 2 * m_lines.m_charAdvance.x + 1_scaled, 0);
        if (m_lines.m_rowToFoldSegments.contains(row)) {
            bool circlesDrawn = false;
            for (auto segments : m_lines.m_rowToFoldSegments[row]) {
                if (segments.m_foldEnd.m_line != lineIndex && m_lines.m_breakpoints.contains(segments.m_foldEnd.m_line + 1))
                    start.x = segments.m_segment.m_start;
                if (m_lines.m_breakpoints.contains(segments.m_foldEnd.m_line + 1)) {
                    if (segments == m_lines.m_rowToFoldSegments[row].back())
                        end.x = lineNumberStartScreenPos.x + contentSize.x + m_lines.m_lineNumberFieldWidth;
                    else
                        end.x = segments.m_segment.m_end;
                    drawList->AddRectFilled(start, end, m_palette[(i32) PaletteIndex::Breakpoint]);
                    if (!circlesDrawn) {
                        circlesDrawn = true;
                        drawList->AddCircleFilled(center + ImVec2(0, m_lines.m_charAdvance.y) / 2, m_lines.m_charAdvance.y / 3, m_palette[(i32) PaletteIndex::Breakpoint]);
                        drawList->AddCircle(center + ImVec2(0, m_lines.m_charAdvance.y) / 2, m_lines.m_charAdvance.y / 3, m_palette[(i32) PaletteIndex::Default]);
                    }
                }
                Coordinates segmentStart = segments.m_foldEnd;
                if (m_lines.m_codeFoldKeyMap.contains(segments.m_foldEnd)) {
                    auto keyValue = m_lines.m_codeFoldKeyMap[segmentStart];
                    auto key = Range(segmentStart, keyValue);
                    if (m_lines.m_codeFoldState.contains(key) && !m_lines.m_codeFoldState[key]) {
                        for (i32 i = key.m_start.m_line + 1; i < key.m_end.m_line; i++) {
                            if (m_lines.m_breakpoints.contains(i + 1)) {
                                start.x = segments.m_segment.m_end;
                                end.x = start.x + Ellipsis.lineTextSize();
                                drawList->AddRectFilled(start, end, m_palette[(i32) PaletteIndex::Breakpoint]);
                                if (!circlesDrawn) {
                                    circlesDrawn = true;
                                    drawList->AddCircleFilled(center + ImVec2(0, m_lines.m_charAdvance.y) / 2, m_lines.m_charAdvance.y / 3, m_palette[(i32) PaletteIndex::Breakpoint]);
                                    drawList->AddCircle(center + ImVec2(0, m_lines.m_charAdvance.y) / 2, m_lines.m_charAdvance.y / 3, m_palette[(i32) PaletteIndex::Default]);
                                }
                            }
                        }
                    }
                }
            }
        } else if (m_lines.m_breakpoints.contains(lineIndex + 1)) {
            end = ImVec2(lineNumberStartScreenPos.x + contentSize.x + m_lines.m_lineNumberFieldWidth, lineStartScreenPos.y + m_lines.m_charAdvance.y);
            drawList->AddRectFilled(start, end, m_palette[(i32) PaletteIndex::Breakpoint]);
            drawList->AddCircleFilled(center + ImVec2(0, m_lines.m_charAdvance.y) / 2, m_lines.m_charAdvance.y / 3, m_palette[(i32) PaletteIndex::Breakpoint]);
            drawList->AddCircle(center + ImVec2(0, m_lines.m_charAdvance.y) / 2, m_lines.m_charAdvance.y / 3, m_palette[(i32) PaletteIndex::Default]);
        }

        ImGui::SetCursorScreenPos(lineNumberStartScreenPos);
        ImGui::PushID((i32) (lineIndex + lineNumberStartScreenPos.y));
        float buttonWidth = m_lines.m_lineNumberFieldWidth;
        auto boxSize = m_lines.m_charAdvance.x + (((u32) m_lines.m_charAdvance.x % 2) ? 2.0f : 1.0f);
        if (m_lines.m_codeFoldKeyLineMap.contains(lineIndex) || m_lines.m_codeFoldValueLineMap.contains(lineIndex)) {
            buttonWidth -= (boxSize - 1) / 2;
        }

        if (buttonWidth > 0 && m_lines.m_charAdvance.y > 0) {
            if (ImGui::InvisibleButton("##breakpoints", ImVec2(buttonWidth, m_lines.m_charAdvance.y))) {
                if (m_lines.m_breakpoints.contains(lineIndex + 1))
                    m_lines.m_breakpoints.erase(lineIndex + 1);
                else
                    m_lines.m_breakpoints.insert(lineIndex + 1);
                m_lines.m_breakPointsChanged = true;
                m_lines.setFocusAtCoords(m_lines.m_state.m_cursorPosition, false);
            }
        }

        if (ImGui::IsItemHovered() && (ImGui::IsKeyDown(ImGuiKey_RightShift) || ImGui::IsKeyDown(ImGuiKey_LeftShift)) && m_lines.m_state.m_cursorPosition.isValid(m_lines)) {
            if (ImGui::BeginTooltip()) {
                auto lineCursor = m_lines.m_state.m_cursorPosition.m_line + 1;
                auto columnCursor = m_lines.m_state.m_cursorPosition.m_column + 1;
                ImGui::Text("(%d/%d)", lineCursor, columnCursor);
            }
            ImGui::EndTooltip();
        }
        ImGui::PopID();

    }

    void TextEditor::drawLineNumbers(float lineIndex) {
        auto row = m_lines.lineIndexToRow(lineIndex);
        auto lineStartScreenPos = m_lines.getLineStartScreenPos(0,row);
        ImVec2 lineNumberStartScreenPos = ImVec2(m_lines.m_lineNumbersStartPos.x, lineStartScreenPos.y);
        auto lineNumber = lineIndex + 1;
        if (lineNumber <= 0)
            return;
        auto color = m_palette[(i32) PaletteIndex::LineNumber];
        auto cursorRow = m_lines.lineIndexToRow(m_lines.m_state.m_cursorPosition.m_line);
        i32 lineNumberToDraw = (i32) lineNumber;
        if (cursorRow == row && m_showCursor) {
            color = m_palette[(i32) PaletteIndex::Default];
            if (m_lines.isMultiLineRow(row)) {
                lineNumberToDraw = m_lines.m_state.m_cursorPosition.m_line + 1;
            }
        }

        i32 padding = std::floor(std::log10(m_lines.size())) - std::floor(std::log10((float)lineNumberToDraw));
        std::string lineNumberStr = std::string(padding, ' ') + std::to_string(lineNumberToDraw);

        TextUnformattedColoredAt(ImVec2(lineNumberStartScreenPos.x, lineStartScreenPos.y), color, lineNumberStr.c_str());
    }

    void TextEditor::drawCursor(float lineIndex, const ImVec2 &contentSize, bool focused, ImDrawList *drawList) {

        auto row = m_lines.lineIndexToRow(lineIndex);
        auto lineStartScreenPos = m_lines.getLineStartScreenPos(0, row);
        ImVec2 lineNumberStartScreenPos = ImVec2(m_lines.m_lineNumbersStartPos.x, lineStartScreenPos.y);
        Coordinates lineCoords = lineCoordinates(lineIndex+1, 0);

        if (lineStartScreenPos == ImVec2(-1, -1)) {
            for (auto key: m_lines.m_codeFoldKeys) {
                if (key.contains(lineCoords) && m_lines.m_codeFoldState.contains(key) && !m_lines.m_codeFoldState[key]) {
                    row = m_lines.m_multiLinesToRow[lineIndex + 1];
                    i32 multilineLineIndex = rowToLineIndex(row);
                    if (m_lines.m_rowToFoldSegments.contains(row) && m_lines.m_rowToFoldSegments[row].size() > 1) {
                        FoldSegment result = *std::find_if(m_lines.m_rowToFoldSegments[row].begin(), m_lines.m_rowToFoldSegments[row].end(), [&](const FoldSegment &segment) {
                            return segment.m_foldEnd == key.m_end;
                        });
                        lineStartScreenPos = ImVec2(result.m_segment.m_start + m_lines.m_unfoldedLines[0].stringTextSize(std::string(m_lines.m_leadingLineSpaces[key.m_end.m_line], ' ')), m_lines.m_lineIndexToScreen[multilineLineIndex].y);
                    }
                    break;
                }
            }
        }

        auto timeEnd = ImGui::GetTime() * 1000;
        auto elapsed = timeEnd - m_lines.m_startTime;
        auto foldedCursorPosition = m_lines.unfoldedToFoldedCoords(m_lines.m_state.m_cursorPosition);
        if (foldedCursorPosition.m_line == (i32) lineIndex) {
            if (focused && elapsed > s_cursorBlinkOnTime) {
                float width = 1.0f;
                u64 charIndex = m_lines.lineCoordsIndex(foldedCursorPosition);
                float cx = m_lines.textDistanceToLineStart(foldedCursorPosition);
                if (m_overwrite && charIndex < m_lines.m_unfoldedLines[foldedCursorPosition.m_line].size()) {
                    auto charSize = utf8CharLength(m_lines.m_unfoldedLines[foldedCursorPosition.m_line][charIndex]);
                    std::string s = m_lines.m_unfoldedLines[foldedCursorPosition.m_line].substr(charIndex, charSize);
                    width = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, s.c_str()).x;
                }
                ImVec2 rectStart(lineStartScreenPos.x + cx, lineStartScreenPos.y);
                ImVec2 rectEnd(lineStartScreenPos.x + cx + width, lineStartScreenPos.y + m_lines.m_charAdvance.y);
                drawList->AddRectFilled(rectStart, rectEnd, m_palette[(i32) PaletteIndex::Cursor]);
                if (elapsed > s_cursorBlinkInterval)
                    m_lines.m_startTime = timeEnd;
            }
            if (!m_lines.hasSelection()) {
                auto end = ImVec2(lineNumberStartScreenPos.x + contentSize.x + m_lines.m_lineNumberFieldWidth, lineStartScreenPos.y + m_lines.m_charAdvance.y);
                drawList->AddRectFilled(lineStartScreenPos, end, m_palette[(i32) (focused ? PaletteIndex::CurrentLineFill : PaletteIndex::CurrentLineFillInactive)]);
                drawList->AddRect(lineStartScreenPos, end, m_palette[(i32) PaletteIndex::CurrentLineEdge], 1.0f);
            }
        }
    }

    void TextEditor::drawButtons(float lineIndex) {
        auto row = m_lines.lineIndexToRow(lineIndex);
         auto lineStartScreenPos = m_lines.getLineStartScreenPos(0,row);
        auto lineText = m_lines.m_unfoldedLines[lineIndex].m_chars;
        Coordinates gotoKey = lineCoordinates( lineIndex + 1, 1);
        if (gotoKey != Invalid) {
            std::string errorLineColumn;
            bool found = false;
            for (const auto &text: m_lines.m_clickableText) {
                if (lineText.starts_with(text)) {
                    errorLineColumn = lineText.substr(text.size());
                    if (!errorLineColumn.empty()) {
                        found = true;
                        break;
                    }
                }
            }
            if (found) {
                i32 currLine = 0, currColumn = 0;
                if (auto idx = errorLineColumn.find(':'); idx != std::string::npos) {
                    auto errorLine = errorLineColumn.substr(0, idx);
                    if (!errorLine.empty())
                        currLine = std::stoi(errorLine) - 1;
                    auto errorColumn = errorLineColumn.substr(idx + 1);
                    if (!errorColumn.empty())
                        currColumn = std::stoi(errorColumn) - 1;
                }
                TextEditor::Coordinates errorPos = getSourceCodeEditor()->m_lines.lineCoordinates(currLine, currColumn);
                if (errorPos != Invalid) {
                    ImVec2 errorStart = ImVec2(lineStartScreenPos.x, lineStartScreenPos.y);
                    auto lineEnd = m_lines.lineCoordinates(lineIndex, -1);
                    if (lineEnd != Invalid) {
                        ImVec2 errorEnd = ImVec2(lineStartScreenPos.x + m_lines.textDistanceToLineStart(lineEnd), lineStartScreenPos.y + m_lines.m_charAdvance.y);
                        ErrorGotoBox box = ErrorGotoBox(ImRect(errorStart, errorEnd), errorPos, getSourceCodeEditor());
                        m_lines.m_errorGotoBoxes[gotoKey] = box;
                        CursorChangeBox cursorBox = CursorChangeBox(ImRect(errorStart, errorEnd));
                        m_lines.m_cursorBoxes[gotoKey] = cursorBox;
                    }
                }
            }
            if (m_lines.m_cursorBoxes.find(gotoKey) != m_lines.m_cursorBoxes.end()) {
                auto box = m_lines.m_cursorBoxes[gotoKey];
                if (box.trigger())
                    box.callback();
            }

            if (m_lines.m_errorGotoBoxes.find(gotoKey) != m_lines.m_errorGotoBoxes.end()) {
                auto box = m_lines.m_errorGotoBoxes[gotoKey];
                if (box.trigger())
                    box.callback();
            }
        }
        row = m_lines.lineIndexToRow(lineIndex);
        if (m_lines.m_rowCodeFoldTooltips.contains(row)) {
            for (auto codeFoldTooltip: m_lines.m_rowCodeFoldTooltips[row]) {
                if (codeFoldTooltip.trigger())
                    codeFoldTooltip.callback();
            }
        }
    }

    void TextEditor::drawText(Coordinates &lineStart, u32 tokenLength, char color) {
        auto row = m_lines.lineIndexToRow(lineStart.m_line);
        auto begin = m_lines.getLineStartScreenPos(0,row);

        Line line = m_lines[lineStart.m_line];
        auto i = line.columnIndex(lineStart.m_column);

        begin.x += line.textSize(i);

        if (color <= (char) TextEditor::PaletteIndex::Comment && color >= (char) TextEditor::PaletteIndex::DocComment)
            fonts::CodeEditor().pushItalic();
        TextUnformattedColoredAt(begin, m_palette[(i32) color], line.substr(i, tokenLength).c_str());

        if (color <= (char) TextEditor::PaletteIndex::Comment && color >= (char) TextEditor::PaletteIndex::DocComment)
           fonts::CodeEditor().pop();

        ErrorMarkers::iterator errorIt;
        auto errorHoverBoxKey = lineStart + lineCoordinates( 1, 1);
        if (errorIt = m_lines.m_errorMarkers.find(errorHoverBoxKey); errorIt != m_lines.m_errorMarkers.end()) {
            auto errorMessage = errorIt->second.second;
            auto errorLength = errorIt->second.first;
            if (errorLength == 0 && line.size() > (u32) i + 1)
                errorLength = line.size() - i - 1;
            if (errorLength > 0) {
                auto end = underWavesAt(begin, errorLength, m_palette[(i32) PaletteIndex::ErrorMarker]);
                ErrorHoverBox box = ErrorHoverBox(ImRect(begin, end), errorHoverBoxKey, errorMessage.c_str());
                m_lines.m_errorHoverBoxes[errorHoverBoxKey] = box;
            }
        }

        if (m_lines.m_errorHoverBoxes.find(errorHoverBoxKey) != m_lines.m_errorHoverBoxes.end()) {
            auto errorHoverBox = m_lines.m_errorHoverBoxes[errorHoverBoxKey];
            if (errorHoverBox.trigger())
                errorHoverBox.callback();
        }

        lineStart = lineStart + lineCoordinates( 0, tokenLength);
    }

    TextEditor::CodeFold::CodeFold(Lines *lines,  TextEditor::Range key, const ImRect &startBox, const ImRect &endBox) :
            ActionableBox(startBox), lines(lines), key(key), codeFoldStartCursorBox(startBox), codeFoldEndActionBox(endBox), codeFoldEndCursorBox(endBox)
            {
    if (lines->m_codeFolds.empty())
        return;
    if (!lines->m_codeFolds.contains(key))
        lines->m_codeFolds[key] = *this;
    if (!lines->m_codeFoldKeys.contains(key))
        lines->m_codeFoldKeys.insert(key);
    lines->m_codeFoldKeyMap[key.m_start] = key.m_end;
    lines->m_codeFoldValueMap[key.m_end] = key.m_start;
    if (!lines->m_codeFoldState.contains(key))
        lines->m_codeFoldState[key] = true;
}

    void TextEditor::postRender(float lineIndex, std::string title) {
        lineIndex--;
        float row = m_lines.lineIndexToRow(lineIndex);
        auto lineStartScreenPos = m_lines.getLineStartScreenPos(0, row);

        float globalRowMax = m_lines.getGlobalRowMax();
        auto rowMax = 0;
        if (globalRowMax > 0)
            rowMax = std::clamp(row + m_lines.m_numberOfLinesDisplayed, 0.0F, globalRowMax - 1.0F);

        if (!m_lines.m_ignoreImGuiChild) {
            ImGui::EndChild();
            if (m_showLineNumbers) {
                ImGui::BeginChild(title.c_str());
                ImGui::SetCursorScreenPos(ImVec2(m_lines.m_lineNumbersStartPos.x, lineStartScreenPos.y));
                ImGui::Dummy(ImVec2(m_lines.m_lineNumberFieldWidth, (globalRowMax - rowMax) * m_lines.m_charAdvance.y + ImGui::GetCurrentWindow()->InnerClipRect.GetHeight() - m_lines.m_charAdvance.y));
                ImGui::EndChild();
            }
            ImGui::BeginChild(m_lines.m_title.c_str());
        }

        ImGui::SetCursorScreenPos(lineStartScreenPos);
        if (m_showLineNumbers)
            ImGui::Dummy(ImVec2(m_longestDrawnLineLength * m_lines.m_charAdvance.x + m_lines.m_charAdvance.x, std::floor((globalRowMax - rowMax) * m_lines.m_charAdvance.y + ImGui::GetCurrentWindow()->InnerClipRect.GetHeight())));
        else
            ImGui::Dummy(ImVec2(m_longestDrawnLineLength * m_lines.m_charAdvance.x + m_lines.m_charAdvance.x, std::floor((globalRowMax - rowMax - 1_scaled) * m_lines.m_charAdvance.y + ImGui::GetCurrentWindow()->InnerClipRect.GetHeight())));

        if (m_topMarginChanged) {
            m_topMarginChanged = false;
            auto window = ImGui::GetCurrentWindow();
            auto maxScroll = window->ScrollMax.y;
            if (maxScroll > 0) {
                float pixelCount;
                if (m_newTopMargin > m_lines.m_topMargin) {
                    pixelCount = m_newTopMargin - m_lines.m_topMargin;
                } else if (m_newTopMargin > 0) {
                    pixelCount = m_lines.m_topMargin - m_newTopMargin;
                } else {
                    pixelCount = m_lines.m_topMargin;
                }
                auto oldScrollY = ImGui::GetScrollY();

                if (m_newTopMargin > m_lines.m_topMargin)
                    m_shiftedScrollY = oldScrollY + pixelCount;
                else
                    m_shiftedScrollY = oldScrollY - pixelCount;
                ImGui::SetScrollY(m_shiftedScrollY);
                m_lines.m_topMargin = m_newTopMargin;
            }
        }
    }

    ImVec2 TextEditor::calculateCharAdvance() const {
        const float fontSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, "#", nullptr, nullptr).x;
        return {fontSize, GImGui->FontSize * m_lineSpacing};
    }

    float TextEditor::Lines::textDistanceToLineStart(const Coordinates &aFrom) {
        if (m_lineIndexToScreen[aFrom.m_line] == ImVec2(-1,-1))
            return 0.0f;
        auto line = operator[](aFrom.m_line);
        i32 colIndex = lineCoordsIndex(aFrom);
        auto substr1 = line.m_chars.substr(0, colIndex);
        auto substr2 =line.m_chars.substr(colIndex, line.m_chars.size() - colIndex);
        if (substr2.size() < substr1.size()) {
            auto distanceToEnd = line.stringTextSize(substr2.c_str());
            line.m_lineMaxColumn = line.lineTextSize();
            return line.m_lineMaxColumn - distanceToEnd;
        }
        return line.stringTextSize(substr1.c_str());
    }

    void TextEditor::drawCodeFolds(float lineIndex, ImDrawList *drawList) {
        auto codeFoldKeyLine = lineIndex;
        auto row = m_lines.lineIndexToRow(codeFoldKeyLine);
        auto state = m_lines.m_rowToFoldSymbol[row];

        if (m_lines.m_codeFoldHighlighted != NoCodeFoldSelected) {
            Range key1, key2;
            if (m_lines.m_foldedLines.contains(row)) {
                auto &foldedLine = m_lines.m_foldedLines.at(row);
                if (m_lines.m_codeFoldValueMap.contains(foldedLine.m_full.m_start))
                    key1 = Range(m_lines.m_codeFoldValueMap[foldedLine.m_full.m_start], foldedLine.m_full.m_start);
                else
                    key1 = Range(foldedLine.m_full.m_start, m_lines.m_codeFoldKeyMap[foldedLine.m_full.m_start]);
                if (m_lines.m_codeFoldKeyMap.contains(foldedLine.m_full.m_end))
                    key2 = Range(foldedLine.m_full.m_end, m_lines.m_codeFoldKeyMap[foldedLine.m_full.m_end]);
                else
                    key2 = Range(m_lines.m_codeFoldValueMap[foldedLine.m_full.m_end], foldedLine.m_full.m_end);

                if (m_lines.m_codeFoldHighlighted == key1) {
                    if (m_lines.m_codeFoldState.contains(m_lines.m_codeFoldHighlighted) && !m_lines.m_codeFoldState[m_lines.m_codeFoldHighlighted])
                        state = FoldSymbol::Square;
                    else {
                        if (codeFoldKeyLine == key1.m_start.m_line)
                            state = FoldSymbol::Down;
                        else if (codeFoldKeyLine == key1.m_end.m_line)
                            state = FoldSymbol::Up;
                    }
                } else if (m_lines.m_codeFoldHighlighted == key2) {
                    if (m_lines.m_codeFoldState.contains(key2) && !m_lines.m_codeFoldState[key2])
                        state = FoldSymbol::Square;
                    else {
                        if (lineIndexToRow(codeFoldKeyLine) == lineIndexToRow(key2.m_start.m_line))
                            state = FoldSymbol::Down;
                        else if (codeFoldKeyLine == key2.m_end.m_line)
                            state = FoldSymbol::Up;
                    }
                }
            } else if (m_lines.m_codeFoldHighlighted.m_start.m_line == codeFoldKeyLine) {
                if (m_lines.m_codeFoldState.contains(m_lines.m_codeFoldHighlighted) && m_lines.m_codeFoldState[m_lines.m_codeFoldHighlighted])
                    state = FoldSymbol::Down;
                else
                    state = FoldSymbol::Square;
            } else if (m_lines.m_codeFoldHighlighted.m_end.m_line == codeFoldKeyLine) {
                if (m_lines.m_codeFoldState.contains(m_lines.m_codeFoldHighlighted) && m_lines.m_codeFoldState[m_lines.m_codeFoldHighlighted])
                    state = FoldSymbol::Up;
                else
                    state = FoldSymbol::Square;
            }
        }

        i32 lineColor;
        Interval highlightedRowInterval = Interval(m_lines.lineIndexToRow(m_lines.m_codeFoldHighlighted.m_start.m_line), m_lines.lineIndexToRow(m_lines.m_codeFoldHighlighted.m_end.m_line));

        if (highlightedRowInterval.contains(row) && (state == FoldSymbol::Line ||
             row == highlightedRowInterval.m_start || row == highlightedRowInterval.m_end))
            lineColor = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_ScrollbarGrabActive]);
        else
            lineColor = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_Border]);

        renderCodeFolds(row, drawList, lineColor, state);

        if (m_lines.m_matchedDelimiter.setNearCursor(&m_lines, m_lines.m_state.m_cursorPosition)) {
            m_lines.m_matchedDelimiter.findMatchingDelimiter(&m_lines);
            if (m_lines.isTrueMatchingDelimiter()) {
                auto nearCursorScreenPos = m_lines.getLineStartScreenPos(0, lineIndexToRow(m_lines.m_matchedDelimiter.m_nearCursor.m_line));
                auto matchedScreenPos = m_lines.getLineStartScreenPos(0, lineIndexToRow(m_lines.m_matchedDelimiter.m_matched.m_line));

                if (nearCursorScreenPos != ImVec2(-1, -1) && matchedScreenPos != ImVec2(-1, -1) && nearCursorScreenPos.y != matchedScreenPos.y) {
                    float lineX = m_lines.m_lineNumbersStartPos.x + m_lines.m_lineNumberFieldWidth - m_lines.m_charAdvance.x + 1_scaled;
                    ImVec2 p1 = ImVec2(lineX, std::min(matchedScreenPos.y, nearCursorScreenPos.y));
                    ImVec2 p2 = ImVec2(lineX, std::max(matchedScreenPos.y, nearCursorScreenPos.y) + m_lines.m_charAdvance.y - 1_scaled);
                    drawList->AddLine(p1, p2, ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]), 1.0f);
                }
            }
        }

        if (!m_lines.m_ignoreImGuiChild)
            ImGui::EndChild();
    }

    void TextEditor::renderCodeFolds(i32 row, ImDrawList *drawList, i32 color, FoldSymbol state) {
        auto boxSize = m_lines.m_charAdvance.x + (((u32)m_lines.m_charAdvance.x % 2) ? 2.0f : 1.0f);
        auto verticalMargin = m_lines.m_charAdvance.y - boxSize;
        auto horizontalMargin = m_lines.m_lineNumberFieldWidth - (boxSize - 1) / 2;
        auto lineStartScreenPos = m_lines.getLineStartScreenPos(horizontalMargin, row);
        auto numberLineStartScreenPos = ImVec2(m_lines.m_lineNumbersStartPos.x + m_lines.m_lineNumberFieldWidth, lineStartScreenPos.y);

        if (state == FoldSymbol::Square) {
            renderSquare(numberLineStartScreenPos, drawList, boxSize - 1, verticalMargin, color);
            renderPlus(numberLineStartScreenPos, drawList, boxSize, std::floor(verticalMargin / 2), color);
        } else if (state == FoldSymbol::Down) {
            renderPointingDown(numberLineStartScreenPos, drawList, boxSize - 1, verticalMargin, color);
            renderMinus(numberLineStartScreenPos, drawList, boxSize, std::floor(verticalMargin / 4), color);
        } else if (state == FoldSymbol::Up) {
            renderPointingUp(numberLineStartScreenPos, drawList, boxSize - 1, verticalMargin, color);
            renderMinus(numberLineStartScreenPos, drawList, boxSize, std::floor(3 * verticalMargin / 4), color);
        } else {
            auto startPos = numberLineStartScreenPos;
            drawList->AddLine(startPos, startPos + ImVec2(0, m_lines.m_charAdvance.y), color, 1.0f);
        }
    }

    void TextEditor::renderVerticals(ImVec2 lineStartScreenPos, ImDrawList *drawList, float boxSize, float verticalMargin, i32 color) {
        ImVec2 p = ImVec2(lineStartScreenPos.x - boxSize / 2, lineStartScreenPos.y + verticalMargin);
        ImVec2 py = ImVec2(0, boxSize);
        drawList->AddLine(p, p + py, color, 1.0f);
        ImVec2 px = ImVec2(boxSize, 0);
        drawList->AddLine(p + px, p + px + py, color, 1.0f);

        for (i32 i = 1; i < boxSize; i++) {
            ImVec2 pxi = ImVec2(i, 0);
            drawList->AddLine(p + pxi, p + pxi + py, m_palette[(i32)PaletteIndex::Background],1.0f);
        }
    }

    void TextEditor::renderMinus(ImVec2 lineStartScreenPos, ImDrawList *drawList, float boxSize, float verticalMargin, i32 color) {
        ImVec2 p = ImVec2(lineStartScreenPos.x - (boxSize - 1) / 2 + 2, lineStartScreenPos.y + (boxSize - 1) / 2 + verticalMargin);
        ImVec2 px = ImVec2(boxSize - 5, 0);
        drawList->AddLine(p, p + px, color, 1.0f);
    }

    void TextEditor::renderPlus(ImVec2 lineStartScreenPos, ImDrawList *drawList, float boxSize, float verticalMargin, i32 color) {
        renderMinus(lineStartScreenPos, drawList, boxSize, verticalMargin, color);
        ImVec2 p = ImVec2(lineStartScreenPos.x, lineStartScreenPos.y + 2 + verticalMargin);
        ImVec2 py = ImVec2(0, boxSize - 5);
        drawList->AddLine(p, p + py, color, 1.0f);
    }

    void TextEditor::renderTopHorizontal(ImVec2 lineStartScreenPos, ImDrawList *drawList, float boxSize, float verticalMargin, i32 color) {
        ImVec2 p = ImVec2(lineStartScreenPos.x - boxSize / 2, lineStartScreenPos.y + verticalMargin);
        ImVec2 px = ImVec2(boxSize, 0);
        drawList->AddLine(p, p + px, color, 1.0f);
    }

    void TextEditor::renderBottomHorizontal(ImVec2 lineStartScreenPos, ImDrawList *drawList, float boxSize, float verticalMargin, i32 color) {
        ImVec2 p = ImVec2(lineStartScreenPos.x - boxSize / 2, lineStartScreenPos.y + boxSize + verticalMargin);
        ImVec2 px = ImVec2(boxSize, 0);
        drawList->AddLine(p, p + px, color, 1.0f);
    }

    void TextEditor::renderSquare(ImVec2 lineStartScreenPos, ImDrawList *drawList, float boxSize, float verticalMargin, i32 color) {
        renderVerticals(lineStartScreenPos, drawList, boxSize, 0, color);
        renderVerticals(lineStartScreenPos, drawList, boxSize, verticalMargin, color);
        renderTopHorizontal(lineStartScreenPos, drawList, boxSize, 0, color);
        renderBottomHorizontal(lineStartScreenPos, drawList, boxSize, verticalMargin, color);
    }

    void TextEditor::renderPointingUp(ImVec2 lineStartScreenPos, ImDrawList *drawList, float boxSize, float verticalMargin, i32 color) {
        ImVec2 p1 = ImVec2(lineStartScreenPos.x - boxSize / 2, lineStartScreenPos.y + verticalMargin);
        ImVec2 px = ImVec2(boxSize, 0);
        ImVec2 py = ImVec2(0, boxSize);

        for (i32 i = 1; i < boxSize / 2; i++) {
            ImVec2 pxi = ImVec2(i, 0);
            ImVec2 pyi = ImVec2(0, boxSize / 2 - verticalMargin - i + 2);
            drawList->AddLine(p1 + py + pxi, p1 + pxi + pyi, m_palette[(i32) PaletteIndex::Background], 1.0f);
        }

        for (i32 i = boxSize / 2; i < boxSize;  i++) {
            ImVec2 pxi = ImVec2(i, 0);
            ImVec2 pyi = ImVec2(0, i - boxSize / 2 - verticalMargin + 2);
            drawList->AddLine(p1 + py + pxi, p1 + pxi + pyi, m_palette[(i32) PaletteIndex::Background], 1.0f);
        }

        renderVerticals(lineStartScreenPos, drawList, boxSize, verticalMargin, color);
        renderBottomHorizontal(lineStartScreenPos, drawList, boxSize, verticalMargin, color);

        ImVec2 p2 = lineStartScreenPos;
        drawList->AddLine(p1, p2, color, 1.0f);
        drawList->AddLine(p1 + px, p2, color, 1.0f);
    }

    void TextEditor::renderPointingDown(ImVec2 lineStartScreenPos, ImDrawList *drawList, float boxSize, float verticalMargin, i32 color) {
        ImVec2 p1 = ImVec2(lineStartScreenPos.x - boxSize / 2, lineStartScreenPos.y);
        ImVec2 px = ImVec2(boxSize, 0);
        ImVec2 py = ImVec2(0, boxSize);

        for (i32 i = 1; i < boxSize / 2; i++) {
            ImVec2 pxi = ImVec2(i, 0);
            ImVec2 pyi = ImVec2(0, verticalMargin - boxSize / 2 + i - 2);
            drawList->AddLine(p1 + pxi, p1 + py + pxi + pyi, m_palette[(i32) PaletteIndex::Background], 1.0f);
        }
        for (i32 i = boxSize / 2; i < boxSize;  i++) {
            ImVec2 pxi = ImVec2(i, 0);
            ImVec2 pyi = ImVec2(0, verticalMargin + boxSize / 2 - i - 2);
            drawList->AddLine(p1 + pxi, p1 + py + pxi + pyi, m_palette[(i32) PaletteIndex::Background], 1.0f);
        }

        renderVerticals(lineStartScreenPos, drawList, boxSize, 0, color);
        renderTopHorizontal(lineStartScreenPos, drawList, boxSize, 0, color);

        ImVec2 p2 = lineStartScreenPos + ImVec2(0, verticalMargin + boxSize);
        drawList->AddLine(p1 + py, p2, color, 1.0f);
        drawList->AddLine(p1 + px + py, p2, color, 1.0f);
    }

    bool TextEditor::areEqual(const std::pair<Range, CodeFold> &a, const std::pair<Range, CodeFold> &b) {
        return a.first == b.first && a.second.isOpen() == b.second.isOpen();
    }
}