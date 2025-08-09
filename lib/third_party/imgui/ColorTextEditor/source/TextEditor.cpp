#include <algorithm>
#include <chrono>
#include <string>
#include <regex>
#include <cmath>
#include <iostream>

#include "TextEditor.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"    // for imGui::GetCurrentWindow()
#include "imgui_internal.h"

namespace hex::ui {
    template<class InputIt1, class InputIt2, class BinaryPredicate>
    bool equals(InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2, BinaryPredicate p) {
        for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
            if (!p(*first1, *first2))
                return false;
        }
        return first1 == last1 && first2 == last2;
    }

    const int32_t TextEditor::s_cursorBlinkInterval = 1200;
    const int32_t TextEditor::s_cursorBlinkOnTime = 800;
    ImVec2 TextEditor::s_cursorScreenPosition;

    TextEditor::Palette s_paletteBase = TextEditor::getDarkPalette();

    TextEditor::FindReplaceHandler::FindReplaceHandler() : m_wholeWord(false), m_findRegEx(false), m_matchCase(false) {}
    const std::string TextEditor::MatchedBracket::s_separators = "()[]{}";
    const std::string TextEditor::MatchedBracket::s_operators = "<>";

// https://en.wikipedia.org/wiki/UTF-8
// We assume that the char is a standalone character (<128) or a leading byte of an UTF-8 code sequence (non-10xxxxxx code)
    int32_t utf8CharLength(uint8_t c) {
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

    int32_t getStringCharacterCount(const std::string &str) {
        if (str.empty())
            return 0;
        int32_t count = 0;
        for (uint32_t idx = 0; idx < str.size(); count++)
            idx += utf8CharLength(str[idx]);
        return count;
    }


    TextEditor::TextEditor() {
        m_startTime = ImGui::GetTime() * 1000;
        setLanguageDefinition(LanguageDefinition::HLSL());
        m_lines.push_back(Line());
    }

    TextEditor::~TextEditor() {
    }


    ImVec2 TextEditor::underwaves(ImVec2 pos, uint32_t nChars, ImColor color, const ImVec2 &size_arg) {
        auto save = ImGui::GetStyle().AntiAliasedLines;
        ImGui::GetStyle().AntiAliasedLines = false;
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        window->DC.CursorPos = pos;
        const ImVec2 label_size = ImGui::CalcTextSize("W", nullptr, true);
        ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x, label_size.y);
        float lineWidth = size.x / 3.0f + 0.5f;
        float halfLineW = lineWidth / 2.0f;

        for (uint32_t i = 0; i < nChars; i++) {
            pos = window->DC.CursorPos;
            float lineY = pos.y + size.y;

            ImVec2 pos1_1 = ImVec2(pos.x + 0 * lineWidth, lineY + halfLineW);
            ImVec2 pos1_2 = ImVec2(pos.x + 1 * lineWidth, lineY - halfLineW);
            ImVec2 pos2_1 = ImVec2(pos.x + 2 * lineWidth, lineY + halfLineW);
            ImVec2 pos2_2 = ImVec2(pos.x + 3 * lineWidth, lineY - halfLineW);

            ImGui::GetWindowDrawList()->AddLine(pos1_1, pos1_2, ImU32(color), 0.4f);
            ImGui::GetWindowDrawList()->AddLine(pos1_2, pos2_1, ImU32(color), 0.4f);
            ImGui::GetWindowDrawList()->AddLine(pos2_1, pos2_2, ImU32(color), 0.4f);

            window->DC.CursorPos = ImVec2(pos.x + size.x, pos.y);
        }
        auto ret = window->DC.CursorPos;
        ret.y += size.y;
        return ret;
    }

    void TextEditor::setLanguageDefinition(const LanguageDefinition &languageDef) {
        m_languageDefinition = languageDef;
        m_regexList.clear();

        for (auto &r: m_languageDefinition.m_tokenRegexStrings)
            m_regexList.push_back(std::make_pair(std::regex(r.first, std::regex_constants::optimize), r.second));

        colorize();
    }

    const TextEditor::Palette &TextEditor::getPalette() { return s_paletteBase; }

    void TextEditor::setPalette(const Palette &value) {
        s_paletteBase = value;
    }

    bool TextEditor::isEndOfLine(const Coordinates &coordinates) const {
        if (coordinates.m_line < (int32_t) m_lines.size())
            return coordinates.m_column >= getStringCharacterCount(m_lines[coordinates.m_line].m_chars);
        return true;
    }

    bool TextEditor::isEndOfFile(const Coordinates &coordinates) const {
        if (coordinates.m_line < (int32_t) m_lines.size())
            return coordinates.m_line >= (int32_t) m_lines.size() - 1 && isEndOfLine(coordinates);
        return true;
    }

    std::string TextEditor::getText(const Selection &from) const {
        std::string result = "";
        auto selection = setCoordinates(from);
        auto columns = selection.getSelectedColumns();
        if (selection.isSingleLine())
            result = m_lines[selection.m_start.m_line].substr(columns.m_line, columns.m_column, Line::LinePart::Utf8);
        else {
            auto lines = selection.getSelectedLines();
            result = m_lines[lines.m_line].substr(columns.m_line, -1, Line::LinePart::Utf8) + '\n';
            for (uint64_t i = lines.m_line + 1; i < lines.m_column; i++) {
                result += m_lines[i].m_chars + '\n';
            }
            result += m_lines[lines.m_column].substr(0, columns.m_column, Line::LinePart::Utf8);
        }

        return result;
    }

    TextEditor::Coordinates TextEditor::setCoordinates(int32_t line, int32_t column) const {
        if (isEmpty())
            return Coordinates(0, 0);

        Coordinates result = Coordinates(0, 0);
        auto lineCount = (int32_t) m_lines.size();
        if (line < 0 && lineCount + line >= 0)
            result.m_line = lineCount + line;
        else
            result.m_line = std::clamp(line, 0, lineCount - 1);

        auto maxColumn = getLineMaxColumn(result.m_line) + 1;
        if (column < 0 && maxColumn + column >= 0)
            result.m_column = maxColumn + column;
        else
            result.m_column = std::clamp(column, 0, maxColumn - 1);

        return result;
    }

    TextEditor::Coordinates TextEditor::setCoordinates(const Coordinates &value) const {
        auto sanitized_value = setCoordinates(value.m_line, value.m_column);
        return sanitized_value;
    }

    TextEditor::Selection TextEditor::setCoordinates(const Selection &value) const {
        auto start = setCoordinates(value.m_start);
        auto end = setCoordinates(value.m_end);
        if (start == Invalid || end == Invalid)
            return Selection(Invalid, Invalid);
        if (start > end) {
            std::swap(start, end);
        }
        return Selection(start, end);
    }

// "Borrowed" from ImGui source
    static inline void imTextCharToUtf8(std::string &buffer, uint32_t c) {
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

    static inline int32_t imTextCharToUtf8(char *buffer, int32_t buf_size, uint32_t c) {
        std::string input;
        imTextCharToUtf8(input, c);
        auto size = input.size();
        int32_t i = 0;
        for (; i < size; i++)
            buffer[i] = input[i];
        buffer[i] = 0;
        return size;
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
        int32_t column = coordinates.m_column;
        std::string lineChar = line[column];
        auto incr = getStringCharacterCount(lineChar);
        coordinates.m_column += incr;
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
            firstLine.erase(columns.m_line, -1);
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
        auto text = replaceStrings(preprocessText(value), "\000", ".");
        if (text.empty())
            return;
        if (isEmpty()) {
            m_lines[0].m_chars = text;
            m_lines[0].m_colors = std::string(text.size(), 0);
            m_lines[0].m_flags = std::string(text.size(), 0);
        } else
            m_lines.push_back(Line(text));

        setCursorPosition(setCoordinates((int32_t) m_lines.size() - 1, 0));
        m_lines.back().m_colorized = false;
        ensureCursorVisible();
        m_textChanged = true;
    }


    int32_t TextEditor::insertTextAt(Coordinates /* inout */ &where, const std::string &value) {
        if (value.empty())
            return 0;
        auto start = setCoordinates(where);
        if (start == Invalid)
            return 0;
        auto &line = m_lines[start.m_line];
        auto stringVector = splitString(value, "\n", false);
        auto lineCount = (int32_t) stringVector.size();
        where.m_line += lineCount - 1;
        where.m_column += getStringCharacterCount(stringVector[lineCount - 1]);
        stringVector[lineCount - 1].append(line.substr(start.m_column, -1, Line::LinePart::Utf8));
        line.erase(start.m_column, -1);

        line.append(stringVector[0]);
        line.m_colorized = false;
        for (int32_t i = 1; i < lineCount; i++) {
            insertLine(start.m_line + i, stringVector[i]);
            m_lines[start.m_line + i].m_colorized = false;
        }
        m_textChanged = true;
        return lineCount;
    }

    void TextEditor::addUndo(UndoRecord &value) {
        if (m_readOnly)
            return;

        m_undoBuffer.resize((uint64_t) (m_undoIndex + 1));
        m_undoBuffer.back() = value;
        ++m_undoIndex;
    }

    TextEditor::Coordinates TextEditor::screenPosToCoordinates(const ImVec2 &position) const {
        ImVec2 local = position - ImGui::GetCursorScreenPos();
        int32_t lineNo = std::max(0, (int32_t) floor(local.y / m_charAdvance.y));
        if (local.x < (m_leftMargin - 2) || lineNo >= (int32_t) m_lines.size() || m_lines[lineNo].empty())
            return setCoordinates(std::min(lineNo, (int32_t) m_lines.size() - 1), 0);
        std::string line = m_lines[lineNo].m_chars;
        local.x -= (m_leftMargin - 5);
        int32_t count = 0;
        uint64_t length;
        int32_t increase;
        do {
            increase = utf8CharLength(line[count]);
            count += increase;
            std::string partialLine = line.substr(0, count);
            length = ImGui::CalcTextSize(partialLine.c_str(), nullptr, false, m_charAdvance.x * count).x;
        } while (length < local.x && count < (int32_t) line.size() + increase);

        auto result = getCharacterCoordinates(lineNo, count - increase);
        result = setCoordinates(result);
        if (result == Invalid)
            return Coordinates(0, 0);
        return result;
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

    bool isWordChar(char c) {
        auto asUChar = static_cast<uint8_t>(c);
        return std::isalnum(asUChar) || c == '_' || asUChar > 0x7F;
    }

    TextEditor::Coordinates TextEditor::findWordStart(const Coordinates &from) const {
        Coordinates at = setCoordinates(from);
        if (at.m_line >= (int32_t) m_lines.size())
            return at;

        auto &line = m_lines[at.m_line];
        auto charIndex = lineCoordinateToIndex(at);

        if (isWordChar(line.m_chars[charIndex])) {
            while (charIndex > 0 && isWordChar(line.m_chars[charIndex - 1]))
                --charIndex;
        } else if (ispunct(line.m_chars[charIndex])) {
            while (charIndex > 0 && ispunct(line.m_chars[charIndex - 1]))
                --charIndex;
        } else if (isspace(line.m_chars[charIndex])) {
            while (charIndex > 0 && isspace(line.m_chars[charIndex - 1]))
                --charIndex;
        }
        return getCharacterCoordinates(at.m_line, charIndex);
    }

    TextEditor::Coordinates TextEditor::findWordEnd(const Coordinates &from) const {
        Coordinates at = from;
        if (at.m_line >= (int32_t) m_lines.size())
            return at;

        auto &line = m_lines[at.m_line];
        auto charIndex = lineCoordinateToIndex(at);

        if (isWordChar(line.m_chars[charIndex])) {
            while (charIndex < (int32_t) line.m_chars.size() && isWordChar(line.m_chars[charIndex]))
                ++charIndex;
        } else if (ispunct(line.m_chars[charIndex])) {
            while (charIndex < (int32_t) line.m_chars.size() && ispunct(line.m_chars[charIndex]))
                ++charIndex;
        } else if (isspace(line.m_chars[charIndex])) {
            while (charIndex < (int32_t) line.m_chars.size() && isspace(line.m_chars[charIndex]))
                ++charIndex;
        }
        return getCharacterCoordinates(at.m_line, charIndex);
    }

    TextEditor::Coordinates TextEditor::findNextWord(const Coordinates &from) const {
        Coordinates at = from;
        if (at.m_line >= (int32_t) m_lines.size())
            return at;

        auto &line = m_lines[at.m_line];
        auto charIndex = lineCoordinateToIndex(at);

        if (isspace(line.m_chars[charIndex])) {
            while (charIndex < (int32_t) line.m_chars.size() && isspace(line.m_chars[charIndex]))
                ++charIndex;
        }
        if (isWordChar(line.m_chars[charIndex])) {
            while (charIndex < (int32_t) line.m_chars.size() && (isWordChar(line.m_chars[charIndex])))
                ++charIndex;
        } else if (ispunct(line.m_chars[charIndex])) {
            while (charIndex < (int32_t) line.m_chars.size() && (ispunct(line.m_chars[charIndex])))
                ++charIndex;
        }
        return getCharacterCoordinates(at.m_line, charIndex);
    }

    TextEditor::Coordinates TextEditor::findPreviousWord(const Coordinates &from) const {
        Coordinates at = from;
        if (at.m_line >= (int32_t) m_lines.size())
            return at;

        auto &line = m_lines[at.m_line];
        auto charIndex = lineCoordinateToIndex(at);

        if (isspace(line.m_chars[charIndex - 1])) {
            while (charIndex > 0 && isspace(line.m_chars[charIndex - 1]))
                --charIndex;
        }
        if (isWordChar(line.m_chars[charIndex - 1])) {
            while (charIndex > 0 && isWordChar(line.m_chars[charIndex - 1]))
                --charIndex;
        } else if (ispunct(line.m_chars[charIndex - 1])) {
            while (charIndex > 0 && ispunct(line.m_chars[charIndex - 1]))
                --charIndex;
        }
        return getCharacterCoordinates(at.m_line, charIndex);
    }

    static TextEditor::Coordinates stringIndexToCoordinates(int32_t strIndex, const std::string &input) {
        if (strIndex < 0 || strIndex > (int32_t) input.size())
            return TextEditor::Coordinates(0, 0);
        std::string str = input.substr(0, strIndex);
        auto line = std::count(str.begin(), str.end(), '\n');
        auto index = str.find_last_of('\n');
        str = str.substr(index + 1);
        auto col = getStringCharacterCount(str);

        return TextEditor::Coordinates(line, col);
    }

    static int32_t utf8CharCount(const std::string &line, int32_t start, int32_t numChars) {
        if (line.empty())
            return 0;

        int32_t index = 0;
        for (int32_t column = 0; start + index < line.size() && column < numChars; ++column)
            index += utf8CharLength(line[start + index]);

        return index;
    }


    int32_t TextEditor::lineCoordinateToIndex(const Coordinates &coordinates) const {
        if (coordinates.m_line >= m_lines.size())
            return -1;

        const auto &line = m_lines[coordinates.m_line];
        return utf8CharCount(line.m_chars, 0, coordinates.m_column);
    }

    int32_t TextEditor::Line::getCharacterColumn(int32_t index) const {
        int32_t col = 0;
        int32_t i = 0;
        while (i < index && i < (int32_t) size()) {
            auto c = m_chars[i];
            i += utf8CharLength(c);
            col++;
        }
        return col;
    }

    TextEditor::Coordinates TextEditor::getCharacterCoordinates(int32_t lineIndex, int32_t strIndex) const {
        if (lineIndex < 0 || lineIndex >= (int32_t) m_lines.size())
            return Coordinates(0, 0);
        auto &line = m_lines[lineIndex];
        return setCoordinates(lineIndex, line.getCharacterColumn(strIndex));
    }

    uint64_t TextEditor::getLineByteCount(int32_t lineIndex) const {
        if (lineIndex >= m_lines.size() || lineIndex < 0)
            return 0;

        auto &line = m_lines[lineIndex];
        return line.size();
    }

    int32_t TextEditor::getLineCharacterCount(int32_t line) const {
        return getLineMaxColumn(line);
    }

    int32_t TextEditor::getLineMaxColumn(int32_t line) const {
        if (line >= m_lines.size() || line < 0)
            return 0;
        return getStringCharacterCount(m_lines[line].m_chars);
    }

    void TextEditor::removeLine(int32_t lineStart, int32_t lineEnd) {

        ErrorMarkers errorMarker;
        for (auto &i: m_errorMarkers) {
            ErrorMarkers::value_type e(i.first.m_line >= lineStart ? setCoordinates(i.first.m_line - 1, i.first.m_column) : i.first, i.second);
            if (e.first.m_line >= lineStart && e.first.m_line <= lineEnd)
                continue;
            errorMarker.insert(e);
        }
        m_errorMarkers = std::move(errorMarker);

        Breakpoints breakpoints;
        for (auto breakpoint: m_breakpoints) {
            if (breakpoint <= lineStart || breakpoint >= lineEnd) {
                if (breakpoint >= lineEnd) {
                    breakpoints.insert(breakpoint - 1);
                    m_breakPointsChanged = true;
                } else
                    breakpoints.insert(breakpoint);
            }
        }

        m_breakpoints = std::move(breakpoints);
        // use clamp to ensure valid results instead of assert.
        auto start = std::clamp(lineStart, 0, (int32_t) m_lines.size() - 1);
        auto end = std::clamp(lineEnd, 0, (int32_t) m_lines.size());
        if (start > end)
            std::swap(start, end);

        m_lines.erase(m_lines.begin() + lineStart, m_lines.begin() + lineEnd + 1);

        m_textChanged = true;
    }

    void TextEditor::removeLine(int32_t index) {
        removeLine(index, index);
    }

    void TextEditor::insertLine(int32_t index, const std::string &text) {
        if (index < 0 || index > (int32_t) m_lines.size())
            return;
        auto &line = insertLine(index);
        line.append(text);
        line.m_colorized = false;
        m_textChanged = true;
    }

    TextEditor::Line &TextEditor::insertLine(int32_t index) {

        if (isEmpty())
            return *m_lines.insert(m_lines.begin(), Line());

        if (index == m_lines.size())
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
            if (breakpoint >= index) {
                breakpoints.insert(breakpoint + 1);
                m_breakPointsChanged = true;
            } else
                breakpoints.insert(breakpoint);
        }
        if (m_breakPointsChanged)
            m_breakpoints = std::move(breakpoints);

        return result;
    }

    TextEditor::PaletteIndex TextEditor::getColorIndexFromFlags(Line::Flags flags) {
        if (flags.m_bits.globalDocComment)
            return PaletteIndex::GlobalDocComment;
        if (flags.m_bits.blockDocComment)
            return PaletteIndex::DocBlockComment;
        if (flags.m_bits.docComment)
            return PaletteIndex::DocComment;
        if (flags.m_bits.blockComment)
            return PaletteIndex::BlockComment;
        if (flags.m_bits.comment)
            return PaletteIndex::Comment;
        if (flags.m_bits.deactivated)
            return PaletteIndex::PreprocessorDeactivated;
        if (flags.m_bits.preprocessor)
            return PaletteIndex::Directive;
        return PaletteIndex::Default;
    }

    void TextEditor::handleKeyboardInputs() {
        ImGuiIO &io = ImGui::GetIO();

        // command => Ctrl
        // control => Super
        // option  => Alt
        auto ctrl = io.KeyCtrl;
        auto alt = io.KeyAlt;
        auto shift = io.KeyShift;

        if (ImGui::IsWindowFocused()) {
            if (ImGui::IsWindowHovered())
                ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);

            io.WantCaptureKeyboard = true;
            io.WantTextInput = true;

            if (!m_readOnly && !ctrl && !shift && !alt && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)))
                enterCharacter('\n', false);
            else if (!m_readOnly && !ctrl && !alt && ImGui::IsKeyPressed(ImGuiKey_Tab))
                enterCharacter('\t', shift);

            if (!m_readOnly && !io.InputQueueCharacters.empty()) {
                for (int32_t i = 0; i < io.InputQueueCharacters.Size; i++) {
                    auto c = io.InputQueueCharacters[i];
                    if (c != 0 && (c == '\n' || c >= 32)) {
                        enterCharacter(c, shift);
                    }
                }
                io.InputQueueCharacters.resize(0);
            }
        }
    }

    void TextEditor::handleMouseInputs() {
        ImGuiIO &io = ImGui::GetIO();
        auto shift = io.KeyShift;
        auto ctrl = io.ConfigMacOSXBehaviors ? io.KeyAlt : io.KeyCtrl;
        auto alt = io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;

        if (ImGui::IsWindowHovered()) {
            if (!alt) {
                auto click = ImGui::IsMouseClicked(0);
                auto doubleClick = ImGui::IsMouseDoubleClicked(0);
                auto rightClick = ImGui::IsMouseClicked(1);
                auto t = ImGui::GetTime();
                auto tripleClick = click && !doubleClick && (m_lastClick != -1.0f && (t - m_lastClick) < io.MouseDoubleClickTime);
                bool resetBlinking = false;
                /*
                Left mouse button triple click
                */

                if (tripleClick) {
                    if (!ctrl) {
                        m_state.m_cursorPosition = screenPosToCoordinates(ImGui::GetMousePos());
                        auto line = m_state.m_cursorPosition.m_line;
                        m_state.m_selection.m_start = setCoordinates(line, 0);
                        m_state.m_selection.m_end = setCoordinates(line, getLineMaxColumn(line));
                    }

                    m_lastClick = -1.0f;
                    resetBlinking = true;
                }

                /*
                Left mouse button double click
                */

                else if (doubleClick) {
                    if (!ctrl) {
                        m_state.m_cursorPosition = screenPosToCoordinates(ImGui::GetMousePos());
                        m_state.m_selection.m_start = findWordStart(m_state.m_cursorPosition);
                        m_state.m_selection.m_end = findWordEnd(m_state.m_cursorPosition);
                    }

                    m_lastClick = (float) ImGui::GetTime();
                    resetBlinking = true;
                }

                /*
                Left mouse button click
                */
                else if (click) {
                    if (ctrl) {
                        m_state.m_cursorPosition = m_interactiveSelection.m_start = m_interactiveSelection.m_end = screenPosToCoordinates(ImGui::GetMousePos());
                        selectWordUnderCursor();
                    } else if (shift) {
                        m_interactiveSelection.m_end = screenPosToCoordinates(ImGui::GetMousePos());
                        m_state.m_cursorPosition = m_interactiveSelection.m_end;
                        setSelection(m_interactiveSelection);
                    } else {
                        m_state.m_cursorPosition = m_interactiveSelection.m_start = m_interactiveSelection.m_end = screenPosToCoordinates(ImGui::GetMousePos());
                        setSelection(m_interactiveSelection);
                    }
                    resetCursorBlinkTime();

                    ensureCursorVisible();
                    m_lastClick = (float) ImGui::GetTime();
                } else if (rightClick) {
                    auto cursorPosition = screenPosToCoordinates(ImGui::GetMousePos());

                    if (!hasSelection() || m_state.m_selection.m_start > cursorPosition || cursorPosition > m_state.m_selection.m_end) {
                        m_state.m_cursorPosition = m_interactiveSelection.m_start = m_interactiveSelection.m_end = cursorPosition;
                        setSelection(m_interactiveSelection);
                    }
                    resetCursorBlinkTime();
                    m_raiseContextMenu = true;
                    ImGui::SetWindowFocus();
                }
                    // Mouse left button dragging (=> update selection)
                else if (ImGui::IsMouseDragging(0) && ImGui::IsMouseDown(0)) {
                    io.WantCaptureMouse = true;
                    m_state.m_cursorPosition = m_interactiveSelection.m_end = screenPosToCoordinates(ImGui::GetMousePos());
                    setSelection(m_interactiveSelection);
                    resetBlinking = true;
                }
                if (resetBlinking)
                    resetCursorBlinkTime();
            }
        }
    }

    inline void TextUnformattedColoredAt(const ImVec2 &pos, const ImU32 &color, const char *text) {
        ImGui::SetCursorScreenPos(pos);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
    }

    uint32_t TextEditor::skipSpaces(const Coordinates &from) {
        auto line = from.m_line;
        if (line >= m_lines.size())
            return 0;
        auto &lines = m_lines[line].m_chars;
        auto &colors = m_lines[line].m_colors;
        auto charIndex = lineCoordinateToIndex(from);
        uint32_t s = 0;
        while (charIndex < (int32_t) lines.size() && lines[charIndex] == ' ' && colors[charIndex] == 0x00) {
            ++s;
            ++charIndex;
        }
        if (m_updateFocus)
            setFocus();
        return s;
    }

    void TextEditor::setFocus() {
        m_state.m_cursorPosition = m_focusAtCoords;
        resetCursorBlinkTime();
        ensureCursorVisible();
        if (!this->m_readOnly)
            ImGui::SetKeyboardFocusHere(0);
        m_updateFocus = false;
    }

    bool TextEditor::MatchedBracket::checkPosition(TextEditor *editor, const Coordinates &from) {
        auto lineIndex = from.m_line;
        auto line = editor->m_lines[lineIndex].m_chars;
        auto colors = editor->m_lines[lineIndex].m_colors;
        if (!line.empty() && colors.empty())
            return false;
        auto result = editor->lineCoordinateToIndex(from);
        auto character = line[result];
        auto color = colors[result];
        if (s_separators.find(character) != std::string::npos && (static_cast<PaletteIndex>(color) == PaletteIndex::Separator || static_cast<PaletteIndex>(color) == PaletteIndex::WarningText) ||
            s_operators.find(character)  != std::string::npos && (static_cast<PaletteIndex>(color) == PaletteIndex::Operator  || static_cast<PaletteIndex>(color) == PaletteIndex::WarningText)) {
            if (m_nearCursor != editor->getCharacterCoordinates(lineIndex, result)) {
                m_nearCursor = editor->getCharacterCoordinates(lineIndex, result);
                m_changed = true;
            }
            m_active = true;
            return true;
        }
        return false;
    }

    int32_t TextEditor::MatchedBracket::detectDirection(TextEditor *editor, const Coordinates &from) {
        std::string brackets = "()[]{}<>";
        int32_t result = -2; // dont check either
        auto start = editor->setCoordinates(from);
        if (start == TextEditor::Invalid)
            return result;
        auto lineIndex = start.m_line;
        auto line = editor->m_lines[lineIndex].m_chars;
        auto charIndex = editor->lineCoordinateToIndex(start);
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
        else if (idx2 != std::string::npos) // only second bracket
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
        auto charIndex = editor->lineCoordinateToIndex(start);
        auto direction1 = detectDirection(editor, start);
        auto charCoords = editor->getCharacterCoordinates(lineIndex, charIndex);
        int32_t direction2 = 1;
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
        uint64_t result = 0;
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
        auto charIndex = editor->lineCoordinateToIndex(from);
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
        int32_t direction = 1 - 2 * (idx % 2);
        if (idx > 5)
            color1 = static_cast<char>(PaletteIndex::Operator);
        else
            color1 = static_cast<char>(PaletteIndex::Separator);
        char color = static_cast<char>(PaletteIndex::WarningText);
        int32_t depth = 1;
        if (charIndex == (line.size() - 1) * (1 + direction) / 2) {
            if (lineIndex == maxLineIndex * (1 + direction) / 2) {
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
        for (int32_t i = charIndex + direction;; i += direction) {
            if (direction == 1)
                idx = line.find_first_of(brackets, i);
            else
                idx = line.find_last_of(brackets, i);
            if (idx != std::string::npos) {
                if (line[idx] == bracketChar && (colors[idx] == color || colors[idx] == color1)) {
                    ++depth;
                    i = idx;
                } else if (line[idx] == bracketChar2 && (colors[idx] == color) || colors[idx] == color1) {
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
            if ((int32_t) (direction * i) >= (int32_t) ((line.size() - 1) * (1 + direction) / 2)) {
                if (lineIndex == maxLineIndex * (1 + direction) / 2) {
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

    void TextEditor::preRender() {
        m_charAdvance = calculateCharAdvance();

        /* Update palette with the current alpha from style */
        for (int32_t i = 0; i < (int32_t) PaletteIndex::Max; ++i) {
            auto color = ImGui::ColorConvertU32ToFloat4(s_paletteBase[i]);
            color.w *= ImGui::GetStyle().Alpha;
            m_palette[i] = ImGui::ColorConvertFloat4ToU32(color);
        }

        m_numberOfLinesDisplayed = getPageSize();

        if (m_scrollToTop) {
            m_scrollToTop = false;
            ImGui::SetScrollY(0.f);
        }

        if (m_scrollToBottom && ImGui::GetScrollMaxY() >= ImGui::GetScrollY()) {
            m_scrollToBottom = false;
            ImGui::SetScrollY(ImGui::GetScrollMaxY());
        }

    }

    void TextEditor::drawSelection(float lineNo) {
        ImVec2 lineStartScreenPos = s_cursorScreenPosition + ImVec2(m_leftMargin, m_topMargin + std::floor(lineNo) * m_charAdvance.y);
        Selection lineCoords = Selection(setCoordinates(lineNo, 0), setCoordinates(lineNo, -1));
        auto drawList = ImGui::GetWindowDrawList();

        if (m_state.m_selection.m_start <= lineCoords.m_end && m_state.m_selection.m_end > lineCoords.m_start) {
            float selectionStart = textDistanceToLineStart(std::max(m_state.m_selection.m_start, lineCoords.m_start));
            float selectionEnd = textDistanceToLineStart(std::min(m_state.m_selection.m_end, lineCoords.m_end)) + m_charAdvance.x * (m_state.m_selection.m_end.m_line > lineNo);

            if (selectionStart < selectionEnd) {
                ImVec2 rectStart(lineStartScreenPos.x + selectionStart, lineStartScreenPos.y);
                ImVec2 rectEnd(lineStartScreenPos.x + selectionEnd, lineStartScreenPos.y + m_charAdvance.y);
                drawList->AddRectFilled(rectStart, rectEnd, m_palette[(int32_t) PaletteIndex::Selection]);
            }
        }
    }

    void TextEditor::drawLineNumbers(ImVec2 position, float lineNo, const ImVec2 &contentSize, bool focused, ImDrawList *drawList) {
        ImVec2 lineStartScreenPos = s_cursorScreenPosition + ImVec2(m_leftMargin, m_topMargin + std::floor(lineNo) * m_charAdvance.y);
        ImVec2 lineNoStartScreenPos = ImVec2(position.x, m_topMargin + s_cursorScreenPosition.y + std::floor(lineNo) * m_charAdvance.y);
        auto start = ImVec2(lineNoStartScreenPos.x + m_lineNumberFieldWidth, lineStartScreenPos.y);
        int32_t totalDigitCount = std::floor(std::log10(m_lines.size())) + 1;
        ImGui::SetCursorScreenPos(position);
        if (!m_ignoreImGuiChild)
            ImGui::BeginChild("##lineNumbers");

        int32_t padding = totalDigitCount - std::floor(std::log10(lineNo + 1)) - 1;
        std::string space = std::string(padding, ' ');
        std::string lineNoStr = space + std::to_string((int32_t) (lineNo + 1));
        ImGui::SetCursorScreenPos(ImVec2(position.x, lineStartScreenPos.y));
        if (ImGui::InvisibleButton(lineNoStr.c_str(), ImVec2(m_lineNumberFieldWidth, m_charAdvance.y))) {
            if (m_breakpoints.contains(lineNo + 1))
                m_breakpoints.erase(lineNo + 1);
            else
                m_breakpoints.insert(lineNo + 1);
            m_breakPointsChanged = true;
            auto cursorPosition = setCoordinates(lineNo, 0);
            if (cursorPosition == Invalid)
                return;

            m_state.m_cursorPosition = cursorPosition;

            jumpToCoords(m_state.m_cursorPosition);
        }
        // Draw breakpoints
        if (m_breakpoints.count(lineNo + 1) != 0) {
            auto end = ImVec2(lineNoStartScreenPos.x + contentSize.x + m_lineNumberFieldWidth, lineStartScreenPos.y + m_charAdvance.y);
            drawList->AddRectFilled(ImVec2(position.x, lineStartScreenPos.y), end, m_palette[(int32_t) PaletteIndex::Breakpoint]);

            drawList->AddCircleFilled(start + ImVec2(0, m_charAdvance.y) / 2, m_charAdvance.y / 3, m_palette[(int32_t) PaletteIndex::Breakpoint]);
            drawList->AddCircle(start + ImVec2(0, m_charAdvance.y) / 2, m_charAdvance.y / 3, m_palette[(int32_t) PaletteIndex::Default]);
            drawList->AddText(ImVec2(lineNoStartScreenPos.x + m_leftMargin, lineStartScreenPos.y), m_palette[(int32_t) PaletteIndex::LineNumber], lineNoStr.c_str());
        }

        if (m_state.m_cursorPosition.m_line == lineNo && m_showCursor) {

            // Highlight the current line (where the cursor is)
            if (!hasSelection()) {
                auto end = ImVec2(lineNoStartScreenPos.x + contentSize.x + m_lineNumberFieldWidth, lineStartScreenPos.y + m_charAdvance.y);
                drawList->AddRectFilled(ImVec2(position.x, lineStartScreenPos.y), end, m_palette[(int32_t) (focused ? PaletteIndex::CurrentLineFill : PaletteIndex::CurrentLineFillInactive)]);
                drawList->AddRect(ImVec2(position.x, lineStartScreenPos.y), end, m_palette[(int32_t) PaletteIndex::CurrentLineEdge], 1.0f);
            }
        }

        TextUnformattedColoredAt(ImVec2(m_leftMargin + lineNoStartScreenPos.x, lineStartScreenPos.y), m_palette[(int32_t) PaletteIndex::LineNumber], lineNoStr.c_str());

        if (!m_ignoreImGuiChild)
            ImGui::EndChild();
    }

    void TextEditor::renderCursor(float lineNo, ImDrawList *drawList) {
        ImVec2 lineStartScreenPos = s_cursorScreenPosition + ImVec2(m_leftMargin, m_topMargin + std::floor(lineNo) * m_charAdvance.y);
        auto timeEnd = ImGui::GetTime() * 1000;
        auto elapsed = timeEnd - m_startTime;
        if (elapsed > s_cursorBlinkOnTime) {
            float width = 1.0f;
            uint64_t charIndex = lineCoordinateToIndex(m_state.m_cursorPosition);
            float cx = textDistanceToLineStart(m_state.m_cursorPosition);
            auto &line = m_lines[std::floor(lineNo)];
            if (m_overwrite && charIndex < (int32_t) line.size()) {
                char c = line[charIndex];
                std::string s(1, c);
                width = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, s.c_str()).x;
            }
            ImVec2 rectStart(lineStartScreenPos.x + cx, lineStartScreenPos.y);
            ImVec2 rectEnd(lineStartScreenPos.x + cx + width, lineStartScreenPos.y + m_charAdvance.y);
            drawList->AddRectFilled(rectStart, rectEnd, m_palette[(int32_t) PaletteIndex::Cursor]);
            if (elapsed > s_cursorBlinkInterval)
                m_startTime = timeEnd;
            if (m_matchedBracket.isNearABracket(this, m_state.m_cursorPosition))
                m_matchedBracket.findMatchingBracket(this);
        }
    }

    void TextEditor::renderGotoButtons(float lineNo) {
        ImVec2 lineStartScreenPos = s_cursorScreenPosition + ImVec2(m_leftMargin, m_topMargin + std::floor(lineNo) * m_charAdvance.y);
        auto lineText = getLineText(lineNo);
        Coordinates gotoKey = setCoordinates(lineNo + 1, 0);
        if (gotoKey != Invalid) {
            std::string errorLineColumn;
            bool found = false;
            for (auto text: m_clickableText) {
                if (lineText.find(text) == 0) {
                    errorLineColumn = lineText.substr(text.size());
                    if (!errorLineColumn.empty()) {
                        found = true;
                        break;
                    }
                }
            }
            if (found) {
                int32_t currLine = 0, currColumn = 0;
                if (auto idx = errorLineColumn.find(":"); idx != std::string::npos) {
                    auto errorLine = errorLineColumn.substr(0, idx);
                    if (!errorLine.empty())
                        currLine = std::stoi(errorLine) - 1;
                    auto errorColumn = errorLineColumn.substr(idx + 1);
                    if (!errorColumn.empty())
                        currColumn = std::stoi(errorColumn) - 1;
                }
                TextEditor::Coordinates errorPos = GetSourceCodeEditor()->setCoordinates(currLine, currColumn);
                if (errorPos != Invalid) {
                    ImVec2 errorStart = ImVec2(lineStartScreenPos.x, lineStartScreenPos.y);
                    auto lineEnd = setCoordinates(lineNo, -1);
                    if (lineEnd != Invalid) {
                        ImVec2 errorEnd = ImVec2(lineStartScreenPos.x + textDistanceToLineStart(lineEnd), lineStartScreenPos.y + m_charAdvance.y);
                        ErrorGotoBox box = ErrorGotoBox(ImRect({errorStart, errorEnd}), errorPos, GetSourceCodeEditor());
                        m_errorGotoBoxes[gotoKey] = box;
                        CursorChangeBox cursorBox = CursorChangeBox(ImRect({errorStart, errorEnd}));
                        m_cursorBoxes[gotoKey] = cursorBox;
                    }
                }
            }
            if (m_cursorBoxes.find(gotoKey) != m_cursorBoxes.end()) {
                auto box = m_cursorBoxes[gotoKey];
                if (box.trigger()) box.callback();
            }

            if (m_errorGotoBoxes.find(gotoKey) != m_errorGotoBoxes.end()) {
                auto box = m_errorGotoBoxes[gotoKey];
                if (box.trigger()) box.callback();
            }
        }
    }

    void TextEditor::drawText(Coordinates &lineStart, uint64_t i, uint32_t tokenLength, char color) {
        auto &line = m_lines[lineStart.m_line];
        ImVec2 lineStartScreenPos = s_cursorScreenPosition + ImVec2(m_leftMargin, m_topMargin + std::floor(lineStart.m_line) * m_charAdvance.y);
        auto textStart = textDistanceToLineStart(lineStart);
        auto begin = lineStartScreenPos + ImVec2(textStart, 0);

        TextUnformattedColoredAt(begin, m_palette[(int32_t) color], line.substr(i, tokenLength).c_str());

        ErrorMarkers::iterator errorIt;
        auto key = lineStart + Coordinates(1, 1);
        if (errorIt = m_errorMarkers.find(key); errorIt != m_errorMarkers.end()) {
            auto errorMessage = errorIt->second.second;
            auto errorLength = errorIt->second.first;
            if (errorLength == 0)
                errorLength = line.size() - i - 1;

            auto end = underwaves(begin, errorLength, m_palette[(int32_t) PaletteIndex::ErrorMarker]);
            ErrorHoverBox box = ErrorHoverBox(ImRect({begin, end}), key, errorMessage.c_str());
            m_errorHoverBoxes[key] = box;
        }
        if (m_errorHoverBoxes.find(key) != m_errorHoverBoxes.end()) {
            auto box = m_errorHoverBoxes[key];
            if (box.trigger()) box.callback();
        }
        lineStart = lineStart + Coordinates(0, tokenLength);
    }

    void TextEditor::postRender(const char *title, ImVec2 position, float lineNo) {
        ImVec2 lineStartScreenPos = ImVec2(s_cursorScreenPosition.x + m_leftMargin, m_topMargin + s_cursorScreenPosition.y + std::floor(lineNo) * m_charAdvance.y);
        float globalLineMax = m_lines.size();
        auto lineMax = std::clamp(lineNo + m_numberOfLinesDisplayed, 0.0F, globalLineMax - 1.0F);
        if (!m_ignoreImGuiChild)
            ImGui::EndChild();

        if (m_showLineNumbers && !m_ignoreImGuiChild) {
            ImGui::BeginChild("##lineNumbers");
            ImGui::SetCursorScreenPos(ImVec2(position.x, lineStartScreenPos.y));
            ImGui::Dummy(ImVec2(m_lineNumberFieldWidth, (globalLineMax - lineMax - 1) * m_charAdvance.y + ImGui::GetCurrentWindow()->InnerClipRect.GetHeight() - m_charAdvance.y));
            ImGui::EndChild();
        }
        if (!m_ignoreImGuiChild)
            ImGui::BeginChild(title);

        ImGui::SetCursorScreenPos(lineStartScreenPos);
        if (m_showLineNumbers)
            ImGui::Dummy(ImVec2(m_longestLineLength * m_charAdvance.x + m_charAdvance.x, (globalLineMax - lineMax - 2.0F) * m_charAdvance.y + ImGui::GetCurrentWindow()->InnerClipRect.GetHeight()));
        else
            ImGui::Dummy(ImVec2(m_longestLineLength * m_charAdvance.x + m_charAdvance.x, (globalLineMax - lineMax - 3.0F) * m_charAdvance.y + ImGui::GetCurrentWindow()->InnerClipRect.GetHeight() - 1.0f));


        if (m_topMarginChanged) {
            m_topMarginChanged = false;
            auto window = ImGui::GetCurrentWindow();
            auto maxScroll = window->ScrollMax.y;
            if (maxScroll > 0) {
                float pixelCount;
                if (m_newTopMargin > m_topMargin) {
                    pixelCount = m_newTopMargin - m_topMargin;
                } else if (m_newTopMargin > 0) {
                    pixelCount = m_topMargin - m_newTopMargin;
                } else {
                    pixelCount = m_topMargin;
                }
                auto oldScrollY = ImGui::GetScrollY();

                if (m_newTopMargin > m_topMargin)
                    m_shiftedScrollY = oldScrollY + pixelCount;
                else
                    m_shiftedScrollY = oldScrollY - pixelCount;
                ImGui::SetScrollY(m_shiftedScrollY);
                m_topMargin = m_newTopMargin;
            }
        }
    }

    ImVec2 TextEditor::calculateCharAdvance() const {
        /* Compute mCharAdvance regarding scaled font size (Ctrl + mouse wheel)*/
        const float fontSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, "#", nullptr, nullptr).x;
        return ImVec2(fontSize, ImGui::GetTextLineHeightWithSpacing() * m_lineSpacing);
    }

    void TextEditor::renderText(const char *title, const ImVec2 &lineNumbersStartPos, const ImVec2 &textEditorSize) {

        preRender();
        auto drawList = ImGui::GetWindowDrawList();
        s_cursorScreenPosition = ImGui::GetCursorScreenPos();
        ImVec2 position = lineNumbersStartPos;
        auto scrollX = ImGui::GetScrollX();
        if (m_setScrollY)
            setScrollY();
        auto scrollY = ImGui::GetScrollY();
        if (m_setTopLine)
            setTopLine();
        else
            m_topLine = std::max<float>(0.0F, (scrollY - m_topMargin) / m_charAdvance.y);
        auto lineNo = m_topLine;

        auto lineMax = std::clamp(lineNo + m_numberOfLinesDisplayed, 0.0F, m_lines.size() - 1.0F);

        if (!m_lines.empty()) {
            bool focused = ImGui::IsWindowFocused();

            while (lineNo <= lineMax) {

                drawSelection(lineNo);

                if (!m_ignoreImGuiChild)
                    ImGui::EndChild();

                if (m_showLineNumbers)
                    drawLineNumbers(position, lineNo, textEditorSize, focused, drawList);

                if (!m_ignoreImGuiChild)
                    ImGui::BeginChild(title);

                if (m_state.m_cursorPosition.m_line == lineNo && m_showCursor && focused)
                    renderCursor(lineNo, drawList);

                renderGotoButtons(lineNo);

                // Render colorized text

                auto &line = m_lines[lineNo];
                if (line.empty()) {
                    ImGui::Dummy(m_charAdvance);
                    lineNo = std::floor(lineNo + 1.0F);
                    if (m_updateFocus)
                        setFocus();
                    continue;
                }
                auto colors = m_lines[lineNo].m_colors;
                uint64_t colorsSize = colors.size();
                uint64_t i = skipSpaces(setCoordinates(lineNo, 0));
                while (i < colorsSize) {
                    char color = std::clamp(colors[i], (char) PaletteIndex::Default, (char) ((uint8_t) PaletteIndex::Max - 1));
                    uint32_t tokenLength = std::clamp((uint64_t) (colors.find_first_not_of(color, i) - i),(uint64_t) 1, colorsSize - i);
                    if (m_updateFocus)
                        setFocus();
                    auto lineStart = setCoordinates(lineNo, i);

                    drawText(lineStart, i, tokenLength, color);

                    i += (tokenLength + skipSpaces(lineStart));
                }

                lineNo = std::floor(lineNo + 1.0F);
            }
        }

        if (m_scrollToCursor)
            ensureCursorVisible();

        postRender(title, position, lineNo);

    }

    void TextEditor::render(const char *title, const ImVec2 &size, bool border) {
        m_withinRender = true;

        if (m_lines.capacity() < 2 * m_lines.size())
            m_lines.reserve(2 * m_lines.size());

        auto scrollBg = ImGui::GetStyleColorVec4(ImGuiCol_ScrollbarBg);
        scrollBg.w = 0.0f;
        auto scrollBarSize = ImGui::GetStyle().ScrollbarSize;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(m_palette[(int32_t) PaletteIndex::Background]));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImGui::ColorConvertFloat4ToU32(scrollBg));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, scrollBarSize);

        auto position = ImGui::GetCursorScreenPos();
        if (m_showLineNumbers) {
            std::string lineNumber = " " + std::to_string(m_lines.size()) + " ";
            m_lineNumberFieldWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, lineNumber.c_str(), nullptr, nullptr).x + m_leftMargin;
            ImGui::SetNextWindowPos(position);
            ImGui::SetCursorScreenPos(position);
            auto lineNoSize = ImVec2(m_lineNumberFieldWidth, size.y);
            if (!m_ignoreImGuiChild) {
                ImGui::BeginChild("##lineNumbers", lineNoSize, false, ImGuiWindowFlags_NoScrollbar);
                ImGui::EndChild();
            }
        } else {
            m_lineNumberFieldWidth = 0;
        }

        ImVec2 textEditorSize = size;
        textEditorSize.x -= m_lineNumberFieldWidth;

        bool scroll_x = m_longestLineLength * m_charAdvance.x >= textEditorSize.x;

        bool scroll_y = m_lines.size() > 1;
        if (!border)
            textEditorSize.x -= scrollBarSize;
        ImGui::SetCursorScreenPos(ImVec2(position.x + m_lineNumberFieldWidth, position.y));
        ImGuiChildFlags childFlags = border ? ImGuiChildFlags_Borders : ImGuiChildFlags_None;
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove;
        if (!m_ignoreImGuiChild)
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

        if (m_handleKeyboardInputs) {
            handleKeyboardInputs();
        }

        if (m_handleMouseInputs)
            handleMouseInputs();


        colorizeInternal();
        renderText(title, position, textEditorSize);

        if (!m_ignoreImGuiChild)
            ImGui::EndChild();

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);

        m_withinRender = false;
        ImGui::SetCursorScreenPos(ImVec2(position.x, position.y + size.y - 1));
        ImGui::Dummy({});
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
        auto vectorString = splitString(text, "\n", false);
        auto lineCount = vectorString.size();
        if (lineCount == 0) {
            m_lines.resize(1);
            m_lines[0].clear();
        } else {
            m_lines.resize(lineCount);
            uint64_t i = 0;
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
                if (end.m_line >= (int32_t) m_lines.size())
                    end.m_line = isEmpty() ? 0 : (int32_t) m_lines.size() - 1;
                end.m_column = getLineMaxColumn(end.m_line);

                u.m_removedSelection = Selection(start, end);
                u.m_removed = getText(u.m_removedSelection);

                bool modified = false;

                for (int32_t i = start.m_line; i <= end.m_line; i++) {
                    auto &line = m_lines[i];
                    if (shift) {
                        if (!line.empty()) {
                            auto index = line.m_chars.find_first_not_of(' ', 0);
                            if (index == std::string::npos)
                                index = line.size() - 1;
                            if (index == 0) continue;
                            uint64_t spacesToRemove = (index % m_tabSize) ? (index % m_tabSize) : m_tabSize;
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
                for (uint64_t it = 0; it < line.size() && isascii(line[it]) && isblank(line[it]); ++it)
                    newLine.push_back(line[it]);

            const uint64_t whitespaceSize = newLine.size();
            int32_t charStart = 0;
            int32_t charPosition = 0;
            auto charIndex = lineCoordinateToIndex(coord);
            if (charIndex < whitespaceSize && m_languageDefinition.m_autoIndentation) {
                charStart = (int32_t) whitespaceSize;
                charPosition = charIndex;
            } else {
                charStart = charIndex;
                charPosition = (int32_t) whitespaceSize;
            }
            newLine.insert(newLine.end(), line.begin() + charStart, line.end());
            line.erase(line.begin() + charStart, -1);
            line.m_colorized = false;
            setCursorPosition(getCharacterCoordinates(coord.m_line + 1, charPosition));
            u.m_added = (char) character;
            u.m_addedSelection.m_end = setCoordinates(m_state.m_cursorPosition);
        } else if (character == '\t') {
            auto &line = m_lines[coord.m_line];
            auto charIndex = lineCoordinateToIndex(coord);

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
                spacesToRemove = std::min(spacesToRemove, (int32_t) line.size());
                auto spacesRemoved = 0;
                for (int32_t j = 0; j < spacesToRemove; j++) {
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
                auto charIndex = lineCoordinateToIndex(coord);

                if (m_overwrite && charIndex < (int32_t) line.size()) {
                    std::string c = line[coord.m_column];
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

        std::string findWord = m_findReplaceHandler.getFindWord();
        if (!findWord.empty()) {
            m_findReplaceHandler.resetMatches();
            m_findReplaceHandler.findAllMatches(this, findWord);
        }

        ensureCursorVisible();
    }

    void TextEditor::setCursorPosition(const Coordinates &position) {
        if (m_state.m_cursorPosition != position) {
            m_state.m_cursorPosition = position;
            ensureCursorVisible();
        }
    }

    void TextEditor::setSelection(const Selection &selection) {
        auto oldSelection = m_state.m_selection;
        m_state.m_selection = setCoordinates(selection);

    }

    TextEditor::Selection TextEditor::getSelection() const {
        return m_state.m_selection;
    }

    void TextEditor::setTabSize(int32_t value) {
        m_tabSize = std::max(0, std::min(32, value));
    }

    void TextEditor::insertText(const std::string &value) {
        insertText(value.c_str());
    }

    void TextEditor::insertText(const char *value) {
        if (value == nullptr)
            return;

        auto pos = setCoordinates(m_state.m_cursorPosition);
        auto start = std::min(pos, m_state.m_selection.m_start);

        insertTextAt(pos, value);
        m_lines[pos.m_line].m_colorized = false;

        setSelection(Selection(pos, pos));
        setCursorPosition(pos);

        std::string findWord = m_findReplaceHandler.getFindWord();
        if (!findWord.empty()) {
            m_findReplaceHandler.resetMatches();
            m_findReplaceHandler.findAllMatches(this, findWord);
        }
        colorize();
    }

    void TextEditor::deleteSelection() {

        if (m_state.m_selection.m_end == m_state.m_selection.m_start)
            return;

        deleteRange(m_state.m_selection);

        setSelection(Selection(m_state.m_selection.m_start, m_state.m_selection.m_start));
        setCursorPosition(m_state.m_selection.m_start);
        std::string findWord = m_findReplaceHandler.getFindWord();
        if (!findWord.empty()) {
            m_findReplaceHandler.resetMatches();
            m_findReplaceHandler.findAllMatches(this, findWord);
        }
        colorize();
    }

    void TextEditor::jumpToLine(int32_t line) {
        auto newPos = m_state.m_cursorPosition;
        if (line != -1) {
            newPos = setCoordinates(line, 0);
        }
        jumpToCoords(newPos);
    }

    void TextEditor::jumpToCoords(const Coordinates &coords) {
        setSelection(Selection(coords, coords));
        setCursorPosition(coords);
        ensureCursorVisible();

        setFocusAtCoords(coords);
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
                        m_interactiveSelection = Selection(newPos, oldPos);
                    }
                } else
                    m_interactiveSelection.m_start = m_interactiveSelection.m_end = newPos;

                setSelection(m_interactiveSelection);
                setCursorPosition(newPos);
                ensureCursorVisible();
            }
        }
    }

    void TextEditor::moveUp(int32_t amount, bool select) {
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

    void TextEditor::moveDown(int32_t amount, bool select) {
        IM_ASSERT(m_state.m_cursorPosition.m_column >= 0);
        resetCursorBlinkTime();
        auto oldPos = m_state.m_cursorPosition;
        if (amount < 0) {
            m_scrollYIncrement = 1.0;
            setScrollY();
            return;
        }

        m_state.m_cursorPosition.m_line = std::clamp(m_state.m_cursorPosition.m_line + amount, 0, (int32_t) m_lines.size() - 1);
        if (oldPos.m_line == (m_lines.size() - 1)) {
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

    void TextEditor::moveLeft(int32_t amount, bool select, bool wordMode) {
        resetCursorBlinkTime();

        auto oldPos = m_state.m_cursorPosition;


        if (isEmpty() || oldPos.m_line >= m_lines.size())
            return;

        auto lindex = m_state.m_cursorPosition.m_line;
        auto lineMaxColumn = getLineMaxColumn(lindex);
        auto column = std::min(m_state.m_cursorPosition.m_column, lineMaxColumn);

        while (amount-- > 0) {
            const auto &line = m_lines[lindex];
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

    void TextEditor::moveRight(int32_t amount, bool select, bool wordMode) {
        resetCursorBlinkTime();

        auto oldPos = m_state.m_cursorPosition;

        if (isEmpty() || oldPos.m_line >= m_lines.size())
            return;

        auto lindex = m_state.m_cursorPosition.m_line;
        auto lineMaxColumn = getLineMaxColumn(lindex);
        auto column = std::min(m_state.m_cursorPosition.m_column, lineMaxColumn);

        while (amount-- > 0) {
            const auto &line = m_lines[lindex];
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
            if (oldPos == m_interactiveSelection.m_end) {
                m_interactiveSelection.m_end = Coordinates(m_state.m_cursorPosition);
                if (m_interactiveSelection.m_end == Invalid)
                    return;
            } else if (oldPos == m_interactiveSelection.m_start)
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
        setCursorPosition(setCoordinates(0, 0));

        if (m_state.m_cursorPosition != oldPos) {
            if (select) {
                m_interactiveSelection = Selection(m_state.m_cursorPosition, oldPos);
            } else
                m_interactiveSelection.m_start = m_interactiveSelection.m_end = m_state.m_cursorPosition;
            setSelection(m_interactiveSelection);
        }
    }

    void TextEditor::TextEditor::moveBottom(bool select) {
        resetCursorBlinkTime();
        auto oldPos = getCursorPosition();
        auto newPos = setCoordinates(-1, -1);
        setCursorPosition(newPos);
        if (select) {
            m_interactiveSelection = Selection(oldPos, newPos);
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
        auto home = 0;
        if (!prefix.empty()) {
            auto idx = prefix.find_first_not_of(" ");
            if (idx == std::string::npos) {
                auto postIdx = postfix.find_first_of(" ");
                if (postIdx == std::string::npos || postIdx == 0)
                    home = 0;
                else {
                    postIdx = postfix.find_first_not_of(" ");
                    if (postIdx == std::string::npos)
                        home = getLineMaxColumn(oldPos.m_line);
                    else if (postIdx == 0)
                        home = 0;
                    else
                        home = oldPos.m_column + postIdx;
                }
            } else
                home = idx;
        } else {
            auto postIdx = postfix.find_first_of(" ");
            if (postIdx == std::string::npos)
                home = 0;
            else {
                postIdx = postfix.find_first_not_of(" ");
                if (postIdx == std::string::npos)
                    home = getLineMaxColumn(oldPos.m_line);
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
        setCursorPosition(setCoordinates(m_state.m_cursorPosition.m_line, getLineMaxColumn(oldPos.m_line)));

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

    void TextEditor::deleteChar() {
        resetCursorBlinkTime();
        IM_ASSERT(!m_readOnly);

        if (isEmpty())
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
                if (pos.m_line == (int32_t) m_lines.size() - 1)
                    return;

                u.m_removed = '\n';
                u.m_removedSelection.m_start = u.m_removedSelection.m_end = setCoordinates(m_state.m_cursorPosition);
                advance(u.m_removedSelection.m_end);

                auto &nextLine = m_lines[pos.m_line + 1];
                line.insert(line.end(), nextLine.begin(), nextLine.end());
                line.m_colorized = false;
                removeLine(pos.m_line + 1);

            } else {
                auto charIndex = lineCoordinateToIndex(pos);
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
        std::string findWord = m_findReplaceHandler.getFindWord();
        if (!findWord.empty()) {
            m_findReplaceHandler.resetMatches();
            m_findReplaceHandler.findAllMatches(this, findWord);
        }
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
                std::string charToRemove = line[pos.m_column];
                if (pos.m_column < (int32_t) line.size() - 1) {
                    std::string charToRemoveNext = line[pos.m_column + 1];
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
                auto charStart = lineCoordinateToIndex(pos);
                auto charEnd = lineCoordinateToIndex(m_state.m_cursorPosition);
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
        std::string findWord = m_findReplaceHandler.getFindWord();
        if (!findWord.empty()) {
            m_findReplaceHandler.resetMatches();
            m_findReplaceHandler.findAllMatches(this, findWord);
        }
    }

    void TextEditor::selectWordUnderCursor() {
        auto wordStart = findWordStart(getCursorPosition());
        setSelection(Selection(wordStart, findWordEnd(wordStart)));
    }

    void TextEditor::selectAll() {
        setSelection(Selection(setCoordinates(0, 0), setCoordinates(-1, -1)));
    }

    bool TextEditor::hasSelection() const {
        return !isEmpty() && m_state.m_selection.m_end > m_state.m_selection.m_start;
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
                if (lineIndex < 0 || lineIndex >= (int32_t) m_lines.size())
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
        std::string findWord = m_findReplaceHandler.getFindWord();
        if (!findWord.empty()) {
            m_findReplaceHandler.resetMatches();
            m_findReplaceHandler.findAllMatches(this, findWord);
        }
    }

    std::string TextEditor::replaceStrings(std::string string, const std::string &search, const std::string &replace) {
        std::string result;
        if (string.empty())
            return string;
        if (search.empty()) {
            for (auto c: string) {
                if (c == '\0') {
                    result.append(replace);
                } else {
                    result.push_back(c);
                }
            }
        } else {
            result = string;
            std::uint64_t pos = 0;
            while ((pos = result.find(search, pos)) != std::string::npos) {
                result.replace(pos, search.size(), replace);
                pos += replace.size();
            }
        }

        return result;
    }

    std::vector<std::string> TextEditor::splitString(const std::string &string, const std::string &delimiter, bool removeEmpty) {
        if (delimiter.empty() || string.empty()) {
            return {string};
        }

        std::vector<std::string> result;

        uint64_t start = 0, end = 0;
        while ((end = string.find(delimiter, start)) != std::string::npos) {
            uint64_t size = end - start;
            if (start + size > string.length())
                break;

            auto token = string.substr(start, end - start);
            start = end + delimiter.length();
            result.emplace_back(std::move(token));
        }

        if (start <= string.length())
            result.emplace_back(string.substr(start));

        if (removeEmpty)
            std::erase_if(result, [](const auto &string) { return string.empty(); });

        return result;
    }


    std::string TextEditor::replaceTabsWithSpaces(const std::string &string, uint32_t tabSize) {
        if (tabSize == 0 || string.empty() || string.find('\t') == std::string::npos)
            return string;

        auto stringVector = splitString(string, "\n", false);
        auto size = stringVector.size();
        std::string result;
        for (uint64_t i = 0; i < size; i++) {
            auto &line = stringVector[i];
            std::uint64_t pos = 0;
            while ((pos = line.find('\t', pos)) != std::string::npos) {
                auto spaces = tabSize - (pos % tabSize);
                line.replace(pos, 1, std::string(spaces, ' '));
                pos += spaces - 1;
            }
            result += line;
            if (i < size - 1)
                result += "\n";
        }
        return result;
    }


    std::string TextEditor::preprocessText(const std::string &code) {
        std::string result = replaceStrings(code, "\r\n", "\n");
        result = replaceStrings(result, "\r", "\n");
        result = replaceTabsWithSpaces(result, m_tabSize);

        return result;
    }

    void TextEditor::paste() {
        if (m_readOnly)
            return;

        auto clipText = ImGui::GetClipboardText();
        if (clipText != nullptr) {
            auto len = strlen(clipText);
            if (len > 0) {
                std::string text = preprocessText(clipText);
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
        std::string findWord = m_findReplaceHandler.getFindWord();
        if (!findWord.empty()) {
            m_findReplaceHandler.resetMatches();
            m_findReplaceHandler.findAllMatches(this, findWord);
        }
    }

    bool TextEditor::canUndo() {
        return !m_readOnly && m_undoIndex > 0;
    }

    bool TextEditor::canRedo() const {
        return !m_readOnly && m_undoIndex < (int32_t) m_undoBuffer.size();
    }

    void TextEditor::undo(int32_t steps) {
        while (canUndo() && steps-- > 0)
            m_undoBuffer[--m_undoIndex].undo(this);
        std::string findWord = m_findReplaceHandler.getFindWord();
        if (!findWord.empty()) {
            m_findReplaceHandler.resetMatches();
            m_findReplaceHandler.findAllMatches(this, findWord);
        }
    }

    void TextEditor::redo(int32_t steps) {
        while (canRedo() && steps-- > 0)
            m_undoBuffer[m_undoIndex++].redo(this);
        std::string findWord = m_findReplaceHandler.getFindWord();
        if (!findWord.empty()) {
            m_findReplaceHandler.resetMatches();
            m_findReplaceHandler.findAllMatches(this, findWord);
        }
    }

// the index here is array index so zero based
    void TextEditor::FindReplaceHandler::selectFound(TextEditor *editor, int32_t found) {
        IM_ASSERT(found >= 0 && found < m_matches.size());
        auto selection = m_matches[found].m_selection;
        editor->setSelection(selection);
        editor->setCursorPosition(selection.m_end);
        editor->ensureCursorVisible();
    }

// The returned index is shown in the form
//  'index of count' so 1 based
    uint32_t TextEditor::FindReplaceHandler::findMatch(TextEditor *editor, bool isNex) {

        if (editor->m_textChanged || m_optionsChanged) {
            std::string findWord = getFindWord();
            if (findWord.empty())
                return 0;
            resetMatches();
            findAllMatches(editor, findWord);
        }

        auto targetPos = editor->m_state.m_cursorPosition;
        auto count = m_matches.size();

        if (count == 0) {
            editor->setCursorPosition(targetPos);
            return 0;
        }

        for (uint32_t i = 0; i < count; i++) {
            if (targetPos >= m_matches[i].m_selection.m_start && targetPos <= m_matches[i].m_selection.m_end) {
                if (isNex) {
                    if (i == count - 1) {
                        selectFound(editor, 0);
                        return 1;
                    } else {
                        selectFound(editor, i + 1);
                        return (i + 2);
                    }
                } else {
                    if (i == 0) {
                        selectFound(editor, count - 1);
                        return count;
                    } else {
                        selectFound(editor, i - 1);
                        return i;
                    }
                }
            }
        }

        if ((targetPos > m_matches[count - 1].m_selection.m_end) || (targetPos < m_matches[0].m_selection.m_start)) {
            if (isNex) {
                selectFound(editor, 0);
                return 1;
            } else {
                selectFound(editor, count - 1);
                return count;
            }
        }

        for (uint32_t i = 1; i < count; i++) {

            if (m_matches[i - 1].m_selection.m_end <= targetPos && m_matches[i].m_selection.m_start >= targetPos) {
                if (isNex) {
                    selectFound(editor, i);
                    return i + 1;
                } else {
                    selectFound(editor, i - 1);
                    return i;
                }
            }
        }

        return 0;
    }

// returns 1 based index
    uint32_t TextEditor::FindReplaceHandler::findPosition(TextEditor *editor, Coordinates pos, bool isNext) {
        if (editor->m_textChanged || m_optionsChanged) {
            std::string findWord = getFindWord();
            if (findWord.empty())
                return 0;
            resetMatches();
            findAllMatches(editor, findWord);
        }

        int32_t count = m_matches.size();
        if (count == 0)
            return 0;
        if (isNext) {
            if (pos > m_matches[count - 1].m_selection.m_end || pos <= m_matches[0].m_selection.m_end)
                return 1;
            for (uint32_t i = 1; i < count; i++) {
                if (pos > m_matches[i - 1].m_selection.m_end && pos <= m_matches[i].m_selection.m_end)
                    return i + 1;
            }
        } else {
            if (pos >= m_matches[count - 1].m_selection.m_start || pos < m_matches[0].m_selection.m_start)
                return count;
            for (uint32_t i = 1; i < count; i++) {
                if (pos >= m_matches[i - 1].m_selection.m_start && pos < m_matches[i].m_selection.m_start)
                    return i;
            }
        }
        return 0;
    }

// Create a string that escapes special characters
// and separate word from non word
    std::string make_wholeWord(const std::string &s) {
        static const char metacharacters[] = R"(\.^$-+()[]{}|?*)";
        std::string out;
        out.reserve(s.size());
        if (s[0] == '#')
            out.push_back('#');
        out.push_back('\\');
        out.push_back('b');
        for (auto ch: s) {
            if (strchr(metacharacters, ch))
                out.push_back('\\');
            out.push_back(ch);
        }
        out.push_back('\\');
        out.push_back('b');
        return out;
    }

// Performs actual search to fill mMatches
    bool TextEditor::FindReplaceHandler::findNext(TextEditor *editor) {
        Coordinates curPos;
        curPos.m_line = m_matches.empty() ? editor->m_state.m_cursorPosition.m_line : m_matches.back().m_cursorPosition.m_line;
        curPos.m_column = m_matches.empty() ? editor->m_state.m_cursorPosition.m_column : editor->lineCoordinateToIndex(m_matches.back().m_cursorPosition);

        uint64_t matchLength = getStringCharacterCount(m_findWord);
        uint64_t byteIndex = 0;

        for (uint64_t ln = 0; ln < curPos.m_line; ln++)
            byteIndex += editor->getLineByteCount(ln) + 1;
        byteIndex += curPos.m_column;

        std::string wordLower = m_findWord;
        if (!getMatchCase())
            std::transform(wordLower.begin(), wordLower.end(), wordLower.begin(), ::tolower);

        std::string textSrc = editor->getText();
        if (!getMatchCase())
            std::transform(textSrc.begin(), textSrc.end(), textSrc.begin(), ::tolower);

        uint64_t textLoc;
        // TODO: use regexp find iterator in all cases
        //  to find all matches for FindAllMatches.
        //  That should make things faster (no need
        //  to call FindNext many times) and remove
        //  clunky match case code
        if (getWholeWord() || getFindRegEx()) {
            std::regex regularExpression;
            if (getFindRegEx()) {
                try {
                    regularExpression.assign(wordLower);
                } catch (const std::regex_error &e) {
                    return false;
                }
            } else {
                try {
                    regularExpression.assign(make_wholeWord(wordLower));
                } catch (const std::regex_error &e) {
                    return false;
                }
            }

            uint64_t pos = 0;
            std::sregex_iterator iter = std::sregex_iterator(textSrc.begin(), textSrc.end(), regularExpression);
            std::sregex_iterator end;
            if (!iter->ready())
                return false;
            uint64_t firstLoc = iter->position();
            uint64_t firstLength = iter->length();

            if (firstLoc > byteIndex) {
                pos = firstLoc;
                matchLength = firstLength;
            } else {

                while (iter != end) {
                    iter++;
                    if (((pos = iter->position()) > byteIndex) && ((matchLength = iter->length()) > 0))
                        break;
                }
            }

            if (iter == end)
                return false;

            textLoc = pos;
        } else {
            // non regex search
            textLoc = textSrc.find(wordLower, byteIndex);
            if (textLoc == std::string::npos)
                return false;
        }
        if (textLoc == std::string::npos)
            return false;
        TextEditor::EditorState state;
        state.m_selection = Selection(stringIndexToCoordinates(textLoc, textSrc), stringIndexToCoordinates(textLoc + matchLength, textSrc));
        state.m_cursorPosition = state.m_selection.m_end;
        m_matches.push_back(state);
        return true;
    }

    void TextEditor::FindReplaceHandler::findAllMatches(TextEditor *editor, std::string findWord) {

        if (findWord.empty()) {
            editor->ensureCursorVisible();
            m_findWord = "";
            m_matches.clear();
            return;
        }

        if (findWord == m_findWord && !editor->m_textChanged && !m_optionsChanged)
            return;

        if (m_optionsChanged)
            m_optionsChanged = false;

        m_matches.clear();
        m_findWord = findWord;
        auto startingPos = editor->m_state.m_cursorPosition;
        auto saveState = editor->m_state;
        Coordinates begin = editor->setCoordinates(0, 0);
        editor->m_state.m_cursorPosition = begin;

        if (!findNext(editor)) {
            editor->m_state = saveState;
            editor->ensureCursorVisible();
            return;
        }
        TextEditor::EditorState state = m_matches.back();

        while (state.m_cursorPosition < startingPos) {
            if (!findNext(editor)) {
                editor->m_state = saveState;
                editor->ensureCursorVisible();
                return;
            }
            state = m_matches.back();
        }

        while (findNext(editor));

        editor->m_state = saveState;
        editor->ensureCursorVisible();
        return;
    }


    bool TextEditor::FindReplaceHandler::replace(TextEditor *editor, bool right) {
        if (m_matches.empty() || m_findWord == m_replaceWord || m_findWord.empty())
            return false;


        auto state = editor->m_state;

        if (editor->m_state.m_cursorPosition <= editor->m_state.m_selection.m_end &&
            editor->m_state.m_selection.m_end > editor->m_state.m_selection.m_start &&
            editor->m_state.m_cursorPosition > editor->m_state.m_selection.m_start) {

            editor->m_state.m_cursorPosition = editor->m_state.m_selection.m_start;
            if (editor->m_state.m_cursorPosition.m_column == 0) {
                editor->m_state.m_cursorPosition.m_line--;
                editor->m_state.m_cursorPosition.m_column = editor->getLineMaxColumn(editor->m_state.m_cursorPosition.m_line);
            } else
                editor->m_state.m_cursorPosition.m_column--;
        }
        auto matchIndex = findMatch(editor, right);
        if (matchIndex != 0) {
            UndoRecord u;
            u.m_before = editor->m_state;

            auto selectionEnd = editor->m_state.m_selection.m_end;

            u.m_removed = editor->getSelectedText();
            u.m_removedSelection = editor->m_state.m_selection;
            editor->deleteSelection();
            if (getFindRegEx()) {
                std::string replacedText = std::regex_replace(editor->getText(), std::regex(m_findWord), m_replaceWord, std::regex_constants::format_first_only | std::regex_constants::format_no_copy);
                u.m_added = replacedText;
            } else
                u.m_added = m_replaceWord;

            u.m_addedSelection.m_start = editor->setCoordinates(editor->m_state.m_cursorPosition);
            editor->insertText(u.m_added);

            editor->setCursorPosition(editor->m_state.m_selection.m_end);

            u.m_addedSelection.m_end = editor->setCoordinates(editor->m_state.m_cursorPosition);

            editor->ensureCursorVisible();
            ImGui::SetKeyboardFocusHere(0);

            u.m_after = editor->m_state;
            editor->addUndo(u);
            editor->m_textChanged = true;

            return true;
        }
        editor->m_state = state;
        return false;
    }

    bool TextEditor::FindReplaceHandler::replaceAll(TextEditor *editor) {
        uint32_t count = m_matches.size();

        for (uint32_t i = 0; i < count; i++)
            replace(editor, true);

        return true;
    }

    const TextEditor::Palette &TextEditor::getDarkPalette() {
        const static Palette p = {
                {
                        0xff7f7f7f, // Default
                        0xffd69c56, // Keyword
                        0xff00ff00, // Number
                        0xff7070e0, // String
                        0xff70a0e0, // Char literal
                        0xffffffff, // Punctuation
                        0xff408080, // Preprocessor
                        0xffaaaaaa, // Identifier
                        0xff9bc64d, // Known identifier
                        0xffc040a0, // Preproc identifier
                        0xff708020, // Global Doc Comment
                        0xff586820, // Doc Comment
                        0xff206020, // Comment (single line)
                        0xff406020, // Comment (multi line)
                        0xff004545, // Preprocessor deactivated
                        0xff101010, // Background
                        0xffe0e0e0, // Cursor
                        0x80a06020, // Selection
                        0x800020ff, // ErrorMarker
                        0x40f08000, // Breakpoint
                        0xff707000, // Line number
                        0x40000000, // Current line fill
                        0x40808080, // Current line fill (inactive)
                        0x40a0a0a0, // Current line edge
                }
        };
        return p;
    }

    const TextEditor::Palette &TextEditor::getLightPalette() {
        const static Palette p = {
                {
                        0xff7f7f7f, // None
                        0xffff0c06, // Keyword
                        0xff008000, // Number
                        0xff2020a0, // String
                        0xff304070, // Char literal
                        0xff000000, // Punctuation
                        0xff406060, // Preprocessor
                        0xff404040, // Identifier
                        0xff606010, // Known identifier
                        0xffc040a0, // Preproc identifier
                        0xff707820, // Global Doc Comment
                        0xff586020, // Doc Comment
                        0xff205020, // Comment (single line)
                        0xff405020, // Comment (multi line)
                        0xffa7cccc, // Preprocessor deactivated
                        0xffffffff, // Background
                        0xff000000, // Cursor
                        0x80600000, // Selection
                        0xa00010ff, // ErrorMarker
                        0x80f08000, // Breakpoint
                        0xff505000, // Line number
                        0x40000000, // Current line fill
                        0x40808080, // Current line fill (inactive)
                        0x40000000, // Current line edge
                }
        };
        return p;
    }

    const TextEditor::Palette &TextEditor::getRetroBluePalette() {
        const static Palette p = {
                {
                        0xff00ffff, // None
                        0xffffff00, // Keyword
                        0xff00ff00, // Number
                        0xff808000, // String
                        0xff808000, // Char literal
                        0xffffffff, // Punctuation
                        0xff008000, // Preprocessor
                        0xff00ffff, // Identifier
                        0xffffffff, // Known identifier
                        0xffff00ff, // Preproc identifier
                        0xff101010, // Global Doc Comment
                        0xff202020, // Doc Comment
                        0xff808080, // Comment (single line)
                        0xff404040, // Comment (multi line)
                        0xff004000, // Preprocessor deactivated
                        0xff800000, // Background
                        0xff0080ff, // Cursor
                        0x80ffff00, // Selection
                        0xa00000ff, // ErrorMarker
                        0x80ff8000, // Breakpoint
                        0xff808000, // Line number
                        0x40000000, // Current line fill
                        0x40808080, // Current line fill (inactive)
                        0x40000000, // Current line edge
                }
        };
        return p;
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

    std::string TextEditor::getLineText(int32_t line) const {
        auto sanitizedLine = setCoordinates(line, 0);
        auto endLine = setCoordinates(line, -1);
        if (sanitizedLine == Invalid || endLine == Invalid)
            return "";
        return getText(Selection(sanitizedLine, endLine));
    }

    void TextEditor::colorize() {
        m_updateFlags = true;
    }

    void TextEditor::colorizeRange() {

        if (isEmpty())
            return;

        std::smatch results;
        std::string id;
        if (m_languageDefinition.m_tokenize == nullptr)
            m_languageDefinition.m_tokenize = [](strConstIter, strConstIter, strConstIter &, strConstIter &, PaletteIndex &) { return false; };
        auto linesSize = m_lines.size();
        for (int32_t i = 0; i < linesSize; ++i) {
            auto &line = m_lines[i];
            auto size = line.size();

            if (line.m_colors.size() != size) {
                line.m_colors.resize(size, 0);
                line.m_colorized = false;
            }

            if (line.m_colorized || line.empty())
                continue;

            auto last = line.end();

            auto first = line.begin();
            for (auto current = first; (current - first) < size;) {
                strConstIter token_begin;
                strConstIter token_end;
                PaletteIndex token_color = PaletteIndex::Default;

                bool hasTokenizeResult = m_languageDefinition.m_tokenize(current.m_charsIter, last.m_charsIter, token_begin, token_end, token_color);
                auto token_offset = token_begin - first.m_charsIter;

                if (!hasTokenizeResult) {
                    // todo : remove
                    // printf("using regex for %.*s\n", first + 10 < last ? 10 : int32_t(last - first), first);

                    for (auto &p: m_regexList) {
                        if (std::regex_search(first.m_charsIter, last.m_charsIter, results, p.first, std::regex_constants::match_continuous)) {
                            hasTokenizeResult = true;

                            const auto &v = results.begin();
                            token_begin = v->first;
                            token_end = v->second;
                            token_color = p.second;
                            break;
                        }
                    }
                }

                if (!hasTokenizeResult)
                    current = current + 1;
                else {
                    current = first + token_offset;
                    uint64_t token_length = 0;
                    Line::Flags flags(0);
                    flags.m_value = line.m_flags[token_offset];
                    if (flags.m_value == 0) {
                        token_length = token_end - token_begin;
                        if (token_color == PaletteIndex::Identifier) {
                            id.assign(token_begin, token_end);

                            // todo : almost all language definitions use lower case to specify keywords, so shouldn't this use ::tolower ?
                            if (!m_languageDefinition.m_caseSensitive)
                                std::transform(id.begin(), id.end(), id.begin(), ::toupper);
                            else if (m_languageDefinition.m_keywords.count(id) != 0)
                                token_color = PaletteIndex::Keyword;
                            else if (m_languageDefinition.m_identifiers.count(id) != 0)
                                token_color = PaletteIndex::BuiltInType;
                            else if (id == "$")
                                token_color = PaletteIndex::GlobalVariable;
                        }
                    } else {
                        if ((token_color == PaletteIndex::Identifier || flags.m_bits.preprocessor) && !flags.m_bits.deactivated && !(flags.m_value & Line::inComment)) {
                            id.assign(token_begin, token_end);
                            if (m_languageDefinition.m_preprocIdentifiers.count(id) != 0) {
                                token_color = PaletteIndex::Directive;
                                token_begin -= 1;
                                token_length = token_end - token_begin;
                                token_offset -= 1;
                            } else if (flags.m_bits.preprocessor) {
                                token_color = PaletteIndex::PreprocIdentifier;
                                token_length = token_end - token_begin;
                            }
                        }
                        if (flags.m_bits.matchedBracket) {
                            token_color = PaletteIndex::WarningText;
                            token_length = token_end - token_begin;
                        } else if (flags.m_bits.preprocessor && !flags.m_bits.deactivated) {
                            token_length = token_end - token_begin;
                        } else if ((token_color != PaletteIndex::Directive && token_color != PaletteIndex::PreprocIdentifier) || flags.m_bits.deactivated) {
                            if (flags.m_bits.deactivated && flags.m_bits.preprocessor) {
                                token_color = PaletteIndex::PreprocessorDeactivated;
                                token_begin -= 1;
                                token_offset -= 1;
                            } else if (id.assign(token_begin, token_end);flags.m_value & Line::inComment && m_languageDefinition.m_preprocIdentifiers.count(id) != 0) {
                                token_color = getColorIndexFromFlags(flags);
                                token_begin -= 1;
                                token_offset -= 1;
                            }

                            auto flag = line.m_flags[token_offset];
                            token_length = line.m_flags.find_first_not_of(flag, token_offset + 1);
                            if (token_length == std::string::npos)
                                token_length = line.size() - token_offset;
                            else
                                token_length -= token_offset;

                            token_end = token_begin + token_length;
                            if (!flags.m_bits.preprocessor || flags.m_bits.deactivated)
                                token_color = getColorIndexFromFlags(flags);
                        }
                    }

                    if (token_color != PaletteIndex::Identifier || *current.m_colorsIter == static_cast<char>(PaletteIndex::Identifier)) {
                        if (token_offset + token_length >= (int32_t) line.m_colors.size()) {
                            auto colors = line.m_colors;
                            line.m_colors.resize(token_offset + token_length, static_cast<char>(PaletteIndex::Default));
                            std::copy(colors.begin(), colors.end(), line.m_colors.begin());
                        }
                        try {
                            line.m_colors.replace(token_offset, token_length, token_length, static_cast<char>(token_color));
                        } catch (const std::exception &e) {
                            std::cerr << "Error replacing color: " << e.what() << std::endl;
                            return;
                        }
                    }
                    current = current + token_length;
                }
            }
            line.m_colorized = true;
        }
    }

    void TextEditor::colorizeInternal() {
        if (isEmpty() || !m_colorizerEnabled)
            return;

        if (m_updateFlags) {
            auto endLine = m_lines.size();
            auto commentStartLine = endLine;
            auto commentStartIndex = 0;
            auto withinGlobalDocComment = false;
            auto withinBlockDocComment = false;
            auto withinString = false;
            auto withinBlockComment = false;
            auto withinNotDef = false;
            auto currentLine = 0;
            auto commentLength = 0;
            auto matchedBracket = false;
            std::string brackets = "()[]{}<>";

            std::vector<bool> ifDefs;
            ifDefs.push_back(true);
            m_defines.push_back("__IMHEX__");
            for (currentLine = 0; currentLine < endLine; currentLine++) {
                auto &line = m_lines[currentLine];
                auto lineLength = line.size();

                if (line.m_flags.size() != lineLength) {
                    line.m_flags.resize(lineLength, 0);
                    line.m_colorized = false;
                }


                auto withinComment = false;
                auto withinDocComment = false;
                auto withinPreproc = false;
                auto firstChar = true;   // there is no other non-whitespace characters in the line before

                auto setGlyphFlags = [&](int32_t index) {
                    Line::Flags flags(0);
                    flags.m_bits.comment = withinComment;
                    flags.m_bits.blockComment = withinBlockComment;
                    flags.m_bits.docComment = withinDocComment;
                    flags.m_bits.globalDocComment = withinGlobalDocComment;
                    flags.m_bits.blockDocComment = withinBlockDocComment;
                    flags.m_bits.deactivated = withinNotDef;
                    flags.m_bits.matchedBracket = matchedBracket;
                    if (m_lines[currentLine].m_flags[index] != flags.m_value) {
                        m_lines[currentLine].m_colorized = false;
                        m_lines[currentLine].m_flags[index] = flags.m_value;
                    }
                };

                uint64_t currentIndex = 0;
                if (line.empty())
                    continue;
                while (currentIndex < lineLength) {

                    char c = line[currentIndex];

                    matchedBracket = false;
                    if (MatchedBracket::s_separators.contains(c) && m_matchedBracket.isActive()) {
                        if (m_matchedBracket.m_nearCursor == getCharacterCoordinates(currentLine, currentIndex) || m_matchedBracket.m_matched == getCharacterCoordinates(currentLine, currentIndex))
                            matchedBracket = true;
                    } else if (MatchedBracket::s_operators.contains(c) && m_matchedBracket.isActive()) {
                        if (m_matchedBracket.m_nearCursor == setCoordinates(currentLine, currentIndex) || m_matchedBracket.m_matched == setCoordinates(currentLine, currentIndex)) {
                            if ((c == '<' && line.m_colors[currentIndex - 1] == static_cast<char>(PaletteIndex::UserDefinedType)) ||
                                (c == '>' && (m_matchedBracket.m_matched.m_column > 0 && line.m_colors[lineCoordinateToIndex(m_matchedBracket.m_matched) - 1] == static_cast<char>(PaletteIndex::UserDefinedType)) ||
                                 (m_matchedBracket.m_nearCursor.m_column > 0 && line.m_colors[lineCoordinateToIndex(m_matchedBracket.m_nearCursor) - 1] == static_cast<char>(PaletteIndex::UserDefinedType)))) {
                                matchedBracket = true;
                            }
                        }
                    }


                    if (c != m_languageDefinition.m_preprocChar && !isspace(c))
                        firstChar = false;

                    bool inComment = (commentStartLine < currentLine || (commentStartLine == currentLine && commentStartIndex <= currentIndex));

                    if (withinString) {
                        setGlyphFlags(currentIndex);
                        if (c == '\\') {
                            currentIndex++;
                            setGlyphFlags(currentIndex);
                        } else if (c == '\"')
                            withinString = false;
                    } else {
                        if (firstChar && c == m_languageDefinition.m_preprocChar && !inComment && !withinComment && !withinDocComment && !withinString) {
                            withinPreproc = true;
                            std::string directive;
                            auto start = currentIndex + 1;
                            while (start < (int32_t) line.size() && !isspace(line[start])) {
                                directive += line[start];
                                start++;
                            }

                            while (start < (int32_t) line.size() && isspace(line[start]))
                                start++;

                            if (directive == "endif" && !ifDefs.empty()) {
                                ifDefs.pop_back();
                                withinNotDef = !ifDefs.back();
                            } else {
                                std::string identifier;
                                while (start < (int32_t) line.size() && !isspace(line[start])) {
                                    identifier += line[start];
                                    start++;
                                }
                                if (directive == "define") {
                                    if (identifier.size() > 0 && !withinNotDef && std::find(m_defines.begin(), m_defines.end(), identifier) == m_defines.end())
                                        m_defines.push_back(identifier);
                                } else if (directive == "undef") {
                                    if (identifier.size() > 0 && !withinNotDef)
                                        m_defines.erase(std::remove(m_defines.begin(), m_defines.end(), identifier), m_defines.end());
                                } else if (directive == "ifdef") {
                                    if (!withinNotDef) {
                                        bool isConditionMet = std::find(m_defines.begin(), m_defines.end(), identifier) != m_defines.end();
                                        ifDefs.push_back(isConditionMet);
                                    } else
                                        ifDefs.push_back(false);
                                } else if (directive == "ifndef") {
                                    if (!withinNotDef) {
                                        bool isConditionMet = std::find(m_defines.begin(), m_defines.end(), identifier) == m_defines.end();
                                        ifDefs.push_back(isConditionMet);
                                    } else
                                        ifDefs.push_back(false);
                                }
                            }
                        }

                        if (c == '\"' && !withinPreproc && !inComment && !withinComment && !withinDocComment) {
                            withinString = true;
                            setGlyphFlags(currentIndex);
                        } else {
                            auto pred = [](const char &a, const char &b) { return a == b; };

                            auto compareForth = [&](const std::string &a, const std::string &b) {
                                return !a.empty() && (currentIndex + a.size() <= b.size()) && equals(a.begin(), a.end(), b.begin() + currentIndex, b.begin() + (currentIndex + a.size()), pred);
                            };

                            auto compareBack = [&](const std::string &a, const std::string &b) {
                                return !a.empty() && currentIndex + 1 >= (int32_t) a.size() && equals(a.begin(), a.end(), b.begin() + (currentIndex + 1 - a.size()), b.begin() + (currentIndex + 1), pred);
                            };

                            if (!inComment && !withinComment && !withinDocComment && !withinPreproc && !withinString) {
                                if (compareForth(m_languageDefinition.m_docComment, line.m_chars)) {
                                    withinDocComment = !inComment;
                                    commentLength = 3;
                                } else if (compareForth(m_languageDefinition.m_singleLineComment, line.m_chars)) {
                                    withinComment = !inComment;
                                    commentLength = 2;
                                } else {
                                    bool isGlobalDocComment = compareForth(m_languageDefinition.m_globalDocComment, line.m_chars);
                                    bool isBlockDocComment = compareForth(m_languageDefinition.m_blockDocComment, line.m_chars);
                                    bool isBlockComment = compareForth(m_languageDefinition.m_commentStart, line.m_chars);
                                    if (isGlobalDocComment || isBlockDocComment || isBlockComment) {
                                        commentStartLine = currentLine;
                                        commentStartIndex = currentIndex;
                                        if (currentIndex < line.size() - 4 && isBlockComment &&
                                            line.m_chars[currentIndex + 2] == '*' &&
                                            line.m_chars[currentIndex + 3] == '/') {
                                            withinBlockComment = true;
                                            commentLength = 2;
                                        } else if (isGlobalDocComment) {
                                            withinGlobalDocComment = true;
                                            commentLength = 3;
                                        } else if (isBlockDocComment) {
                                            withinBlockDocComment = true;
                                            commentLength = 3;
                                        } else {
                                            withinBlockComment = true;
                                            commentLength = 2;
                                        }
                                    }
                                }
                                inComment = (commentStartLine < currentLine || (commentStartLine == currentLine && commentStartIndex <= currentIndex));
                            }
                            setGlyphFlags(currentIndex);

                            if (compareBack(m_languageDefinition.m_commentEnd, line.m_chars) && ((commentStartLine != currentLine) || (commentStartIndex + commentLength < currentIndex))) {
                                withinBlockComment = false;
                                withinBlockDocComment = false;
                                withinGlobalDocComment = false;
                                commentStartLine = endLine;
                                commentStartIndex = 0;
                                commentLength = 0;
                            }
                        }
                    }
                    if (currentIndex < line.size()) {
                        Line::Flags flags(0);
                        flags.m_value = m_lines[currentLine].m_flags[currentIndex];
                        flags.m_bits.preprocessor = withinPreproc;
                        m_lines[currentLine].m_flags[currentIndex] = flags.m_value;
                    }
                    auto utf8CharLen = utf8CharLength(c);
                    if (utf8CharLen > 1) {
                        Line::Flags flags(0);
                        flags.m_value = m_lines[currentLine].m_flags[currentIndex];
                        for (int32_t j = 1; j < utf8CharLen; j++) {
                            currentIndex++;
                            m_lines[currentLine].m_flags[currentIndex] = flags.m_value;
                        }
                    }
                    currentIndex++;
                }
                withinNotDef = !ifDefs.back();
            }
            m_defines.clear();
        }
        colorizeRange();
    }

    float TextEditor::textDistanceToLineStart(const Coordinates &aFrom) const {
        auto &line = m_lines[aFrom.m_line];
        int32_t colIndex = lineCoordinateToIndex(aFrom);
        auto substr = line.m_chars.substr(0, colIndex);
        return ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, substr.c_str(), nullptr, nullptr).x;
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

    void TextEditor::setTopLine() {
        if (!m_withinRender) {
            m_setTopLine = true;
            return;
        } else {
            m_setTopLine = false;
            ImGui::SetScrollY(m_topLine * m_charAdvance.y);
            ensureCursorVisible();
        }
    }

    void TextEditor::ensureCursorVisible() {
        if (!m_withinRender) {
            m_scrollToCursor = true;
            return;
        }

        auto scrollBarSize = ImGui::GetStyle().ScrollbarSize;
        float scrollX = ImGui::GetScrollX();
        float scrollY = ImGui::GetScrollY();

        auto windowPadding = ImGui::GetStyle().FramePadding * 2.0f;

        auto height = ImGui::GetWindowHeight() - m_topMargin - scrollBarSize - m_charAdvance.y;
        auto width = ImGui::GetWindowWidth() - windowPadding.x - scrollBarSize;

        auto top = (int32_t) rint((m_topMargin > scrollY ? m_topMargin - scrollY : scrollY) / m_charAdvance.y);
        auto bottom = top + (int32_t) rint(height / m_charAdvance.y);

        auto left = (int32_t) rint(scrollX / m_charAdvance.x);
        auto right = left + (int32_t) rint(width / m_charAdvance.x);

        auto pos = setCoordinates(m_state.m_cursorPosition);
        pos.m_column = (int32_t) rint(textDistanceToLineStart(pos) / m_charAdvance.x);

        bool mScrollToCursorX = true;
        bool mScrollToCursorY = true;

        if (pos.m_line >= top && pos.m_line <= bottom)
            mScrollToCursorY = false;
        if ((pos.m_column >= left) && (pos.m_column <= right))
            mScrollToCursorX = false;
        if (!mScrollToCursorX && !mScrollToCursorY && m_oldTopMargin == m_topMargin) {
            m_scrollToCursor = false;
            return;
        }

        if (mScrollToCursorY) {
            if (pos.m_line < top)
                ImGui::SetScrollY(std::max(0.0f, pos.m_line * m_charAdvance.y));
            if (pos.m_line > bottom)
                ImGui::SetScrollY(std::max(0.0f, pos.m_line * m_charAdvance.y - height));
        }
        if (mScrollToCursorX) {
            if (pos.m_column < left)
                ImGui::SetScrollX(std::max(0.0f, pos.m_column * m_charAdvance.x));
            if (pos.m_column > right)
                ImGui::SetScrollX(std::max(0.0f, pos.m_column * m_charAdvance.x - width));
        }
        m_oldTopMargin = m_topMargin;
    }

    float TextEditor::getPageSize() const {
        auto height = ImGui::GetCurrentWindow()->InnerClipRect.GetHeight();
        return height / m_charAdvance.y;
    }

    void TextEditor::resetCursorBlinkTime() {
        m_startTime = ImGui::GetTime() * 1000 - s_cursorBlinkOnTime;
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

    bool tokenizeCStyleString(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end) {
        auto p = in_begin;

        if (*p == '"') {
            p++;

            while (p < in_end) {
                // handle end of string
                if (*p == '"') {
                    out_begin = in_begin;
                    out_end = p + 1;
                    return true;
                }

                // handle escape character for "
                if (*p == '\\' && p + 1 < in_end && p[1] == '\\')
                    p++;
                else if (*p == '\\' && p + 1 < in_end && p[1] == '"')
                    p++;

                p++;
            }
        }

        return false;
    }

    bool tokenizeCStyleCharacterLiteral(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end) {
        auto p = in_begin;

        if (*p == '\'') {
            p++;

            // handle escape characters
            if (p < in_end && *p == '\\')
                p++;

            if (p < in_end)
                p++;

            // handle end of character literal
            if (p < in_end && *p == '\'') {
                out_begin = in_begin;
                out_end = p + 1;
                return true;
            }
        }

        return false;
    }

    bool tokenizeCStyleIdentifier(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end) {
        auto p = in_begin;

        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_' || *p == '$') {
            p++;

            while ((p < in_end) && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_'))
                p++;

            out_begin = in_begin;
            out_end = p;
            return true;
        }

        return false;
    }

    bool tokenizeCStyleNumber(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end) {
        auto p = in_begin;

        const bool startsWithNumber = *p >= '0' && *p <= '9';

        if (!startsWithNumber)
            return false;

        p++;

        bool hasNumber = startsWithNumber;

        while (p < in_end && (*p >= '0' && *p <= '9')) {
            hasNumber = true;

            p++;
        }

        if (!hasNumber)
            return false;

        bool isFloat = false;
        bool isHex = false;
        bool isBinary = false;

        if (p < in_end) {
            if (*p == '.') {
                isFloat = true;

                p++;

                while (p < in_end && (*p >= '0' && *p <= '9'))
                    p++;
            } else if (*p == 'x' || *p == 'X') {
                // hex formatted integer of the type 0xef80

                isHex = true;

                p++;

                while (p < in_end && ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F') || *p == '.' || *p == 'p' || *p == 'P'))
                    p++;
            } else if (*p == 'b' || *p == 'B') {
                // binary formatted integer of the type 0b01011101

                isBinary = true;

                p++;

                while (p < in_end && (*p >= '0' && *p <= '1'))
                    p++;
            }
        }

        if (!isHex && !isBinary) {
            // floating point exponent
            if (p < in_end && (*p == 'e' || *p == 'E')) {
                isFloat = true;

                p++;

                if (p < in_end && (*p == '+' || *p == '-'))
                    p++;

                bool hasDigits = false;

                while (p < in_end && (*p >= '0' && *p <= '9')) {
                    hasDigits = true;

                    p++;
                }

                if (!hasDigits)
                    return false;
            }

            // single and double precision floating point type
            if (p < in_end && (*p == 'f' || *p == 'F' || *p == 'd' || *p == 'D'))
                p++;
        }

        if (!isFloat) {
            // integer size type
            while (p < in_end && (*p == 'u' || *p == 'U' || *p == 'l' || *p == 'L'))
                p++;
        }

        out_begin = in_begin;
        out_end = p;
        return true;
    }

    bool tokenizeCStyleOperator(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end) {
        (void) in_end;

        switch (*in_begin) {
            case '!':
            case '%':
            case '^':
            case '&':
            case '*':
            case '-':
            case '+':
            case '=':
            case '~':
            case '|':
            case '<':
            case '>':
            case '?':
            case ':':
            case '/':
            case '@':
                out_begin = in_begin;
                out_end = in_begin + 1;
                return true;
        }

        return false;
    }

    bool tokenizeCStyleSeparator(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end) {
        (void) in_end;

        switch (*in_begin) {
            case '[':
            case ']':
            case '{':
            case '}':
            case '(':
            case ')':
            case ';':
            case ',':
            case '.':
                out_begin = in_begin;
                out_end = in_begin + 1;
                return true;
        }

        return false;
    }

    const TextEditor::LanguageDefinition &TextEditor::LanguageDefinition::CPlusPlus() {
        static bool inited = false;
        static LanguageDefinition langDef;
        if (!inited) {
            static const char *const cppKeywords[] = {
                    "alignas", "alignof", "and", "and_eq", "asm", "atomic_cancel", "atomic_commit", "atomic_noexcept",
                    "auto", "bitand", "bitor", "bool", "break", "case", "catch", "char", "char16_t", "char32_t",
                    "class", "compl", "concept", "const", "constexpr", "const_cast", "continue", "decltype", "default",
                    "delete", "do", "double", "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false",
                    "float", "for", "friend", "goto", "if", "import", "inline", "int", "long", "module", "mutable",
                    "namespace", "new", "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private",
                    "protected", "public", "register", "reinterpret_cast", "requires", "return", "short", "signed",
                    "sizeof", "static", "static_assert", "static_cast", "struct", "switch", "synchronized", "template",
                    "this", "thread_local", "throw", "true", "try", "typedef", "typeid", "typename", "union",
                    "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq"
            };
            for (auto &k: cppKeywords)
                langDef.m_keywords.insert(k);

            static const char *const identifiers[] = {
                    "abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh",
                    "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha",
                    "isdigit", "isgraph", "ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp",
                    "modf", "pow", "printf", "sprintf", "snprintf", "putchar", "putenv", "puts", "rand", "remove",
                    "rename", "sinh", "sqrt", "srand", "strcat", "strcmp", "strerror", "time", "tolower", "toupper",
                    "std", "string", "vector", "map", "unordered_map", "set", "unordered_set", "min", "max"
            };
            for (auto &k: identifiers) {
                Identifier id;
                id.m_declaration = "Built-in function";
                langDef.m_identifiers.insert(std::make_pair(std::string(k), id));
            }

            langDef.m_tokenize = [](strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end, PaletteIndex &paletteIndex) -> bool {
                paletteIndex = PaletteIndex::Max;

                while (in_begin < in_end && isascii(*in_begin) && isblank(*in_begin))
                    in_begin++;

                if (in_begin == in_end) {
                    out_begin = in_end;
                    out_end = in_end;
                    paletteIndex = PaletteIndex::Default;
                } else if (tokenizeCStyleString(in_begin, in_end, out_begin, out_end))
                    paletteIndex = PaletteIndex::StringLiteral;
                else if (tokenizeCStyleCharacterLiteral(in_begin, in_end, out_begin, out_end))
                    paletteIndex = PaletteIndex::CharLiteral;
                else if (tokenizeCStyleIdentifier(in_begin, in_end, out_begin, out_end))
                    paletteIndex = PaletteIndex::Identifier;
                else if (tokenizeCStyleNumber(in_begin, in_end, out_begin, out_end))
                    paletteIndex = PaletteIndex::NumericLiteral;
                else if (tokenizeCStyleSeparator(in_begin, in_end, out_begin, out_end))
                    paletteIndex = PaletteIndex::Separator;
                else if (tokenizeCStyleOperator(in_begin, in_end, out_begin, out_end))
                    paletteIndex = PaletteIndex::Operator;

                return paletteIndex != PaletteIndex::Max;
            };

            langDef.m_commentStart = "/*";
            langDef.m_commentEnd = "*/";
            langDef.m_singleLineComment = "//";

            langDef.m_caseSensitive = true;
            langDef.m_autoIndentation = true;

            langDef.m_name = "C++";

            inited = true;
        }
        return langDef;
    }

    const TextEditor::LanguageDefinition &TextEditor::LanguageDefinition::HLSL() {
        static bool inited = false;
        static LanguageDefinition langDef;
        if (!inited) {
            static const char *const keywords[] = {
                    "AppendStructuredBuffer",
                    "asm",
                    "asm_fragment",
                    "BlendState",
                    "bool",
                    "break",
                    "Buffer",
                    "ByteAddressBuffer",
                    "case",
                    "cbuffer",
                    "centroid",
                    "class",
                    "column_major",
                    "compile",
                    "compile_fragment",
                    "CompileShader",
                    "const",
                    "continue",
                    "ComputeShader",
                    "ConsumeStructuredBuffer",
                    "default",
                    "DepthStencilState",
                    "DepthStencilView",
                    "discard",
                    "do",
                    "double",
                    "DomainShader",
                    "dword",
                    "else",
                    "export",
                    "extern",
                    "false",
                    "float",
                    "for",
                    "fxgroup",
                    "GeometryShader",
                    "groupshared",
                    "half",
                    "Hullshader",
                    "if",
                    "in",
                    "inline",
                    "inout",
                    "InputPatch",
                    "int",
                    "interface",
                    "line",
                    "lineadj",
                    "linear",
                    "LineStream",
                    "matrix",
                    "min16float",
                    "min10float",
                    "min16int",
                    "min12int",
                    "min16uint",
                    "namespace",
                    "nointerpolation",
                    "noperspective",
                    "NULL",
                    "out",
                    "OutputPatch",
                    "packoffset",
                    "pass",
                    "pixelfragment",
                    "PixelShader",
                    "point",
                    "PointStream",
                    "precise",
                    "RasterizerState",
                    "RenderTargetView",
                    "return",
                    "register",
                    "row_major",
                    "RWBuffer",
                    "RWByteAddressBuffer",
                    "RWStructuredBuffer",
                    "RWTexture1D",
                    "RWTexture1DArray",
                    "RWTexture2D",
                    "RWTexture2DArray",
                    "RWTexture3D",
                    "sample",
                    "sampler",
                    "SamplerState",
                    "SamplerComparisonState",
                    "shared",
                    "snorm",
                    "stateblock",
                    "stateblock_state",
                    "static",
                    "string",
                    "struct",
                    "switch",
                    "StructuredBuffer",
                    "tbuffer",
                    "technique",
                    "technique10",
                    "technique11",
                    "texture",
                    "Texture1D",
                    "Texture1DArray",
                    "Texture2D",
                    "Texture2DArray",
                    "Texture2DMS",
                    "Texture2DMSArray",
                    "Texture3D",
                    "TextureCube",
                    "TextureCubeArray",
                    "true",
                    "typedef",
                    "triangle",
                    "triangleadj",
                    "TriangleStream",
                    "uint",
                    "uniform",
                    "unorm",
                    "unsigned",
                    "vector",
                    "vertexfragment",
                    "VertexShader",
                    "void",
                    "volatile",
                    "while",
                    "bool1",
                    "bool2",
                    "bool3",
                    "bool4",
                    "double1",
                    "double2",
                    "double3",
                    "double4",
                    "float1",
                    "float2",
                    "float3",
                    "float4",
                    "int1",
                    "int2",
                    "int3",
                    "int4",
                    "in",
                    "out",
                    "inout",
                    "uint1",
                    "uint2",
                    "uint3",
                    "uint4",
                    "dword1",
                    "dword2",
                    "dword3",
                    "dword4",
                    "half1",
                    "half2",
                    "half3",
                    "half4",
                    "float1x1",
                    "float2x1",
                    "float3x1",
                    "float4x1",
                    "float1x2",
                    "float2x2",
                    "float3x2",
                    "float4x2",
                    "float1x3",
                    "float2x3",
                    "float3x3",
                    "float4x3",
                    "float1x4",
                    "float2x4",
                    "float3x4",
                    "float4x4",
                    "half1x1",
                    "half2x1",
                    "half3x1",
                    "half4x1",
                    "half1x2",
                    "half2x2",
                    "half3x2",
                    "half4x2",
                    "half1x3",
                    "half2x3",
                    "half3x3",
                    "half4x3",
                    "half1x4",
                    "half2x4",
                    "half3x4",
                    "half4x4",
            };
            for (auto &k: keywords)
                langDef.m_keywords.insert(k);

            static const char *const identifiers[] = {
                    "abort", "abs", "acos", "all", "AllMemoryBarrier", "AllMemoryBarrierWithGroupSync", "any",
                    "asdouble", "asfloat", "asin", "asint", "asint", "asuint", "asuint", "atan", "atan2", "ceil",
                    "CheckAccessFullyMapped", "clamp", "clip", "cos", "cosh", "countbits", "cross", "D3DCOLORtoUBYTE4",
                    "ddx", "ddx_coarse", "ddx_fine", "ddy", "ddy_coarse", "ddy_fine", "degrees", "determinant",
                    "DeviceMemoryBarrier", "DeviceMemoryBarrierWithGroupSync", "distance", "dot", "dst", "errorf",
                    "EvaluateAttributeAtCentroid", "EvaluateAttributeAtSample", "EvaluateAttributeSnapped", "exp",
                    "exp2", "f16tof32", "f32tof16", "faceforward", "firstbithigh", "firstbitlow", "floor", "fma",
                    "fmod", "frac", "frexp", "fwidth", "GetRenderTargetSampleCount", "GetRenderTargetSamplePosition",
                    "GroupMemoryBarrier", "GroupMemoryBarrierWithGroupSync", "InterlockedAdd", "InterlockedAnd",
                    "InterlockedCompareExchange", "InterlockedCompareStore", "InterlockedExchange", "InterlockedMax",
                    "InterlockedMin", "InterlockedOr", "InterlockedXor", "isfinite", "isinf", "isnan", "ldexp",
                    "length", "lerp", "lit", "log", "log10", "log2", "mad", "max", "min", "modf", "msad4", "mul",
                    "noise", "normalize", "pow", "printf", "Process2DQuadTessFactorsAvg", "Process2DQuadTessFactorsMax",
                    "Process2DQuadTessFactorsMin", "ProcessIsolineTessFactors", "ProcessQuadTessFactorsAvg",
                    "ProcessQuadTessFactorsMax", "ProcessQuadTessFactorsMin", "ProcessTriTessFactorsAvg",
                    "ProcessTriTessFactorsMax", "ProcessTriTessFactorsMin", "radians", "rcp", "reflect", "refract",
                    "reversebits", "round", "rsqrt", "saturate", "sign", "sin", "sincos", "sinh", "smoothstep", "sqrt",
                    "step", "tan", "tanh", "tex1D", "tex1D", "tex1Dbias", "tex1Dgrad", "tex1Dlod", "tex1Dproj", "tex2D",
                    "tex2D", "tex2Dbias", "tex2Dgrad", "tex2Dlod", "tex2Dproj", "tex3D", "tex3D", "tex3Dbias",
                    "tex3Dgrad", "tex3Dlod", "tex3Dproj", "texCUBE", "texCUBE", "texCUBEbias", "texCUBEgrad",
                    "texCUBElod", "texCUBEproj", "transpose", "trunc"
            };
            for (auto &k: identifiers) {
                Identifier id;
                id.m_declaration = "Built-in function";
                langDef.m_identifiers.insert(std::make_pair(std::string(k), id));
            }

            langDef.m_tokenRegexStrings.push_back(std::make_pair("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Directive));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::StringLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::NumericLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("[\\!\\%\\^\\&\\*\\-\\+\\=\\~\\|\\<\\>\\?\\/]", PaletteIndex::Operator));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("[\\[\\]\\{\\}\\(\\)\\;\\,\\.]", PaletteIndex::Separator));
            langDef.m_commentStart = "/*";
            langDef.m_commentEnd = "*/";
            langDef.m_singleLineComment = "//";

            langDef.m_caseSensitive = true;
            langDef.m_autoIndentation = true;

            langDef.m_name = "HLSL";

            inited = true;
        }
        return langDef;
    }

    const TextEditor::LanguageDefinition &TextEditor::LanguageDefinition::GLSL() {
        static bool inited = false;
        static LanguageDefinition langDef;
        if (!inited) {
            static const char *const keywords[] = {
                    "auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum",
                    "extern", "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return",
                    "short", "signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void",
                    "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic",
                    "_Imaginary", "_Noreturn", "_Static_assert", "_Thread_local"
            };
            for (auto &k: keywords)
                langDef.m_keywords.insert(k);

            static const char *const identifiers[] = {
                    "abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh",
                    "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha",
                    "isdigit", "isgraph", "ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp",
                    "modf", "pow", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand",
                    "strcat", "strcmp", "strerror", "time", "tolower", "toupper"
            };
            for (auto &k: identifiers) {
                Identifier id;
                id.m_declaration = "Built-in function";
                langDef.m_identifiers.insert(std::make_pair(std::string(k), id));
            }

            langDef.m_tokenRegexStrings.push_back(std::make_pair("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Directive));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::StringLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?",PaletteIndex::NumericLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("[\\!\\%\\^\\&\\*\\-\\+\\=\\~\\|\\<\\>\\?\\/]", PaletteIndex::Operator));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("[\\[\\]\\{\\}\\(\\)\\;\\,\\.]", PaletteIndex::Separator));
            langDef.m_commentStart = "/*";
            langDef.m_commentEnd = "*/";
            langDef.m_singleLineComment = "//";

            langDef.m_caseSensitive = true;
            langDef.m_autoIndentation = true;

            langDef.m_name = "GLSL";

            inited = true;
        }
        return langDef;
    }

    const TextEditor::LanguageDefinition &TextEditor::LanguageDefinition::C() {
        static bool inited = false;
        static LanguageDefinition langDef;
        if (!inited) {
            static const char *const keywords[] = {
                    "auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum",
                    "extern", "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return",
                    "short", "signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void",
                    "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic",
                    "_Imaginary", "_Noreturn", "_Static_assert", "_Thread_local"
            };
            for (auto &k: keywords)
                langDef.m_keywords.insert(k);

            static const char *const identifiers[] = {
                    "abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh",
                    "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha",
                    "isdigit", "isgraph", "ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp",
                    "modf", "pow", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand",
                    "strcat", "strcmp", "strerror", "time", "tolower", "toupper"
            };
            for (auto &k: identifiers) {
                Identifier id;
                id.m_declaration = "Built-in function";
                langDef.m_identifiers.insert(std::make_pair(std::string(k), id));
            }

            langDef.m_tokenize = [](strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end, PaletteIndex &paletteIndex) -> bool {
                paletteIndex = PaletteIndex::Max;

                while (in_begin < in_end && isascii(*in_begin) && isblank(*in_begin))
                    in_begin++;

                if (in_begin == in_end) {
                    out_begin = in_end;
                    out_end = in_end;
                    paletteIndex = PaletteIndex::Default;
                } else if (tokenizeCStyleString(in_begin, in_end, out_begin, out_end))
                    paletteIndex = PaletteIndex::StringLiteral;
                else if (tokenizeCStyleCharacterLiteral(in_begin, in_end, out_begin, out_end))
                    paletteIndex = PaletteIndex::CharLiteral;
                else if (tokenizeCStyleIdentifier(in_begin, in_end, out_begin, out_end))
                    paletteIndex = PaletteIndex::Identifier;
                else if (tokenizeCStyleNumber(in_begin, in_end, out_begin, out_end))
                    paletteIndex = PaletteIndex::NumericLiteral;
                else if (tokenizeCStyleOperator(in_begin, in_end, out_begin, out_end))
                    paletteIndex = PaletteIndex::Operator;
                else if (tokenizeCStyleSeparator(in_begin, in_end, out_begin, out_end))
                    paletteIndex = PaletteIndex::Separator;

                return paletteIndex != PaletteIndex::Max;
            };

            langDef.m_commentStart = "/*";
            langDef.m_commentEnd = "*/";
            langDef.m_singleLineComment = "//";

            langDef.m_caseSensitive = true;
            langDef.m_autoIndentation = true;

            langDef.m_name = "C";

            inited = true;
        }
        return langDef;
    }

    const TextEditor::LanguageDefinition &TextEditor::LanguageDefinition::SQL() {
        static bool inited = false;
        static LanguageDefinition langDef;
        if (!inited) {
            static const char *const keywords[] = {
                    "ADD", "EXCEPT", "PERCENT", "ALL", "EXEC", "PLAN", "ALTER", "EXECUTE", "PRECISION", "AND", "EXISTS",
                    "PRIMARY", "ANY", "EXIT", "PRINT", "AS", "FETCH", "PROC", "ASC", "FILE", "PROCEDURE",
                    "AUTHORIZATION", "FILLFACTOR", "PUBLIC", "BACKUP", "FOR", "RAISERROR", "BEGIN", "FOREIGN", "READ",
                    "BETWEEN", "FREETEXT", "READTEXT", "BREAK", "FREETEXTTABLE", "RECONFIGURE", "BROWSE", "FROM",
                    "REFERENCES", "BULK", "FULL", "REPLICATION", "BY", "FUNCTION", "RESTORE", "CASCADE", "GOTO",
                    "RESTRICT", "CASE", "GRANT", "RETURN", "CHECK", "GROUP", "REVOKE", "CHECKPOINT", "HAVING", "RIGHT",
                    "CLOSE", "HOLDLOCK", "ROLLBACK", "CLUSTERED", "IDENTITY", "ROWCOUNT", "COALESCE", "IDENTITY_INSERT",
                    "ROWGUIDCOL", "COLLATE", "IDENTITYCOL", "RULE", "COLUMN", "IF", "SAVE", "COMMIT", "IN", "SCHEMA",
                    "COMPUTE", "INDEX", "SELECT", "CONSTRAINT", "INNER", "SESSION_USER", "CONTAINS", "INSERT", "SET",
                    "CONTAINSTABLE", "INTERSECT", "SETUSER", "CONTINUE", "INTO", "SHUTDOWN", "CONVERT", "IS", "SOME",
                    "CREATE", "JOIN", "STATISTICS", "CROSS", "KEY", "SYSTEM_USER", "CURRENT", "KILL", "TABLE",
                    "CURRENT_DATE", "LEFT", "TEXTSIZE", "CURRENT_TIME", "LIKE", "THEN", "CURRENT_TIMESTAMP", "LINENO",
                    "TO", "CURRENT_USER", "LOAD", "TOP", "CURSOR", "NATIONAL", "TRAN", "DATABASE", "NOCHECK",
                    "TRANSACTION", "DBCC", "NONCLUSTERED", "TRIGGER", "DEALLOCATE", "NOT", "TRUNCATE", "DECLARE",
                    "NULL", "TSEQUAL", "DEFAULT", "NULLIF", "UNION", "DELETE", "OF", "UNIQUE", "DENY", "OFF", "UPDATE",
                    "DESC", "OFFSETS", "UPDATETEXT", "DISK", "ON", "USE", "DISTINCT", "OPEN", "USER", "DISTRIBUTED",
                    "OPENDATASOURCE", "VALUES", "DOUBLE", "OPENQUERY", "VARYING", "DROP", "OPENROWSET", "VIEW", "DUMMY",
                    "OPENXML", "WAITFOR", "DUMP", "OPTION", "WHEN", "ELSE", "OR", "WHERE", "END", "ORDER", "WHILE",
                    "ERRLVL", "OUTER", "WITH", "ESCAPE", "OVER", "WRITETEXT"
            };

            for (auto &k: keywords)
                langDef.m_keywords.insert(k);

            static const char *const identifiers[] = {
                    "ABS", "ACOS", "ADD_MONTHS", "ASCII", "ASCIISTR", "ASIN", "ATAN", "ATAN2", "AVG", "BFILENAME",
                    "BIN_TO_NUM", "BITAND", "CARDINALITY", "CASE", "CAST", "CEIL", "CHARTOROWID", "CHR", "COALESCE",
                    "COMPOSE", "CONCAT", "CONVERT", "CORR", "COS", "COSH", "COUNT", "COVAR_POP", "COVAR_SAMP",
                    "CUME_DIST", "CURRENT_DATE", "CURRENT_TIMESTAMP", "DBTIMEZONE", "DECODE", "DECOMPOSE", "DENSE_RANK",
                    "DUMP", "EMPTY_BLOB", "EMPTY_CLOB", "EXP", "EXTRACT", "FIRST_VALUE", "FLOOR", "FROM_TZ", "GREATEST",
                    "GROUP_ID", "HEXTORAW", "INITCAP", "INSTR", "INSTR2", "INSTR4", "INSTRB", "INSTRC", "LAG",
                    "LAST_DAY", "LAST_VALUE", "LEAD", "LEAST", "LENGTH", "LENGTH2", "LENGTH4", "LENGTHB", "LENGTHC",
                    "LISTAGG", "LN", "LNNVL", "LOCALTIMESTAMP", "LOG", "LOWER", "LPAD", "LTRIM", "MAX", "MEDIAN", "MIN",
                    "MOD", "MONTHS_BETWEEN", "NANVL", "NCHR", "NEW_TIME", "NEXT_DAY", "NTH_VALUE", "NULLIF",
                    "NUMTODSINTERVAL", "NUMTOYMINTERVAL", "NVL", "NVL2", "POWER", "RANK", "RAWTOHEX", "REGEXP_COUNT",
                    "REGEXP_INSTR", "REGEXP_REPLACE", "REGEXP_SUBSTR", "REMAINDER", "REPLACE", "ROUND", "ROWNUM",
                    "RPAD", "RTRIM", "SESSIONTIMEZONE", "SIGN", "SIN", "SINH", "SOUNDEX", "SQRT", "STDDEV", "SUBSTR",
                    "SUM", "SYS_CONTEXT", "SYSDATE", "SYSTIMESTAMP", "TAN", "TANH", "TO_CHAR", "TO_CLOB", "TO_DATE",
                    "TO_DSINTERVAL", "TO_LOB", "TO_MULTI_BYTE", "TO_NCLOB", "TO_NUMBER", "TO_SINGLE_BYTE",
                    "TO_TIMESTAMP", "TO_TIMESTAMP_TZ", "TO_YMINTERVAL", "TRANSLATE", "TRIM", "TRUNC", "TZ_OFFSET",
                    "UID", "UPPER", "USER", "USERENV", "VAR_POP", "VAR_SAMP", "VARIANCE", "VSIZE "
            };
            for (auto &k: identifiers) {
                Identifier id;
                id.m_declaration = "Built-in function";
                langDef.m_identifiers.insert(std::make_pair(std::string(k), id));
            }

            langDef.m_tokenRegexStrings.push_back(std::make_pair("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Directive));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::StringLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?",PaletteIndex::NumericLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("[\\!\\%\\^\\&\\*\\-\\+\\=\\~\\|\\<\\>\\?\\/]", PaletteIndex::Operator));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("[\\[\\]\\{\\}\\(\\)\\;\\,\\.]", PaletteIndex::Separator));

            langDef.m_commentStart = "/*";
            langDef.m_commentEnd = "*/";
            langDef.m_singleLineComment = "//";

            langDef.m_caseSensitive = false;
            langDef.m_autoIndentation = false;

            langDef.m_name = "SQL";

            inited = true;
        }
        return langDef;
    }

    const TextEditor::LanguageDefinition &TextEditor::LanguageDefinition::AngelScript() {
        static bool inited = false;
        static LanguageDefinition langDef;
        if (!inited) {
            static const char *const keywords[] = {
                    "and", "abstract", "auto", "bool", "break", "case", "cast", "class", "const", "continue", "default",
                    "do", "double", "else", "enum", "false", "final", "float", "for", "from", "funcdef", "function",
                    "get", "if", "import", "in", "inout", "int", "interface", "int8", "int16", "int32", "int64", "is",
                    "mixin", "namespace", "not", "null", "or", "out", "override", "private", "protected", "return",
                    "set", "shared", "super", "switch", "this ", "true", "typedef", "uint", "uint8", "uint16", "uint32",
                    "uint64", "void", "while", "xor"
            };

            for (auto &k: keywords)
                langDef.m_keywords.insert(k);

            static const char *const identifiers[] = {
                    "cos", "sin", "tab", "acos", "asin", "atan", "atan2", "cosh", "sinh", "tanh", "log", "log10", "pow",
                    "sqrt", "abs", "ceil", "floor", "fraction", "closeTo", "fpFromIEEE", "fpToIEEE", "complex",
                    "opEquals", "opAddAssign", "opSubAssign", "opMulAssign", "opDivAssign", "opAdd", "opSub", "opMul",
                    "opDiv"
            };
            for (auto &k: identifiers) {
                Identifier id;
                id.m_declaration = "Built-in function";
                langDef.m_identifiers.insert(std::make_pair(std::string(k), id));
            }

            langDef.m_tokenRegexStrings.push_back(std::make_pair("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Directive));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::StringLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?",PaletteIndex::NumericLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("[\\!\\%\\^\\&\\*\\-\\+\\=\\~\\|\\<\\>\\?\\/]", PaletteIndex::Operator));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("[\\[\\]\\{\\}\\(\\)\\;\\,\\.]", PaletteIndex::Separator));

            langDef.m_commentStart = "/*";
            langDef.m_commentEnd = "*/";
            langDef.m_singleLineComment = "//";

            langDef.m_caseSensitive = true;
            langDef.m_autoIndentation = true;

            langDef.m_name = "AngelScript";

            inited = true;
        }
        return langDef;
    }

    const TextEditor::LanguageDefinition &TextEditor::LanguageDefinition::Lua() {
        static bool inited = false;
        static LanguageDefinition langDef;
        if (!inited) {
            static const char *const keywords[] = {
                    "and", "break", "do", "", "else", "elseif", "end", "false", "for", "function", "if", "in", "",
                    "local", "nil", "not", "or", "repeat", "return", "then", "true", "until", "while"
            };

            for (auto &k: keywords)
                langDef.m_keywords.insert(k);

            static const char *const identifiers[] = {
                    "assert", "collectgarbage", "dofile", "error", "getmetatable", "ipairs", "loadfile", "load",
                    "loadstring", "next", "pairs", "pcall", "print", "rawequal", "rawlen", "rawget", "rawset", "select",
                    "setmetatable", "tonumber", "tostring", "type", "xpcall", "_G", "_VERSION", "arshift", "band",
                    "bnot", "bor", "bxor", "btest", "extract", "lrotate", "lshift", "replace", "rrotate", "rshift",
                    "create", "resume", "running", "status", "wrap", "yield", "isyieldable", "debug", "getuservalue",
                    "gethook", "getinfo", "getlocal", "getregistry", "getmetatable", "getupvalue", "upvaluejoin",
                    "upvalueid", "setuservalue", "sethook", "setlocal", "setmetatable", "setupvalue", "traceback",
                    "close", "flush", "input", "lines", "open", "output", "popen", "read", "tmpfile", "type", "write",
                    "close", "flush", "lines", "read", "seek", "setvbuf", "write", "__gc", "__tostring", "abs", "acos",
                    "asin", "atan", "ceil", "cos", "deg", "exp", "tointeger", "floor", "fmod", "ult", "log", "max",
                    "min", "modf", "rad", "random", "randomseed", "sin", "sqrt", "string", "tan", "type", "atan2",
                    "cosh", "sinh", "tanh", "pow", "frexp", "ldexp", "log10", "pi", "huge", "maxinteger", "mininteger",
                    "loadlib", "searchpath", "seeall", "preload", "cpath", "path", "searchers", "loaded", "module",
                    "require", "clock", "date", "difftime", "execute", "exit", "getenv", "remove", "rename",
                    "setlocale", "time", "tmpname", "byte", "char", "dump", "find", "format", "gmatch", "gsub", "len",
                    "lower", "match", "rep", "reverse", "sub", "upper", "pack", "packsize", "unpack", "concat", "maxn",
                    "insert", "pack", "unpack", "remove", "move", "sort", "offset", "codepoint", "char", "len", "codes",
                    "charpattern", "coroutine", "table", "io", "os", "string", "utf8", "bit32", "math", "debug",
                    "package"
            };
            for (auto &k: identifiers) {
                Identifier id;
                id.m_declaration = "Built-in function";
                langDef.m_identifiers.insert(std::make_pair(std::string(k), id));
            }

            langDef.m_tokenRegexStrings.push_back(std::make_pair("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Directive));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::StringLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::NumericLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("[\\!\\%\\^\\&\\*\\-\\+\\=\\~\\|\\<\\>\\?\\/]", PaletteIndex::Operator));
            langDef.m_tokenRegexStrings.push_back(std::make_pair("[\\[\\]\\{\\}\\(\\)\\;\\,\\.]", PaletteIndex::Separator));

            langDef.m_commentStart = "--[[";
            langDef.m_commentEnd = "]]";
            langDef.m_singleLineComment = "--";

            langDef.m_caseSensitive = true;
            langDef.m_autoIndentation = false;

            langDef.m_name = "Lua";

            inited = true;
        }
        return langDef;
    }
}