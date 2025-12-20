#include "imgui.h"
#include <ui/text_editor.hpp>
#include <algorithm>


namespace hex::ui {
    static bool isWordChar(char c) {
        auto asUChar = static_cast<u8>(c);
        return std::isalnum(asUChar) || c == '_' || asUChar > 0x7F;
    }

    void TextEditor::jumpToLine(i32 line) {
        auto newPos = m_state.m_cursorPosition;
        if (line != -1) {
            newPos = setCoordinates(line, 0);
        }
        jumpToCoords(newPos);
    }

    void TextEditor::jumpToCoords(const Coordinates &coords) {
        setSelection(Range(coords, coords));
        setCursorPosition(coords, true);
        ensureCursorVisible();

        setFocusAtCoords(coords, true);
    }

    void TextEditor::moveToMatchedBracket(bool select) {
        resetCursorBlinkTime();
        if (m_matchedBracket.isNearABracket(this, m_state.m_cursorPosition)) {
            m_matchedBracket.findMatchingBracket(this);
            auto oldPos = m_matchedBracket.m_nearCursor;
            auto newPos = m_matchedBracket.m_matched;
            if (newPos != setCoordinates(-1, -1)) {
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

    void TextEditor::moveUp(i32 amount, bool select) {
        resetCursorBlinkTime();
        auto oldPos = m_state.m_cursorPosition;
        if (amount < 0) {
            m_scrollYIncrement = -1.0;
            setScrollY();
            return;
        }
        m_state.m_cursorPosition.m_line = std::max(0, m_state.m_cursorPosition.m_line - amount);
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
            setSelection(m_interactiveSelection);

            ensureCursorVisible();
        }
    }

    void TextEditor::moveDown(i32 amount, bool select) {
        IM_ASSERT(m_state.m_cursorPosition.m_column >= 0);
        resetCursorBlinkTime();
        auto oldPos = m_state.m_cursorPosition;
        if (amount < 0) {
            m_scrollYIncrement = 1.0;
            setScrollY();
            return;
        }

        m_state.m_cursorPosition.m_line = std::clamp(m_state.m_cursorPosition.m_line + amount, 0, (i32) m_lines.size() - 1);
        if (oldPos.m_line == (i64) (m_lines.size() - 1)) {
            m_topLine += amount;
            m_topLine = std::clamp(m_topLine, 0.0F, m_lines.size() - 1.0F);
            setTopLine();
            ensureCursorVisible();
            return;
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
            setSelection(m_interactiveSelection);

            ensureCursorVisible();
        }
    }

    void TextEditor::moveLeft(i32 amount, bool select, bool wordMode) {
        resetCursorBlinkTime();

        auto oldPos = m_state.m_cursorPosition;


        if (isEmpty() || oldPos < Coordinates(0, 0))
            return;

        auto lindex = m_state.m_cursorPosition.m_line;
        auto lineMaxColumn = this->lineMaxColumn(lindex);
        auto column = std::min(m_state.m_cursorPosition.m_column, lineMaxColumn);

        while (amount-- > 0) {
            if (column == 0) {
                if (lindex == 0)
                    m_state.m_cursorPosition = Coordinates(0, 0);
                else {
                    lindex--;
                    m_state.m_cursorPosition = setCoordinates(lindex, -1);
                }
            } else if (wordMode)
                m_state.m_cursorPosition = findPreviousWord(m_state.m_cursorPosition);
            else
                m_state.m_cursorPosition = Coordinates(lindex, column - 1);
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

        setSelection(m_interactiveSelection);

        ensureCursorVisible();
    }

    void TextEditor::moveRight(i32 amount, bool select, bool wordMode) {
        resetCursorBlinkTime();

        auto oldPos = m_state.m_cursorPosition;

        if (isEmpty() || oldPos > setCoordinates(-1, -1))
            return;

        auto lindex = m_state.m_cursorPosition.m_line;
        auto lineMaxColumn = this->lineMaxColumn(lindex);
        auto column = std::min(m_state.m_cursorPosition.m_column, lineMaxColumn);

        while (amount-- > 0) {
            if (isEndOfLine(oldPos)) {
                if (!isEndOfFile(oldPos)) {
                    lindex++;
                    m_state.m_cursorPosition = Coordinates(lindex, 0);
                } else
                    m_state.m_cursorPosition = setCoordinates(-1, -1);
            } else if (wordMode)
                m_state.m_cursorPosition = findNextWord(m_state.m_cursorPosition);
            else
                m_state.m_cursorPosition = Coordinates(lindex, column + 1);
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

        setSelection(m_interactiveSelection);

        ensureCursorVisible();
    }

    void TextEditor::moveTop(bool select) {
        resetCursorBlinkTime();
        auto oldPos = m_state.m_cursorPosition;
        setCursorPosition(setCoordinates(0, 0), false);

        if (m_state.m_cursorPosition != oldPos) {
            if (select) {
                m_interactiveSelection = Range(m_state.m_cursorPosition, oldPos);
            } else
                m_interactiveSelection.m_start = m_interactiveSelection.m_end = m_state.m_cursorPosition;
            setSelection(m_interactiveSelection);
        }
    }

    void TextEditor::TextEditor::moveBottom(bool select) {
        resetCursorBlinkTime();
        auto oldPos = getCursorPosition();
        auto newPos = setCoordinates(-1, -1);
        setCursorPosition(newPos, false);
        if (select) {
            m_interactiveSelection = Range(oldPos, newPos);
        } else
            m_interactiveSelection.m_start = m_interactiveSelection.m_end = newPos;
        setSelection(m_interactiveSelection);
    }

    void TextEditor::moveHome(bool select) {
        resetCursorBlinkTime();
        auto oldPos = m_state.m_cursorPosition;

        auto &line = m_lines[oldPos.m_line];
        auto prefix = line.substr(0, oldPos.m_column);
        auto postfix = line.substr(oldPos.m_column);
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
                        home = this->lineMaxColumn(oldPos.m_line);
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
                    home = lineMaxColumn(oldPos.m_line);
                else
                    home = oldPos.m_column + postIdx;
            }
        }

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
            setSelection(m_interactiveSelection);
        }
    }

    void TextEditor::moveEnd(bool select) {
        resetCursorBlinkTime();
        auto oldPos = m_state.m_cursorPosition;
        setCursorPosition(setCoordinates(m_state.m_cursorPosition.m_line, lineMaxColumn(oldPos.m_line)));

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
            setSelection(m_interactiveSelection);
        }
    }

    void TextEditor::setScrollY() {
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
        if (!m_withinRender) {
            m_scroll = scroll;
            m_setScroll = true;
            return;
        } else {
            m_setScroll = false;
            ImGui::SetScrollX(scroll.x);
            ImGui::SetScrollY(scroll.y);
            //m_updateFocus = true;
        }
    }

    void TextEditor::setFocusAtCoords(const Coordinates &coords, bool scrollToCursor) {
        m_focusAtCoords = coords;
        m_state.m_cursorPosition = coords;
        m_updateFocus = true;
        m_scrollToCursor = scrollToCursor;
    }


        void TextEditor::setCursorPosition(const Coordinates &position, bool scrollToCursor) {
        if (m_state.m_cursorPosition != position) {
            m_state.m_cursorPosition = position;
            m_scrollToCursor = scrollToCursor;
            if (scrollToCursor)
                ensureCursorVisible();
        }
    }

    void TextEditor::setCursorPosition() {
        setCursorPosition(m_state.m_selection.m_end);
    }

    TextEditor::Coordinates::Coordinates(TextEditor *editor, i32 line, i32 column)
        : m_line(line), m_column(column) {
        sanitize(editor);
    }

    bool TextEditor::Coordinates::isValid(TextEditor *editor) const {

        auto maxLine = editor->m_lines.size();
        if (std::abs(m_line) > (i32) maxLine)
            return false;
        auto maxColumn = editor->lineMaxColumn(m_line);
        return std::abs(m_column) <= maxColumn;
    }

    TextEditor::Coordinates TextEditor::Coordinates::sanitize(TextEditor *editor) {

        i32 lineCount = editor->m_lines.size();
        if (m_line < 0) {
            m_line = std::clamp(m_line, -lineCount, -1);
            m_line = lineCount + m_line;
        } else
            m_line = std::clamp(m_line, 0, lineCount - 1);


        auto maxColumn = editor->lineMaxColumn(m_line) + 1;
        if (m_column < 0) {
            m_column = std::clamp(m_column, -maxColumn, -1);
            m_column = maxColumn + m_column;
        } else
            m_column = std::clamp(m_column, 0, maxColumn);

        return *this;
    }

    TextEditor::Coordinates TextEditor::setCoordinates(i32 line, i32 column) {
        if (isEmpty())
            return {0, 0};
        return {this, line, column};
    }

    TextEditor::Coordinates TextEditor::setCoordinates(const Coordinates &value) {
        auto sanitized_value = setCoordinates(value.m_line, value.m_column);
        return sanitized_value;
    }

    TextEditor::Range TextEditor::setCoordinates(const Range &value) {
        auto start = setCoordinates(value.m_start);
        auto end = setCoordinates(value.m_end);
        if (start == Invalid || end == Invalid)
            return {Invalid, Invalid};
        if (start > end) {
            std::swap(start, end);
        }
        return {start, end};
    }

    void TextEditor::advance(Coordinates &coordinates) const {
        if (isEndOfFile(coordinates))
            return;
        if (isEndOfLine(coordinates)) {
            ++coordinates.m_line;
            coordinates.m_column = 0;
            return;
        }
        auto &line = m_lines[coordinates.m_line];
        i64 column = coordinates.m_column;
        std::string lineChar = line[column];
        auto incr = stringCharacterCount(lineChar);
        coordinates.m_column += incr;
    }

    TextEditor::Coordinates TextEditor::findWordStart(const Coordinates &from) {
        Coordinates at = setCoordinates(from);
        if (at.m_line >= (i32) m_lines.size())
            return at;

        auto &line = m_lines[at.m_line];
        auto charIndex = lineCoordinatesToIndex(at);

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
        return getCharacterCoordinates(at.m_line, charIndex);
    }

    TextEditor::Coordinates TextEditor::findWordEnd(const Coordinates &from) {
        Coordinates at = from;
        if (at.m_line >= (i32) m_lines.size())
            return at;

        auto &line = m_lines[at.m_line];
        auto charIndex = lineCoordinatesToIndex(at);

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

        return getCharacterCoordinates(at.m_line, charIndex);
    }

    TextEditor::Coordinates TextEditor::findNextWord(const Coordinates &from)  {
        Coordinates at = from;
        if (at.m_line >= (i32) m_lines.size())
            return at;

        auto &line = m_lines[at.m_line];
        auto charIndex = lineCoordinatesToIndex(at);

        while (charIndex < (i32) line.m_chars.size() && isspace(line.m_chars[charIndex]))
            ++charIndex;
        bool found = false;
        while (charIndex < (i32) line.m_chars.size() && (isWordChar(line.m_chars[charIndex]))) {
            found = true;
            ++charIndex;
        }
        while (!found && charIndex < (i32) line.m_chars.size() && (ispunct(line.m_chars[charIndex])))
            ++charIndex;

        return getCharacterCoordinates(at.m_line, charIndex);
    }

    TextEditor::Coordinates TextEditor::findPreviousWord(const Coordinates &from) {
        Coordinates at = from;
        if (at.m_line >= (i32) m_lines.size())
            return at;

        auto &line = m_lines[at.m_line];
        auto charIndex = lineCoordinatesToIndex(at);

        bool found = false;
        while (charIndex > 0 && isspace(line.m_chars[charIndex - 1]))
            --charIndex;

        while (charIndex > 0 && isWordChar(line.m_chars[charIndex - 1])) {
            found = true;
            --charIndex;
        }
        while (!found && charIndex > 0 && ispunct(line.m_chars[charIndex - 1]))
            --charIndex;

        return getCharacterCoordinates(at.m_line, charIndex);
    }

    u32 TextEditor::skipSpaces(const Coordinates &from) {
        auto line = from.m_line;
        if (line >= (i64) m_lines.size())
            return 0;
        auto &lines = m_lines[line].m_chars;
        auto &colors = m_lines[line].m_colors;
        auto charIndex = lineCoordinatesToIndex(from);
        u32 s = 0;
        while (charIndex < (i32) lines.size() && lines[charIndex] == ' ' && colors[charIndex] == 0x00) {
            ++s;
            ++charIndex;
        }
        if (m_updateFocus)
            setFocus();
        return s;
    }

    bool TextEditor::MatchedBracket::checkPosition(TextEditor *editor, const Coordinates &from) {
        auto lineIndex = from.m_line;
        auto line = editor->m_lines[lineIndex].m_chars;
        auto colors = editor->m_lines[lineIndex].m_colors;
        if (!line.empty() && colors.empty())
            return false;
        auto result = editor->lineCoordinatesToIndex(from);
        auto character = line[result];
        auto color = colors[result];
        if ((s_separators.find(character) != std::string::npos && (static_cast<PaletteIndex>(color) == PaletteIndex::Separator)) || (static_cast<PaletteIndex>(color) == PaletteIndex::WarningText) ||
            (s_operators.find(character)  != std::string::npos && (static_cast<PaletteIndex>(color) == PaletteIndex::Operator))  || (static_cast<PaletteIndex>(color) == PaletteIndex::WarningText)) {
            if (m_nearCursor != editor->getCharacterCoordinates(lineIndex, result)) {
                m_nearCursor = editor->getCharacterCoordinates(lineIndex, result);
                m_changed = true;
            }
            m_active = true;
            return true;
        }
        return false;
    }

    i32 TextEditor::MatchedBracket::detectDirection(TextEditor *editor, const Coordinates &from) {
        std::string brackets = "()[]{}<>";
        i32 result = -2; // dont check either
        auto start = editor->setCoordinates(from);
        if (start == TextEditor::Invalid)
            return result;
        auto lineIndex = start.m_line;
        auto line = editor->m_lines[lineIndex].m_chars;
        auto charIndex = editor->lineCoordinatesToIndex(start);
        auto ch2 = line[charIndex];
        auto idx2 = brackets.find(ch2);
        if (charIndex == 0) {// no previous character
            if (idx2 == std::string::npos) // no brackets
                return -2;// dont check either
            else
                return 1; // check only current
        }// charIndex > 0
        auto ch1 = line[charIndex - 1];
        auto idx1 = brackets.find(ch1);
        if (idx1 == std::string::npos && idx2 == std::string::npos) // no brackets
            return -2;
        if (idx1 != std::string::npos && idx2 != std::string::npos) {
            if (idx1 % 2) // closing bracket + any bracket
                return -1; // check both and previous first
            else if (!(idx1 % 2) && !(idx2 % 2)) // opening bracket + opening bracket
                return 0; // check both and current first
        } else if (idx1 != std::string::npos) // only first bracket
            return 1; // check only previous
        else  // only second bracket
            return 2; // check only current

        return result;
    }

    bool TextEditor::MatchedBracket::isNearABracket(TextEditor *editor, const Coordinates &from) {
        if (editor->isEmpty())
            return false;
        auto start = editor->setCoordinates(from);
        if (start == TextEditor::Invalid)
            return false;
        auto lineIndex = start.m_line;
        auto charIndex = editor->lineCoordinatesToIndex(start);
        auto direction1 = detectDirection(editor, start);
        auto charCoords = editor->getCharacterCoordinates(lineIndex, charIndex);
        i32 direction2 = 1;
        if (direction1 == -1 || direction1 == 1) {
            if (checkPosition(editor, editor->setCoordinates(charCoords.m_line, charCoords.m_column - 1)))
                return true;
            if (direction1 == -1)
                direction2 = 0;
        } else if (direction1 == 2 || direction1 == 0) {
            if (checkPosition(editor, charCoords))
                return true;
            if (direction1 == 0)
                direction2 = -1;
        }
        if (direction2 != 1) {
            if (checkPosition(editor, editor->setCoordinates(charCoords.m_line, charCoords.m_column + direction2)))
                return true;
        }
        u64 result;
        auto strLine = editor->m_lines[lineIndex].m_chars;
        if (charIndex == 0) {
            if (strLine[0] == ' ') {
                result = std::string::npos;
            } else {
                result = 0;
            }
        } else {
            result = strLine.find_last_not_of(' ', charIndex - 1);
        }
        if (result != std::string::npos) {
            auto resultCoords = editor->getCharacterCoordinates(lineIndex, result);
            if (checkPosition(editor, resultCoords))
                return true;
        }
        result = strLine.find_first_not_of(' ', charIndex);
        if (result != std::string::npos) {
            auto resultCoords = editor->getCharacterCoordinates(lineIndex, result);
            if (checkPosition(editor, resultCoords))
                return true;
        }
        if (isActive()) {
            editor->m_lines[m_nearCursor.m_line].m_colorized = false;
            editor->m_lines[m_matched.m_line].m_colorized = false;
            m_active = false;
            editor->colorize();
        }
        return false;
    }

    void TextEditor::MatchedBracket::findMatchingBracket(TextEditor *editor) {
        auto from = editor->setCoordinates(m_nearCursor);
        if (from == TextEditor::Invalid) {
            m_active = false;
            return;
        }
        m_matched = from;
        auto lineIndex = from.m_line;
        auto maxLineIndex = editor->m_lines.size() - 1;
        auto charIndex = editor->lineCoordinatesToIndex(from);
        std::string line = editor->m_lines[lineIndex].m_chars;
        std::string colors = editor->m_lines[lineIndex].m_colors;
        if (!line.empty() && colors.empty()) {
            m_active = false;
            return;
        }
        std::string brackets = "()[]{}<>";
        char bracketChar = line[charIndex];
        char color1;
        auto idx = brackets.find_first_of(bracketChar);
        if (idx == std::string::npos) {
            if (m_active) {
                m_active = false;
                editor->colorize();
            }
            return;
        }
        auto bracketChar2 = brackets[idx ^ 1];
        brackets = bracketChar;
        brackets += bracketChar2;
        i32 direction = 1 - 2 * (idx % 2);
        if (idx > 5)
            color1 = static_cast<char>(PaletteIndex::Operator);
        else
            color1 = static_cast<char>(PaletteIndex::Separator);
        char color = static_cast<char>(PaletteIndex::WarningText);
        i32 depth = 1;
        if (charIndex == (i64) (line.size() - 1) * (1 + direction) / 2) {
            if (lineIndex == (i64) maxLineIndex * (1 + direction) / 2) {
                m_active = false;
                return;
            }
            lineIndex += direction;
            line = editor->m_lines[lineIndex].m_chars;
            colors = editor->m_lines[lineIndex].m_colors;
            if (!line.empty() && colors.empty()) {
                m_active = false;
                return;
            }
            charIndex = (line.size() - 1) * (1 - direction) / 2 - direction;
        }
        for (i32 i = charIndex + direction;; i += direction) {
            if (direction == 1)
                idx = line.find_first_of(brackets, i);
            else
                idx = line.find_last_of(brackets, i);
            if (idx != std::string::npos) {
                if (line[idx] == bracketChar && (colors[idx] == color || colors[idx] == color1)) {
                    ++depth;
                    i = idx;
                } else if ((line[idx] == bracketChar2 && (colors[idx] == color)) || colors[idx] == color1) {
                    --depth;
                    if (depth == 0) {
                        if (m_matched != editor->getCharacterCoordinates(lineIndex, idx)) {
                            m_matched = editor->getCharacterCoordinates(lineIndex, idx);
                            m_changed = true;
                        }
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
                if (lineIndex == (i64) maxLineIndex * (1 + direction) / 2) {
                    if (m_active) {
                        m_active = false;
                        m_changed = true;
                    }
                    break;
                } else {
                    lineIndex += direction;
                    line = editor->m_lines[lineIndex].m_chars;
                    colors = editor->m_lines[lineIndex].m_colors;
                    if (!line.empty() && colors.empty()) {
                        m_active = false;
                        return;
                    }
                    i = (line.size() - 1) * (1 - direction) / 2 - direction;
                }
            }
        }

        if (hasChanged()) {
            editor->m_lines[m_nearCursor.m_line].m_colorized = false;
            editor->m_lines[m_matched.m_line].m_colorized = false;

            editor->colorize();
            m_changed = false;
        }
    }
}
