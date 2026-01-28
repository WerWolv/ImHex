#include <ui/text_editor.hpp>
#include <algorithm>
#include <string>
#include <regex>
#include <cmath>
#include <utility>
#include <wolv/utils/string.hpp>
#include <popups/popup_question.hpp>
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"

namespace hex::ui {
    using StringVector  = TextEditor::StringVector;
    using Range         = TextEditor::Range;
    using EditorState  = TextEditor::EditorState;

    TextEditor::FindReplaceHandler::FindReplaceHandler() : m_matchCase(false), m_wholeWord(false), m_findRegEx(false), m_optionsChanged(false) {}

    TextEditor::TextEditor() {
        m_lines.m_startTime = ImGui::GetTime() * 1000;
        m_lines.setLanguageDefinition(LanguageDefinition::HLSL());
        m_lines.m_unfoldedLines.emplace_back();
        m_lineSpacing = 1.0f;
        m_tabSize = 4;

        m_lines.m_state.m_cursorPosition = lineCoordinates( 0, 0);
        m_lines.m_state.m_selection.m_start = m_lines.m_state.m_selection.m_end = lineCoordinates( 0, 0);
    }

    TextEditor::~TextEditor() = default;

    std::string TextEditor::Lines::getRange(const Range &rangeToGet) {
        std::string result;
        auto selection = lineCoordinates(const_cast<Range &>(rangeToGet));
        selection.m_end = rangeToGet.m_end;
        auto columns = selection.getSelectedColumns();

        if (selection.isSingleLine()) {
            result = m_unfoldedLines[selection.m_start.m_line].substr(columns.m_line, columns.m_column, Line::LinePart::Utf8);
        } else {
            auto lines = selection.getSelectedLines();
            StringVector lineContents;
            lineContents.push_back(m_unfoldedLines[lines.m_line].substr(columns.m_line, (u64) -1, Line::LinePart::Utf8));
            for (i32 i = lines.m_line + 1; i < lines.m_column; i++) {
                lineContents.push_back(m_unfoldedLines[i].m_chars);
            }
            lineContents.push_back(m_unfoldedLines[lines.m_column].substr(0, columns.m_column, Line::LinePart::Utf8));
            result = wolv::util::combineStrings(lineContents, "\n");
        }

        return result;
    }

    void TextEditor::Lines::deleteRange(const Range &rangeToDelete) {
        if (m_readOnly)
            return;
        Range selection = lineCoordinates(const_cast<Range &>(rangeToDelete));
        auto columns = selection.getSelectedColumns();

        if (selection.isSingleLine()) {
            auto &line = m_unfoldedLines[selection.m_start.m_line];
            line.erase(columns.m_line, columns.m_column);
        } else {
            auto lines = selection.getSelectedLines();
            auto &firstLine = m_unfoldedLines[lines.m_line];
            auto &lastLine = m_unfoldedLines[lines.m_column];
            firstLine.erase(columns.m_line,(u64) -1);
            lastLine.erase(0, columns.m_column);

            if (!lastLine.empty()) {
                firstLine.insert(firstLine.end(), lastLine.begin(), lastLine.end());
                firstLine.m_colorized = false;
            }
            if (lines.m_line + 1 < lines.m_column)
                removeLines(lines.m_line + 1, lines.m_column);
            else
                removeLine(lines.m_column);
        }

        m_textChanged = true;
    }

    void TextEditor::Lines::appendLine(const std::string &value) {
        auto text = wolv::util::replaceStrings(wolv::util::preprocessText(value), "\000", ".");
        if (text.empty())
            return;
        if (isEmpty()) {
            m_unfoldedLines[0].m_chars = text;
            m_unfoldedLines[0].m_colors = std::string(text.size(), 0);
            m_unfoldedLines[0].m_flags = std::string(text.size(), 0);
        } else {
            m_unfoldedLines.emplace_back(text);
            auto line = m_unfoldedLines.back();
            line.m_lineMaxColumn = line.maxColumn();
        }
        m_unfoldedLines.back().m_colorized = false;
    }

    void TextEditor::appendLine(const std::string &value) {
       m_lines.appendLine(value);
        m_lines.setCursorPosition(m_lines.lineCoordinates(m_lines.size() - 1, 0), false);
        m_lines.m_unfoldedLines.back().m_colorized = false;
        m_lines.ensureCursorVisible();
        m_lines.m_textChanged = true;
    }

    i32 TextEditor::Lines::insertTextAtCursor(const std::string &value) {
        if (value.empty())
            return 0;
        auto start = lineCoordinates(m_state.m_cursorPosition);
        if (start == Invalid)
            return 0;
        auto &line = m_unfoldedLines[start.m_line];
        auto stringVector = wolv::util::splitString(value, "\n", false);
        auto lineCount = (i32) stringVector.size();
        auto state = m_state;
        state.m_cursorPosition.m_line  += lineCount - 1;
        state.m_cursorPosition.m_column += stringCharacterCount(stringVector[lineCount - 1]);
        m_state = state;
        stringVector[lineCount - 1].append(line.substr(start.m_column,(u64) -1, Line::LinePart::Utf8));
        line.erase(start.m_column, (u64) -1);

        line.append(stringVector[0]);
        line.m_colorized = false;
        for (i32 i = 1; i < lineCount; i++) {
            insertLine(start.m_line + i, stringVector[i]);
            m_unfoldedLines[start.m_line + i].m_colorized =false;
        }
        m_textChanged = true;
        return lineCount;
    }

    i32 TextEditor::Lines::insertTextAt(Coordinates /* inout */ &where, const std::string &value) {
        if (value.empty())
            return 0;
        auto start = lineCoordinates(where);
        if (start == Invalid)
            return 0;
        auto &line = m_unfoldedLines[start.m_line];
        auto stringVector = wolv::util::splitString(value, "\n", false);
        auto lineCount = (i32) stringVector.size();
        where.m_line += lineCount - 1;
        where.m_column += stringCharacterCount(stringVector[lineCount - 1]);
        stringVector[lineCount - 1].append(line.substr(start.m_column,(u64) -1, Line::LinePart::Utf8));
        line.erase(start.m_column, (u64) -1);

        line.append(stringVector[0]);
        line.m_colorized = false;
        for (i32 i = 1; i < lineCount; i++) {
            insertLine(start.m_line + i, stringVector[i]);
            m_unfoldedLines[start.m_line + i].m_colorized = false;
        }
        m_textChanged = true;
        return lineCount;
    }

    void TextEditor::deleteWordLeft() {
        const auto wordEnd = getCursorPosition();
        setSelection(Range(m_lines.findPreviousWord(wordEnd), wordEnd));
        backspace();
    }

    void TextEditor::deleteWordRight() {
        const auto wordStart = getCursorPosition();
        setSelection(Range(wordStart, m_lines.findNextWord(wordStart)));
        backspace();
    }

    void TextEditor::Lines::removeLines(i32 lineStart, i32 lineEnd) {
        ErrorMarkers errorMarkers;
        for (auto &errorMarker : m_errorMarkers) {
            if (errorMarker.first.m_line <= lineStart || errorMarker.first.m_line > lineEnd + 1) {
                if (errorMarker.first.m_line >= lineEnd + 1) {
                    auto newRow = errorMarker.first.m_line - (lineEnd - lineStart + 1);
                    auto newCoord = lineCoordinates(newRow, errorMarker.first.m_column);
                    errorMarkers.insert(ErrorMarkers::value_type(newCoord, errorMarker.second));
                } else
                    errorMarkers.insert(errorMarker);
            }
        }
        m_errorMarkers = std::move(errorMarkers);

        u32 uLineStart = static_cast<u32>(lineStart);
        u32 uLineEnd = static_cast<u32>(lineEnd);
        Breakpoints breakpoints;
        for (auto breakpoint : m_breakpoints) {
            if (breakpoint <= uLineStart || breakpoint > uLineEnd + 1) {
                if (breakpoint > uLineEnd + 1) {
                    breakpoints.insert(breakpoint - (uLineEnd - uLineStart + 1));
                    m_breakPointsChanged = true;
                } else
                    breakpoints.insert(breakpoint);
            }
        }

        m_breakpoints = std::move(breakpoints);

        CodeFoldKeys folds;
        bool foldsChanged = false;
        CodeFoldState codeFoldState;
        Range fold;
        for (auto interval : m_codeFoldKeys) {
            fold = interval;
            if (interval.m_end.m_line < lineStart) {
                folds.insert(fold);
                codeFoldState[fold] = m_codeFoldState[interval];
            } else if (interval.m_start.m_line > lineEnd) {
                fold.m_start.m_line -= (lineEnd - lineStart + 1);
                fold.m_end.m_line -= (lineEnd - lineStart + 1);
                codeFoldState[fold] = m_codeFoldState[interval];
                foldsChanged = true;
                folds.insert(fold);
            } else if (interval.m_start.m_line < lineStart && interval.m_end.m_line > lineEnd) {
                fold.m_end.m_line -=  (lineEnd - lineStart + 1);
                codeFoldState[fold] = m_codeFoldState[interval];
                foldsChanged = true;
                folds.insert(fold);
            } else
                foldsChanged = true;
        }
        if (foldsChanged) {
            m_codeFoldState = std::move(codeFoldState);
            m_codeFoldKeys = std::move(folds);
            m_saveCodeFoldStateRequested = true;
        }

        // use clamp to ensure valid results instead of assert.
        auto start = std::clamp(lineStart, 0, size() - 1);
        auto end = std::clamp(lineEnd, 0, size());
        if (start > end)
            std::swap(start, end);

        m_unfoldedLines.erase(m_unfoldedLines.begin() + lineStart, m_unfoldedLines.begin() + lineEnd + 1);

        m_textChanged = true;
        m_globalRowMaxChanged = true;
    }

    void TextEditor::Lines::removeLine(i32 index) {
        removeLines(index, index);
    }

    void TextEditor::Lines::insertLine(i32 index, const std::string &text) {
        if (index < 0 || index > size())
            return;
        auto &line = insertLine(index);
        line.append(text);
        line.m_colorized = false;
        m_textChanged = true;
    }

    TextEditor::Line &TextEditor::Lines::insertLine(i32 index) {
        m_globalRowMaxChanged = true;
        if (isEmpty())
            return *m_unfoldedLines.insert(m_unfoldedLines.begin(), Line());

        if (index == size())
            return *m_unfoldedLines.insert(m_unfoldedLines.end(), Line());

        TextEditor::Line &result = *m_unfoldedLines.insert(m_unfoldedLines.begin() + index, Line());

        result.m_colorized = false;

        ErrorMarkers errorMarkers;
        bool errorMarkerChanged = false;
        for (auto &errorMarker : m_errorMarkers) {
            if (errorMarker.first.m_line > index) {
                auto newCoord = lineCoordinates(errorMarker.first.m_line + 1, errorMarker.first.m_column);
                errorMarkers.insert(ErrorMarkers::value_type(newCoord, errorMarker.second));
                errorMarkerChanged = true;
            } else
                errorMarkers.insert(errorMarker);
        }
        if (errorMarkerChanged)
            m_errorMarkers = std::move(errorMarkers);

        Breakpoints breakpoints;
        for (auto breakpoint : m_breakpoints) {
            if (breakpoint > (u32) index) {
                breakpoints.insert(breakpoint + 1);
                m_breakPointsChanged = true;
            } else
                breakpoints.insert(breakpoint);
        }
        if (m_breakPointsChanged)
            m_breakpoints = std::move(breakpoints);

        CodeFoldKeys folds;
        Range fold;
        CodeFoldState codeFoldState;
        bool foldsChanged = false;
        for (auto key : m_codeFoldKeys) {
            fold = key;
            if (index <= key.m_start.m_line) {
                fold.m_start.m_line++;
                fold.m_end.m_line++;
                foldsChanged = true;
            } else if (index <= key.m_end.m_line) {
                fold.m_end.m_line++;
                foldsChanged = true;
            }
            codeFoldState[fold] = m_codeFoldState[key];
            folds.insert(fold);
        }
        if (foldsChanged) {
            m_codeFoldState = std::move(codeFoldState);
            m_codeFoldKeys = std::move(folds);
            m_saveCodeFoldStateRequested = true;
        }

        return result;
    }

    void TextEditor::setText(const std::string &text, bool undo) {
        UndoRecord u;
        if (!m_lines.m_readOnly && undo) {
            u.m_before = m_lines.m_state;
            u.m_removed = m_lines.getText();
            u.m_removedRange.m_start = m_lines.lineCoordinates(0, 0);
            u.m_removedRange.m_end = m_lines.lineCoordinates(-1, -1);
            if (u.m_removedRange.m_start == Invalid || u.m_removedRange.m_end == Invalid)
                return;
        }
        auto vectorString = wolv::util::splitString(text, "\n", false);
        auto lineCount = vectorString.size();
        if (lineCount == 0) {
            m_lines.m_unfoldedLines.resize(1);
            m_lines.m_unfoldedLines[0].clear();
        } else {
            lineCount = vectorString.size();
            u64 i = 0;
            m_lines.m_unfoldedLines.clear();
            m_lines.m_unfoldedLines.resize(lineCount);

            for (const auto &line: vectorString) {
                auto &unfoldedLine = m_lines.m_unfoldedLines[i];

                unfoldedLine.setLine(line);
                unfoldedLine.m_colorized = false;
                unfoldedLine.m_lineMaxColumn = unfoldedLine.maxColumn();
                i++;
            }
        }
        if (!m_lines.m_readOnly && undo) {
            u.m_added = text;
            u.m_addedRange.m_start = m_lines.lineCoordinates(0, 0);
            u.m_addedRange.m_end = m_lines.lineCoordinates(-1, -1);
            if (u.m_addedRange.m_start == Invalid || u.m_addedRange.m_end == Invalid)
                return;
        }
        m_lines.m_textChanged = true;
        if (!m_lines.m_readOnly && undo) {
            u.m_after = m_lines.m_state;
            UndoRecords v;
            v.push_back(u);
            m_lines.addUndo(v);
        }
        m_lines.colorize();
    }

    void TextEditor::enterCharacter(ImWchar character, bool shift) {
        if (m_lines.m_readOnly)
            return;
        if (!m_lines.m_codeFoldsDisabled) {
            auto row = lineIndexToRow(m_lines.m_state.m_cursorPosition.m_line);
            if (m_lines.m_foldedLines.contains(row)) {
                auto foldedLine = m_lines.m_foldedLines[row];
                auto foldedCoords = m_lines.unfoldedToFoldedCoords(m_lines.m_state.m_cursorPosition);
                if (foldedLine.m_foldedLine.m_chars[foldedCoords.m_column] == '.') {
                    auto keyCount = foldedLine.m_keys.size();
                    for (u32 i = 0; i < keyCount; i++) {
                        if (foldedCoords.m_column >= foldedLine.m_ellipsisIndices[i] &&
                            foldedCoords.m_column <= foldedLine.m_ellipsisIndices[i] + 3) {
                            m_lines.openCodeFold(m_lines.m_foldedLines[row].m_keys[i]);
                            return;
                        }
                    }
                }
            }
        }

        UndoRecord u;

        u.m_before =m_lines. m_state;

        m_lines.resetCursorBlinkTime();

        if (m_lines.hasSelection()) {
            if (character == '\t') {

                auto start = m_lines.m_state.m_selection.m_start;
                auto end = m_lines.m_state.m_selection.m_end;
                auto originalEnd = end;

                start.m_column = 0;

                if (end.m_column == 0 && end.m_line > 0)
                    --end.m_line;
                if (end.m_line >= (i32) m_lines.getGlobalRowMax())
                    end.m_line = m_lines.isEmpty() ? 0 : (i32) m_lines.getGlobalRowMax();
                end.m_column = m_lines.lineMaxColumn(end.m_line);

                u.m_removedRange = Range(start, end);
                u.m_removed = m_lines.getRange(u.m_removedRange);

                bool modified = false;

                for (i32 i = start.m_line; i <= end.m_line; i++) {
                    auto &line = m_lines.m_unfoldedLines[i];
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
                        end = m_lines.lineCoordinates(end.m_line, -1);
                        if (end == Invalid)
                            return;
                        rangeEnd = end;
                        u.m_added = m_lines.getRange(Range(start, end));
                    } else {
                        end = m_lines.lineCoordinates(originalEnd.m_line, 0);
                        rangeEnd = m_lines.lineCoordinates(end.m_line - 1, -1);
                        if (end == Invalid || rangeEnd == Invalid)
                            return;
                        u.m_added = m_lines.getRange(Range(start, rangeEnd));
                    }

                    u.m_addedRange = Range(start, rangeEnd);
                    u.m_after = m_lines.m_state;

                    m_lines.m_state.m_selection = Range(start, end);
                    UndoRecords v;
                    v.push_back(u);
                    m_lines.addUndo(v);

                    m_lines.m_textChanged = true;

                    m_lines.ensureCursorVisible();
                }

                return;
            }    // c == '\t'
            else {
                u.m_removed = m_lines.getSelectedText();
                u.m_removedRange = Range(m_lines.m_state.m_selection);
                m_lines.deleteSelection();
            }
        }    // HasSelection

        auto coord = m_lines.lineCoordinates(m_lines.m_state.m_cursorPosition);
        u.m_addedRange.m_start = coord;

        if (m_lines.m_unfoldedLines.empty())
            m_lines.m_unfoldedLines.emplace_back();

        if (character == '\n') {
            m_lines.insertLine(coord.m_line + 1);
            auto &line = m_lines.m_unfoldedLines[coord.m_line];
            auto &newLine = m_lines.m_unfoldedLines[coord.m_line + 1];

            if (m_lines.m_languageDefinition.m_autoIndentation)
                newLine.append(std::string(m_lines.m_leadingLineSpaces[coord.m_line], ' '));

            const u64 whitespaceSize = newLine.size();
            i32 charStart;
            i32 charPosition;
            auto charIndex = m_lines.lineCoordsIndex(coord);
            if (charIndex < (i32) whitespaceSize && m_lines.m_languageDefinition.m_autoIndentation) {
                charStart = (i32) whitespaceSize;
                charPosition = charIndex;
            } else {
                charStart = charIndex;
                charPosition = (i32) whitespaceSize;
            }
            newLine.insert(newLine.end(), line.begin() + charStart, line.end());
            line.erase(line.begin() + charStart,(u64) -1);
            line.m_colorized = false;
            m_lines.setCursorPosition(m_lines.lineIndexCoords(coord.m_line + 2, charPosition), false);
            u.m_added = (char) character + std::string(charPosition, ' ');
            u.m_addedRange.m_end = m_lines.lineCoordinates(m_lines.m_state.m_cursorPosition);
        } else if (character == '\t') {
            auto &line = m_lines.m_unfoldedLines[coord.m_line];
            auto charIndex = m_lines.lineCoordsIndex(coord);

            if (!shift) {
                auto spacesToInsert = m_tabSize - (charIndex % m_tabSize);
                std::string spaces(spacesToInsert, ' ');
                line.insert(line.begin() + charIndex, spaces.begin(), spaces.end());
                line.m_colorized = false;
                m_lines.setCursorPosition(m_lines.lineIndexCoords(coord.m_line + 1, charIndex + spacesToInsert), false);
                u.m_added = spaces;
                u.m_addedRange.m_end = m_lines.lineCoordinates(m_lines.m_state.m_cursorPosition);
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
                u.m_removedRange = Range(lineCoordinates( coord.m_line, charIndex), lineCoordinates( coord.m_line, charIndex + spacesRemoved));
                line.m_colorized = false;
                m_lines.setCursorPosition(m_lines.lineIndexCoords(coord.m_line + 1, std::max(0, charIndex)), false);
            }
            u.m_addedRange.m_end = m_lines.lineCoordinates(m_lines.m_state.m_cursorPosition);
        } else {
            std::string buf;
            imTextCharToUtf8(buf, character);
            if (!buf.empty()) {
                auto &line = m_lines.m_unfoldedLines[coord.m_line];
                auto charIndex = m_lines.lineCoordsIndex(coord);

                if (m_overwrite && charIndex < (i32) line.size()) {
                    i64 column = coord.m_column;
                    std::string c = line[column];
                    auto charCount = stringCharacterCount(c);
                    auto d = c.size();

                    u.m_removedRange = Range(m_lines.m_state.m_cursorPosition, m_lines.lineIndexCoords(coord.m_line + 1, coord.m_column + charCount));
                    u.m_removed = std::string(line.m_chars.begin() + charIndex, line.m_chars.begin() + charIndex + d);
                    line.erase(line.begin() + charIndex, d);
                    line.m_colorized = false;
                }
                auto charCount = stringCharacterCount(buf);
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
                u.m_addedRange.m_end = m_lines.lineIndexCoords(coord.m_line + 1, charIndex + buf.size());
                m_lines.setCursorPosition(m_lines.lineIndexCoords(coord.m_line + 1, charIndex + charCount), false);
            } else
                return;
        }

        u.m_after = m_lines.m_state;
        m_lines.m_textChanged = true;

        UndoRecords v;
        v.push_back(u);
        m_lines.addUndo(v);
        m_lines.colorize();
        m_lines.refreshSearchResults();
        m_lines.ensureCursorVisible();
    }

    void TextEditor::Lines::refreshSearchResults() {
        std::string findWord = m_findReplaceHandler.getFindWord();
        if (!findWord.empty()) {
            m_findReplaceHandler.resetMatches();
            m_findReplaceHandler.findAllMatches(this, findWord);
        }
    }

    void TextEditor::Lines::insertText(const std::string &value) {
        insertText(value.c_str());
    }

    void TextEditor::Lines::insertText(const char *value) {
        if (value == nullptr)
            return;

        insertTextAtCursor(value);

        refreshSearchResults();
        colorize();
    }

    void TextEditor::Lines::deleteSelection() {

        if (m_state.m_selection.m_end == m_state.m_selection.m_start)
            return;

        deleteRange(m_state.m_selection);

        setSelection(Range(m_state.m_selection.m_start, m_state.m_selection.m_start));
        setCursorPosition(m_state.m_selection.m_start, false);
        refreshSearchResults();
        colorize();
    }

    void TextEditor::deleteChar() {
        m_lines.resetCursorBlinkTime();

        if (m_lines.isEmpty() || m_lines.m_readOnly)
            return;

        UndoRecord u;
        u.m_before = m_lines.m_state;

        if (m_lines.hasSelection()) {
            u.m_removed = m_lines.getSelectedText();
            u.m_removedRange = m_lines.m_state.m_selection;
            m_lines.deleteSelection();
        } else {
            auto pos = m_lines.lineCoordinates(m_lines.m_state.m_cursorPosition);
            m_lines.setCursorPosition(pos, false);
            auto &line = m_lines.m_unfoldedLines[pos.m_line];

            if (pos.m_column == line.maxColumn()) {
                if (pos.m_line == m_lines.size() - 1)
                    return;

                u.m_removed = '\n';
                u.m_removedRange.m_start = u.m_removedRange.m_end = m_lines.lineCoordinates(m_lines.m_state.m_cursorPosition);
                advance(u.m_removedRange.m_end);

                auto &nextLine = m_lines.m_unfoldedLines[pos.m_line + 1];
                line.insert(line.end(), nextLine.begin(), nextLine.end());
                line.m_colorized = false;
                m_lines.removeLine(pos.m_line + 1);

            } else {
                i64 charIndex = m_lines.lineCoordsIndex(pos);
                u.m_removedRange.m_start = u.m_removedRange.m_end = m_lines.lineCoordinates(m_lines.m_state.m_cursorPosition);
                u.m_removedRange.m_end.m_column++;
                u.m_removed = m_lines.getRange(u.m_removedRange);

                auto d = utf8CharLength(line[charIndex][0]);
                line.erase(line.begin() + charIndex, d);
                line.m_colorized = false;
            }

            m_lines.m_textChanged = true;

            m_lines.colorize();
        }

        u.m_after = m_lines.m_state;
        UndoRecords v;
        v.push_back(u);
        m_lines.addUndo(v);
        m_lines.refreshSearchResults();
    }

    void TextEditor::backspace() {
        m_lines.resetCursorBlinkTime();
        if (m_lines.isEmpty() || m_lines.m_readOnly)
            return;

        UndoRecord u;
        u.m_before = m_lines.m_state;

        if (m_lines.hasSelection()) {
            u.m_removed = m_lines.getSelectedText();
            u.m_removedRange = m_lines.m_state.m_selection;
            m_lines.deleteSelection();
        } else {
            auto pos = m_lines.lineCoordinates(m_lines.m_state.m_cursorPosition);
            auto &line = m_lines.m_unfoldedLines[pos.m_line];

            if (pos.m_column == 0) {
                if (pos.m_line == 0)
                    return;

                u.m_removed = '\n';
                u.m_removedRange.m_start = u.m_removedRange.m_end = m_lines.lineCoordinates(pos.m_line - 1, -1);
                advance(u.m_removedRange.m_end);

                auto &prevLine = m_lines.m_unfoldedLines[pos.m_line - 1];
                auto prevSize = prevLine.maxColumn();
                if (prevSize == 0)
                    prevLine = line;
                else
                    prevLine.insert(prevLine.end(), line.begin(), line.end());
                prevLine.m_colorized = false;


                ErrorMarkers errorMarker;
                for (auto &i: m_lines.m_errorMarkers)
                    errorMarker.insert(ErrorMarkers::value_type(i.first.m_line - 1 == m_lines.m_state.m_cursorPosition.m_line ? m_lines.lineCoordinates(i.first.m_line - 1, i.first.m_column) : i.first, i.second));
                m_lines.m_errorMarkers = std::move(errorMarker);
                m_lines.removeLine(m_lines.m_state.m_cursorPosition.m_line);
                m_lines.m_state.m_cursorPosition.m_line--;
                m_lines.m_state.m_cursorPosition.m_column = prevSize;
            } else {
                pos.m_column -= 1;
                i64 column = pos.m_column;
                std::string charToRemove = line[column];
                if (pos.m_column < (i32) line.size() - 1) {
                    std::string charToRemoveNext = line[column + 1];
                    if (charToRemove == "{" && charToRemoveNext == "}") {
                        charToRemove += "}";
                        m_lines.m_state.m_cursorPosition.m_column += 1;
                    } else if (charToRemove == "[" && charToRemoveNext == "]") {
                        charToRemove += "]";
                        m_lines.m_state.m_cursorPosition.m_column += 1;
                    } else if (charToRemove == "(" && charToRemoveNext == ")") {
                        charToRemove += ")";
                        m_lines.m_state.m_cursorPosition.m_column += 1;
                    } else if (charToRemove == "\"" && charToRemoveNext == "\"") {
                        charToRemove += "\"";
                        m_lines.m_state.m_cursorPosition.m_column += 1;
                    } else if (charToRemove == "'" && charToRemoveNext == "'") {
                        charToRemove += "'";
                        m_lines.m_state.m_cursorPosition.m_column += 1;
                    }
                }
                u.m_removedRange = Range(pos, m_lines.m_state.m_cursorPosition);
                u.m_removed = charToRemove;
                auto charStart = m_lines.lineCoordsIndex(pos);
                auto charEnd = m_lines.lineCoordsIndex(m_lines.m_state.m_cursorPosition);
                line.erase(line.begin() + charStart, charEnd - charStart);
                m_lines.m_state.m_cursorPosition = pos;
                line.m_colorized = false;
            }

            m_lines.m_textChanged = true;

            m_lines.ensureCursorVisible();
            m_lines.colorize();
        }

        u.m_after = m_lines.m_state;
        UndoRecords v;
        v.push_back(u);
        m_lines.addUndo(v);
        m_lines.refreshSearchResults();
    }

    void TextEditor::copy() {
        if (m_lines.hasSelection()) {
            ImGui::SetClipboardText(m_lines.getSelectedText().c_str());
        } else {
            if (!m_lines.isEmpty()) {
                std::string str;
                const auto &line = m_lines.m_unfoldedLines[m_lines.lineCoordinates(m_lines.m_state.m_cursorPosition).m_line];
                std::ranges::copy(line.m_chars, std::back_inserter(str));
                ImGui::SetClipboardText(str.c_str());
            }
        }
    }

    void TextEditor::cut() {
        if (m_lines.m_readOnly) {
            copy();
        } else {
            if (!m_lines.hasSelection()) {
                auto lineIndex = m_lines.lineCoordinates(m_lines.m_state.m_cursorPosition).m_line;
                if (lineIndex < 0 || lineIndex >= m_lines.size())
                    return;
                setSelection(Range(m_lines.lineCoordinates(lineIndex, 0), m_lines.lineCoordinates(lineIndex + 1, 0)));
            }
            UndoRecord u;
            u.m_before = m_lines.m_state;
            u.m_removed = m_lines.getSelectedText();
            u.m_removedRange = m_lines.m_state.m_selection;

            copy();
            m_lines.deleteSelection();

            u.m_after = m_lines.m_state;
            UndoRecords v;
            v.push_back(u);
            m_lines.addUndo(v);
        }
        m_lines.refreshSearchResults();
    }

    void TextEditor::doPaste(const char *clipText) {
        UndoRecord u;
        if (clipText != nullptr) {
            auto clipTextStr = wolv::util::preprocessText(clipText);

            u.m_before = m_lines.m_state;

            if (m_lines.hasSelection()) {
                u.m_removed = m_lines.getSelectedText();
                u.m_removedRange = m_lines.m_state.m_selection;
                m_lines.deleteSelection();
            }

            u.m_added = clipTextStr;
            u.m_addedRange.m_start = m_lines.lineCoordinates(m_lines.m_state.m_cursorPosition);
            m_lines.insertText(clipTextStr);

            u.m_addedRange.m_end = m_lines.lineCoordinates(m_lines.m_state.m_cursorPosition);


            m_lines.setCursorPosition(u.m_addedRange.m_end, false);
            setSelection(Range(u.m_addedRange.m_end, u.m_addedRange.m_end));
            u.m_after = m_lines.m_state;
            UndoRecords v;
            v.push_back(u);
            m_lines.addUndo(v);
        }
        m_lines.refreshSearchResults();
    }

    void TextEditor::paste() {
        if (m_lines.m_readOnly)
            return;

        const char *clipText =  ImGui::GetClipboardText();
        if (clipText != nullptr) {
            auto stringVector = wolv::util::splitString(clipText, "\n", false);
            if (std::ranges::any_of(stringVector, [](const std::string &s) { return s.size() > 1024; })) {
                ui::PopupQuestion::open("hex.builtin.view.pattern_editor.warning_paste_large"_lang, [this, clipText]() {
                    this->doPaste(clipText);
                }, [] {});
            } else {
                doPaste(clipText);
            }
        }
    }

    bool TextEditor::canUndo() {
        return !m_lines.m_readOnly && m_lines.m_undoIndex > 0;
    }

    bool TextEditor::canRedo() const {
        return !m_lines.m_readOnly && m_lines.m_undoIndex < (i32) m_lines.m_undoBuffer.size();
    }

    void TextEditor::undo() {
        if (canUndo()) {
            m_lines.m_undoIndex--;
            m_lines.m_undoBuffer[m_lines.m_undoIndex].undo(this);
        }
        m_lines.refreshSearchResults();
    }

    void TextEditor::redo() {
        if (canRedo()) {
            m_lines.m_undoBuffer[m_lines.m_undoIndex].redo(this);
            m_lines.m_undoIndex++;
        }
        m_lines.refreshSearchResults();
    }

    std::string TextEditor::Lines::getText(bool includeHiddenLines) {
        auto start = lineCoordinates(0, 0);
        auto size = m_unfoldedLines.size();
        auto line = m_unfoldedLines[size - 1];
        auto end = Coordinates( size - 1, line.m_lineMaxColumn);
        if (start == Invalid || end == Invalid)
            return "";
        std::string result;
        if (includeHiddenLines) {
            for (const auto &hiddenLine: m_hiddenLines) {
                result += hiddenLine.m_line + "\n";
            }
        }
        result += getRange(Range(start, end));
        return result;
    }

    StringVector TextEditor::getTextLines() const {
        StringVector result;

        result.reserve(m_lines.size());

        for (const auto &line: m_lines.m_unfoldedLines) {
            std::string text = line.m_chars;
            result.emplace_back(std::move(text));
        }

        return result;
    }

    std::string TextEditor::Lines::getSelectedText() {
        return getRange(m_state.m_selection);
    }

    std::string TextEditor::getLineText(i32 line) {
        auto sanitizedLine = m_lines.lineCoordinates(line, 0);
        auto endLine = m_lines.lineCoordinates(line, -1);
        if (!sanitizedLine.isValid(m_lines) || !endLine.isValid(m_lines))
            return "";
        return m_lines.getRange(Range(sanitizedLine, endLine));
    }

    TextEditor::UndoRecord::UndoRecord(
            std::string added,
            Range addedRange,
            std::string removed,
            Range removedRange,
            EditorState before,
            EditorState after) : m_added(std::move(added)), m_addedRange(addedRange), m_removed(std::move(removed)), m_removedRange(removedRange), m_before(std::move(before)), m_after(std::move(after)) {}

    void TextEditor::UndoRecord::undo(TextEditor *editor) {
        if (!m_added.empty()) {
            editor->m_lines.deleteRange(m_addedRange);
            editor->m_lines.colorize();
        }

        if (!m_removed.empty()) {
            auto start = m_removedRange.m_start;
            editor->m_lines.insertTextAt(start, m_removed.c_str());
            editor->m_lines.colorize();
        }

        editor->m_lines.m_state = m_before;
        editor->m_lines.ensureCursorVisible();
    }

    void TextEditor::UndoRecord::redo(TextEditor *editor) {
        if (!m_removed.empty()) {
            editor->m_lines.deleteRange(m_removedRange);
            editor->m_lines.colorize();
        }

        if (!m_added.empty()) {
            auto start = m_addedRange.m_start;
            editor->m_lines.insertTextAt(start, m_added.c_str());
            editor->m_lines.colorize();
        }

        editor->m_lines.m_state = m_after;
        editor->m_lines.ensureCursorVisible();
    }

    void TextEditor::UndoAction::undo(TextEditor *editor) {
        for (auto &record : std::ranges::reverse_view(m_records))
            record.undo(editor);
    }

    void TextEditor::UndoAction::redo(TextEditor *editor) {
        for (auto &record : m_records)
            record.redo(editor);
    }

}