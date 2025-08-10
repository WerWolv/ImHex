#include <ui/text_editor.hpp>
#include <algorithm>
#include <chrono>
#include <string>
#include <regex>
#include <cmath>
#include <iostream>
#include <hex/helpers/utils.hpp>
#include <wolv/utils/string.hpp>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

namespace hex::ui {

    const i32 TextEditor::s_cursorBlinkInterval = 1200;
    const i32 TextEditor::s_cursorBlinkOnTime = 800;
    ImVec2 TextEditor::s_cursorScreenPosition;

    TextEditor::FindReplaceHandler::FindReplaceHandler() : m_matchCase(false), m_wholeWord(false), m_findRegEx(false) {}
    const std::string TextEditor::MatchedBracket::s_separators = "()[]{}";
    const std::string TextEditor::MatchedBracket::s_operators = "<>";

    TextEditor::TextEditor() {
        m_startTime = ImGui::GetTime() * 1000;
        setLanguageDefinition(LanguageDefinition::HLSL());
        m_lines.push_back(Line());
    }

    TextEditor::~TextEditor() {
    }

    std::string TextEditor::getText(const Selection &from) const {
        std::string result;
        auto selection = setCoordinates(from);
        auto columns = selection.getSelectedColumns();
        if (selection.isSingleLine())
            result = m_lines[selection.m_start.m_line].substr(columns.m_line, columns.m_column, Line::LinePart::Utf8);
        else {
            auto lines = selection.getSelectedLines();
            result = m_lines[lines.m_line].substr(columns.m_line, (u64) -1, Line::LinePart::Utf8) + '\n';
            for (i32 i = lines.m_line + 1; i < lines.m_column; i++) {
                result += m_lines[i].m_chars + '\n';
            }
            result += m_lines[lines.m_column].substr(0, columns.m_column, Line::LinePart::Utf8);
        }

        return result;
    }

    void TextEditor::deleteRange(const Selection &rangeToDelete) {
        if (m_readOnly)
            return;
        Selection selection = setCoordinates(rangeToDelete);
        auto columns = selection.getSelectedColumns();

        if (selection.isSingleLine()) {
            auto &line = m_lines[selection.m_start.m_line];
            line.erase(columns.m_line, columns.m_column);
        } else {
            auto lines = selection.getSelectedLines();
            auto &firstLine = m_lines[lines.m_line];
            auto &lastLine = m_lines[lines.m_column];
            firstLine.erase(columns.m_line,(u64) -1);
            lastLine.erase(0, columns.m_column);

            if (!lastLine.empty()) {
                firstLine.insert(firstLine.end(), lastLine.begin(), lastLine.end());
                firstLine.m_colorized = false;
            }
            if (lines.m_line < lines.m_column)
                removeLine(lines.m_line + 1, lines.m_column);
        }

        m_textChanged = true;
    }

    void TextEditor::appendLine(const std::string &value) {
        auto text = wolv::util::replaceStrings(wolv::util::preprocessText(value), "\000", ".");
        if (text.empty())
            return;
        if (isEmpty()) {
            m_lines[0].m_chars = text;
            m_lines[0].m_colors = std::string(text.size(), 0);
            m_lines[0].m_flags = std::string(text.size(), 0);
        } else
            m_lines.push_back(Line(text));

        setCursorPosition(setCoordinates((i32) m_lines.size() - 1, 0));
        m_lines.back().m_colorized = false;
        ensureCursorVisible();
        m_textChanged = true;
    }


    i32 TextEditor::insertTextAt(Coordinates /* inout */ &where, const std::string &value) {
        if (value.empty())
            return 0;
        auto start = setCoordinates(where);
        if (start == Invalid)
            return 0;
        auto &line = m_lines[start.m_line];
        auto stringVector = wolv::util::splitString(value, "\n", false);
        auto lineCount = (i32) stringVector.size();
        where.m_line += lineCount - 1;
        where.m_column += getStringCharacterCount(stringVector[lineCount - 1]);
        stringVector[lineCount - 1].append(line.substr(start.m_column,(u64) -1, Line::LinePart::Utf8));
        line.erase(start.m_column, (u64) -1);

        line.append(stringVector[0]);
        line.m_colorized = false;
        for (i32 i = 1; i < lineCount; i++) {
            insertLine(start.m_line + i, stringVector[i]);
            m_lines[start.m_line + i].m_colorized = false;
        }
        m_textChanged = true;
        return lineCount;
    }

    void TextEditor::deleteWordLeft() {
        const auto wordEnd = getCursorPosition();
        const auto wordStart = findPreviousWord(getCursorPosition());
        setSelection(Selection(wordStart, wordEnd));
        backspace();
    }

    void TextEditor::deleteWordRight() {
        const auto wordStart = getCursorPosition();
        const auto wordEnd = findNextWord(getCursorPosition());
        setSelection(Selection(wordStart, wordEnd));
        backspace();
    }

    void TextEditor::removeLine(i32 lineStart, i32 lineEnd) {

        ErrorMarkers errorMarker;
        u32 uLineStart = static_cast<u32>(lineStart);
        u32 uLineEnd = static_cast<u32>(lineEnd);
        for (auto &i: m_errorMarkers) {
            ErrorMarkers::value_type e(i.first.m_line >= lineStart ? setCoordinates(i.first.m_line - 1, i.first.m_column) : i.first, i.second);
            if (e.first.m_line >= lineStart && e.first.m_line <= lineEnd)
                continue;
            errorMarker.insert(e);
        }
        m_errorMarkers = std::move(errorMarker);

        Breakpoints breakpoints;
        for (auto breakpoint: m_breakpoints) {
            if (breakpoint <= uLineStart || breakpoint >= uLineEnd) {
                if (breakpoint >= uLineEnd) {
                    breakpoints.insert(breakpoint - 1);
                    m_breakPointsChanged = true;
                } else
                    breakpoints.insert(breakpoint);
            }
        }

        m_breakpoints = std::move(breakpoints);
        // use clamp to ensure valid results instead of assert.
        auto start = std::clamp(lineStart, 0, (i32) m_lines.size() - 1);
        auto end = std::clamp(lineEnd, 0, (i32) m_lines.size());
        if (start > end)
            std::swap(start, end);

        m_lines.erase(m_lines.begin() + lineStart, m_lines.begin() + lineEnd + 1);

        m_textChanged = true;
    }

    void TextEditor::removeLine(i32 index) {
        removeLine(index, index);
    }

    void TextEditor::insertLine(i32 index, const std::string &text) {
        if (index < 0 || index > (i32) m_lines.size())
            return;
        auto &line = insertLine(index);
        line.append(text);
        line.m_colorized = false;
        m_textChanged = true;
    }

    TextEditor::Line &TextEditor::insertLine(i32 index) {

        if (isEmpty())
            return *m_lines.insert(m_lines.begin(), Line());

        if (index == (i32)m_lines.size())
            return *m_lines.insert(m_lines.end(), Line());

        auto newLine = Line();

        TextEditor::Line &result = *m_lines.insert(m_lines.begin() + index, newLine);
        result.m_colorized = false;

        ErrorMarkers errorMarker;
        for (auto &i: m_errorMarkers)
            errorMarker.insert(ErrorMarkers::value_type(i.first.m_line >= index ? setCoordinates(i.first.m_line + 1, i.first.m_column) : i.first, i.second));
        m_errorMarkers = std::move(errorMarker);

        Breakpoints breakpoints;
        for (auto breakpoint: m_breakpoints) {
            if (breakpoint >= (u32)index) {
                breakpoints.insert(breakpoint + 1);
                m_breakPointsChanged = true;
            } else
                breakpoints.insert(breakpoint);
        }
        if (m_breakPointsChanged)
            m_breakpoints = std::move(breakpoints);

        return result;
    }

    void TextEditor::setText(const std::string &text, bool undo) {
        UndoRecord u;
        if (!m_readOnly && undo) {
            u.m_before = m_state;
            u.m_removed = getText();
            u.m_removedSelection.m_start = setCoordinates(0, 0);
            u.m_removedSelection.m_end = setCoordinates(-1, -1);
            if (u.m_removedSelection.m_start == Invalid || u.m_removedSelection.m_end == Invalid)
                return;
        }
        auto vectorString = wolv::util::splitString(text, "\n", false);
        auto lineCount = vectorString.size();
        if (lineCount == 0) {
            m_lines.resize(1);
            m_lines[0].clear();
        } else {
            m_lines.resize(lineCount);
            u64 i = 0;
            for (auto line: vectorString) {
                m_lines[i].setLine(line);
                m_lines[i].m_colorized = false;
                i++;
            }
        }
        if (!m_readOnly && undo) {
            u.m_added = text;
            u.m_addedSelection.m_start = setCoordinates(0, 0);
            u.m_addedSelection.m_end = setCoordinates(-1, -1);
            if (u.m_addedSelection.m_start == Invalid || u.m_addedSelection.m_end == Invalid)
                return;
        }
        m_textChanged = true;
        m_scrollToTop = true;
        if (!m_readOnly && undo) {
            u.m_after = m_state;

            addUndo(u);
        }

        colorize();
    }

    void TextEditor::enterCharacter(ImWchar character, bool shift) {
        if (m_readOnly)
            return;

        UndoRecord u;

        u.m_before = m_state;

        resetCursorBlinkTime();

        if (hasSelection()) {
            if (character == '\t') {

                auto start = m_state.m_selection.m_start;
                auto end = m_state.m_selection.m_end;
                auto originalEnd = end;

                start.m_column = 0;

                if (end.m_column == 0 && end.m_line > 0)
                    --end.m_line;
                if (end.m_line >= (i32) m_lines.size())
                    end.m_line = isEmpty() ? 0 : (i32) m_lines.size() - 1;
                end.m_column = getLineMaxColumn(end.m_line);

                u.m_removedSelection = Selection(start, end);
                u.m_removed = getText(u.m_removedSelection);

                bool modified = false;

                for (i32 i = start.m_line; i <= end.m_line; i++) {
                    auto &line = m_lines[i];
                    if (shift) {
                        if (!line.empty()) {
                            auto index = line.m_chars.find_first_not_of(' ', 0);
                            if (index == std::string::npos)
                                index = line.size() - 1;
                            if (index == 0) continue;
                            u64 spacesToRemove = (index % m_tabSize) ? (index % m_tabSize) : m_tabSize;
                            spacesToRemove = std::min(spacesToRemove, line.size());
                            line.erase(line.begin(), spacesToRemove);
                            line.m_colorized = false;
                            modified = true;
                        }
                    } else {
                        auto spacesToInsert = m_tabSize - (start.m_column % m_tabSize);
                        std::string spaces(spacesToInsert, ' ');
                        line.insert(line.begin(), spaces.begin(), spaces.end());
                        line.m_colorized = false;
                        modified = true;
                    }
                }

                if (modified) {
                    Coordinates rangeEnd;
                    if (originalEnd.m_column != 0) {
                        end = setCoordinates(end.m_line, -1);
                        if (end == Invalid)
                            return;
                        rangeEnd = end;
                        u.m_added = getText(Selection(start, end));
                    } else {
                        end = setCoordinates(originalEnd.m_line, 0);
                        rangeEnd = setCoordinates(end.m_line - 1, -1);
                        if (end == Invalid || rangeEnd == Invalid)
                            return;
                        u.m_added = getText(Selection(start, rangeEnd));
                    }

                    u.m_addedSelection = Selection(start, rangeEnd);
                    u.m_after = m_state;

                    m_state.m_selection = Selection(start, end);
                    addUndo(u);

                    m_textChanged = true;

                    ensureCursorVisible();
                }

                return;
            }    // c == '\t'
            else {
                u.m_removed = getSelectedText();
                u.m_removedSelection = Selection(m_state.m_selection);
                deleteSelection();
            }
        }    // HasSelection

        auto coord = setCoordinates(m_state.m_cursorPosition);
        u.m_addedSelection.m_start = coord;

        if (m_lines.empty())
            m_lines.push_back(Line());

        if (character == '\n') {
            insertLine(coord.m_line + 1);
            auto &line = m_lines[coord.m_line];
            auto &newLine = m_lines[coord.m_line + 1];

            if (m_languageDefinition.m_autoIndentation)
                for (u64 it = 0; it < line.size() && isascii(line[it]) && isblank(line[it]); ++it)
                    newLine.push_back(line[it]);

            const u64 whitespaceSize = newLine.size();
            i32 charStart;
            i32 charPosition;
            auto charIndex = lineCoordinatesToIndex(coord);
            if (charIndex < (i32) whitespaceSize && m_languageDefinition.m_autoIndentation) {
                charStart = (i32) whitespaceSize;
                charPosition = charIndex;
            } else {
                charStart = charIndex;
                charPosition = (i32) whitespaceSize;
            }
            newLine.insert(newLine.end(), line.begin() + charStart, line.end());
            line.erase(line.begin() + charStart,(u64) -1);
            line.m_colorized = false;
            setCursorPosition(getCharacterCoordinates(coord.m_line + 1, charPosition));
            u.m_added = (char) character;
            u.m_addedSelection.m_end = setCoordinates(m_state.m_cursorPosition);
        } else if (character == '\t') {
            auto &line = m_lines[coord.m_line];
            auto charIndex = lineCoordinatesToIndex(coord);

            if (!shift) {
                auto spacesToInsert = m_tabSize - (charIndex % m_tabSize);
                std::string spaces(spacesToInsert, ' ');
                line.insert(line.begin() + charIndex, spaces.begin(), spaces.end());
                line.m_colorized = false;
                setCursorPosition(getCharacterCoordinates(coord.m_line, charIndex + spacesToInsert));
                u.m_added = spaces;
                u.m_addedSelection.m_end = setCoordinates(m_state.m_cursorPosition);
            } else {
                auto spacesToRemove = (charIndex % m_tabSize);
                if (spacesToRemove == 0) spacesToRemove = m_tabSize;
                spacesToRemove = std::min(spacesToRemove, (i32) line.size());
                auto spacesRemoved = 0;
                for (i32 j = 0; j < spacesToRemove; j++) {
                    if (*(line.begin() + (charIndex - 1)) == ' ') {
                        line.erase(line.begin() + (charIndex - 1));
                        charIndex -= 1;
                        spacesRemoved++;
                    }
                }
                std::string spaces(spacesRemoved, ' ');
                u.m_removed = spaces;
                u.m_removedSelection = Selection(Coordinates(coord.m_line, charIndex), Coordinates(coord.m_line, charIndex + spacesRemoved));
                line.m_colorized = false;
                setCursorPosition(getCharacterCoordinates(coord.m_line, std::max(0, charIndex)));
            }
            u.m_addedSelection.m_end = setCoordinates(m_state.m_cursorPosition);
        } else {
            std::string buf = "";
            imTextCharToUtf8(buf, character);
            if (buf.size() > 0) {
                auto &line = m_lines[coord.m_line];
                auto charIndex = lineCoordinatesToIndex(coord);

                if (m_overwrite && charIndex < (i32) line.size()) {
                    i64 column = coord.m_column;
                    std::string c = line[column];
                    auto charCount = getStringCharacterCount(c);
                    auto d = c.size();

                    u.m_removedSelection = Selection(m_state.m_cursorPosition, getCharacterCoordinates(coord.m_line, coord.m_column + charCount));
                    u.m_removed = std::string(line.m_chars.begin() + charIndex, line.m_chars.begin() + charIndex + d);
                    line.erase(line.begin() + charIndex, d);
                    line.m_colorized = false;
                }
                auto charCount = getStringCharacterCount(buf);
                if (buf == "{")
                    buf += "}";
                else if (buf == "[")
                    buf += "]";
                else if (buf == "(")
                    buf += ")";
                if ((buf == "}" || buf == "]" || buf == ")") && buf == line.substr(charIndex, charCount))
                    buf = "";

                if (buf == "\"") {
                    if (buf == line.substr(charIndex, charCount)) {
                        if (line.m_colors[charIndex + 1] == (char) PaletteIndex::StringLiteral)
                            buf += "\"";
                        else
                            buf = "";
                    } else
                        buf += "\"";
                }

                if (buf == "'") {
                    if (buf == line.substr(charIndex, charCount)) {
                        if (line.m_colors[charIndex + 1] == (char) PaletteIndex::CharLiteral)
                            buf += "'";
                        else
                            buf = "";
                    } else
                        buf += "'";
                }

                line.insert(line.begin() + charIndex, buf.begin(), buf.end());
                line.m_colorized = false;
                u.m_added = buf;
                u.m_addedSelection.m_end = getCharacterCoordinates(coord.m_line, charIndex + buf.size());
                setCursorPosition(getCharacterCoordinates(coord.m_line, charIndex + charCount));
            } else
                return;
        }

        u.m_after = m_state;
        m_textChanged = true;

        addUndo(u);
        colorize();
        refreshSearchResults();
        ensureCursorVisible();
    }

    void TextEditor::refreshSearchResults() {
        std::string findWord = m_findReplaceHandler.getFindWord();
        if (!findWord.empty()) {
            m_findReplaceHandler.resetMatches();
            m_findReplaceHandler.findAllMatches(this, findWord);
        }
    }

    void TextEditor::insertText(const std::string &value) {
        insertText(value.c_str());
    }

    void TextEditor::insertText(const char *value) {
        if (value == nullptr)
            return;

        auto pos = setCoordinates(m_state.m_cursorPosition);
        //auto start = std::min(pos, m_state.m_selection.m_start);

        insertTextAt(pos, value);
        m_lines[pos.m_line].m_colorized = false;

        setSelection(Selection(pos, pos));
        setCursorPosition(pos);
        refreshSearchResults();
        colorize();
    }

    void TextEditor::deleteSelection() {

        if (m_state.m_selection.m_end == m_state.m_selection.m_start)
            return;

        deleteRange(m_state.m_selection);

        setSelection(Selection(m_state.m_selection.m_start, m_state.m_selection.m_start));
        setCursorPosition(m_state.m_selection.m_start);
        refreshSearchResults();
        colorize();
    }

    void TextEditor::deleteChar() {
        resetCursorBlinkTime();

        if (isEmpty() || m_readOnly)
            return;

        UndoRecord u;
        u.m_before = m_state;

        if (hasSelection()) {
            u.m_removed = getSelectedText();
            u.m_removedSelection = m_state.m_selection;
            deleteSelection();
        } else {
            auto pos = setCoordinates(m_state.m_cursorPosition);
            setCursorPosition(pos);
            auto &line = m_lines[pos.m_line];

            if (pos.m_column == getLineMaxColumn(pos.m_line)) {
                if (pos.m_line == (i32) m_lines.size() - 1)
                    return;

                u.m_removed = '\n';
                u.m_removedSelection.m_start = u.m_removedSelection.m_end = setCoordinates(m_state.m_cursorPosition);
                advance(u.m_removedSelection.m_end);

                auto &nextLine = m_lines[pos.m_line + 1];
                line.insert(line.end(), nextLine.begin(), nextLine.end());
                line.m_colorized = false;
                removeLine(pos.m_line + 1);

            } else {
                i64 charIndex = lineCoordinatesToIndex(pos);
                u.m_removedSelection.m_start = u.m_removedSelection.m_end = setCoordinates(m_state.m_cursorPosition);
                u.m_removedSelection.m_end.m_column++;
                u.m_removed = getText(u.m_removedSelection);

                auto d = utf8CharLength(line[charIndex][0]);
                line.erase(line.begin() + charIndex, d);
                line.m_colorized = false;
            }

            m_textChanged = true;

            colorize();
        }

        u.m_after = m_state;
        addUndo(u);
        refreshSearchResults();
    }

    void TextEditor::backspace() {
        resetCursorBlinkTime();
        if (isEmpty() || m_readOnly)
            return;

        UndoRecord u;
        u.m_before = m_state;

        if (hasSelection()) {
            u.m_removed = getSelectedText();
            u.m_removedSelection = m_state.m_selection;
            deleteSelection();
        } else {
            auto pos = setCoordinates(m_state.m_cursorPosition);
            auto &line = m_lines[pos.m_line];

            if (pos.m_column == 0) {
                if (pos.m_line == 0)
                    return;

                u.m_removed = '\n';
                u.m_removedSelection.m_start = u.m_removedSelection.m_end = setCoordinates(pos.m_line - 1, -1);
                advance(u.m_removedSelection.m_end);

                auto &prevLine = m_lines[pos.m_line - 1];
                auto prevSize = getLineMaxColumn(pos.m_line - 1);
                if (prevSize == 0)
                    prevLine = line;
                else
                    prevLine.insert(prevLine.end(), line.begin(), line.end());
                prevLine.m_colorized = false;


                ErrorMarkers errorMarker;
                for (auto &i: m_errorMarkers)
                    errorMarker.insert(ErrorMarkers::value_type(i.first.m_line - 1 == m_state.m_cursorPosition.m_line ? setCoordinates(i.first.m_line - 1, i.first.m_column) : i.first, i.second));
                m_errorMarkers = std::move(errorMarker);
                removeLine(m_state.m_cursorPosition.m_line);
                --m_state.m_cursorPosition.m_line;
                m_state.m_cursorPosition.m_column = prevSize;
            } else {
                pos.m_column -= 1;
                i64 column = pos.m_column;
                std::string charToRemove = line[column];
                if (pos.m_column < (i32) line.size() - 1) {
                    std::string charToRemoveNext = line[column + 1];
                    if (charToRemove == "{" && charToRemoveNext == "}") {
                        charToRemove += "}";
                        m_state.m_cursorPosition.m_column += 1;
                    } else if (charToRemove == "[" && charToRemoveNext == "]") {
                        charToRemove += "]";
                        m_state.m_cursorPosition.m_column += 1;
                    } else if (charToRemove == "(" && charToRemoveNext == ")") {
                        charToRemove += ")";
                        m_state.m_cursorPosition.m_column += 1;
                    } else if (charToRemove == "\"" && charToRemoveNext == "\"") {
                        charToRemove += "\"";
                        m_state.m_cursorPosition.m_column += 1;
                    } else if (charToRemove == "'" && charToRemoveNext == "'") {
                        charToRemove += "'";
                        m_state.m_cursorPosition.m_column += 1;
                    }
                }
                u.m_removedSelection = Selection(pos, m_state.m_cursorPosition);
                u.m_removed = charToRemove;
                auto charStart = lineCoordinatesToIndex(pos);
                auto charEnd = lineCoordinatesToIndex(m_state.m_cursorPosition);
                line.erase(line.begin() + charStart, charEnd - charStart);
                m_state.m_cursorPosition = pos;
                line.m_colorized = false;
            }

            m_textChanged = true;

            ensureCursorVisible();
            colorize();
        }

        u.m_after = m_state;
        addUndo(u);
        refreshSearchResults();
    }

    void TextEditor::copy() {
        if (hasSelection()) {
            ImGui::SetClipboardText(getSelectedText().c_str());
        } else {
            if (!isEmpty()) {
                std::string str;
                const auto &line = m_lines[setCoordinates(m_state.m_cursorPosition).m_line];
                std::copy(line.m_chars.begin(), line.m_chars.end(), std::back_inserter(str));
                ImGui::SetClipboardText(str.c_str());
            }
        }
    }

    void TextEditor::cut() {
        if (m_readOnly) {
            copy();
        } else {
            if (!hasSelection()) {
                auto lineIndex = setCoordinates(m_state.m_cursorPosition).m_line;
                if (lineIndex < 0 || lineIndex >= (i32) m_lines.size())
                    return;
                setSelection(Selection(setCoordinates(lineIndex, 0), setCoordinates(lineIndex + 1, 0)));
            }
            UndoRecord u;
            u.m_before = m_state;
            u.m_removed = getSelectedText();
            u.m_removedSelection = m_state.m_selection;

            copy();
            deleteSelection();

            u.m_after = m_state;
            addUndo(u);
        }
        refreshSearchResults();
    }

    void TextEditor::paste() {
        if (m_readOnly)
            return;

        auto clipText = ImGui::GetClipboardText();
        if (clipText != nullptr) {
            auto len = strlen(clipText);
            if (len > 0) {
                std::string text = wolv::util::preprocessText(clipText);
                UndoRecord u;
                u.m_before = m_state;

                if (hasSelection()) {
                    u.m_removed = getSelectedText();
                    u.m_removedSelection = m_state.m_selection;
                    deleteSelection();
                }

                u.m_added = text;
                u.m_addedSelection.m_start = setCoordinates(m_state.m_cursorPosition);
                insertText(text);

                u.m_addedSelection.m_end = setCoordinates(m_state.m_cursorPosition);
                u.m_after = m_state;
                addUndo(u);
            }
        }
        refreshSearchResults();
    }

    bool TextEditor::canUndo() {
        return !m_readOnly && m_undoIndex > 0;
    }

    bool TextEditor::canRedo() const {
        return !m_readOnly && m_undoIndex < (i32) m_undoBuffer.size();
    }

    void TextEditor::undo(i32 steps) {
        while (canUndo() && steps-- > 0)
            m_undoBuffer[--m_undoIndex].undo(this);
        refreshSearchResults();
    }

    void TextEditor::redo(i32 steps) {
        while (canRedo() && steps-- > 0)
            m_undoBuffer[m_undoIndex++].redo(this);
        refreshSearchResults();
    }

    std::string TextEditor::getText() const {
        auto start = setCoordinates(0, 0);
        auto end = setCoordinates(-1, -1);
        if (start == Invalid || end == Invalid)
            return "";
        return getText(Selection(start, end));
    }

    std::vector<std::string> TextEditor::getTextLines() const {
        std::vector<std::string> result;

        result.reserve(m_lines.size());

        for (const auto &line: m_lines) {
            std::string text = line.m_chars;
            result.emplace_back(std::move(text));
        }

        return result;
    }

    std::string TextEditor::getSelectedText() const {
        return getText(m_state.m_selection);
    }

    std::string TextEditor::getLineText(i32 line) const {
        auto sanitizedLine = setCoordinates(line, 0);
        auto endLine = setCoordinates(line, -1);
        if (sanitizedLine == Invalid || endLine == Invalid)
            return "";
        return getText(Selection(sanitizedLine, endLine));
    }

    TextEditor::UndoRecord::UndoRecord(
            const std::string &added,
            const TextEditor::Selection addedSelection,
            const std::string &removed,
            const TextEditor::Selection removedSelection,
            TextEditor::EditorState &before,
            TextEditor::EditorState &after) : m_added(added), m_addedSelection(addedSelection), m_removed(removed), m_removedSelection(removedSelection), m_before(before), m_after(after) {}

    void TextEditor::UndoRecord::undo(TextEditor *editor) {
        if (!m_added.empty()) {
            editor->deleteRange(m_addedSelection);
            editor->colorize();
        }

        if (!m_removed.empty()) {
            auto start = m_removedSelection.m_start;
            editor->insertTextAt(start, m_removed.c_str());
            editor->colorize();
        }

        editor->m_state = m_before;
        editor->ensureCursorVisible();
    }

    void TextEditor::UndoRecord::redo(TextEditor *editor) {
        if (!m_removed.empty()) {
            editor->deleteRange(m_removedSelection);
            editor->colorize();
        }

        if (!m_added.empty()) {
            auto start = m_addedSelection.m_start;
            editor->insertTextAt(start, m_added.c_str());
            editor->colorize();
        }

        editor->m_state = m_after;
        editor->ensureCursorVisible();

    }

}