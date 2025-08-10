#include <ui/text_editor.hpp>
#include <algorithm>

namespace hex::ui {
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

    i32 TextEditor::getStringCharacterCount(const std::string &str) {
        if (str.empty())
            return 0;
        i32 count = 0;
        for (u32 idx = 0; idx < str.size(); count++)
            idx += TextEditor::utf8CharLength(str[idx]);
        return count;
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

    static i32 utf8CharCount(const std::string &line, i32 start, i32 numChars) {
        if (line.empty())
            return 0;

        i32 index = 0;
        for (i32 column = 0; start + index < (i32) line.size() && column < numChars; ++column)
            index += TextEditor::utf8CharLength(line[start + index]);

        return index;
    }

    TextEditor::Coordinates TextEditor::screenPosToCoordinates(const ImVec2 &position) const {
        ImVec2 local = position - ImGui::GetCursorScreenPos();
        i32 lineNo = std::max(0, (i32) floor(local.y / m_charAdvance.y));
        if (local.x < (m_leftMargin - 2) || lineNo >= (i32) m_lines.size() || m_lines[lineNo].empty())
            return setCoordinates(std::min(lineNo, (i32) m_lines.size() - 1), 0);
        std::string line = m_lines[lineNo].m_chars;
        local.x -= (m_leftMargin - 5);
        i32 count = 0;
        u64 length;
        i32 increase;
        do {
            increase = TextEditor::utf8CharLength(line[count]);
            count += increase;
            std::string partialLine = line.substr(0, count);
            length = ImGui::CalcTextSize(partialLine.c_str(), nullptr, false, m_charAdvance.x * count).x;
        } while (length < local.x && count < (i32) line.size() + increase);

        auto result = getCharacterCoordinates(lineNo, count - increase);
        result = setCoordinates(result);
        if (result == Invalid)
            return Coordinates(0, 0);
        return result;
    }

    TextEditor::Coordinates TextEditor::lineCoordsToIndexCoords(const Coordinates &coordinates) const {
        if (coordinates.m_line >= (i64) m_lines.size())
            return Invalid;

        const auto &line = m_lines[coordinates.m_line];
        return Coordinates(coordinates.m_line,utf8CharCount(line.m_chars, 0, coordinates.m_column));
    }

    i32 TextEditor::lineCoordinatesToIndex(const Coordinates &coordinates) const {
        if (coordinates.m_line >= (i64) m_lines.size())
            return -1;

        const auto &line = m_lines[coordinates.m_line];
        return utf8CharCount(line.m_chars, 0, coordinates.m_column);
    }

    i32 TextEditor::Line::getCharacterColumn(i32 index) const {
        i32 col = 0;
        i32 i = 0;
        while (i < index && i < (i32) size()) {
            auto c = m_chars[i];
            i += TextEditor::utf8CharLength(c);
            col++;
        }
        return col;
    }

    TextEditor::Coordinates TextEditor::getCharacterCoordinates(i32 lineIndex, i32 strIndex) const {
        if (lineIndex < 0 || lineIndex >= (i32) m_lines.size())
            return Coordinates(0, 0);
        auto &line = m_lines[lineIndex];
        return setCoordinates(lineIndex, line.getCharacterColumn(strIndex));
    }

    u64 TextEditor::getLineByteCount(i32 lineIndex) const {
        if (lineIndex >= (i64) m_lines.size() || lineIndex < 0)
            return 0;

        auto &line = m_lines[lineIndex];
        return line.size();
    }

    i32 TextEditor::getLineCharacterCount(i32 line) const {
        return getLineMaxColumn(line);
    }

    i32 TextEditor::getLineMaxColumn(i32 line) const {
        if (line >= (i64) m_lines.size() || line < 0)
            return 0;
        return getStringCharacterCount(m_lines[line].m_chars);
    }

    TextEditor::Coordinates TextEditor::stringIndexToCoordinates(i32 strIndex, const std::string &input) {
        if (strIndex < 0 || strIndex > (i32) input.size())
            return TextEditor::Coordinates(0, 0);
        std::string str = input.substr(0, strIndex);
        auto line = std::count(str.begin(), str.end(), '\n');
        auto index = str.find_last_of('\n');
        str = str.substr(index + 1);
        auto col = TextEditor::getStringCharacterCount(str);

        return TextEditor::Coordinates(line, col);
    }
}
