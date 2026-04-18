#include <ui/text_editor.hpp>
#include <hex/helpers/scaling.hpp>
#include <wolv/utils/string.hpp>
#include <fonts/fonts.hpp>
#include <algorithm>

namespace hex::ui {


    using Coordinates   = TextEditor::Coordinates;
    using Segments      = TextEditor::Segments;

    TextEditor::Line TextEditor::Line::trim(TrimMode trimMode) {
        if (m_chars.empty())
            return m_emptyLine;
        std::string trimmed = wolv::util::trim(m_chars);
        auto idx = m_chars.find(trimmed);
        if (idx == std::string::npos)
            return m_emptyLine;
        if (trimMode == TrimMode::TrimNone)
            return *this;
        else if (trimMode == TrimMode::TrimEnd)
            return subLine(0, idx + trimmed.size());
        else if (trimMode == TrimMode::TrimStart)
            return subLine(idx, size() - idx);
        else
            return subLine(idx, trimmed.size());
    }

    i32 TextEditor::Line::columnIndex(i32 column) const {

        i32 idx = 0;
        for (i32 col = 0;  idx < (i32) size() && col < column; ++col)
            idx += TextEditor::utf8CharLength(m_chars[idx]);

        return idx;
    }

    i32 TextEditor::Line::maxColumn() {
        if (m_lineMaxColumn > 0)
            return m_lineMaxColumn;
        m_lineMaxColumn = indexColumn((i32) size());
        return m_lineMaxColumn;
    }

    i32 TextEditor::Line::maxColumn() const {
        if (m_lineMaxColumn > 0)
            return m_lineMaxColumn;
        return indexColumn((i32) size());
    }

    i32 TextEditor::Line::indexColumn(i32 stringIndex) const {
        i32 limit = std::max(0, std::min(stringIndex, (i32) size()));

        i32 col = 0;
        for (i32 idx = 0; idx < limit; col++)
            idx += TextEditor::utf8CharLength(m_chars[idx]);

        return col;
    }

    i32 TextEditor::Line::stringTextSize(const std::string &str) const {
        if (str.empty())
            return 0;
        if (ImGui::GetFont() == nullptr) {
            fonts::CodeEditor().push();
            i32 result =  ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, str.c_str(), nullptr, nullptr).x;
            fonts::CodeEditor().pop();
            return result;
        }
        return ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, str.c_str(), nullptr, nullptr).x;
    }

    i32 TextEditor::Line::textSize(u32 index) const {
        if (m_chars.empty())
            return 0;
        return stringTextSize(m_chars.substr(0, index));
    }

    i32 TextEditor::Line::textSize() const {
        if (m_chars.empty())
            return 0;
        return stringTextSize(m_chars);
    }

    i32 TextEditor::Line::lineTextSize(TrimMode trimMode) {
        auto trimmedLine = trim(trimMode);
        return trimmedLine.textSize();
    }

    i32 TextEditor::Line::textSizeIndex(float textSize, i32 position) {
        i32 result = textSize /  ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, "#", nullptr, nullptr).x;
        auto currentSize = stringTextSize(m_chars.substr(position, result));
        while (currentSize < textSize && (u32)(position + result) < size()) {
            result += TextEditor::utf8CharLength(m_chars[position + result]);
            currentSize = stringTextSize(m_chars.substr(position, result));
        }
        return result;
    }

    // https://en.wikipedia.org/wiki/UTF-8
// We assume that the char is a standalone character (<128) or a leading byte of an UTF-8 code sequence (non-10xxxxxx code)
    i32 TextEditor::utf8CharLength(u8 c) {
        if ((c & 0xFE) == 0xFC)
            return 6;
        if ((c & 0xFC) == 0xF8)
            return 5;
        if ((c & 0xF8) == 0xF0)
            return 4;
        if ((c & 0xF0) == 0xE0)
            return 3;
        if ((c & 0xE0) == 0xC0)
            return 2;
        return 1;
    }

    i32 TextEditor::stringCharacterCount(const std::string &str) {
        if (str.empty())
            return 0;
        i32 count = 0;
        for (u32 idx = 0; idx < str.size(); count++)
            idx += TextEditor::utf8CharLength(str[idx]);
        return count;
    }

    i32 TextEditor::lineIndexColumn(i32 lineIndex, i32 stringIndex) {
        if (lineIndex >= (i64) m_lines.size() || lineIndex < 0)
            return 0;
        Line &line = m_lines[lineIndex];
        return line.indexColumn(stringIndex);
    }

    i32 TextEditor::Lines::lineMaxColumn(i32 lineIndex) {
        if (lineIndex >= (i64) size() || lineIndex < 0)
            return 0;

        return operator[](lineIndex).maxColumn();
    }

    // "Borrowed" from ImGui source
    void TextEditor::imTextCharToUtf8(std::string &buffer, u32 c) {
        if (c < 0x80) {
            buffer += (char) c;
            return;
        }
        if (c < 0x800) {
            buffer += (char) (0xc0 + (c >> 6));
            buffer += (char) (0x80 + (c & 0x3f));
            return;
        }
        if (c >= 0xdc00 && c < 0xe000)
            return;
        if (c >= 0xd800 && c < 0xdc00) {
            buffer += (char) (0xf0 + (c >> 18));
            buffer += (char) (0x80 + ((c >> 12) & 0x3f));
            buffer += (char) (0x80 + ((c >> 6) & 0x3f));
            buffer += (char) (0x80 + ((c) & 0x3f));
            return;
        }
        // else if (c < 0x10000)
        {
            buffer += (char) (0xe0 + (c >> 12));
            buffer += (char) (0x80 + ((c >> 6) & 0x3f));
            buffer += (char) (0x80 + ((c) & 0x3f));
            return;
        }
    }

    i32 TextEditor::imTextCharToUtf8(char *buffer, i32 buf_size, u32 c) {
        std::string input;
        imTextCharToUtf8(input, c);
        auto size = std::min((i32)input.size(),buf_size - 1);
        i32 i = 0;
        for (; i < size; i++)
            buffer[i] = input[i];
        buffer[i] = 0;
        return size;
    }

    Coordinates TextEditor::screenPosCoordinates(const ImVec2 &position) {
        auto boxSize = m_lines.m_charAdvance.x + (((u32)m_lines.m_charAdvance.x % 2) ? 2.0f : 1.0f);
        if (m_lines.isEmpty())
            return m_lines.lineCoordinates( 0, 0);
        auto lineSize = m_lines.size();
        if (position.y > m_lines.getLineStartScreenPos(0, lineIndexToRow(lineSize - 1)).y + m_lines.m_charAdvance.y)
            return m_lines.lineCoordinates( -1, -1);

        auto local = position - m_lines.m_cursorScreenPosition;
        auto row = screenPosToRow(position);

        Coordinates result = lineCoordinates(0,0);
        i32 lineIndex= rowToLineIndex((i32) std::floor(row));
        if (lineIndex < 0 || lineIndex >= m_lines.size())
            return Invalid;
        result.m_line = lineIndex;
        if (m_lines.m_codeFoldKeyLineMap.contains(lineIndex) || m_lines.m_codeFoldValueLineMap.contains(lineIndex)) {
            if (local.x < (boxSize - 1)/2)
                return Invalid;
        }
        else if (local.x < 0 || m_lines[result.m_line].empty())
            return m_lines.lineCoordinates( result.m_line, 0);


        auto &line = m_lines[result.m_line].m_chars;
        i32 count = 0;
        u64 length;
        i32 increase;
        do {
            increase = TextEditor::utf8CharLength(line[count]);
            count += increase;
            std::string partialLine = line.substr(0, count);
            length = ImGui::CalcTextSize(partialLine.c_str(), nullptr, false, m_lines.m_charAdvance.x * count).x;
        } while (length < local.x && count < (i32) line.size() + increase);

        result = m_lines.lineIndexCoords(lineIndex + 1, count - increase);
        result = m_lines.foldedToUnfoldedCoords(result);
        if (result == Invalid)
            return {0, 0};
        return result;
    }

    Coordinates TextEditor::lineCoordsToIndexCoords(const Coordinates &coordinates) {
        if (coordinates.m_line >= (i64) m_lines.size())
            return Invalid;

        const auto &line = m_lines[coordinates.m_line];
        return {coordinates.m_line,line.columnIndex(coordinates.m_column)};
    }

    i32 TextEditor::Lines::lineCoordsIndex(const Coordinates &coordinates)  {
        if (coordinates.m_line >= (i64) size())
            return -1;

        auto &line = operator[](coordinates.m_line);
        return line.columnIndex(coordinates.m_column);
    }

    Coordinates TextEditor::Lines::lineIndexCoords(i32 lineNumber, i32 stringIndex) {
        if (lineNumber < 1 || lineNumber > size())
            return lineCoordinates( 0, 0);
        auto &line = operator[](lineNumber - 1);
        return lineCoordinates(lineNumber - 1, line.indexColumn(stringIndex));
    }

    Segments TextEditor::Lines::unfoldedEllipsisCoordinates(Range delimiterCoordinates) {

        auto lineStart = m_unfoldedLines[delimiterCoordinates.m_start.m_line];
        auto row = lineIndexToRow(delimiterCoordinates.m_start.m_line);
        float unfoldedSpan1, unfoldedSpan2, unfoldedSpan3;
        float unprocessedSpan1, unprocessedSpan2, unprocessedSpan3;
        bool adddsBothEnds = true;
        if (delimiterCoordinates.m_start.m_line == delimiterCoordinates.m_end.m_line) {
            unprocessedSpan1 = unfoldedSpan1 = delimiterCoordinates.m_end.m_column - delimiterCoordinates.m_start.m_column - 1;
            unprocessedSpan3 = unfoldedSpan3 = 0.0f;
            unprocessedSpan2 = unfoldedSpan2 = 0.0f;
        } else if (!m_foldedLines[row].addsFullFirstLineToFold() && !m_foldedLines[row].addsLastLineToFold())  {
            adddsBothEnds = false;
            auto innerLine = m_unfoldedLines[delimiterCoordinates.m_start.m_line];
            unprocessedSpan1 = unfoldedSpan1 = std::max(innerLine.maxColumn() - 1, 0);
            innerLine = m_unfoldedLines[delimiterCoordinates.m_end.m_line];
            unprocessedSpan3 = unfoldedSpan3 = std::max(innerLine.maxColumn() - 1, 0);
            unfoldedSpan2 = 0;
            for (i32 j = delimiterCoordinates.m_start.m_line + 1; j < delimiterCoordinates.m_end.m_line; j++) {
                innerLine = m_unfoldedLines[j];
                unfoldedSpan2 += innerLine.maxColumn();
            }
            unprocessedSpan2 = unfoldedSpan2;
        } else {
            unprocessedSpan1 = unfoldedSpan1 = std::max(lineStart.maxColumn() - delimiterCoordinates.m_start.m_column - 2, 0);
            unprocessedSpan3 = unfoldedSpan3 = std::max(delimiterCoordinates.m_end.m_column - 1, 0);
            unfoldedSpan2 = 0;
            for (i32 j = delimiterCoordinates.m_start.m_line + 1; j < delimiterCoordinates.m_end.m_line; j++) {
                auto innerLine = m_unfoldedLines[j];
                unfoldedSpan2 += innerLine.maxColumn();
            }
            unprocessedSpan2 = unfoldedSpan2;
        }

        auto totalUnfoldedSpan = unfoldedSpan1 + unfoldedSpan2 + unfoldedSpan3;
        if (totalUnfoldedSpan < 2.0f) {
            Segments unfoldedEllipsisCoordinates(2);
            unfoldedEllipsisCoordinates[0] = lineCoordinates( delimiterCoordinates.m_start.m_line, delimiterCoordinates.m_start.m_column + 1);
            unfoldedEllipsisCoordinates[1] = delimiterCoordinates.m_end;
            return unfoldedEllipsisCoordinates;
        }

        float spanFragment = totalUnfoldedSpan / 2.0f;

        Segments unfoldedEllipsisCoordinates(4);
        if (adddsBothEnds) {
            unfoldedEllipsisCoordinates[0] = lineCoordinates(delimiterCoordinates.m_start.m_line, delimiterCoordinates.m_start.m_column + 1);
            unfoldedEllipsisCoordinates[3] = delimiterCoordinates.m_end;
        } else {
            unfoldedEllipsisCoordinates[0] = delimiterCoordinates.m_start;
            unfoldedEllipsisCoordinates[1] = lineCoordinates(delimiterCoordinates.m_start.m_line, delimiterCoordinates.m_start.m_column + 1);
            unfoldedEllipsisCoordinates[2] = delimiterCoordinates.m_end;
            unfoldedEllipsisCoordinates[3] = lineCoordinates(delimiterCoordinates.m_end.m_line, delimiterCoordinates.m_end.m_column + 1);
            return unfoldedEllipsisCoordinates;
        }


        i32 i = 1;
        while ((unprocessedSpan1 > spanFragment || std::fabs(unprocessedSpan1-spanFragment) < 0.001) && i < 3) {
            unfoldedEllipsisCoordinates[i] = lineCoordinates( unfoldedEllipsisCoordinates[0].m_line, 1 + unfoldedEllipsisCoordinates[0].m_column + std::round(i * spanFragment));
            unprocessedSpan1 -= spanFragment;
            i++;
        }
        auto leftOver = unprocessedSpan1;
        unprocessedSpan2 += leftOver;
        if ((unprocessedSpan2 > spanFragment || std::fabs(unprocessedSpan2 - spanFragment) < 0.001) && i < 3) {
            float lineLength = 0.0f;
            for (i32 j = delimiterCoordinates.m_start.m_line + 1; j < delimiterCoordinates.m_end.m_line; j++) {
                auto currentLineLength = (float) m_unfoldedLines[j].maxColumn();
                lineLength += currentLineLength + leftOver;
                leftOver = 0.0f;
                while ((lineLength > spanFragment || std::fabs(lineLength-spanFragment) < 0.001) && i < 3) {
                    unfoldedEllipsisCoordinates[i] = lineCoordinates( j, std::round(currentLineLength - lineLength + spanFragment));
                    unprocessedSpan2 -= spanFragment;
                    lineLength -= spanFragment;
                    i++;
                }
            }
        }
        unprocessedSpan3 += unprocessedSpan2;
        leftOver = unprocessedSpan2;
        auto firstI = i;
        while ((unprocessedSpan3 >= spanFragment || std::fabs(unprocessedSpan3-spanFragment) < 0.001) && i < 3) {
            unfoldedEllipsisCoordinates[i] = lineCoordinates( unfoldedEllipsisCoordinates[3].m_line, std::round((i - firstI + 1) * (spanFragment - leftOver)));
            unprocessedSpan3 -= spanFragment;
            i++;
        }

        return unfoldedEllipsisCoordinates;
    }

    Coordinates TextEditor::Lines::foldedToUnfoldedCoords(const Coordinates &coords) {
        auto row = lineIndexToRow(coords.m_line);
        if (row == -1.0 || !m_foldedLines.contains(row))
            return coords;
        FoldedLine foldedLine = m_foldedLines[row];
        auto foldedSegments = foldedLine.m_foldedSegments;
        i32 foundIndex = -1;
        i32 loopLimit = (i32) 2 *  foldedLine.m_keys.size();
        if (loopLimit == 0)
            return coords;
        Range::EndsInclusive endsInclusive = Range::EndsInclusive::Start;
        for (i32 i = 0; i <= loopLimit; i++) {
            if (Range(foldedSegments[i], foldedSegments[i + 1]).contains(coords, endsInclusive)) {
                foundIndex = i;
                break;
            }

            if ((i + 1) % 2)
                endsInclusive = Range::EndsInclusive::Both;
            else if ((i + 1) == loopLimit)
                endsInclusive = Range::EndsInclusive::End;
            else
                endsInclusive = Range::EndsInclusive::None;

        }
        if (foundIndex < 0)
            return coords;

        Range key = foldedLine.m_keys[(foundIndex / 2)-(foundIndex == loopLimit)];

        if (foundIndex % 2) {

            Range delimiterRange = foldedLine.findDelimiterCoordinates(key);
            Segments unfoldedEllipsisCoordinates = this->unfoldedEllipsisCoordinates(delimiterRange);

            if (unfoldedEllipsisCoordinates.size() > 2)
                return unfoldedEllipsisCoordinates[coords.m_column - foldedLine.m_ellipsisIndices[foundIndex / 2]];
            else {
                auto ellipsisColumn = foldedLine.m_ellipsisIndices[foundIndex / 2];
                if (coords.m_column == ellipsisColumn || coords.m_column == ellipsisColumn + 2)
                    return unfoldedEllipsisCoordinates[0];
                else
                    return unfoldedEllipsisCoordinates[1];
            }
        } else {
            auto unfoldedSegmentStart = foldedLine.m_unfoldedSegments[foundIndex];
            auto foldedSegmentStart = foldedLine.m_foldedSegments[foundIndex];
            if (foundIndex == 0) {
                if (lineNeedsDelimiter(key.m_start.m_line)) {
                    auto line = m_unfoldedLines[key.m_start.m_line];
                    auto delimiterCoordinates = foldedLine.findDelimiterCoordinates(key);
                    if (coords.m_column > line.maxColumn())
                        return delimiterCoordinates.m_start;
                    else
                        return lineCoordinates( unfoldedSegmentStart.m_line, coords.m_column);
                }
                return lineCoordinates( unfoldedSegmentStart.m_line, coords.m_column);
            } else
                return lineCoordinates( unfoldedSegmentStart.m_line, coords.m_column - foldedSegmentStart.m_column + unfoldedSegmentStart.m_column);
        }
     }

     Coordinates TextEditor::nextCoordinate(TextEditor::Coordinates coordinate) {
        auto line = m_lines[coordinate.m_line];
        if (line.isEndOfLine(coordinate.m_column))
            return {coordinate.m_line + 1, 0};
        else
            return {coordinate.m_line,coordinate.m_column + 1};
    }
     bool TextEditor::testfoldMaps(TextEditor::Range toTest) {
        bool result = true;
        for (auto test = toTest.getStart(); test <= toTest.getEnd(); test = nextCoordinate(test)) {
            auto data = test;
            auto folded = m_lines.unfoldedToFoldedCoords(data);
            auto unfolded = m_lines.foldedToUnfoldedCoords(folded);

            result = result &&  (data == unfolded);
        }
         return result;
    }

    Coordinates TextEditor::Lines::unfoldedToFoldedCoords(const Coordinates &coords) {
        auto row = lineIndexToRow(coords.m_line);
        Coordinates result;
        if (row == -1 || !m_foldedLines.contains(row))
            return coords;

        FoldedLine foldedLine = m_foldedLines[row];
        result.m_line = foldedLine.m_full.m_start.m_line;

        i32 foundIndex = -1;
        i32 loopLimit = (i32) 2 * foldedLine.m_keys.size();

        if (loopLimit == 0)
            return coords;
        Range::EndsInclusive endsInclusive = Range::EndsInclusive::Start;
        for (i32 i = 0; i <= loopLimit; i++) {
            Range range = Range(foldedLine.m_unfoldedSegments[i], foldedLine.m_unfoldedSegments[i + 1]);

            if (range.contains(coords, endsInclusive)) {
                foundIndex = i;
                break;
            }
            if ((i + 1) % 2)
                endsInclusive = Range::EndsInclusive::Both;
            else if ((i + 1) == loopLimit)
                endsInclusive = Range::EndsInclusive::End;
            else
                endsInclusive = Range::EndsInclusive::None;
        }
        if (foundIndex < 0)
            return coords;

        Range key = foldedLine.m_keys[(foundIndex / 2) - (foundIndex == loopLimit)];

        if (foundIndex % 2) {
            result.m_column = foldedLine.m_ellipsisIndices[foundIndex / 2];
            Range delimiterRange = foldedLine.findDelimiterCoordinates(key);
            Segments unfoldedEllipsisCoordinates = this->unfoldedEllipsisCoordinates(delimiterRange);

            if (unfoldedEllipsisCoordinates.size() > 2) {
                if (coords == unfoldedEllipsisCoordinates[0])
                    return result;
                if (coords == unfoldedEllipsisCoordinates[3]) {
                    result.m_column += 3;
                    return result;
                }
                if (Range(unfoldedEllipsisCoordinates[0], unfoldedEllipsisCoordinates[1]).contains(coords, Range::EndsInclusive::End)) {
                    result.m_column += 1;
                    return result;
                }
                if (Range(unfoldedEllipsisCoordinates[1], unfoldedEllipsisCoordinates[2]).contains(coords, Range::EndsInclusive::End)) {
                    result.m_column += 2;
                    return result;
                }
                return coords;
            } else {
                if (coords > unfoldedEllipsisCoordinates[0])
                    result.m_column += 3;
                return result;
            }
        } else {
            if (foundIndex == 0) {
                if (foldedLine.firstLineNeedsDelimiter()) {
                    auto line = m_unfoldedLines[foldedLine.m_full.m_start.m_line];
                    if (coords > lineCoordinates(foldedLine.m_full.m_start.m_line, line.maxColumn()))
                        result.m_column = foldedLine.m_ellipsisIndices[0] - 1;
                    else
                        result.m_column = coords.m_column;
                    return result;
                }

                result.m_column = coords.m_column;
                return result;
            } else
                result.m_column = coords.m_column - foldedLine.m_unfoldedSegments[foundIndex].m_column + foldedLine.m_foldedSegments[foundIndex].m_column;

            return result;
        }
    }

    Coordinates TextEditor::Lines::stringIndexCoords(i32 strIndex, const std::string &input) {
        if (strIndex < 0 || strIndex > (i32) input.size())
            return lineCoordinates( 0, 0);
        std::string str = input.substr(0, strIndex);
        auto line = std::count(str.begin(), str.end(), '\n');
        auto index = str.find_last_of('\n');
        str = str.substr(index + 1);
        auto col = stringCharacterCount(str);

        return lineCoordinates( line, col);
    }
}
