#include "imgui.h"
#include <ui/text_editor.hpp>
#include <algorithm>


namespace hex::ui {
    static bool isWordChar(char c) {
        auto asUChar = static_cast<u8>(c);
        return std::isalnum(asUChar) || c == '_' || asUChar > 0x7F;
    }


    void TextEditor::jumpToLine(i32 line) {
        auto newPos = m_lines.m_state.m_cursorPosition;
        if (line != -1) {
            newPos = m_lines.lineCoordinates(line, 0);
        }
        jumpToCoords(newPos);
    }

    void TextEditor::jumpToCoords(const Coordinates &coords) {
        setSelection(Range(coords, coords));
        m_lines.setCursorPosition(coords, true);
        m_lines.ensureCursorVisible();

        m_lines.setFocusAtCoords(coords, true);
    }

    void TextEditor::moveToMatchedDelimiter(bool select) {
        m_lines.moveToMatchedDelimiter(select);
    }

    TextEditor::Coordinates TextEditor::Lines::rfind( const std::string &text, const Coordinates &from) {
        Coordinates result = Invalid;
        if (text.empty() || isEmpty() || from.m_line >= size() || from.m_line < 0)
            return result;
        for (i32 i = from.m_line; i >= 0; --i) {
            auto &line = m_unfoldedLines[i];
            auto index = line.m_chars.rfind(text, (i == from.m_line) ? from.m_column : std::string::npos);
            if (index != std::string::npos) {
                result = Coordinates( i, line.indexColumn(index));
                break;
            }
        }
        return result;
    }


    TextEditor::Coordinates TextEditor::Lines::find(const std::string &text, const Coordinates &from) {
        Coordinates result = Invalid;
        if (text.empty() || isEmpty() || from.m_line >= size() || from.m_line < 0)
            return result;
        for (i32 i = from.m_line; i < size(); ++i) {
            auto &line = m_unfoldedLines[i];
            auto index = line.m_chars.find(text, (i == from.m_line) ? from.m_column : 0);
            if (index != std::string::npos) {
                result = Coordinates( i, line.indexColumn(index));
                break;
            }
        }

        return result;
    }

    void TextEditor::Lines::moveToMatchedDelimiter(bool select) {
        resetCursorBlinkTime();
        if (m_matchedDelimiter.coordinatesNearDelimiter(this, m_state.m_cursorPosition)) {
            auto oldPos = m_matchedDelimiter.m_nearCursor;
            if (operator[](oldPos.m_line).m_colors[oldPos.m_column] == (char) PaletteIndex::WarningText) {
                m_matchedDelimiter.findMatchingDelimiter(this);
                auto newPos = m_matchedDelimiter.m_matched;
                if (newPos != Invalid) {
                    if (select) {
                        if (oldPos == m_interactiveSelection.m_start)
                            m_interactiveSelection.m_start = newPos;
                        else if (oldPos == m_interactiveSelection.m_end)
                            m_interactiveSelection.m_end = newPos;
                        else {
                            m_interactiveSelection = Range(newPos, oldPos);
                        }
                    } else
                        m_interactiveSelection.m_start = m_interactiveSelection.m_end = newPos;

                    setSelection(m_interactiveSelection);
                    setCursorPosition(newPos);
                    ensureCursorVisible();
                }
            }
        }
    }

    void TextEditor::moveUp(i32 amount, bool select) {
        m_lines.resetCursorBlinkTime();
        m_lines.moveUp(amount, select);
        setSelection(m_lines.m_interactiveSelection);
        m_lines.ensureCursorVisible();
    }

    void TextEditor::Lines::moveUp(i32 amount, bool select) {
        auto oldPos = m_state.m_cursorPosition;
        if (amount < 0) {
            m_scrollYIncrement = -1.0;
            setScrollY();
            return;
        }
        auto row = lineIndexToRow(m_state.m_cursorPosition.m_line);
        if (isMultiLineRow(row)) {
            auto position = unfoldedToFoldedCoords(m_state.m_cursorPosition);
            m_state.m_cursorPosition.m_column = position.m_column;
        }
        row -= amount;
        if (isMultiLineRow(row)) {
            auto position = foldedToUnfoldedCoords(Coordinates( rowToLineIndex(row), m_state.m_cursorPosition.m_column));
            m_state.m_cursorPosition = position;
        } else {
            row = std::clamp(row, 0.0f, getGlobalRowMax());
            m_state.m_cursorPosition.m_line = rowToLineIndex(row);
        }

        if (oldPos != m_state.m_cursorPosition) {
            if (select) {
                if (oldPos == m_interactiveSelection.m_start)
                    m_interactiveSelection.m_start = m_state.m_cursorPosition;
                else if (oldPos == m_interactiveSelection.m_end)
                    m_interactiveSelection.m_end = m_state.m_cursorPosition;
                else {
                    m_interactiveSelection.m_start = m_state.m_cursorPosition;
                    m_interactiveSelection.m_end = oldPos;
                }
            } else
                m_interactiveSelection.m_start = m_interactiveSelection.m_end = m_state.m_cursorPosition;
        }
    }

    void TextEditor::moveDown(i32 amount, bool select) {
        m_lines.resetCursorBlinkTime();
        m_lines.moveDown(amount, select);
        setSelection(m_lines.m_interactiveSelection);

        m_lines.ensureCursorVisible();
    }

    void TextEditor::Lines::moveDown(i32 amount, bool select) {
        auto oldPos = m_state.m_cursorPosition;
        if (amount < 0) {
            m_scrollYIncrement = 1.0;
            setScrollY();
            return;
        }

        if (isLastLine(oldPos.m_line)) {
            m_topRow += amount;
            m_topRow = std::clamp(m_topRow, 0.0F, getGlobalRowMax());
            setFirstRow();
            ensureCursorVisible();
            return;
        }

        auto row = lineIndexToRow(m_state.m_cursorPosition.m_line);
        if (isMultiLineRow(row)) {
            auto position = unfoldedToFoldedCoords(m_state.m_cursorPosition);
            m_state.m_cursorPosition.m_column = position.m_column;
        }
        row += amount;
        if (isMultiLineRow(row)) {
            auto position = foldedToUnfoldedCoords(Coordinates( rowToLineIndex(row), m_state.m_cursorPosition.m_column));
            m_state.m_cursorPosition = position;
        } else {
            row = std::clamp(row, 0.0f, getGlobalRowMax());
            m_state.m_cursorPosition.m_line = rowToLineIndex(row);
        }


        if (m_state.m_cursorPosition != oldPos) {
            if (select) {
                if (oldPos == m_interactiveSelection.m_end)
                    m_interactiveSelection.m_end = m_state.m_cursorPosition;
                else if (oldPos == m_interactiveSelection.m_start)
                    m_interactiveSelection.m_start = m_state.m_cursorPosition;
                else {
                    m_interactiveSelection.m_start = oldPos;
                    m_interactiveSelection.m_end = m_state.m_cursorPosition;
                }
            } else
                m_interactiveSelection.m_start = m_interactiveSelection.m_end = m_state.m_cursorPosition;
        }
    }

    void TextEditor::moveLeft(i32 amount, bool select, bool wordMode) {
        m_lines.resetCursorBlinkTime();
        m_lines.moveLeft(amount, select, wordMode);
        setSelection(m_lines.m_interactiveSelection);
        m_lines.ensureCursorVisible();
    }

    void TextEditor::Lines::moveLeft(i32 amount, bool select, bool wordMode) {
        auto oldPos = m_state.m_cursorPosition;
        auto foldedPos = unfoldedToFoldedCoords(oldPos);

        if (isEmpty() || foldedPos < Coordinates( 0, 0) || foldedPos == Invalid)
            return;

        auto lindex = foldedPos.m_line;
        auto line = operator[](lindex);
        auto lineMaxColumn = line.maxColumn();
        auto column = std::min(foldedPos.m_column, lineMaxColumn);
        auto row = lineIndexToRow(lindex);

        while (amount-- > 0) {
            if (column == 0) {
                if (lindex == 0)
                    m_state.m_cursorPosition = Coordinates(0, 0);
                else
                    m_state.m_cursorPosition = foldedToUnfoldedCoords(lineCoordinates(rowToLineIndex(row - 1), -1));

            } else if (wordMode)
                m_state.m_cursorPosition = findPreviousWord(m_state.m_cursorPosition);
            else
                m_state.m_cursorPosition = foldedToUnfoldedCoords(Coordinates( lindex, column - 1));
        }

        if (select) {
            if (oldPos == m_interactiveSelection.m_start)
                m_interactiveSelection.m_start = m_state.m_cursorPosition;
            else if (oldPos == m_interactiveSelection.m_end)
                m_interactiveSelection.m_end = m_state.m_cursorPosition;
            else {
                m_interactiveSelection.m_start = m_state.m_cursorPosition;
                m_interactiveSelection.m_end = oldPos;
            }
        } else
            m_interactiveSelection.m_start = m_interactiveSelection.m_end = m_state.m_cursorPosition;
    }

    void TextEditor::moveRight(i32 amount, bool select, bool wordMode) {
        m_lines.resetCursorBlinkTime();
        m_lines.moveRight(amount, select, wordMode);
        setSelection(m_lines.m_interactiveSelection);
        m_lines.ensureCursorVisible();
    }

    void TextEditor::Lines::moveRight(i32 amount, bool select, bool wordMode) {
        auto oldPos = m_state.m_cursorPosition;
        auto foldedPos = unfoldedToFoldedCoords(oldPos);

        if (isEmpty() || foldedPos > lineCoordinates(-1, -1) || foldedPos == Invalid)
            return;

        auto lindex = foldedPos.m_line;
        auto line = operator[](lindex);
        auto lineMaxColumn = line.maxColumn();
        auto column = std::min(foldedPos.m_column, lineMaxColumn);
        auto row = lineIndexToRow(lindex);

        while (amount-- > 0) {
            if (line.isEndOfLine(foldedPos.m_column)) {
                if (isEndOfFile(foldedPos))
                    m_state.m_cursorPosition = lineCoordinates(-1, -1);
                else
                    m_state.m_cursorPosition = foldedToUnfoldedCoords(Coordinates(rowToLineIndex(row + 1), 0));

            } else if (wordMode)
                m_state.m_cursorPosition = findNextWord(m_state.m_cursorPosition);
            else
                m_state.m_cursorPosition = foldedToUnfoldedCoords(Coordinates(lindex, column + 1));
        }

        if (select) {
            if (oldPos == m_interactiveSelection.m_end)
                m_interactiveSelection.m_end = m_state.m_cursorPosition;
            else if (oldPos == m_interactiveSelection.m_start)
                m_interactiveSelection.m_start = m_state.m_cursorPosition;
            else {
                m_interactiveSelection.m_start = oldPos;
                m_interactiveSelection.m_end = m_state.m_cursorPosition;
            }
        } else
            m_interactiveSelection.m_start = m_interactiveSelection.m_end = m_state.m_cursorPosition;
    }

    void TextEditor::moveTop(bool select) {
        m_lines.resetCursorBlinkTime();
        auto oldPos = m_lines.m_state.m_cursorPosition;
        m_lines.setCursorPosition(m_lines.lineCoordinates(0, 0), false);

        if (m_lines.m_state.m_cursorPosition != oldPos) {
            if (select) {
                m_lines.m_interactiveSelection = Range(m_lines.m_state.m_cursorPosition, oldPos);
            } else
                m_lines.m_interactiveSelection.m_start = m_lines.m_interactiveSelection.m_end = m_lines.m_state.m_cursorPosition;
            setSelection(m_lines.m_interactiveSelection);
        }
    }

    void TextEditor::moveBottom(bool select) {
        m_lines.resetCursorBlinkTime();
        auto oldPos = getCursorPosition();
        auto newPos = m_lines.lineCoordinates(-1, -1);
        m_lines.setCursorPosition(newPos, false);
        if (select) {
            m_lines.m_interactiveSelection = Range(oldPos, newPos);
        } else
            m_lines.m_interactiveSelection.m_start = m_lines.m_interactiveSelection.m_end = newPos;
        setSelection(m_lines.m_interactiveSelection);
    }

    void TextEditor::moveHome(bool select) {
        m_lines.resetCursorBlinkTime();
        m_lines.moveHome(select);
        setSelection(m_lines.m_interactiveSelection);
    }

    void TextEditor::Lines::moveHome(bool select) {
        auto oldPos = m_state.m_cursorPosition;
        Coordinates foldedPos = oldPos;
        auto row = lineIndexToRow(oldPos.m_line);
        Line line;

        if (isMultiLineRow(row)) {
            line = m_foldedLines[row].m_foldedLine;
            foldedPos = unfoldedToFoldedCoords(oldPos);
        } else
            line = m_unfoldedLines[oldPos.m_line];

        auto prefix = line.substr(0, foldedPos.m_column);
        auto postfix = line.substr(foldedPos.m_column);
        if (prefix.empty() && postfix.empty())
            return;
        i32 home;
        if (!prefix.empty()) {
            auto idx = prefix.find_first_not_of(' ');
            if (idx == std::string::npos) {
                auto postIdx = postfix.find_first_of(' ');
                if (postIdx == std::string::npos || postIdx == 0)
                    home = 0;
                else {
                    postIdx = postfix.find_first_not_of(' ');
                    if (postIdx == std::string::npos)
                        home = lineMaxColumn(oldPos.m_line);
                    else if (postIdx == 0)
                        home = 0;
                    else
                        home = oldPos.m_column + postIdx;
                }
            } else
                home = idx;
        } else {
            auto postIdx = postfix.find_first_of(' ');
            if (postIdx == std::string::npos)
                home = 0;
            else {
                postIdx = postfix.find_first_not_of(' ');
                if (postIdx == std::string::npos)
                    home = lineMaxColumn(foldedPos.m_line);
                else
                    home = foldedPos.m_column + postIdx;
            }
        }
        if (isMultiLineRow(row)) {
            setCursorPosition(foldedToUnfoldedCoords(Coordinates(foldedPos.m_line, home)));
        } else
            setCursorPosition(Coordinates(m_state.m_cursorPosition.m_line, home));
        if (m_state.m_cursorPosition != oldPos) {
            if (select) {
                if (oldPos == m_interactiveSelection.m_start)
                    m_interactiveSelection.m_start = m_state.m_cursorPosition;
                else if (oldPos == m_interactiveSelection.m_end)
                    m_interactiveSelection.m_end = m_state.m_cursorPosition;
                else {
                    m_interactiveSelection.m_start = m_state.m_cursorPosition;
                    m_interactiveSelection.m_end = oldPos;
                }
            } else
                m_interactiveSelection.m_start = m_interactiveSelection.m_end = m_state.m_cursorPosition;
        }
    }

    void TextEditor::moveEnd(bool select) {
        m_lines.resetCursorBlinkTime();
        m_lines.moveEnd(select);
        setSelection(m_lines.m_interactiveSelection);
    }

    void TextEditor::Lines::moveEnd(bool select) {
        auto oldPos = m_state.m_cursorPosition;
        auto row = lineIndexToRow(oldPos.m_line);
        if (isMultiLineRow(row)) {
            auto position = unfoldedToFoldedCoords(m_state.m_cursorPosition);
            m_state.m_cursorPosition.m_column = position.m_column;
            setCursorPosition(foldedToUnfoldedCoords(Coordinates( position.m_line, lineMaxColumn(position.m_line))));
        } else {
            row = std::clamp(row, 0.0f, getGlobalRowMax());
            m_state.m_cursorPosition.m_line = rowToLineIndex(row);
            setCursorPosition(lineCoordinates(m_state.m_cursorPosition.m_line, lineMaxColumn(oldPos.m_line)));
        }

        if (m_state.m_cursorPosition != oldPos) {
            if (select) {
                if (oldPos == m_interactiveSelection.m_end)
                    m_interactiveSelection.m_end = m_state.m_cursorPosition;
                else if (oldPos == m_interactiveSelection.m_start)
                    m_interactiveSelection.m_start = m_state.m_cursorPosition;
                else {
                    m_interactiveSelection.m_start = oldPos;
                    m_interactiveSelection.m_end = m_state.m_cursorPosition;
                }
            } else
                m_interactiveSelection.m_start = m_interactiveSelection.m_end = m_state.m_cursorPosition;
        }
    }

    void TextEditor::Lines::setScrollY() {
        if (!m_withinRender) {
            m_setScrollY = true;
            return;
        } else {
            m_setScrollY = false;
            auto scrollY = ImGui::GetScrollY();
            ImGui::SetScrollY(std::clamp(scrollY + m_scrollYIncrement, 0.0f, ImGui::GetScrollMaxY()));
        }
    }

    void TextEditor::setScroll(ImVec2 scroll) {
        if (!m_lines.m_withinRender) {
            m_scroll = scroll;
            m_setScroll = true;
            return;
        } else {
            m_setScroll = false;
            ImGui::SetScrollX(scroll.x);
            ImGui::SetScrollY(scroll.y);
        }
    }

    void TextEditor::Lines::setFocusAtCoords(const Coordinates &coords, bool scrollToCursor) {
        m_focusAtCoords = coords;
        m_state.m_cursorPosition = coords;
        m_updateFocus = true;
        m_scrollToCursor = scrollToCursor;
    }

    void TextEditor::Lines::setCursorPosition(const Coordinates &position, bool unfoldIfNeeded, bool scrollToCursor) {
        if (m_state.m_cursorPosition != position)
            m_state.m_cursorPosition = lineCoordinates(position);

        m_unfoldIfNeeded = unfoldIfNeeded;
        m_focusAtCoords = m_state.m_cursorPosition;
        m_scrollToCursor = scrollToCursor;
        if (scrollToCursor)
            ensureCursorVisible();
    }

    void TextEditor::Lines::setCursorPosition() {
        setCursorPosition(m_state.m_selection.m_end);
    }

    bool TextEditor::Coordinates::isValid(Lines &lines) {

        auto maxLine = lines.size();
        if (std::abs(m_line) > maxLine)
            return false;
        auto maxColumn = lines.lineMaxColumn(m_line);
        return std::abs(m_column) > maxColumn;
    }

    TextEditor::Coordinates TextEditor::Coordinates::sanitize(Lines &lines) {

        i32 lineCount = lines.size();
        if (m_line < 0) {
            m_line = std::clamp(m_line, -lineCount, -1);
            m_line = lineCount + m_line;
        } else
            m_line = std::clamp(m_line, 0, lineCount - 1);


        auto maxColumn = lines.lineMaxColumn(m_line) + 1;
        if (m_column < 0) {
            m_column = std::clamp(m_column, -maxColumn, -1);
            m_column = maxColumn + m_column;
        } else
            m_column = std::clamp(m_column, 0, maxColumn);

        return *this;
    }

    TextEditor::Range::Coordinates TextEditor::lineCoordinates(i32 lineIndex, i32 column)  {
        return m_lines.lineCoordinates(lineIndex, column);
    }

    TextEditor::Range::Coordinates TextEditor::Lines::lineCoordinates(i32 lineIndex, i32 column)  {
        if (isEmpty())
            return {0, 0};
        Coordinates result(lineIndex, column);

        return result.sanitize(*this);
    }

    TextEditor::Coordinates TextEditor::lineCoordinates(const Coordinates &value)  {
        return m_lines.lineCoordinates(value);
    }

    TextEditor::Coordinates TextEditor::Lines::lineCoordinates(const Coordinates &value)  {
        auto result = value;

        return result.sanitize(*this);
    }

    TextEditor::Range TextEditor::lineCoordinates(const Range &value) {
        return m_lines.lineCoordinates(value);
    }

    TextEditor::Range TextEditor::Lines::lineCoordinates(const Range &value) {
        auto start = lineCoordinates(value.m_start);
        auto end = lineCoordinates(value.m_end);
        if (start == Invalid || end == Invalid)
            return {Invalid, Invalid};
        if (start > end) {
            std::swap(start, end);
        }
        return {start, end};
    }

    void TextEditor::advance(Coordinates &coordinates) {
        if (m_lines.isEndOfFile(coordinates))
            return;
        if (m_lines.isEndOfLine(coordinates)) {
            auto row = m_lines.lineIndexToRow(coordinates.m_line);
            coordinates.m_line = m_lines.rowToLineIndex(row + 1);

            coordinates.m_column = 0;
            return;
        }
        auto &line = m_lines.m_unfoldedLines[coordinates.m_line];
        i64 column = coordinates.m_column;
        std::string lineChar = line[column];
        auto incr = stringCharacterCount(lineChar);
        coordinates.m_column += incr;
    }

    TextEditor::Coordinates TextEditor::findWordStart(const Coordinates &from) {
        Coordinates at = m_lines.lineCoordinates(from);
        if (at.m_line >= m_lines.size())
            return at;

        auto &line = m_lines.m_unfoldedLines[at.m_line];
        auto charIndex = m_lines.lineCoordsIndex(at);

        bool found = false;
        while (charIndex > 0 && isWordChar(line.m_chars[charIndex - 1])) {
            found = true;
            --charIndex;
        }
        while (!found && charIndex > 0 && ispunct(line.m_chars[charIndex - 1])) {
            found = true;
            --charIndex;
        }
        while (!found && charIndex > 0 && isspace(line.m_chars[charIndex - 1]))
            --charIndex;
        return m_lines.lineIndexCoords(at.m_line + 1, charIndex);
    }

    TextEditor::Coordinates TextEditor::findWordEnd(const Coordinates &from) {
        Coordinates at = m_lines.lineCoordinates(from);
        if (at.m_line >= m_lines.size())
            return at;

        auto &line = m_lines.m_unfoldedLines[at.m_line];
        auto charIndex = m_lines.lineCoordsIndex(at);

        bool found = false;
        while (charIndex < (i32) line.m_chars.size() && isWordChar(line.m_chars[charIndex])) {
            found = true;
            ++charIndex;
        }
        while (!found && charIndex < (i32) line.m_chars.size() && ispunct(line.m_chars[charIndex])) {
            found = true;
            ++charIndex;
        }
        while (!found && charIndex < (i32) line.m_chars.size() && isspace(line.m_chars[charIndex]))
            ++charIndex;

        return m_lines.lineIndexCoords(at.m_line + 1, charIndex);
    }

    TextEditor::Coordinates TextEditor::Lines::findNextWord(const Coordinates &from) {
        Coordinates at = unfoldedToFoldedCoords(from);
        if (at.m_line >= size())
            return from;

        auto &line = operator[](at.m_line);
        auto charIndex = line.columnIndex(at.m_column);

        while (charIndex < (i32) line.m_chars.size() && isspace(line.m_chars[charIndex]))
            ++charIndex;
        bool found = false;
        while (charIndex < (i32) line.m_chars.size() && (isWordChar(line.m_chars[charIndex]))) {
            found = true;
            ++charIndex;
        }
        while (!found && charIndex < (i32) line.m_chars.size() && (ispunct(line.m_chars[charIndex])))
            ++charIndex;

        return foldedToUnfoldedCoords(lineIndexCoords(at.m_line + 1, charIndex));
    }

    TextEditor::Coordinates TextEditor::Lines::findPreviousWord(const Coordinates &from) {
        Coordinates at = unfoldedToFoldedCoords(from);
        if (at.m_line >= size())
            return from;

        auto &line = operator[](at.m_line);
        auto charIndex = line.columnIndex(at.m_column);

        bool found = false;
        while (charIndex > 0 && isspace(line.m_chars[charIndex - 1]))
            --charIndex;

        while (charIndex > 0 && isWordChar(line.m_chars[charIndex - 1])) {
            found = true;
            --charIndex;
        }
        while (!found && charIndex > 0 && ispunct(line.m_chars[charIndex - 1]))
            --charIndex;

        return foldedToUnfoldedCoords(lineIndexCoords(at.m_line + 1, charIndex));
    }

    u32 TextEditor::Line::skipSpaces(i32 charIndex) {
        u32 s;
        for (s = 0; charIndex < (i32) m_chars.size() && m_chars[charIndex] == ' ' && m_flags[charIndex] == 0x00; ++s, ++charIndex);
        return s;
    }

    u32 TextEditor::Lines::skipSpaces(const Coordinates &from) {
        auto lineIndex = from.m_line;
        if (lineIndex >= (i64) size())
            return 0;
        auto line = m_unfoldedLines[lineIndex];
        auto charIndex = lineCoordsIndex(lineCoordinates(lineIndex, from.m_column));
        return line.skipSpaces(charIndex);
    }

    bool TextEditor::MatchedDelimiter::setNearCursor(Lines *lines,const Coordinates &from) {
        Coordinates fromCopy = from;
        if (coordinatesNearDelimiter(lines, fromCopy)) {
            m_nearCursor = fromCopy;
            return true;
        }
        return false;
    }

    bool TextEditor::MatchedDelimiter::checkPosition(Lines *lines, Coordinates &from) {
        auto start = lines->lineCoordinates(from);
        auto lineIndex = start.m_line;
        auto line = lines->m_unfoldedLines[lineIndex].m_chars;
        auto colors = lines->m_unfoldedLines[lineIndex].m_colors;
        if (!line.empty() && colors.empty())
            return false;
        auto result = lines->lineCoordsIndex(start);
        auto character = line[result];
        auto color = colors[result];
        if ((s_separators.find(character) != std::string::npos && (static_cast<PaletteIndex>(color) == PaletteIndex::Separator)) || (static_cast<PaletteIndex>(color) == PaletteIndex::WarningText) ||
            (s_operators.find(character)  != std::string::npos && (static_cast<PaletteIndex>(color) == PaletteIndex::Operator))  || (static_cast<PaletteIndex>(color) == PaletteIndex::WarningText)) {
            if (from != lines->lineIndexCoords(lineIndex + 1, result)) {
                from = lines->lineIndexCoords(lineIndex + 1, result);
                m_changed = true;
            }
            m_active = true;
            return true;
        }
        return false;
    }

    i32 TextEditor::MatchedDelimiter::detectDirection(Lines *lines, const Coordinates &from) {
        auto start = lines->lineCoordinates(from);
        std::string delimiters = "()[]{}<>";
        i32 result = -2; // dont check either

        if (start == Invalid)
            return result;
        auto lineIndex = start.m_line;
        auto line = lines->m_unfoldedLines[lineIndex].m_chars;
        auto charIndex = lines->lineCoordsIndex(start);
        auto ch2 = line[charIndex];
        auto idx2 = delimiters.find(ch2);
        if (charIndex == 0) {// no previous character
            if (idx2 == std::string::npos) // no delimiters
                return -2;// dont check either
            else
                return 1; // check only current
        }// charIndex > 0
        auto ch1 = line[charIndex - 1];
        auto idx1 = delimiters.find(ch1);
        if (idx1 == std::string::npos && idx2 == std::string::npos) // no delimiters
            return -2;
        if (idx1 != std::string::npos && idx2 != std::string::npos) {
            if (idx1 % 2) // closing delimiter + any delimiter
                return -1; // check both and previous first
            else if (!(idx1 % 2) && !(idx2 % 2)) // opening delimiter + opening delimiter
                return 0; // check both and current first
        } else if (idx1 != std::string::npos) // only first delimiter
            return 1; // check only previous
        else  // only second delimiter
            return 2; // check only current

        return result;
    }

    bool TextEditor::MatchedDelimiter::coordinatesNearDelimiter(Lines *lines, Coordinates &from) {
        auto start = lines->lineCoordinates(from);
        if (lines->isEmpty())
            return false;

        if (start == Invalid)
            return false;
        auto lineIndex = start.m_line;
        auto charIndex = lines->lineCoordsIndex(start);
        auto direction1 = detectDirection(lines, start);
        auto charCoords = lines->lineIndexCoords(lineIndex + 1, charIndex);
        i32 direction2 = 1;
        if (direction1 == -1 || direction1 == 1) {
            Coordinates position = lines->lineCoordinates(charCoords.m_line, charCoords.m_column - 1);
            if (checkPosition(lines, position)) {
                from = position;
                return true;
            }
            if (direction1 == -1)
                direction2 = 0;
        } else if (direction1 == 2 || direction1 == 0) {
            if (checkPosition(lines, charCoords))
                return true;
            if (direction1 == 0)
                direction2 = -1;
        }
        if (direction2 != 1) {
            auto position = lines->lineCoordinates(charCoords.m_line, charCoords.m_column + direction2);
            if (checkPosition(lines, position)) {
                from = position;
                return true;
            }
        }
        u64 result;
        auto strLine = lines->m_unfoldedLines[lineIndex].m_chars;
        if (charIndex == 0) {
            if (strLine[0] == ' ')
                result = std::string::npos;
            else
                result = 0;
        } else
            result = strLine.find_last_not_of(' ', charIndex - 1);
        if (result != std::string::npos) {
            auto resultCoords = lines->lineIndexCoords(lineIndex + 1, result);
            if (checkPosition(lines, resultCoords)) {
                from = resultCoords;
                return true;
            }
        }
        result = strLine.find_first_not_of(' ', charIndex);
        if (result != std::string::npos) {
            auto resultCoords = lines->lineIndexCoords(lineIndex + 1, result);
            if (checkPosition(lines, resultCoords)) {
                from = resultCoords;
                return true;
            }
        }
        if (isActive()) {
            lines->m_unfoldedLines[m_nearCursor.m_line].m_colorized = false;
            lines->m_unfoldedLines[m_matched.m_line].m_colorized = false;
            m_active = false;
            lines->colorize();
        }
        return false;
    }

    TextEditor::Coordinates TextEditor::MatchedDelimiter::findMatchingDelimiter(Lines *lines, Coordinates &from, bool folded) {
        Coordinates start;
        Coordinates result = Invalid;
        if (!folded)
            start = from;
        else
            start = lines->unfoldedToFoldedCoords(lines->lineCoordinates(from));
        if (start == Invalid) {
            m_active = false;
            return result;
        }
        result = start;
        auto lineIndex = start.m_line;
        auto row = lines->lineIndexToRow(lineIndex);
        auto maxLineIndex = lines->size() - 1;
        auto charIndex = lines->lineCoordsIndex(start);
        std::string line;
        std::string colors;
        if (!folded) {
            line = lines->m_unfoldedLines[lineIndex].m_chars;
            colors = lines->m_unfoldedLines[lineIndex].m_colors;
        } else {
            line = lines->operator[](lineIndex).m_chars;
            colors = lines->operator[](lineIndex).m_colors;
        }
        if (!line.empty() && colors.empty()) {
            m_active = false;
            return Invalid;
        }
        std::string delimiters = "()[]{}<>";
        char delimiterChar = line[charIndex];
        char color1;
        auto idx = delimiters.find_first_of(delimiterChar);
        if (idx == std::string::npos) {
            if (m_active) {
                m_active = false;
                lines->colorize();
            }
            return Invalid;
        }
        auto delimiterChar2 = delimiters[idx ^ 1];
        delimiters = delimiterChar;
        delimiters += delimiterChar2;
        i32 direction = 1 - 2 * (idx % 2);
        if (idx > 5)
            color1 = static_cast<char>(PaletteIndex::Operator);
        else
            color1 = static_cast<char>(PaletteIndex::Separator);
        char color = static_cast<char>(PaletteIndex::WarningText);
        i32 depth = 1;
        if (charIndex == (i64) (line.size() - 1) * (1 + direction) / 2) {
            if (row + direction < 0 || lines->rowToLineIndex(row + direction) > (i64) maxLineIndex) {
                m_active = false;
                return Invalid;
            }
            if (!folded) {
                if (lineIndex += direction; lineIndex < 0 || lineIndex > (i64) maxLineIndex) {
                    m_active = false;
                    return Invalid;
                }
            } else {
                row += direction;
                if(lineIndex = lines->rowToLineIndex(row); lineIndex <0 || lineIndex > (i64) maxLineIndex) {
                    m_active = false;
                    return Invalid;
                }
            }
            if (!folded) {
                line = lines->m_unfoldedLines[lineIndex].m_chars;
                colors = lines->m_unfoldedLines[lineIndex].m_colors;
            } else {
                line = lines->operator[](lineIndex).m_chars;
                colors = lines->operator[](lineIndex).m_colors;
            }
            if (!line.empty() && colors.empty()) {
                m_active = false;
                return Invalid;
            }
            charIndex = (line.size() - 1) * (1 - direction) / 2 - direction;
        }
        for (i32 i = charIndex + direction;; i += direction) {
            if (direction == 1)
                idx = line.find_first_of(delimiters, i);
            else
                idx = line.find_last_of(delimiters, i);
            if (idx != std::string::npos) {
                if (line[idx] == delimiterChar && (colors[idx] == color || colors[idx] == color1)) {
                    ++depth;
                    i = idx;
                } else if ((line[idx] == delimiterChar2 && (colors[idx] == color)) || colors[idx] == color1) {
                    --depth;
                    if (depth == 0) {
                            if (!folded)
                                result = lines->lineIndexCoords(lineIndex + 1, idx);
                            else
                                result = lines->foldedToUnfoldedCoords( lines->lineIndexCoords(lineIndex + 1, idx));
                        m_active = true;
                        break;
                    }
                    i = idx;
                } else {
                    i = idx;
                }
            } else {
                if (direction == 1)
                    i = line.size() - 1;
                else
                    i = 0;
            }
            if ((direction * i) >= (i32) ((line.size() - 1) * (1 + direction) / 2)) {
                if (lines->rowToLineIndex(row) == (i64) maxLineIndex * ((1 + direction) >> 1)) {
                    if (m_active) {
                        m_active = false;
                        m_changed = true;
                    }
                    break;
                } else {
                    if (!folded) {
                        lineIndex += direction;
                    } else {
                        row += direction;
                        lineIndex = lines->rowToLineIndex(row);
                    }
                    if (lineIndex < 0 || lineIndex >= (i64) lines->size()) {
                        if (m_active) {
                            m_active = false;
                            m_changed = true;
                        }
                        return Invalid;
                    }
                    if (!folded) {
                        line = lines->m_unfoldedLines[lineIndex].m_chars;
                        colors = lines->m_unfoldedLines[lineIndex].m_colors;
                    } else {
                         line = lines->operator[](lineIndex).m_chars;
                         colors = lines->operator[](lineIndex).m_colors;
                    }
                    if (!line.empty() && colors.empty()) {
                        m_active = false;
                        return Invalid;
                    }
                    i = (line.size() - 1) * (1 - direction) / 2 - direction;
                }
            }
        }

        return result;
    }

    void TextEditor::MatchedDelimiter::findMatchingDelimiter(Lines *lines, bool folded) {
        Coordinates from = m_nearCursor;
        Coordinates result = findMatchingDelimiter(lines, from, folded);
        if (result != Invalid) {
            if (result != m_matched || hasChanged()) {
                m_matched = result;
                auto mls = (i32) lines->m_unfoldedLines.size();
                auto mline = m_matched.m_line;
                auto nline = m_nearCursor.m_line;
                if (mline < 0 || mline >= mls || nline < 0 || nline >= mls) {
                    m_active = false;
                    return;
                }
                i32 mcs = lines->m_unfoldedLines[mline].size();
                i32 ncs = lines->m_unfoldedLines[nline].size();
                auto mcol = m_matched.m_column;
                auto ncol = m_nearCursor.m_column;
                if (mcol < 0 || mcol >= mcs || ncol < 0 || ncol >= ncs) {
                    m_active = false;
                    return;
                }
                lines->m_unfoldedLines[m_nearCursor.m_line].m_colorized = false;
                lines->m_unfoldedLines[m_matched.m_line].m_colorized = false;

                lines->colorize();
                m_changed = false;
            }
        }
    }
}
