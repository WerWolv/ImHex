#include <ui/text_editor.hpp>
#include <hex/helpers/scaling.hpp>
#include <wolv/utils/string.hpp>
#include <fonts/fonts.hpp>
#include <algorithm>

namespace hex::ui {

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
         i32 result = 0;
        if (str.empty())
            return 0;
        if (ImGui::GetFont() == nullptr) {
            fonts::CodeEditor().push();
            result =  ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, str.c_str(), nullptr, nullptr).x;
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

    i32 TextEditor::lineMaxColumn(i32 lineIndex) {
        if (lineIndex >= (i64) m_lines.size() || lineIndex < 0)
            return 0;

        return m_lines[lineIndex].maxColumn();
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

    TextEditor::Coordinates TextEditor::screenPosToCoordinates(const ImVec2 &position) {
        ImVec2 local = position - ImGui::GetCursorScreenPos();
        i32 lineNo = std::max(0, (i32) floor(local.y / m_charAdvance.y));
        if (lineNo >= (i32) m_lines.size())
            return setCoordinates((i32) m_lines.size() - 1, -1);
        else if (local.x < (m_leftMargin - 2_scaled) || m_lines[lineNo].empty())
            return setCoordinates(lineNo, 0);
        std::string line = m_lines[lineNo].m_chars;
        local.x -= (m_leftMargin - 5_scaled);
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
            return {0, 0};
        return result;
    }

    TextEditor::Coordinates TextEditor::lineCoordsToIndexCoords(const Coordinates &coordinates) const {
        if (coordinates.m_line >= (i64) m_lines.size())
            return Invalid;

        const auto &line = m_lines[coordinates.m_line];
        return {coordinates.m_line,line.columnIndex(coordinates.m_column)};
    }

    i32 TextEditor::lineCoordinatesToIndex(const Coordinates &coordinates) const {
        if (coordinates.m_line >= (i64) m_lines.size())
            return -1;

        const auto &line = m_lines[coordinates.m_line];
        return line.columnIndex(coordinates.m_column);
    }

    TextEditor::Coordinates TextEditor::getCharacterCoordinates(i32 lineIndex, i32 strIndex) {
        if (lineIndex < 0 || lineIndex >= (i32) m_lines.size())
            return {0, 0};
        auto &line = m_lines[lineIndex];
        return setCoordinates(lineIndex, line.indexColumn(strIndex));
    }

    u64 TextEditor::getLineByteCount(i32 lineIndex) const {
        if (lineIndex >= (i64) m_lines.size() || lineIndex < 0)
            return 0;

        auto &line = m_lines[lineIndex];
        return line.size();
    }

    TextEditor::Coordinates TextEditor::stringIndexToCoordinates(i32 strIndex, const std::string &input) {
        if (strIndex < 0 || strIndex > (i32) input.size())
            return {0, 0};
        std::string str = input.substr(0, strIndex);
        i32 line = std::count(str.begin(), str.end(), '\n');
        auto index = str.find_last_of('\n');
        str = str.substr(index + 1);
        i32 col = TextEditor::stringCharacterCount(str);

        return {line, col};
    }
}
