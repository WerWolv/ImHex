#include "imgui.h"
#include <ui/text_editor.hpp>
#include <algorithm>


namespace hex::ui {
    using Coordinates       = TextEditor::Coordinates;
    using Range             = TextEditor::Range;
    using Line              = TextEditor::Line;
    using Lines             = TextEditor::Lines;
    using MatchedDelimiter  = TextEditor::MatchedDelimiter;


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

    Coordinates Lines::rfind( const std::string &text, const Coordinates &from) {
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


    Coordinates Lines::find(const std::string &text, const Coordinates &from) {
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

    void Lines::resetInteractiveSelection() {
        m_interactiveSelection.m_start = m_interactiveSelection.m_end = m_state.m_cursorPosition;
    }

    void Lines::selectUsingStart(bool select, const Coordinates &oldPos) {
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
            resetInteractiveSelection();
    }


    void Lines::selectUsingEnd(bool select, const Coordinates &oldPos) {
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
            resetInteractiveSelection();
    }

    void Lines::moveToMatchedDelimiter(bool select) {
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

    void Lines::moveUp(i32 amount, bool select) {
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

        if (oldPos != m_state.m_cursorPosition)
            selectUsingEnd(select, oldPos);
    }

    void TextEditor::moveDown(i32 amount, bool select) {
        m_lines.resetCursorBlinkTime();
        m_lines.moveDown(amount, select);
        setSelection(m_lines.m_interactiveSelection);

        m_lines.ensureCursorVisible();
    }

    void Lines::moveDown(i32 amount, bool select) {
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

        if (m_state.m_cursorPosition != oldPos)
            selectUsingStart(select, oldPos);
    }

    void TextEditor::moveLeft(i32 amount, bool select, bool wordMode) {
        m_lines.resetCursorBlinkTime();
        m_lines.moveLeft(amount, select, wordMode);
        setSelection(m_lines.m_interactiveSelection);
        m_lines.ensureCursorVisible();
    }

    void Lines::moveLeft(i32 amount, bool select, bool wordMode) {
        auto oldPos = m_state.m_cursorPosition;
        auto foldedPos = unfoldedToFoldedCoords(oldPos);

        if (isEmpty() || foldedPos < Coordinates( 0, 0) || foldedPos == Invalid)
            return;

        auto lindex = foldedPos.m_line;
        auto lineMaxCol = lineMaxColumn(lindex);
        auto column = std::min(foldedPos.m_column, lineMaxCol);
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

        selectUsingEnd(select, oldPos);
    }

    void TextEditor::moveRight(i32 amount, bool select, bool wordMode) {
        m_lines.resetCursorBlinkTime();
        m_lines.moveRight(amount, select, wordMode);
        setSelection(m_lines.m_interactiveSelection);
        m_lines.ensureCursorVisible();
    }

    void Lines::moveRight(i32 amount, bool select, bool wordMode) {
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

        selectUsingStart(select, oldPos);
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

    void Lines::moveHome(bool select) {
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

        if (m_state.m_cursorPosition != oldPos)
            selectUsingEnd(select, oldPos);
    }

    void TextEditor::moveEnd(bool select) {
        m_lines.resetCursorBlinkTime();
        m_lines.moveEnd(select);
        setSelection(m_lines.m_interactiveSelection);
    }

    void Lines::moveEnd(bool select) {
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

        if (m_state.m_cursorPosition != oldPos)
            selectUsingStart(select, oldPos);

    }

    void Lines::setScrollY() {
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
        if (m_lines.m_withinRender) {
            ImGui::SetScrollX(scroll.x);
            ImGui::SetScrollY(scroll.y);
            m_setScroll = false;
        } else {
            m_scroll = scroll;
            m_setScroll = true;
        }
    }

    void TextEditor::setFocus(bool scrollToCursor) {
        m_lines.setFocusAtCoords(m_lines.m_state.m_cursorPosition, scrollToCursor);
    }

    void Lines::setFocusAtCoords(const Coordinates &coords, bool scrollToCursor) {
        m_focusAtCoords = coords;
        m_state.m_cursorPosition = coords;
        m_updateFocus = true;
        m_scrollToCursor = scrollToCursor;
    }

    void Lines::setCursorPosition(const Coordinates &position, bool unfoldIfNeeded, bool scrollToCursor) {
        auto positionCoordinates = lineCoordinates(position);
        if (m_state.m_cursorPosition != positionCoordinates)
            m_state.m_cursorPosition = positionCoordinates;

        m_unfoldIfNeeded = unfoldIfNeeded;
        m_focusAtCoords = m_state.m_cursorPosition;
        m_scrollToCursor = scrollToCursor;
        if (scrollToCursor)
            ensureCursorVisible();
    }

    void Lines::setCursorPosition() {
        setCursorPosition(m_state.m_selection.m_end);
    }

    void Lines::setEditorState(const Coordinates &coordinates, bool setInteractiveStart) {
        m_state.m_cursorPosition = m_interactiveSelection.m_end  =  coordinates;
        if (setInteractiveStart)
            m_interactiveSelection.m_start = coordinates;
        setSelection(m_interactiveSelection);
    }

    bool Coordinates::isValid(Lines &lines) {

        auto maxLine = lines.size();
        if (std::abs(m_line) > maxLine)
            return false;
        auto maxColumn = lines.lineMaxColumn(m_line);
        return std::abs(m_column) <= maxColumn;
    }

    Coordinates Coordinates::sanitize(Lines &lines) {

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

    Coordinates TextEditor::lineCoordinates(i32 lineIndex, i32 column)  {
        return m_lines.lineCoordinates(lineIndex, column);
    }

    Coordinates Lines::lineCoordinates(i32 lineIndex, i32 column)  {
        if (isEmpty())
            return {0, 0};
        Coordinates result(lineIndex, column);

        return result.sanitize(*this);
    }

    Coordinates TextEditor::lineCoordinates(const Coordinates &value)  {
        return m_lines.lineCoordinates(value);
    }

    Coordinates Lines::lineCoordinates(const Coordinates &value)  {
        auto result = value;

        return result.sanitize(*this);
    }

    Range TextEditor::lineCoordinates(const Range &value) {
        return m_lines.lineCoordinates(value);
    }

    Range TextEditor::Lines::lineCoordinates(const Range &value) {
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

    Coordinates TextEditor::findWordStart(const Coordinates &from) {
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

    Coordinates TextEditor::findWordEnd(const Coordinates &from) {
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

    Coordinates Lines::findNextWord(const Coordinates &from) {
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

    Coordinates Lines::findPreviousWord(const Coordinates &from) {
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

    u32 Line::skipSpaces(i32 charIndex) {
        u32 s;
        for (s = 0; charIndex < (i32) m_chars.size() && m_chars[charIndex] == ' ' && (m_flags[charIndex] & ~(static_cast<u8>(FlagValues::Preprocessor) >> 1))== 0x00; ++s, ++charIndex);
        return s;
    }

    u32 Lines::skipSpaces(const Coordinates &from) {
        auto lineIndex = from.m_line;
        if (lineIndex >= (i64) size())
            return 0;
        auto line = m_unfoldedLines[lineIndex];
        auto charIndex = lineCoordsIndex(lineCoordinates(lineIndex, from.m_column));
        return line.skipSpaces(charIndex);
    }

    bool MatchedDelimiter::setNearCursor(Lines *lines,const Coordinates &from) {
        Coordinates fromCopy = from;
        if (fromCopy.m_line >= lines->size())
            return false;
        if (m_nearCursor.m_line >= lines->size()) {
            m_nearCursor = Invalid;
            return false;
        }
        if (coordinatesNearDelimiter(lines, fromCopy)) {
            m_nearCursor = fromCopy;
            return true;
        }
        return false;
    }

    bool MatchedDelimiter::checkPosition(Lines *lines, Coordinates &from) {
        auto start = lines->lineCoordinates(from);
        auto lineIndex = start.m_line;
        auto line = lines->m_unfoldedLines[lineIndex].m_chars;
        auto colors = lines->m_unfoldedLines[lineIndex].m_colors;
        if (!line.empty() && colors.empty())
            return false;
        auto result = lines->lineCoordsIndex(start);
        auto character = line[result];
        auto color = colors[result];
        if ((s_separators.find(character) != std::string::npos && (static_cast<PaletteIndex>(color) == PaletteIndex::Separator || static_cast<PaletteIndex>(color) == PaletteIndex::WarningText)) ||
            (s_operators.find(character)  != std::string::npos && (static_cast<PaletteIndex>(color) == PaletteIndex::Operator || static_cast<PaletteIndex>(color) == PaletteIndex::WarningText))) {
            if (from != lines->lineIndexCoords(lineIndex + 1, result)) {
                from = lines->lineIndexCoords(lineIndex + 1, result);
                m_changed = true;
            }
            m_active = true;
            return true;
        }
        return false;
    }

    i32 MatchedDelimiter::detectDirection(Lines *lines, const Coordinates &from) {
        auto start = lines->lineCoordinates(from);

        i32 result = -2; // don't check either previous or current char

        if (start == Invalid)
            return result;
        auto lineIndex = start.m_line;
        auto line = lines->m_unfoldedLines[lineIndex].m_chars;
        auto charIndex = lines->lineCoordsIndex(start);
        auto ch2 = line[charIndex];
        auto idx2 = s_delimiters.find(ch2);
        if (charIndex == 0) {// no previous character on same line
            if (idx2 == std::string::npos) // no delimiters found
                return -2;// dont check either
            else
                return 1; // check only current char
        }
        // cursor is not at first char in line.
        auto ch1 = line[charIndex - 1];
        auto idx1 = s_delimiters.find(ch1);
        if (idx1 == std::string::npos && idx2 == std::string::npos) // no delimiters
            return -2;
        if (idx1 != std::string::npos && idx2 != std::string::npos) { //two delimiters detected
            if (idx1 % 2) // closing delimiter + any delimiter
                return -1; // check both directions, but check previous char first
            else if (!(idx1 % 2) && !(idx2 % 2)) // opening delimiter + opening delimiter
                return 0; // check both directions,  but current char first
        } else if (idx1 != std::string::npos) // only first delimiter
            return 1; // check only previous char
        else  // only second delimiter
            return 2; // check only current char

        return result;
    }

    bool MatchedDelimiter::coordinatesNearDelimiter(Lines *lines, Coordinates &from) {
        auto start = lines->lineCoordinates(from);
        if (lines->isEmpty())
            return false;

        if (start == Invalid)
            return false;
        auto lineIndex = start.m_line;
        if(lineIndex >= (i64) lines->size())
            return false;
        auto charIndex = lines->lineCoordsIndex(start);
        auto direction1 = detectDirection(lines, start);
        auto charCoords = lines->lineIndexCoords(lineIndex + 1, charIndex);
        i32 direction2 = 1;
        if (direction1 == -1 || direction1 == 1) {
            if (charCoords.m_column > 0) {
                Coordinates position = lines->lineCoordinates(charCoords.m_line, charCoords.m_column - 1);
                if (checkPosition(lines, position)) {
                    from = position;
                    return true;
                }
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
            if (charCoords.m_column + direction2 >= 0) {
                auto position = lines->lineCoordinates(charCoords.m_line, charCoords.m_column + direction2);
                if (checkPosition(lines, position)) {
                    from = position;
                    return true;
                }
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
        if (isActive() && m_nearCursor.m_line < lines->size() && m_matched.m_line < lines->size()) {
            lines->m_unfoldedLines[m_nearCursor.m_line].m_colorized = false;
            lines->m_unfoldedLines[m_matched.m_line].m_colorized = false;
            m_active = false;
            lines->colorize();
        }
        return false;
    }

    Line *MatchedDelimiter::getLine(Lines *lines, i32 lineIndex, bool folded) {
        if (folded)
            return &lines->operator[](lineIndex);

        return &lines->m_unfoldedLines[lineIndex];
    }

    Coordinates MatchedDelimiter::findMatchingDelimiter(Lines *lines, Coordinates &from, bool folded) {
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
        Line *line = getLine(lines, lineIndex, folded);

        auto idx = s_delimiters.find_first_of(line->m_chars[charIndex]);
        if (idx == std::string::npos) {
            if (m_active) {
                m_active = false;
                lines->colorize();
            }
            return Invalid;
        }

        std::string auxiliaryDelimiters, delimiters = std::string(1,s_delimiters[idx ^ 1]);
        delimiters += s_delimiters[idx];
        i32 direction = 1 - ((idx & 1) << 1);
        i32 auxDepth = 0;

        char color1, auxColor = static_cast<char>(PaletteIndex::Separator);
        bool firstTimeAux = false;
        bool useIdx = false;
        if (idx > 5) {
            auxiliaryDelimiters += s_delimiters[(idx - 6) ^ 1];
            auxiliaryDelimiters += s_delimiters[idx - 6];
            firstTimeAux = true;
            color1 = static_cast<char>(PaletteIndex::Operator);
        } else
            color1 = static_cast<char>(PaletteIndex::Separator);
        char color = static_cast<char>(PaletteIndex::WarningText);
        u64 auxIdx;
        i32 depth = 1;
        if (charIndex == (i64) (line->size() - 1) * ((1 + direction) >> 1)) {

            if (folded) {
                row += direction;
                lineIndex = lines->rowToLineIndex(row);
            } else
                lineIndex += direction;

            if (row < 0 ||lineIndex < 0 || lineIndex > (i64) maxLineIndex) {
                m_active = false;
                return Invalid;
            }

            line = getLine(lines, lineIndex, folded);

            charIndex = (line->size() - 1) * ((1 - direction) >> 1) - direction;
        }
        for (i32 i = charIndex + direction;; i += direction) {
            if (auxDepth > 0 || firstTimeAux) {
                if (direction == 1) {
                    auxIdx = line->m_chars.find_first_of(auxiliaryDelimiters, i);
                    if (firstTimeAux) {
                        idx = line->m_chars.find_first_of(delimiters, i);
                        if (idx < auxIdx)
                            useIdx = true;
                        firstTimeAux = false;
                    }
                } else {
                    auxIdx = line->m_chars.find_last_of(auxiliaryDelimiters, i);
                    if (firstTimeAux) {
                        idx = line->m_chars.find_last_of(delimiters, i);
                        if (idx > auxIdx)
                            useIdx = true;
                        firstTimeAux = false;
                    }
                }
            } else
                auxIdx = std::string::npos;

            if (!useIdx && auxIdx != std::string::npos) {
                if (line->m_chars[auxIdx] == auxiliaryDelimiters[1] && (line->m_colors[auxIdx] == color || line->m_colors[auxIdx] == auxColor)) {
                    ++auxDepth;
                    i = auxIdx;
                } else if (line->m_chars[auxIdx] == auxiliaryDelimiters[0] && (line->m_colors[auxIdx] == color || line->m_colors[auxIdx] == auxColor)) {
                    --auxDepth;
                    if (auxDepth >= 0)
                        i = auxIdx;
                }
            } else {
                if (!useIdx) {
                    if (direction == 1)
                        idx = line->m_chars.find_first_of(delimiters, i);
                    else
                        idx = line->m_chars.find_last_of(delimiters, i);
                } else
                    useIdx = false;

                if (idx != std::string::npos) {
                    if (line->m_chars[idx] == delimiters[1] && (line->m_colors[idx] == color || line->m_colors[idx] == color1)) {
                        ++depth;
                        i = idx;
                    } else if (line->m_chars[idx] == delimiters[0] && (line->m_colors[idx] == color || line->m_colors[idx] == color1)) {
                        --depth;
                        if (depth == 0) {
                            if (!folded)
                                result = lines->lineIndexCoords(lineIndex + 1, idx);
                            else
                                result = lines->foldedToUnfoldedCoords(lines->lineIndexCoords(lineIndex + 1, idx));
                            m_active = true;
                            break;
                        }
                        i = idx;
                    } else {
                        i = idx;
                    }
                } else {
                    i = (line->size() - 1) * ((1 + direction) >> 1);
                }
            }
            if ((direction * i) >= (i32) ((line->size() - 1) * ((1 + direction) >> 1))) {
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

                    line = getLine(lines, lineIndex, folded);

                    i = (line->size() - 1) * ((1 - direction) >> 1) - direction;
                }
            }
        }

        return result;
    }

    void MatchedDelimiter::findMatchingDelimiter(Lines *lines, bool folded) {
        Coordinates from = m_nearCursor;
        Coordinates result = findMatchingDelimiter(lines, from, folded);
        if (result != Invalid) {
            if (result != m_matched || hasChanged()) {
                from = result;
                Coordinates nearCursor = findMatchingDelimiter(lines, from, folded);
                if (nearCursor != m_nearCursor) {
                    m_active = false;
                    return;
                }
                m_matched = result;
                auto linesSize = (i32) lines->m_unfoldedLines.size();
                auto matchedLine = m_matched.m_line;
                auto nearLine = m_nearCursor.m_line;
                if (matchedLine < 0 || matchedLine >= linesSize || nearLine < 0 || nearLine >= linesSize) {
                    m_active = false;
                    return;
                }
                i32 matchedLineSize = lines->m_unfoldedLines[matchedLine].size();
                i32 nearLineSize = lines->m_unfoldedLines[nearLine].size();
                auto matchedColumn = m_matched.m_column;
                auto nearColumn = m_nearCursor.m_column;
                if (matchedColumn < 0 || matchedColumn >= matchedLineSize || nearColumn < 0 || nearColumn >= nearLineSize) {
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
