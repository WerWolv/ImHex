#include <algorithm>
#include <chrono>
#include <string>
#include <regex>
#include <cmath>

#include "TextEditor.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"    // for imGui::GetCurrentWindow()
#include "imgui_internal.h"

// TODO
// - multiline comments vs single-line: latter is blocking start of a ML

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

TextEditor::Palette TextEditor::s_paletteBase = TextEditor::getDarkPalette();

TextEditor::FindReplaceHandler::FindReplaceHandler() : m_wholeWord(false), m_findRegEx(false), m_matchCase(false)  {}

TextEditor::TextEditor()
    : m_lineSpacing(1.0f), m_undoIndex(0), m_tabSize(4), m_overwrite(false), m_readOnly(false), m_withinRender(false), m_scrollToCursor(false), m_scrollToTop(false), m_scrollToBottom(false), m_textChanged(false), m_colorizerEnabled(true), m_textStart(20.0f), m_leftMargin(10), m_topMargin(0), m_cursorPositionChanged(false), m_colorRangeMin(0), m_colorRangeMax(0), m_selectionMode(SelectionMode::Normal), m_checkComments(true), m_lastClick(-1.0f), m_handleKeyboardInputs(true), m_handleMouseInputs(true), m_ignoreImGuiChild(false), m_showWhitespaces(true), m_showCursor(true), m_showLineNumbers(true), m_startTime(ImGui::GetTime() * 1000) {
    setLanguageDefinition(LanguageDefinition::HLSL());
    m_lines.push_back(Line());
}

TextEditor::~TextEditor() {
}

void TextEditor::setLanguageDefinition(const LanguageDefinition &aLanguageDef) {
    mL_languageDefinition = aLanguageDef;
    m_regexList.clear();

    for (auto &r : mL_languageDefinition.m_tokenRegexStrings)
        m_regexList.push_back(std::make_pair(std::regex(r.first, std::regex_constants::optimize), r.second));

    colorize();
}

void TextEditor::setPalette(const Palette &aValue) {
    s_paletteBase = aValue;
}

std::string TextEditor::getText(const Coordinates &aStart, const Coordinates &aEnd) const {
    std::string result;

    auto line_start = aStart.m_line;
    auto line_end   = aEnd.m_line;
    auto index_start = getCharacterIndex(aStart);
    auto index_end   = getCharacterIndex(aEnd);
    size_t s    = 0;

    for (size_t i = line_start; i < line_end; i++)
        s += m_lines[i].size();

    result.reserve(s + s / 8);

    while (index_start < index_end || line_start < line_end) {
        if (line_start >= (int32_t)m_lines.size())
            break;

        auto &line = m_lines[line_start];
        if (index_start < (int32_t)line.size()) {
            result += line[index_start].m_char;
            index_start++;
        } else {
            index_start = 0;
            ++line_start;
            result += '\n';
        }
    }

    return result;
}

TextEditor::Coordinates TextEditor::getActualCursorCoordinates() const {
    return sanitizeCoordinates(m_state.m_cursorPosition);
}

TextEditor::Coordinates TextEditor::sanitizeCoordinates(const Coordinates &aValue) const {
    auto line   = aValue.m_line;
    auto column = aValue.m_column;
    if (line >= (int32_t)m_lines.size()) {
        if (m_lines.empty()) {
            line   = 0;
            column = 0;
        } else {
            line   = (int32_t)m_lines.size() - 1;
            column = getLineMaxColumn(line);
        }
        return Coordinates(line, column);
    } else {
        column = m_lines.empty() ? 0 : std::min(column, getLineMaxColumn(line));
        return Coordinates(line, column);
    }
}

// https://en.wikipedia.org/wiki/UTF-8
// We assume that the char is a standalone character (<128) or a leading byte of an UTF-8 code sequence (non-10xxxxxx code)
static int32_t UTF8CharLength(TextEditor::Char c) {
    if ((c & 0xFE) == 0xFC)
        return 6;
    if ((c & 0xFC) == 0xF8)
        return 5;
    if ((c & 0xF8) == 0xF0)
        return 4;
    else if ((c & 0xF0) == 0xE0)
        return 3;
    else if ((c & 0xE0) == 0xC0)
        return 2;
    return 1;
}

// "Borrowed" from ImGui source
static inline int32_t ImTextCharToUtf8(char *buffer, int32_t buffer_size, uint32_t c) {
    if (c < 0x80) {
        buffer[0] = (char)c;
        return 1;
    }
    if (c < 0x800) {
        if (buffer_size < 2) return 0;
        buffer[0] = (char)(0xc0 + (c >> 6));
        buffer[1] = (char)(0x80 + (c & 0x3f));
        return 2;
    }
    if (c >= 0xdc00 && c < 0xe000) {
        return 0;
    }
    if (c >= 0xd800 && c < 0xdc00) {
        if (buffer_size < 4) return 0;
        buffer[0] = (char)(0xf0 + (c >> 18));
        buffer[1] = (char)(0x80 + ((c >> 12) & 0x3f));
        buffer[2] = (char)(0x80 + ((c >> 6) & 0x3f));
        buffer[3] = (char)(0x80 + ((c) & 0x3f));
        return 4;
    }
    // else if (c < 0x10000)
    {
        if (buffer_size < 3) return 0;
        buffer[0] = (char)(0xe0 + (c >> 12));
        buffer[1] = (char)(0x80 + ((c >> 6) & 0x3f));
        buffer[2] = (char)(0x80 + ((c) & 0x3f));
        return 3;
    }
}

void TextEditor::advance(Coordinates &aCoordinates) const {
    if (aCoordinates.m_line < (int32_t)m_lines.size()) {
        auto &line  = m_lines[aCoordinates.m_line];
        auto charIndex = getCharacterIndex(aCoordinates);

        if (charIndex + 1 < (int32_t)line.size()) {
            auto delta = UTF8CharLength(line[charIndex].m_char);
            charIndex     = std::min(charIndex + delta, (int32_t)line.size() - 1);
        } else {
            ++aCoordinates.m_line;
            charIndex = 0;
        }
        aCoordinates.m_column = getCharacterColumn(aCoordinates.m_line, charIndex);
    }
}

void TextEditor::deleteRange(const Coordinates &aStart, const Coordinates &aEnd) {
    assert(aEnd >= aStart);
    assert(!m_readOnly);

    // printf("D(%d.%d)-(%d.%d)\n", aStart.m_line, aStart.m_column, aEnd.m_line, aEnd.m_column);

    if (aEnd == aStart)
        return;

    auto start = getCharacterIndex(aStart);
    auto end   = getCharacterIndex(aEnd);

    if (aStart.m_line == aEnd.m_line) {
        auto &line = m_lines[aStart.m_line];
        auto n     = getLineMaxColumn(aStart.m_line);
        if (aEnd.m_column >= n)
            line.erase(line.begin() + start, line.end());
        else
            line.erase(line.begin() + start, line.begin() + end);
    } else {
        auto &firstLine = m_lines[aStart.m_line];
        auto &lastLine  = m_lines[aEnd.m_line];

        firstLine.erase(firstLine.begin() + start, firstLine.end());
        lastLine.erase(lastLine.begin(), lastLine.begin() + end);

        if (aStart.m_line < aEnd.m_line)
            firstLine.insert(firstLine.end(), lastLine.begin(), lastLine.end());

        if (aStart.m_line < aEnd.m_line)
            removeLine(aStart.m_line + 1, aEnd.m_line + 1);
    }

    m_textChanged = true;
}

int32_t TextEditor::insertTextAt(Coordinates /* inout */ &aWhere, const char *aValue) {
    int32_t charIndex     = getCharacterIndex(aWhere);
    int32_t totalLines = 0;
    while (*aValue != '\0') {
        assert(!m_lines.empty());

        if (*aValue == '\r') {
            // skip
            ++aValue;
        } else if (*aValue == '\n') {
            if (charIndex < (int32_t)m_lines[aWhere.m_line].size()) {
                auto &newLine = insertLine(aWhere.m_line + 1);
                auto &line    = m_lines[aWhere.m_line];
                newLine.insert(newLine.begin(), line.begin() + charIndex, line.end());
                line.erase(line.begin() + charIndex, line.end());
            } else {
                insertLine(aWhere.m_line + 1);
            }
            ++aWhere.m_line;
            aWhere.m_column = 0;
            charIndex         = 0;
            ++totalLines;
            ++aValue;
        } else {
            auto &line = m_lines[aWhere.m_line];
            auto d     = UTF8CharLength(*aValue);
            while (d-- > 0 && *aValue != '\0')
                line.insert(line.begin() + charIndex++, Glyph(*aValue++, PaletteIndex::Default));
            ++aWhere.m_column;
        }

        m_textChanged = true;
    }

    return totalLines;
}

void TextEditor::addUndo(UndoRecord &aValue) {
    assert(!m_readOnly);
    // printf("addUndo: (@%d.%d) +\'%s' [%d.%d .. %d.%d], -\'%s', [%d.%d .. %d.%d] (@%d.%d)\n",
    //	aValue.m_before.m_cursorPosition.m_line, aValue.m_before.m_cursorPosition.m_column,
    //	aValue.m_added.c_str(), aValue.m_addedStart.m_line, aValue.m_addedStart.m_column, aValue.m_addedEnd.m_line, aValue.m_addedEnd.m_column,
    //	aValue.m_removed.c_str(), aValue.m_removedStart.m_line, aValue.m_removedStart.m_column, aValue.m_removedEnd.m_line, aValue.m_removedEnd.m_column,
    //	aValue.m_after.m_cursorPosition.m_line, aValue.m_after.m_cursorPosition.m_column
    //	);

    m_undoBuffer.resize((size_t)(m_undoIndex + 1));
    m_undoBuffer.back() = aValue;
    ++m_undoIndex;
}

TextEditor::Coordinates TextEditor::screenPosToCoordinates(const ImVec2 &aPosition) const {
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 local(aPosition.x - origin.x, aPosition.y - origin.y);

    int32_t lineNo = std::max(0, (int32_t)floor(local.y / m_charAdvance.y));

    int32_t columnCoord = 0;

    if (lineNo >= 0 && lineNo < (int32_t)m_lines.size()) {
        auto &line = m_lines.at(lineNo);

        int32_t columnIndex = 0;
        float columnX   = 0.0f;

        while ((size_t)columnIndex < line.size()) {
            float columnWidth = 0.0f;

            if (line[columnIndex].m_char == '\t') {
                float spaceSize  = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ").x;
                float oldX       = columnX;
                float newColumnX = (1.0f + std::floor((1.0f + columnX) / (float(m_tabSize) * spaceSize))) * (float(m_tabSize) * spaceSize);
                columnWidth      = newColumnX - oldX;
                if (m_textStart + columnX + columnWidth * 0.5f > local.x)
                    break;
                columnX     = newColumnX;
                columnCoord = (columnCoord / m_tabSize) * m_tabSize + m_tabSize;
                columnIndex++;
            } else {
                char buf[7];
                auto d = UTF8CharLength(line[columnIndex].m_char);
                int32_t i  = 0;
                while (i < 6 && d-- > 0)
                    buf[i++] = line[columnIndex++].m_char;
                buf[i]      = '\0';
                columnWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf).x;
                if (m_textStart + columnX + columnWidth * 0.5f > local.x)
                    break;
                columnX += columnWidth;
                columnCoord++;
            }
        }
    }

    return sanitizeCoordinates(Coordinates(lineNo, columnCoord));
}

TextEditor::Coordinates TextEditor::findWordStart(const Coordinates &aFrom) const {
    Coordinates at = aFrom;
    if (at.m_line >= (int32_t)m_lines.size())
        return at;

    auto &line  = m_lines[at.m_line];
    auto charIndex = getCharacterIndex(at);

    if (charIndex >= (int32_t)line.size())
        return at;

    while (charIndex > 0 && isspace(line[charIndex].m_char))
        --charIndex;

    auto charStart = line[charIndex].m_char;
    while (charIndex > 0) {
        auto c = line[charIndex].m_char;
        if ((c & 0xC0) != 0x80)    // not UTF code sequence 10xxxxxx
        {
            if (c <= 32 && isspace(c)) {
                charIndex++;
                break;
            }

            if (isalnum(charStart) || charStart == '_') {
                if (!isalnum(c) && c != '_') {
                    charIndex++;
                    break;
                }
            } else {
                break;
            }
        }
        --charIndex;
    }
    return Coordinates(at.m_line, getCharacterColumn(at.m_line, charIndex));
}

TextEditor::Coordinates TextEditor::findWordEnd(const Coordinates &aFrom) const {
    Coordinates at = aFrom;
    if (at.m_line >= (int32_t)m_lines.size())
        return at;

    auto &line  = m_lines[at.m_line];
    auto charIndex = getCharacterIndex(at);

    if (charIndex >= (int32_t)line.size())
        return at;

    bool previousSpace = (bool)isspace(line[charIndex].m_char);
    auto charStart    = (PaletteIndex)line[charIndex].m_colorIndex;
    while (charIndex < (int32_t)line.size()) {
        auto c = line[charIndex].m_char;
        auto d = UTF8CharLength(c);
        if (charStart != (PaletteIndex)line[charIndex].m_colorIndex)
            break;

        if (previousSpace != !!isspace(c)) {
            if (isspace(c))
                while (charIndex < (int32_t)line.size() && isspace(line[charIndex].m_char))
                    ++charIndex;
            break;
        }
        charIndex += d;
    }
    return Coordinates(aFrom.m_line, getCharacterColumn(aFrom.m_line, charIndex));
}

TextEditor::Coordinates TextEditor::findNextWord(const Coordinates &aFrom) const {
    Coordinates at = aFrom;
    if (at.m_line >= (int32_t)m_lines.size())
        return at;

    // skip to the next non-word character
    auto charIndex = getCharacterIndex(aFrom);
    bool isword = false;
    bool skip   = false;
    if (charIndex < (int32_t)m_lines[at.m_line].size()) {
        auto &line = m_lines[at.m_line];
        isword     = isalnum(line[charIndex].m_char);
        skip       = isword;
    }

    while (!isword || skip) {
        if (at.m_line >= m_lines.size()) {
            auto l = std::max(0, (int32_t)m_lines.size() - 1);
            return Coordinates(l, getLineMaxColumn(l));
        }

        auto &line = m_lines[at.m_line];
        if (charIndex < (int32_t)line.size()) {
            isword = isalnum(line[charIndex].m_char);

            if (isword && !skip)
                return Coordinates(at.m_line, getCharacterColumn(at.m_line, charIndex));

            if (!isword)
                skip = false;

            charIndex++;
        } else {
            charIndex = 0;
            ++at.m_line;
            skip   = false;
            isword = false;
        }
    }

    return at;
}

int32_t TextEditor::getCharacterIndex(const Coordinates &aCoordinates) const {
    if (aCoordinates.m_line >= m_lines.size())
        return -1;
    auto &line = m_lines[aCoordinates.m_line];
    int32_t c      = 0;
    int32_t i      = 0;
    for (; i < line.size() && c < aCoordinates.m_column;) {
        if (line[i].m_char == '\t')
            c = (c / m_tabSize) * m_tabSize + m_tabSize;
        else
            ++c;
        i += UTF8CharLength(line[i].m_char);
    }
    return i;
}

int32_t TextEditor::getCharacterColumn(int32_t aLine, int32_t aIndex) const {
    if (aLine >= m_lines.size())
        return 0;
    auto &line = m_lines[aLine];
    int32_t col    = 0;
    int32_t i      = 0;
    while (i < aIndex && i < (int32_t)line.size()) {
        auto c = line[i].m_char;
        i += UTF8CharLength(c);
        if (c == '\t')
            col = (col / m_tabSize) * m_tabSize + m_tabSize;
        else
            col++;
    }
    return col;
}

int32_t TextEditor::getStringCharacterCount(std::string str) const {
    if (str.empty())
        return 0;
    int32_t c      = 0;
    for (uint32_t i = 0; i < str.size(); c++)
        i += UTF8CharLength(str[i]);
    return c;
}

int32_t TextEditor::getLineCharacterCount(int32_t aLine) const {
    if (aLine >= m_lines.size())
        return 0;
    auto &line = m_lines[aLine];
    int32_t c      = 0;
    for (uint32_t i = 0; i < line.size(); c++)
        i += UTF8CharLength(line[i].m_char);
    return c;
}

uint64_t TextEditor::getLineByteCount(int32_t aLine) const {
    if (aLine >= m_lines.size())
        return 0;
    auto &line = m_lines[aLine];
    return line.size();
}

int32_t TextEditor::getLineMaxColumn(int32_t aLine) const {
    if (aLine >= m_lines.size())
        return 0;
    auto &line = m_lines[aLine];
    int32_t col    = 0;
    for (uint32_t i = 0; i < line.size();) {
        auto c = line[i].m_char;
        if (c == '\t')
            col = (col / m_tabSize) * m_tabSize + m_tabSize;
        else
            col++;
        i += UTF8CharLength(c);
    }
    return col;
}

bool TextEditor::isOnWordBoundary(const Coordinates &aAt) const {
    if (aAt.m_line >= (int32_t)m_lines.size() || aAt.m_column == 0)
        return true;

    auto &line  = m_lines[aAt.m_line];
    auto charIndex = getCharacterIndex(aAt);
    if (charIndex >= (int32_t)line.size())
        return true;

    if (m_colorizerEnabled)
        return line[charIndex].m_colorIndex != line[size_t(charIndex - 1)].m_colorIndex;

    return isspace(line[charIndex].m_char) != isspace(line[charIndex - 1].m_char);
}

void TextEditor::removeLine(int32_t aStart, int32_t aEnd) {
    assert(!m_readOnly);
    assert(aEnd >= aStart);
    assert(m_lines.size() > (size_t)(aEnd - aStart));

    ErrorMarkers errorMarker;
    for (auto &i : m_errorMarkers) {
        ErrorMarkers::value_type e(i.first >= aStart ? i.first - 1 : i.first, i.second);
        if (e.first >= aStart && e.first <= aEnd)
            continue;
        errorMarker.insert(e);
    }
    m_errorMarkers = std::move(errorMarker);

    Breakpoints breakpoints;
    for (auto i : m_breakpoints) {
        if (i >= aStart && i <= aEnd)
            continue;
        breakpoints.insert(i >= aStart ? i - 1 : i);
    }
    m_breakpoints = std::move(breakpoints);

    m_lines.erase(m_lines.begin() + aStart, m_lines.begin() + aEnd);
    assert(!m_lines.empty());

    m_textChanged = true;
}

void TextEditor::removeLine(int32_t aIndex) {
    assert(!m_readOnly);
    assert(m_lines.size() > 1);

    ErrorMarkers errorMarkers;
    for (auto &i : m_errorMarkers) {
        ErrorMarkers::value_type e(i.first > aIndex ? i.first - 1 : i.first, i.second);
        if (e.first - 1 == aIndex)
            continue;
        errorMarkers.insert(e);
    }
    m_errorMarkers = std::move(errorMarkers);

    Breakpoints breakpoints;
    for (auto i : m_breakpoints) {
        if (i == aIndex)
            continue;
        breakpoints.insert(i >= aIndex ? i - 1 : i);
    }
    m_breakpoints = std::move(breakpoints);

    m_lines.erase(m_lines.begin() + aIndex);
    assert(!m_lines.empty());

    m_textChanged = true;
}

TextEditor::Line &TextEditor::insertLine(int32_t aIndex) {
    auto &result = *m_lines.insert(m_lines.begin() + aIndex, Line());

    ErrorMarkers errorMarkers;
    for (auto &i : m_errorMarkers)
        errorMarkers.insert(ErrorMarkers::value_type(i.first >= aIndex ? i.first + 1 : i.first, i.second));
    m_errorMarkers = std::move(errorMarkers);

    Breakpoints breakpoints;
    for (auto i : m_breakpoints)
        breakpoints.insert(i >= aIndex ? i + 1 : i);
    m_breakpoints = std::move(breakpoints);

    return result;
}

std::string TextEditor::getWordUnderCursor() const {
    auto c = getCursorPosition();
    return getWordAt(c);
}

std::string TextEditor::getWordAt(const Coordinates &aCoords) const {
    auto start = findWordStart(aCoords);
    auto end   = findWordEnd(aCoords);

    std::string r;

    auto istart = getCharacterIndex(start);
    auto iend   = getCharacterIndex(end);

    for (auto it = istart; it < iend; ++it)
        r.push_back(m_lines[aCoords.m_line][it].m_char);

    return r;
}

ImU32 TextEditor::getGlyphColor(const Glyph &aGlyph) const {
    if (!m_colorizerEnabled)
        return m_palette[(int32_t)PaletteIndex::Default];
    if (aGlyph.m_globalDocComment)
        return m_palette[(int32_t)PaletteIndex::GlobalDocComment];
    if (aGlyph.m_docComment)
        return m_palette[(int32_t)PaletteIndex::DocComment];
    if (aGlyph.m_comment)
        return m_palette[(int32_t)PaletteIndex::Comment];
    if (aGlyph.m_multiLineComment)
        return m_palette[(int32_t)PaletteIndex::MultiLineComment];
    if (aGlyph.m_deactivated)
        return m_palette[(int32_t)PaletteIndex::PreprocessorDeactivated];
    const auto color = m_palette[(int32_t)aGlyph.m_colorIndex];
    if (aGlyph.m_preprocessor) {
        const auto preprocessorColor = m_palette[(int32_t)PaletteIndex::Preprocessor];
        const int32_t c0       = ((preprocessorColor & 0xff) + (color & 0xff)) / 2;
        const int32_t c1       = (((preprocessorColor >> 8) & 0xff) + ((color >> 8) & 0xff)) / 2;
        const int32_t c2       = (((preprocessorColor >> 16) & 0xff) + ((color >> 16) & 0xff)) / 2;
        const int32_t c3       = (((preprocessorColor >> 24) & 0xff) + ((color >> 24) & 0xff)) / 2;
        return ImU32(c0 | (c1 << 8) | (c2 << 16) | (c3 << 24));
    }
    return color;
}

void TextEditor::handleKeyboardInputs() {
    ImGuiIO &io   = ImGui::GetIO();

    // command => Ctrl
    // control => Super
    // option  => Alt

    auto shift    = io.KeyShift;
    auto left     = ImGui::IsKeyPressed(ImGuiKey_LeftArrow);
    auto right    = ImGui::IsKeyPressed(ImGuiKey_RightArrow);
    auto up       = ImGui::IsKeyPressed(ImGuiKey_UpArrow);
    auto down     = ImGui::IsKeyPressed(ImGuiKey_DownArrow);
    auto ctrl     = io.KeyCtrl;
    auto alt      = io.KeyAlt;
    auto home     = io.ConfigMacOSXBehaviors ? io.KeySuper && left : ImGui::IsKeyPressed(ImGuiKey_Home);
    auto end      = io.ConfigMacOSXBehaviors ? io.KeySuper && right : ImGui::IsKeyPressed(ImGuiKey_End);
    auto top      = io.ConfigMacOSXBehaviors ? io.KeySuper && up : ctrl && ImGui::IsKeyPressed(ImGuiKey_Home);
    auto bottom   = io.ConfigMacOSXBehaviors ? io.KeySuper && down : ctrl && ImGui::IsKeyPressed(ImGuiKey_End);
    auto pageUp   = io.ConfigMacOSXBehaviors ? ctrl && up : ImGui::IsKeyPressed(ImGuiKey_PageUp);
    auto pageDown = io.ConfigMacOSXBehaviors ? ctrl && down : ImGui::IsKeyPressed(ImGuiKey_PageDown);

    if (ImGui::IsWindowFocused()) {
        if (ImGui::IsWindowHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
        // ImGui::CaptureKeyboardFromApp(true);

        io.WantCaptureKeyboard = true;
        io.WantTextInput       = true;

        bool handledKeyEvent = true;

        if (!isReadOnly() && ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_Z))
            undo();
        else if (!isReadOnly() && !ctrl && !shift && alt && ImGui::IsKeyPressed(ImGuiKey_Backspace))
            undo();
        else if (!isReadOnly() && ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_Y))
            redo();
        else if (!ctrl && !alt && up)
            moveUp(1, shift);
        else if (!ctrl && !alt && down)
            moveDown(1, shift);
        else if (!alt && left)
            moveLeft(1, shift, ctrl);
        else if (!alt && right)
            moveRight(1, shift, ctrl);
        else if (!alt && pageUp)
            moveUp(getPageSize() - 4, shift);
        else if (!alt && pageDown)
            moveDown(getPageSize() - 4, shift);
        else if (!alt && top)
            moveTop(shift);
        else if (!alt && bottom)
            moveBottom(shift);
        else if (!ctrl && !alt && home)
            moveHome(shift);
        else if (!ctrl && !alt && end)
            moveEnd(shift);
        else if (!isReadOnly() && !ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_Delete))
            doDelete();
        else if (!isReadOnly() && ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            auto wordStart = getCursorPosition();
            moveRight();
            auto wordEnd = findWordEnd(getCursorPosition());
            setSelection(wordStart, wordEnd);
            backspace();
        }
        else if (!isReadOnly() && !ctrl && !alt && ImGui::IsKeyPressed(ImGuiKey_Backspace))
            backspace();
        else if (!isReadOnly() && ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
            auto wordEnd = getCursorPosition();
            moveLeft();
            auto wordStart = findWordStart(getCursorPosition());
            setSelection(wordStart, wordEnd);
            backspace();
        }
        else if (!ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_Insert))
            m_overwrite = !m_overwrite;
        else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_Insert))
            copy();
        else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_C))
            copy();
        else if (!isReadOnly() && !ctrl && shift && !alt && ImGui::IsKeyPressed(ImGuiKey_Insert))
            paste();
        else if (!isReadOnly() && ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_V))
            paste();
        else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_X))
            cut();
        else if (!ctrl && shift && !alt && ImGui::IsKeyPressed(ImGuiKey_Delete))
            cut();
        else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGuiKey_A))
            selectAll();
        else if (!isReadOnly() && !ctrl && !shift && !alt && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)))
            enterCharacter('\n', false);
        else if (!isReadOnly() && !ctrl && !alt && ImGui::IsKeyPressed(ImGuiKey_Tab))
            enterCharacter('\t', shift);
        else if (!ctrl && !alt && !shift && ImGui::IsKeyPressed(ImGuiKey_F3))
            m_findReplaceHandler.findMatch(this, true);
        else if (!ctrl && !alt && shift && ImGui::IsKeyPressed(ImGuiKey_F3))
            m_findReplaceHandler.findMatch(this, false);
        else if (!ctrl && alt && !shift && ImGui::IsKeyPressed(ImGuiKey_C))
            m_findReplaceHandler.setMatchCase(this, !m_findReplaceHandler.getMatchCase());
        else if (!ctrl && alt && !shift && ImGui::IsKeyPressed(ImGuiKey_R))
            m_findReplaceHandler.setFindRegEx(this, !m_findReplaceHandler.getFindRegEx());
        else if (!ctrl && alt && !shift && ImGui::IsKeyPressed(ImGuiKey_W))
            m_findReplaceHandler.setWholeWord(this, !m_findReplaceHandler.getWholeWord());
        else
            handledKeyEvent = false;

        if (handledKeyEvent)
            resetCursorBlinkTime();

        if (!isReadOnly() && !io.InputQueueCharacters.empty()) {
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
    auto shift  = io.KeyShift;
    auto ctrl   = io.ConfigMacOSXBehaviors ? io.KeyAlt : io.KeyCtrl;
    auto alt    = io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;

    if (ImGui::IsWindowHovered()) {
        if (!alt) {
            auto click       = ImGui::IsMouseClicked(0);
            auto doubleClick = ImGui::IsMouseDoubleClicked(0);
            auto t           = ImGui::GetTime();
            auto tripleClick = click && !doubleClick && (m_lastClick != -1.0f && (t - m_lastClick) < io.MouseDoubleClickTime);

            /*
            Left mouse button triple click
            */

            if (tripleClick) {
                if (!ctrl) {
                    m_state.m_cursorPosition = m_interactiveStart = m_interactiveEnd = screenPosToCoordinates(
                            ImGui::GetMousePos());
                    m_selectionMode                                               = SelectionMode::Line;
                    setSelection(m_interactiveStart, m_interactiveEnd, m_selectionMode);
                }

                m_lastClick = -1.0f;
                resetCursorBlinkTime();
            }

            /*
            Left mouse button double click
            */

            else if (doubleClick) {
                if (!ctrl) {
                    m_state.m_cursorPosition = m_interactiveStart = m_interactiveEnd = screenPosToCoordinates(
                            ImGui::GetMousePos());
                    if (m_selectionMode == SelectionMode::Line)
                        m_selectionMode = SelectionMode::Normal;
                    else
                        m_selectionMode = SelectionMode::Word;
                    setSelection(m_interactiveStart, m_interactiveEnd, m_selectionMode);
                }

                m_lastClick = (float)ImGui::GetTime();
                resetCursorBlinkTime();
            }

            /*
            Left mouse button click
            */
            else if (click) {
                if (ctrl) {
                    m_state.m_cursorPosition = m_interactiveStart = m_interactiveEnd = screenPosToCoordinates(
                            ImGui::GetMousePos());
                    m_selectionMode = SelectionMode::Word;
                } else if (shift) {
                    m_selectionMode = SelectionMode::Normal;
                    m_interactiveEnd = screenPosToCoordinates(ImGui::GetMousePos());
                } else {
                    m_state.m_cursorPosition = m_interactiveStart = m_interactiveEnd = screenPosToCoordinates(
                            ImGui::GetMousePos());
                    m_selectionMode = SelectionMode::Normal;
                }
                setSelection(m_interactiveStart, m_interactiveEnd, m_selectionMode);
                resetCursorBlinkTime();

                m_lastClick = (float)ImGui::GetTime();
            }
            // Mouse left button dragging (=> update selection)
            else if (ImGui::IsMouseDragging(0) && ImGui::IsMouseDown(0)) {
                io.WantCaptureMouse    = true;
                m_state.m_cursorPosition = m_interactiveEnd = screenPosToCoordinates(ImGui::GetMousePos());
                setSelection(m_interactiveStart, m_interactiveEnd, m_selectionMode);
                resetCursorBlinkTime();
            }
        }
    }
}

void TextEditor::Render() {
    /* Compute m_charAdvance regarding scaled font size (Ctrl + mouse wheel)*/
    const float fontSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, "#", nullptr, nullptr).x;
    m_charAdvance         = ImVec2(fontSize, ImGui::GetTextLineHeightWithSpacing() * m_lineSpacing);

    /* Update palette with the current alpha from style */
    for (int32_t i = 0; i < (int32_t)PaletteIndex::Max; ++i) {
        auto color = ImGui::ColorConvertU32ToFloat4(s_paletteBase[i]);
        color.w *= ImGui::GetStyle().Alpha;
        m_palette[i] = ImGui::ColorConvertFloat4ToU32(color);
    }

    assert(m_lineBuffer.empty());

    auto contentSize = ImGui::GetCurrentWindowRead()->ContentRegionRect.Max - ImGui::GetWindowPos() - ImVec2(0, m_topMargin);
    auto drawList    = ImGui::GetWindowDrawList();
    float longest(m_textStart);

    if (m_scrollToTop) {
        m_scrollToTop = false;
        ImGui::SetScrollY(0.f);
    }

    if (m_scrollToBottom && ImGui::GetScrollMaxY() > ImGui::GetScrollY()) {
        m_scrollToBottom = false;
        ImGui::SetScrollY(ImGui::GetScrollMaxY());
    }

    ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos() + ImVec2(0, m_topMargin);
    auto scrollX           = ImGui::GetScrollX();
    auto scrollY           = ImGui::GetScrollY();

    auto lineNo        = (int32_t)(std::floor(scrollY / m_charAdvance.y));// + linesAdded);
    auto globalLineMax = (int32_t)m_lines.size();
    auto lineMax       = std::max(0, std::min((int32_t)m_lines.size() - 1, lineNo + (int32_t)std::ceil((scrollY + contentSize.y) / m_charAdvance.y)));

    // Deduce m_textStart by evaluating m_lines size (global lineMax) plus two spaces as text width
    char buf[16];

    if (m_showLineNumbers)
        snprintf(buf, 16, " %d ", globalLineMax);
    else
        buf[0] = '\0';
    m_textStart = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf, nullptr, nullptr).x + m_leftMargin;

    if (!m_lines.empty()) {
        float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ", nullptr, nullptr).x;

        while (lineNo <= lineMax) {
            ImVec2 lineStartScreenPos = ImVec2(cursorScreenPos.x, cursorScreenPos.y + lineNo * m_charAdvance.y);
            ImVec2 textScreenPos      = ImVec2(lineStartScreenPos.x + m_textStart, lineStartScreenPos.y);

            auto &line    = m_lines[lineNo];
            longest       = std::max(m_textStart +
                                             textDistanceToLineStart(Coordinates(lineNo, getLineMaxColumn(lineNo))), longest);
            auto columnNo = 0;
            Coordinates lineStartCoord(lineNo, 0);
            Coordinates lineEndCoord(lineNo, getLineMaxColumn(lineNo));

            // Draw selection for the current line
            float selectionStart = -1.0f;
            float selectionEnd  = -1.0f;

            assert(m_state.m_selectionStart <= m_state.m_selectionEnd);
            if (m_state.m_selectionStart <= lineEndCoord)
                selectionStart = m_state.m_selectionStart > lineStartCoord ? textDistanceToLineStart(m_state.m_selectionStart) : 0.0f;
            if (m_state.m_selectionEnd > lineStartCoord)
                selectionEnd = textDistanceToLineStart(
                        m_state.m_selectionEnd < lineEndCoord ? m_state.m_selectionEnd : lineEndCoord);

            if (m_state.m_selectionEnd.m_line > lineNo)
                selectionEnd += m_charAdvance.x;

            if (selectionStart != -1 && selectionEnd != -1 && selectionStart < selectionEnd) {
                ImVec2 vstart(lineStartScreenPos.x + m_textStart + selectionStart, lineStartScreenPos.y);
                ImVec2 vend(lineStartScreenPos.x + m_textStart + selectionEnd, lineStartScreenPos.y + m_charAdvance.y);
                drawList->AddRectFilled(vstart, vend, m_palette[(int32_t)PaletteIndex::Selection]);
            }

            auto start = ImVec2(lineStartScreenPos.x + scrollX, lineStartScreenPos.y);

            // Draw error markers
            auto errorIt = m_errorMarkers.find(lineNo + 1);
            if (errorIt != m_errorMarkers.end()) {
                auto end = ImVec2(lineStartScreenPos.x + contentSize.x + 2.0f * scrollX, lineStartScreenPos.y + m_charAdvance.y);
                drawList->AddRectFilled(start, end, m_palette[(int32_t)PaletteIndex::ErrorMarker]);

                if (ImGui::IsMouseHoveringRect(lineStartScreenPos, end)) {
                    ImGui::BeginTooltip();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
                    ImGui::Text("Error at line %d:", errorIt->first);
                    ImGui::PopStyleColor();
                    ImGui::Separator();
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.2f, 1.0f));
                    ImGui::Text("%s", errorIt->second.c_str());
                    ImGui::PopStyleColor();
                    ImGui::EndTooltip();
                }
            }

            // Draw line number (right aligned)
            if (m_showLineNumbers) {
                snprintf(buf, 16, "%d  ", lineNo + 1);

                auto lineNoWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf, nullptr, nullptr).x;
                drawList->AddText(ImVec2(lineStartScreenPos.x + m_textStart - lineNoWidth, lineStartScreenPos.y), m_palette[(int32_t)PaletteIndex::LineNumber], buf);
            }

            // Draw breakpoints
            if (m_breakpoints.count(lineNo + 1) != 0) {
                auto end = ImVec2(lineStartScreenPos.x + contentSize.x + 2.0f * scrollX, lineStartScreenPos.y + m_charAdvance.y);
                drawList->AddRectFilled(start + ImVec2(m_textStart, 0), end, m_palette[(int32_t)PaletteIndex::Breakpoint]);

                drawList->AddCircleFilled(start + ImVec2(0, m_charAdvance.y) / 2, m_charAdvance.y / 3, m_palette[(int32_t)PaletteIndex::Breakpoint]);
                drawList->AddCircle(start + ImVec2(0, m_charAdvance.y) / 2, m_charAdvance.y / 3, m_palette[(int32_t)PaletteIndex::Default]);
            }

            if (m_state.m_cursorPosition.m_line == lineNo && m_showCursor) {
                bool focused = ImGui::IsWindowFocused();
                ImGuiViewport *viewport = ImGui::GetWindowViewport();

                // Highlight the current line (where the cursor is)
                if (!hasSelection()) {
                    auto end = ImVec2(start.x + contentSize.x + scrollX, start.y + m_charAdvance.y);
                    drawList->AddRectFilled(start, end, m_palette[(int32_t)(focused ? PaletteIndex::CurrentLineFill : PaletteIndex::CurrentLineFillInactive)]);
                    drawList->AddRect(start, end, m_palette[(int32_t)PaletteIndex::CurrentLineEdge], 1.0f);
                }

                // Render the cursor
                if (focused) {
                    auto timeEnd = ImGui::GetTime() * 1000;
                    auto elapsed = timeEnd - m_startTime;
                    if (elapsed > s_cursorBlinkOnTime) {
                        float width = 1.0f;
                        auto charIndex = getCharacterIndex(m_state.m_cursorPosition);
                        float toLineStart    = textDistanceToLineStart(m_state.m_cursorPosition);

                        if (m_overwrite && charIndex < (int32_t)line.size()) {
                            auto c = line[charIndex].m_char;
                            if (c == '\t') {
                                auto x = (1.0f + std::floor((1.0f + toLineStart) / (float(m_tabSize) * spaceSize))) * (float(m_tabSize) * spaceSize);
                                width  = x - toLineStart;
                            } else {
                                char buf2[2];
                                buf2[0] = line[charIndex].m_char;
                                buf2[1] = '\0';
                                width   = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf2).x;
                            }
                        }
                        ImVec2 charStart(textScreenPos.x + toLineStart, lineStartScreenPos.y);
                        ImVec2 charEnd(textScreenPos.x + toLineStart + width, lineStartScreenPos.y + m_charAdvance.y);
                        drawList->AddRectFilled(charStart, charEnd, m_palette[(int32_t)PaletteIndex::Cursor]);
                        if (elapsed > s_cursorBlinkInterval)
                            m_startTime = timeEnd;
                    }
                }
            }

            // Render colorized text
            auto prevColor = line.empty() ? m_palette[(int32_t)PaletteIndex::Default] : getGlyphColor(line[0]);
            ImVec2 bufferOffset;

            for (int32_t i = 0; i < line.size();) {
                auto &glyph = line[i];
                auto color  = getGlyphColor(glyph);

                if ((color != prevColor || glyph.m_char == '\t' || glyph.m_char == ' ') && !m_lineBuffer.empty()) {
                    const ImVec2 newOffset(textScreenPos.x + bufferOffset.x, textScreenPos.y + bufferOffset.y);
                    drawList->AddText(newOffset, prevColor, m_lineBuffer.c_str());
                    auto textSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, m_lineBuffer.c_str(), nullptr, nullptr);
                    bufferOffset.x += textSize.x;
                    m_lineBuffer.clear();
                }
                prevColor = color;

                if (glyph.m_char == '\t') {
                    auto oldX      = bufferOffset.x;
                    bufferOffset.x = (1.0f + std::floor((1.0f + bufferOffset.x) / (float(m_tabSize) * spaceSize))) * (float(m_tabSize) * spaceSize);
                    ++i;

                    if (m_showWhitespaces) {
                        const auto s  = ImGui::GetFontSize();
                        const auto x1 = textScreenPos.x + oldX + 1.0f;
                        const auto x2 = textScreenPos.x + bufferOffset.x - 1.0f;
                        const auto y  = textScreenPos.y + bufferOffset.y + s * 0.5f;
                        const ImVec2 p1(x1, y);
                        const ImVec2 p2(x2, y);
                        const ImVec2 p3(x2 - s * 0.2f, y - s * 0.2f);
                        const ImVec2 p4(x2 - s * 0.2f, y + s * 0.2f);
                        drawList->AddLine(p1, p2, 0x90909090);
                        drawList->AddLine(p2, p3, 0x90909090);
                        drawList->AddLine(p2, p4, 0x90909090);
                    }
                } else if (glyph.m_char == ' ') {
                    if (m_showWhitespaces) {
                        const auto s = ImGui::GetFontSize();
                        const auto x = textScreenPos.x + bufferOffset.x + spaceSize * 0.5f;
                        const auto y = textScreenPos.y + bufferOffset.y + s * 0.5f;
                        drawList->AddCircleFilled(ImVec2(x, y), 1.5f, 0x80808080, 4);
                    }
                    bufferOffset.x += spaceSize;
                    i++;
                } else {
                    auto l = UTF8CharLength(glyph.m_char);
                    while (l-- > 0)
                        m_lineBuffer.push_back(line[i++].m_char);
                }
                ++columnNo;
            }

            if (!m_lineBuffer.empty()) {
                const ImVec2 newOffset(textScreenPos.x + bufferOffset.x, textScreenPos.y + bufferOffset.y);
                drawList->AddText(newOffset, prevColor, m_lineBuffer.c_str());
                m_lineBuffer.clear();
            }

            ++lineNo;
        }
        if (lineNo < m_lines.size() && ImGui::GetScrollMaxX() > 0.0f)
            longest       = std::max(m_textStart +
                                             textDistanceToLineStart(Coordinates(lineNo, getLineMaxColumn(lineNo))), longest);

        // Draw a tooltip on known identifiers/preprocessor symbols
        if (ImGui::IsMousePosValid()) {
            auto id = getWordAt(screenPosToCoordinates(ImGui::GetMousePos()));
            if (!id.empty()) {
                auto it = mL_languageDefinition.m_identifiers.find(id);
                if (it != mL_languageDefinition.m_identifiers.end() && !it->second.m_declaration.empty()) {
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted(it->second.m_declaration.c_str());
                    ImGui::EndTooltip();
                } else {
                    auto pi = mL_languageDefinition.m_preprocIdentifiers.find(id);
                    if (pi != mL_languageDefinition.m_preprocIdentifiers.end() && !pi->second.m_declaration.empty()) {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted(pi->second.m_declaration.c_str());
                        ImGui::EndTooltip();
                    }
                }
            }
        }
    }

    ImGui::Dummy(ImVec2((longest + 2), m_lines.size() * m_charAdvance.y));

    if (m_scrollToCursor) {
        ensureCursorVisible();
        m_scrollToCursor = false;
    }

    ImGuiPopupFlags_ popup_flags = ImGuiPopupFlags_None;
    ImGuiContext& g = *GImGui;
    auto oldTopMargin = m_topMargin;
    auto popupStack = g.OpenPopupStack;
    if (popupStack.Size > 0) {
        for (int32_t n = 0; n < popupStack.Size; n++){
            auto window = popupStack[n].Window;
            if (window != nullptr) {
                if (window->Size.x == m_findReplaceHandler.getFindWindowSize().x &&
                    window->Size.y == m_findReplaceHandler.getFindWindowSize().y &&
                    window->Pos.x == m_findReplaceHandler.getFindWindowPos().x &&
                    window->Pos.y == m_findReplaceHandler.getFindWindowPos().y) {
                    m_topMargin = m_findReplaceHandler.getFindWindowSize().y;
                }
            }
        }
    } else {
        m_topMargin = 0;
    }


    if (m_topMargin != oldTopMargin) {
        static float linesAdded = 0;
        static float pixelsAdded = 0;
        static float savedScrollY = 0;
        static float shiftedScrollY = 0;
        if (oldTopMargin == 0)
            savedScrollY = ImGui::GetScrollY();
        auto window = ImGui::GetCurrentWindow();
        auto maxScroll = window->ScrollMax.y;
        if (maxScroll > 0) {
            float lineCount;
            float pixelCount;
            if (m_topMargin > oldTopMargin) {
                pixelCount = m_topMargin - oldTopMargin;
                lineCount = pixelCount / m_charAdvance.y;
            } else if (m_topMargin > 0) {
                pixelCount = oldTopMargin - m_topMargin;
                lineCount = pixelCount / m_charAdvance.y;
            } else {
                pixelCount = oldTopMargin;
                lineCount = std::round(linesAdded);
            }
            auto state = m_state;
            auto oldScrollY = ImGui::GetScrollY();
            int32_t lineCountInt;

            if (m_topMargin > oldTopMargin) {
                lineCountInt = std::round(lineCount + linesAdded - std::floor(linesAdded));
            } else
                lineCountInt = std::round(lineCount);
            for (int32_t i = 0; i < lineCountInt; i++) {
                if (m_topMargin > oldTopMargin)
                    m_lines.insert(m_lines.begin() + m_lines.size(), Line());
                else
                    m_lines.erase(m_lines.begin() + m_lines.size() - 1);
            }
            if (m_topMargin > oldTopMargin) {
                linesAdded += lineCount;
                pixelsAdded += pixelCount;
            } else if (m_topMargin > 0) {
                linesAdded -= lineCount;
                pixelsAdded -= pixelCount;
            } else {
                linesAdded = 0;
                pixelsAdded = 0;
            }
            if (oldScrollY + pixelCount < maxScroll) {
                if (m_topMargin > oldTopMargin)
                    shiftedScrollY = oldScrollY + pixelCount;
                else if (m_topMargin > 0)
                    shiftedScrollY = oldScrollY - pixelCount;
                else if (ImGui::GetScrollY() == shiftedScrollY)
                    shiftedScrollY = savedScrollY;
                else
                    shiftedScrollY = ImGui::GetScrollY() - pixelCount;
                ImGui::SetScrollY(shiftedScrollY);
            } else {
                if (m_topMargin > oldTopMargin)
                    m_scrollToBottom = true;
            }
            m_state = state;
        }
    }
}

void TextEditor::render(const char *aTitle, const ImVec2 &aSize, bool aBorder) {
    m_withinRender          = true;
    m_textChanged           = false;
    m_cursorPositionChanged = false;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(m_palette[(int32_t)PaletteIndex::Background]));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    if (!m_ignoreImGuiChild)
        ImGui::BeginChild(aTitle, aSize, aBorder, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoMove);

    if (m_handleKeyboardInputs) {
        handleKeyboardInputs();
        ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, false);
    }

    if (m_handleMouseInputs)
        handleMouseInputs();

    colorizeInternal();
    Render();

    if (m_handleKeyboardInputs)
        ImGui::PopItemFlag();

    if (!m_ignoreImGuiChild)
        ImGui::EndChild();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    m_withinRender = false;
}

void TextEditor::setText(const std::string &aText) {
    m_lines.clear();
    m_lines.emplace_back(Line());
    for (auto chr : aText) {
        if (chr == '\r') {
            // ignore the carriage return character
        } else if (chr == '\n')
            m_lines.emplace_back(Line());
        else {
            m_lines.back().emplace_back(Glyph(chr, PaletteIndex::Default));
        }
    }

    m_textChanged = true;
    m_scrollToTop = true;

    m_undoBuffer.clear();
    m_undoIndex = 0;

    colorize();
}

void TextEditor::setTextLines(const std::vector<std::string> &aLines) {
    m_lines.clear();

    if (aLines.empty()) {
        m_lines.emplace_back(Line());
    } else {
        m_lines.resize(aLines.size());

        for (size_t i = 0; i < aLines.size(); ++i) {
            const std::string &aLine = aLines[i];

            m_lines[i].reserve(aLine.size());
            for (size_t j = 0; j < aLine.size(); ++j)
                m_lines[i].emplace_back(Glyph(aLine[j], PaletteIndex::Default));
        }
    }

    m_textChanged = true;
    m_scrollToTop = true;

    m_undoBuffer.clear();
    m_undoIndex = 0;

    colorize();
}

void TextEditor::enterCharacter(ImWchar aChar, bool aShift) {
    assert(!m_readOnly);

    UndoRecord u;

    u.m_before = m_state;

    resetCursorBlinkTime();

    if (hasSelection()) {
        if (aChar == '\t' && m_state.m_selectionStart.m_line != m_state.m_selectionEnd.m_line) {

            auto start       = m_state.m_selectionStart;
            auto end         = m_state.m_selectionEnd;
            auto originalEnd = end;

            if (start > end)
                std::swap(start, end);
            start.m_column = 0;

            if (end.m_column == 0 && end.m_line > 0)
                --end.m_line;
            if (end.m_line >= (int32_t)m_lines.size())
                end.m_line = m_lines.empty() ? 0 : (int32_t)m_lines.size() - 1;
            end.m_column = getLineMaxColumn(end.m_line);

            u.m_removedStart = start;
            u.m_removedEnd   = end;
            u.m_removed      = getText(start, end);

            bool modified = false;

            for (int32_t i = start.m_line; i <= end.m_line; i++) {
                auto &line = m_lines[i];
                if (aShift) {
                    if (!line.empty()) {
                        if (line.front().m_char == '\t') {
                            line.erase(line.begin());
                            modified = true;
                        } else {
                            for (int32_t j = 0; j < m_tabSize && !line.empty() && line.front().m_char == ' '; j++) {
                                line.erase(line.begin());
                                modified = true;
                            }
                        }
                    }
                } else {
                    for (int32_t j = start.m_column % m_tabSize; j < m_tabSize; j++)
                        line.insert(line.begin(), Glyph(' ', PaletteIndex::Background));
                    modified = true;
                }
            }

            if (modified) {
                start = Coordinates(start.m_line, getCharacterColumn(start.m_line, 0));
                Coordinates rangeEnd;
                if (originalEnd.m_column != 0) {
                    end      = Coordinates(end.m_line, getLineMaxColumn(end.m_line));
                    rangeEnd = end;
                    u.m_added = getText(start, end);
                } else {
                    end      = Coordinates(originalEnd.m_line, 0);
                    rangeEnd = Coordinates(end.m_line - 1, getLineMaxColumn(end.m_line - 1));
                    u.m_added = getText(start, rangeEnd);
                }

                u.m_addedStart = start;
                u.m_addedEnd   = rangeEnd;
                u.m_after      = m_state;

                m_state.m_selectionStart = start;
                m_state.m_selectionEnd   = end;
                addUndo(u);

                m_textChanged = true;

                ensureCursorVisible();
            }

            return;
        }    // c == '\t'
        else {
            u.m_removed      = getSelectedText();
            u.m_removedStart = m_state.m_selectionStart;
            u.m_removedEnd   = m_state.m_selectionEnd;
            deleteSelection();
        }
    }    // hasSelection

    auto coord    = getActualCursorCoordinates();
    u.m_addedStart = coord;

    assert(!m_lines.empty());

    if (aChar == '\n') {
        insertLine(coord.m_line + 1);
        auto &line    = m_lines[coord.m_line];
        auto &newLine = m_lines[coord.m_line + 1];

        if (mL_languageDefinition.m_autoIndentation)
            for (size_t it = 0; it < line.size() && isascii(line[it].m_char) && isblank(line[it].m_char); ++it)
                newLine.push_back(line[it]);

        const size_t whitespaceSize = newLine.size();
        int32_t charStart                  = 0;
        int32_t charPos                    = 0;
        auto charIndex                 = getCharacterIndex(coord);
        if (charIndex < whitespaceSize && mL_languageDefinition.m_autoIndentation) {
            charStart = (int32_t) whitespaceSize;
            charPos = charIndex;
        } else {
            charStart = charIndex;
            charPos = (int32_t) whitespaceSize;
        }
        newLine.insert(newLine.end(), line.begin() + charStart, line.end());
        line.erase(line.begin() + charStart, line.begin() + line.size());
        setCursorPosition(Coordinates(coord.m_line + 1, getCharacterColumn(coord.m_line + 1, charPos)));
        u.m_added = (char)aChar;
    } else if (aChar == '\t') {
        auto &line  = m_lines[coord.m_line];
        auto charIndex = getCharacterIndex(coord);

        if (!aShift) {
            auto spacesToInsert = m_tabSize - (charIndex % m_tabSize);
            for (int32_t j = 0; j < spacesToInsert; j++)
                line.insert(line.begin() + charIndex, Glyph(' ', PaletteIndex::Background));
            setCursorPosition(Coordinates(coord.m_line, getCharacterColumn(coord.m_line, charIndex + spacesToInsert)));
        } else {
            auto spacesToRemove = (charIndex % m_tabSize);
            if (spacesToRemove == 0) spacesToRemove = 4;

            for (int32_t j = 0; j < spacesToRemove; j++) {
                if ((line.begin() + charIndex - 1)->m_char == ' ') {
                    line.erase(line.begin() + charIndex - 1);
                    charIndex -= 1;
                }
            }

            setCursorPosition(Coordinates(coord.m_line, getCharacterColumn(coord.m_line, std::max(0, charIndex))));
        }

    } else {
        char buffer[7];
        int32_t e = ImTextCharToUtf8(buffer, 7, aChar);
        if (e > 0) {
            buffer[e]      = '\0';
            auto &line  = m_lines[coord.m_line];
            auto charIndex = getCharacterIndex(coord);

            if (m_overwrite && charIndex < (int32_t)line.size()) {
                auto d = UTF8CharLength(line[charIndex].m_char);

                u.m_removedStart = m_state.m_cursorPosition;
                u.m_removedEnd   = Coordinates(coord.m_line, getCharacterColumn(coord.m_line, charIndex + d));

                while (d-- > 0 && charIndex < (int32_t)line.size()) {
                    u.m_removed += line[charIndex].m_char;
                    line.erase(line.begin() + charIndex);
                }
            }

            for (auto p = buffer; *p != '\0'; p++, ++charIndex)
                line.insert(line.begin() + charIndex, Glyph(*p, PaletteIndex::Default));
            u.m_added = buffer;

            setCursorPosition(Coordinates(coord.m_line, getCharacterColumn(coord.m_line, charIndex)));
        } else
            return;
    }

    m_textChanged = true;

    u.m_addedEnd = getActualCursorCoordinates();
    u.m_after    = m_state;

    addUndo(u);

    colorize(coord.m_line - 1, 3);
    ensureCursorVisible();
}

void TextEditor::setReadOnly(bool aValue) {
    m_readOnly = aValue;
}

void TextEditor::setColorizerEnable(bool aValue) {
    m_colorizerEnabled = aValue;
}

void TextEditor::setCursorPosition(const Coordinates &aPosition) {
    if (m_state.m_cursorPosition != aPosition) {
        m_state.m_cursorPosition = aPosition;
        m_cursorPositionChanged = true;
        ensureCursorVisible();
    }
}

void TextEditor::setSelectionStart(const Coordinates &aPosition) {
    m_state.m_selectionStart = sanitizeCoordinates(aPosition);
    if (m_state.m_selectionStart > m_state.m_selectionEnd)
        std::swap(m_state.m_selectionStart, m_state.m_selectionEnd);
}

void TextEditor::setSelectionEnd(const Coordinates &aPosition) {
    m_state.m_selectionEnd = sanitizeCoordinates(aPosition);
    if (m_state.m_selectionStart > m_state.m_selectionEnd)
        std::swap(m_state.m_selectionStart, m_state.m_selectionEnd);
}

void TextEditor::setSelection(const Coordinates &aStart, const Coordinates &aEnd, SelectionMode aMode) {
    auto oldSelStart = m_state.m_selectionStart;
    auto oldSelEnd   = m_state.m_selectionEnd;

    m_state.m_selectionStart = sanitizeCoordinates(aStart);
    m_state.m_selectionEnd   = sanitizeCoordinates(aEnd);
    if (m_state.m_selectionStart > m_state.m_selectionEnd)
        std::swap(m_state.m_selectionStart, m_state.m_selectionEnd);

    switch (aMode) {
        case TextEditor::SelectionMode::Normal:
            break;
        case TextEditor::SelectionMode::Word:
            {
                m_state.m_selectionStart = findWordStart(m_state.m_selectionStart);
                if (!isOnWordBoundary(m_state.m_selectionEnd))
                    m_state.m_selectionEnd = findWordEnd(findWordStart(m_state.m_selectionEnd));
                break;
            }
        case TextEditor::SelectionMode::Line:
            {
                const auto lineNo      = m_state.m_selectionEnd.m_line;
                const auto lineSize    = (size_t)lineNo < m_lines.size() ? m_lines[lineNo].size() : 0;
                m_state.m_selectionStart = Coordinates(m_state.m_selectionStart.m_line, 0);
                m_state.m_selectionEnd   = Coordinates(lineNo, getLineMaxColumn(lineNo));
                break;
            }
        default:
            break;
    }

    if (m_state.m_selectionStart != oldSelStart ||
        m_state.m_selectionEnd != oldSelEnd)
        m_cursorPositionChanged = true;
}

void TextEditor::setTabSize(int32_t aValue) {
    m_tabSize = std::max(0, std::min(32, aValue));
}

void TextEditor::insertText(const std::string &aValue) {
    insertText(aValue.c_str());
}

void TextEditor::insertText(const char *aValue) {
    if (aValue == nullptr)
        return;

    auto pos       = getActualCursorCoordinates();
    auto start     = std::min(pos, m_state.m_selectionStart);
    int32_t totalLines = pos.m_line - start.m_line;

    totalLines += insertTextAt(pos, aValue);

    setSelection(pos, pos);
    setCursorPosition(pos);
    colorize(start.m_line - 1, totalLines + 2);
}

void TextEditor::deleteSelection() {
    assert(m_state.m_selectionEnd >= m_state.m_selectionStart);

    if (m_state.m_selectionEnd == m_state.m_selectionStart)
        return;

    deleteRange(m_state.m_selectionStart, m_state.m_selectionEnd);

    setSelection(m_state.m_selectionStart, m_state.m_selectionStart);
    setCursorPosition(m_state.m_selectionStart);
    colorize(m_state.m_selectionStart.m_line, 1);
}

void TextEditor::moveUp(int32_t aAmount, bool aSelect) {
    auto oldPos                  = m_state.m_cursorPosition;
    m_state.m_cursorPosition.m_line = std::max(0, m_state.m_cursorPosition.m_line - aAmount);
    if (oldPos != m_state.m_cursorPosition) {
        if (aSelect) {
            if (oldPos == m_interactiveStart)
                m_interactiveStart = m_state.m_cursorPosition;
            else if (oldPos == m_interactiveEnd)
                m_interactiveEnd = m_state.m_cursorPosition;
            else {
                m_interactiveStart = m_state.m_cursorPosition;
                m_interactiveEnd   = oldPos;
            }
        } else
            m_interactiveStart = m_interactiveEnd = m_state.m_cursorPosition;
        setSelection(m_interactiveStart, m_interactiveEnd);

        ensureCursorVisible();
    }
}

void TextEditor::moveDown(int32_t aAmount, bool aSelect) {
    assert(m_state.m_cursorPosition.m_column >= 0);
    auto oldPos                  = m_state.m_cursorPosition;
    m_state.m_cursorPosition.m_line = std::max(0, std::min((int32_t)m_lines.size() - 1, m_state.m_cursorPosition.m_line + aAmount));

    if (m_state.m_cursorPosition != oldPos) {
        if (aSelect) {
            if (oldPos == m_interactiveEnd)
                m_interactiveEnd = m_state.m_cursorPosition;
            else if (oldPos == m_interactiveStart)
                m_interactiveStart = m_state.m_cursorPosition;
            else {
                m_interactiveStart = oldPos;
                m_interactiveEnd   = m_state.m_cursorPosition;
            }
        } else
            m_interactiveStart = m_interactiveEnd = m_state.m_cursorPosition;
        setSelection(m_interactiveStart, m_interactiveEnd);

        ensureCursorVisible();
    }
}

static bool IsUTFSequence(char c) {
    return (c & 0xC0) == 0x80;
}

void TextEditor::moveLeft(int32_t aAmount, bool aSelect, bool aWordMode) {
    if (m_lines.empty())
        return;

    auto oldPos            = m_state.m_cursorPosition;
    m_state.m_cursorPosition = getActualCursorCoordinates();
    auto line              = m_state.m_cursorPosition.m_line;
    auto charIndex            = getCharacterIndex(m_state.m_cursorPosition);

    while (aAmount-- > 0) {
        if (charIndex == 0) {
            if (line > 0) {
                --line;
                if ((int32_t)m_lines.size() > line)
                    charIndex = (int32_t)m_lines[line].size();
                else
                    charIndex = 0;
            }
        } else {
            --charIndex;
            if (charIndex > 0) {
                if ((int32_t)m_lines.size() > line) {
                    while (charIndex > 0 && IsUTFSequence(m_lines[line][charIndex].m_char))
                        --charIndex;
                }
            }
        }

        m_state.m_cursorPosition = Coordinates(line, getCharacterColumn(line, charIndex));
        if (aWordMode) {
            m_state.m_cursorPosition = findWordStart(m_state.m_cursorPosition);
            charIndex                 = getCharacterIndex(m_state.m_cursorPosition);
        }
    }

    m_state.m_cursorPosition = Coordinates(line, getCharacterColumn(line, charIndex));

    assert(m_state.m_cursorPosition.m_column >= 0);
    if (aSelect) {
        if (oldPos == m_interactiveStart)
            m_interactiveStart = m_state.m_cursorPosition;
        else if (oldPos == m_interactiveEnd)
            m_interactiveEnd = m_state.m_cursorPosition;
        else {
            m_interactiveStart = m_state.m_cursorPosition;
            m_interactiveEnd   = oldPos;
        }
    } else
        m_interactiveStart = m_interactiveEnd = m_state.m_cursorPosition;
    setSelection(m_interactiveStart, m_interactiveEnd,
                 aSelect && aWordMode ? SelectionMode::Word : SelectionMode::Normal);

    ensureCursorVisible();
}

void TextEditor::moveRight(int32_t aAmount, bool aSelect, bool aWordMode) {
    auto oldPos = m_state.m_cursorPosition;

    if (m_lines.empty() || oldPos.m_line >= m_lines.size())
        return;

    auto charIndex = getCharacterIndex(m_state.m_cursorPosition);
    while (aAmount-- > 0) {
        auto lindex = m_state.m_cursorPosition.m_line;
        auto &line  = m_lines[lindex];

        if (charIndex >= line.size()) {
            if (m_state.m_cursorPosition.m_line < m_lines.size() - 1) {
                m_state.m_cursorPosition.m_line   = std::max(0, std::min((int32_t)m_lines.size() - 1, m_state.m_cursorPosition.m_line + 1));
                m_state.m_cursorPosition.m_column = 0;
            } else
                return;
        } else {
            charIndex += UTF8CharLength(line[charIndex].m_char);
            m_state.m_cursorPosition = Coordinates(lindex, getCharacterColumn(lindex, charIndex));
            if (aWordMode)
                m_state.m_cursorPosition = findNextWord(m_state.m_cursorPosition);
        }
    }

    if (aSelect) {
        if (oldPos == m_interactiveEnd)
            m_interactiveEnd = sanitizeCoordinates(m_state.m_cursorPosition);
        else if (oldPos == m_interactiveStart)
            m_interactiveStart = m_state.m_cursorPosition;
        else {
            m_interactiveStart = oldPos;
            m_interactiveEnd   = m_state.m_cursorPosition;
        }
    } else
        m_interactiveStart = m_interactiveEnd = m_state.m_cursorPosition;
    setSelection(m_interactiveStart, m_interactiveEnd,
                 aSelect && aWordMode ? SelectionMode::Word : SelectionMode::Normal);

    ensureCursorVisible();
}

void TextEditor::moveTop(bool aSelect) {
    auto oldPos = m_state.m_cursorPosition;
    setCursorPosition(Coordinates(0, 0));

    if (m_state.m_cursorPosition != oldPos) {
        if (aSelect) {
            m_interactiveEnd   = oldPos;
            m_interactiveStart = m_state.m_cursorPosition;
        } else
            m_interactiveStart = m_interactiveEnd = m_state.m_cursorPosition;
        setSelection(m_interactiveStart, m_interactiveEnd);
    }
}

void TextEditor::TextEditor::moveBottom(bool aSelect) {
    auto oldPos = getCursorPosition();
    auto newPos = Coordinates((int32_t)m_lines.size() - 1, 0);
    setCursorPosition(newPos);
    if (aSelect) {
        m_interactiveStart = oldPos;
        m_interactiveEnd   = newPos;
    } else
        m_interactiveStart = m_interactiveEnd = newPos;
    setSelection(m_interactiveStart, m_interactiveEnd);
}

void TextEditor::moveHome(bool aSelect) {
    auto oldPos = m_state.m_cursorPosition;
    setCursorPosition(Coordinates(m_state.m_cursorPosition.m_line, 0));

    if (m_state.m_cursorPosition != oldPos) {
        if (aSelect) {
            if (oldPos == m_interactiveStart)
                m_interactiveStart = m_state.m_cursorPosition;
            else if (oldPos == m_interactiveEnd)
                m_interactiveEnd = m_state.m_cursorPosition;
            else {
                m_interactiveStart = m_state.m_cursorPosition;
                m_interactiveEnd   = oldPos;
            }
        } else
            m_interactiveStart = m_interactiveEnd = m_state.m_cursorPosition;
        setSelection(m_interactiveStart, m_interactiveEnd);
    }
}

void TextEditor::moveEnd(bool aSelect) {
    auto oldPos = m_state.m_cursorPosition;
    setCursorPosition(Coordinates(m_state.m_cursorPosition.m_line, getLineMaxColumn(oldPos.m_line)));

    if (m_state.m_cursorPosition != oldPos) {
        if (aSelect) {
            if (oldPos == m_interactiveEnd)
                m_interactiveEnd = m_state.m_cursorPosition;
            else if (oldPos == m_interactiveStart)
                m_interactiveStart = m_state.m_cursorPosition;
            else {
                m_interactiveStart = oldPos;
                m_interactiveEnd   = m_state.m_cursorPosition;
            }
        } else
            m_interactiveStart = m_interactiveEnd = m_state.m_cursorPosition;
        setSelection(m_interactiveStart, m_interactiveEnd);
    }
}

void TextEditor::doDelete() {
    assert(!m_readOnly);

    if (m_lines.empty())
        return;

    UndoRecord u;
    u.m_before = m_state;

    if (hasSelection()) {
        u.m_removed      = getSelectedText();
        u.m_removedStart = m_state.m_selectionStart;
        u.m_removedEnd   = m_state.m_selectionEnd;

        deleteSelection();
    } else {
        auto pos = getActualCursorCoordinates();
        setCursorPosition(pos);
        auto &line = m_lines[pos.m_line];

        if (pos.m_column == getLineMaxColumn(pos.m_line)) {
            if (pos.m_line == (int32_t)m_lines.size() - 1)
                return;

            u.m_removed      = '\n';
            u.m_removedStart = u.m_removedEnd = getActualCursorCoordinates();
            advance(u.m_removedEnd);

            auto &nextLine = m_lines[pos.m_line + 1];
            line.insert(line.end(), nextLine.begin(), nextLine.end());
            removeLine(pos.m_line + 1);
        } else {
            auto charIndex     = getCharacterIndex(pos);
            u.m_removedStart = u.m_removedEnd = getActualCursorCoordinates();
            u.m_removedEnd.m_column++;
            u.m_removed = getText(u.m_removedStart, u.m_removedEnd);

            auto d = UTF8CharLength(line[charIndex].m_char);
            while (d-- > 0 && charIndex < (int32_t)line.size())
                line.erase(line.begin() + charIndex);
        }

        m_textChanged = true;

        colorize(pos.m_line, 1);
    }

    u.m_after = m_state;
    addUndo(u);
}

void TextEditor::backspace() {
    assert(!m_readOnly);

    if (m_lines.empty())
        return;

    UndoRecord u;
    u.m_before = m_state;

    if (hasSelection()) {
        u.m_removed      = getSelectedText();
        u.m_removedStart = m_state.m_selectionStart;
        u.m_removedEnd   = m_state.m_selectionEnd;

        deleteSelection();
    } else {
        auto pos = getActualCursorCoordinates();
        setCursorPosition(pos);

        if (m_state.m_cursorPosition.m_column == 0) {
            if (m_state.m_cursorPosition.m_line == 0)
                return;

            u.m_removed      = '\n';
            u.m_removedStart = u.m_removedEnd = Coordinates(pos.m_line - 1, getLineMaxColumn(pos.m_line - 1));
            advance(u.m_removedEnd);

            auto &line     = m_lines[m_state.m_cursorPosition.m_line];
            auto &prevLine = m_lines[m_state.m_cursorPosition.m_line - 1];
            auto prevSize  = getLineMaxColumn(m_state.m_cursorPosition.m_line - 1);
            prevLine.insert(prevLine.end(), line.begin(), line.end());

            ErrorMarkers errorMarkers;
            for (auto &i : m_errorMarkers)
                errorMarkers.insert(ErrorMarkers::value_type(i.first - 1 == m_state.m_cursorPosition.m_line ? i.first - 1 : i.first, i.second));
            m_errorMarkers = std::move(errorMarkers);

            removeLine(m_state.m_cursorPosition.m_line);
            --m_state.m_cursorPosition.m_line;
            m_state.m_cursorPosition.m_column = prevSize;
        } else {
            auto &line  = m_lines[m_state.m_cursorPosition.m_line];
            auto charIndex = getCharacterIndex(pos) - 1;
            auto charEnd   = charIndex + 1;
            while (charIndex > 0 && IsUTFSequence(line[charIndex].m_char))
                --charIndex;

            u.m_removedStart = u.m_removedEnd = getActualCursorCoordinates();
            --u.m_removedStart.m_column;
            m_state.m_cursorPosition.m_column = getCharacterColumn(m_state.m_cursorPosition.m_line, charIndex);

            while (charIndex < line.size() && charEnd-- > charIndex) {
                u.m_removed += line[charIndex].m_char;
                line.erase(line.begin() + charIndex);
            }
        }

        m_textChanged = true;

        ensureCursorVisible();
        colorize(m_state.m_cursorPosition.m_line, 1);
    }

    u.m_after = m_state;
    addUndo(u);
}

void TextEditor::selectWordUnderCursor() {
    auto c = getCursorPosition();
    setSelection(findWordStart(c), findWordEnd(c));
}

void TextEditor::selectAll() {
    setSelection(Coordinates(0, 0), Coordinates((int32_t) m_lines.size(), 0));
}

bool TextEditor::hasSelection() const {
    return m_state.m_selectionEnd > m_state.m_selectionStart;
}

void TextEditor::copy() {
    if (hasSelection()) {
        ImGui::SetClipboardText(getSelectedText().c_str());
    } else {
        if (!m_lines.empty()) {
            std::string str;
            auto &line = m_lines[getActualCursorCoordinates().m_line];
            for (auto &g : line)
                str.push_back(g.m_char);
            ImGui::SetClipboardText(str.c_str());
        }
    }
}

void TextEditor::cut() {
    if (isReadOnly()) {
        copy();
    } else {
        if (hasSelection()) {
            UndoRecord u;
            u.m_before       = m_state;
            u.m_removed      = getSelectedText();
            u.m_removedStart = m_state.m_selectionStart;
            u.m_removedEnd   = m_state.m_selectionEnd;

            copy();
            deleteSelection();

            u.m_after = m_state;
            addUndo(u);
        }
    }
}

void TextEditor::paste() {
    if (isReadOnly())
        return;

    auto clipText = ImGui::GetClipboardText();
    if (clipText != nullptr && strlen(clipText) > 0) {
        UndoRecord u;
        u.m_before = m_state;

        if (hasSelection()) {
            u.m_removed      = getSelectedText();
            u.m_removedStart = m_state.m_selectionStart;
            u.m_removedEnd   = m_state.m_selectionEnd;
            deleteSelection();
        }

        u.m_added      = clipText;
        u.m_addedStart = getActualCursorCoordinates();

        insertText(clipText);

        u.m_addedEnd = getActualCursorCoordinates();
        u.m_after    = m_state;
        addUndo(u);
    }
}

bool TextEditor::canUndo() const {
    return !m_readOnly && m_undoIndex > 0;
}

bool TextEditor::canRedo() const {
    return !m_readOnly && m_undoIndex < (int32_t)m_undoBuffer.size();
}

void TextEditor::undo(int32_t aSteps) {
    while (canUndo() && aSteps-- > 0)
        m_undoBuffer[--m_undoIndex].undo(this);
}

void TextEditor::redo(int32_t aSteps) {
    while (canRedo() && aSteps-- > 0)
        m_undoBuffer[m_undoIndex++].redo(this);
}

// the index here is array index so zero based
void TextEditor::FindReplaceHandler::selectFound(TextEditor *editor, int32_t found) {
    assert(found >= 0 && found < m_matches.size());
    auto selectionStart = m_matches[found].m_selectionStart;
    auto selectionEnd = m_matches[found].m_selectionEnd;
    editor->setSelection(selectionStart, selectionEnd);
    editor->setCursorPosition(selectionEnd);
    editor->m_scrollToCursor = true;
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

    for (uint32_t i=0; i < count; i++) {
        if (targetPos >= m_matches[i].m_selectionStart && targetPos <= m_matches[i].m_selectionEnd) {
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

    if ((targetPos > m_matches[count - 1].m_selectionEnd) || (targetPos < m_matches[0].m_selectionStart)) {
        if (isNex) {
            selectFound(editor, 0);
            return 1;
        } else {
            selectFound(editor, count - 1);
            return count;
        }
    }

    for (uint32_t i=1;i < count;i++) {

        if (m_matches[i - 1].m_selectionEnd <= targetPos &&
            m_matches[i].m_selectionStart >= targetPos ) {
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
    if( isNext) {
        if (pos > m_matches[count - 1].m_selectionEnd || pos <= m_matches[0].m_selectionEnd)
            return 1;
        for (uint32_t i = 1; i < count; i++) {
            if (pos > m_matches[i - 1].m_selectionEnd && pos <= m_matches[i].m_selectionEnd)
                return i+1;
        }
    } else {
        if (pos >= m_matches[count - 1].m_selectionStart || pos < m_matches[0].m_selectionStart)
            return count;
        for (uint32_t i = 1; i < count; i++) {
            if (pos >= m_matches[i - 1].m_selectionStart && pos < m_matches[i].m_selectionStart)
                return i ;
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
    for (auto ch : s) {
        if (strchr(metacharacters, ch))
            out.push_back('\\');
        out.push_back(ch);
    }
    out.push_back('\\');
    out.push_back('b');
    return out;
}

// Performs actual search to fill m_matches
bool TextEditor::FindReplaceHandler::findNext(TextEditor *editor, bool wrapAround) {
    auto curPos = editor->m_state.m_cursorPosition;
    uint64_t selectionLength = editor->getStringCharacterCount(m_findWord);
    size_t byteIndex = 0;

    for (size_t ln = 0; ln < curPos.m_line; ln++)
        byteIndex += editor->getLineByteCount(ln) + 1;
    byteIndex += curPos.m_column;

    std::string wordLower = m_findWord;
    if (!getMatchCase())
        std::transform(wordLower.begin(), wordLower.end(), wordLower.begin(), ::tolower);

    std::string textSrc = editor->getText();
    if (!getMatchCase())
        std::transform(textSrc.begin(), textSrc.end(), textSrc.begin(), ::tolower);

    size_t textLoc;
    // TODO: use regexp find iterator in all cases
    //  to find all matches for findAllMatches.
    //  That should make things faster (no need
    //  to call findNext many times) and remove
    //  clunky match case code
    if (getWholeWord() || getFindRegEx()) {
        std::regex regularExpression;
        if (getFindRegEx()) {
            try {
                regularExpression.assign(wordLower);
            } catch (std::regex_error &e) {
                return false;
            }
        } else {
            try {
                regularExpression.assign(make_wholeWord(wordLower));
            } catch (std::regex_error &e) {
                return false;
            }
        }

        size_t pos=0;
        std::sregex_iterator iter = std::sregex_iterator(textSrc.begin(), textSrc.end(), regularExpression);
        std::sregex_iterator end;
        if (!iter->ready())
            return false;
        size_t firstLoc = iter->position();
        uint64_t firstLength = iter->length();

        if(firstLoc > byteIndex) {
            pos = firstLoc;
            selectionLength = firstLength;
        }  else {

            while (iter != end) {
                iter++;
                if (((pos = iter->position()) > byteIndex) && ((selectionLength = iter->length()) > 0))
                    break;
            }
        }

        if (iter == end && !wrapAround)
            return false;

        textLoc = pos;
        if (wrapAround) {
            if (iter == end) {
                pos = firstLoc;
                selectionLength = firstLength;
            }
        }
    } else {
        // non regex search
        textLoc = textSrc.find(wordLower, byteIndex);
        if (textLoc == std::string::npos) {
            if (wrapAround)
                textLoc = textSrc.find(wordLower, 0);
            else
                return false;
        }
    }
    if (textLoc != std::string::npos) {
        curPos.m_line = curPos.m_column = 0;
        byteIndex = 0;

        for (size_t ln = 0; ln < editor->m_lines.size(); ln++) {
            auto byteCount = editor->getLineByteCount(ln) + 1;

            if (byteIndex + byteCount > textLoc) {
                curPos.m_line = ln;
                curPos.m_column = textLoc - byteIndex;

                auto &line = editor->m_lines[curPos.m_line];
                for (int32_t i = 0; i < line.size(); i++) {
                    if (line[i].m_char == '\t')
                        curPos.m_column += (editor->m_tabSize - 1);
                }
                break;
            } else {// just keep adding
                byteIndex += byteCount;
            }
        }
    } else
        return false;

    auto selStart = curPos, selEnd = curPos;
    selEnd.m_column += selectionLength;
    editor->setSelection(selStart, selEnd);
    editor->setCursorPosition(selEnd);
    editor->m_scrollToCursor = true;
    return true;
}

void TextEditor::FindReplaceHandler::findAllMatches(TextEditor *editor, std::string findWord) {

    if (findWord.empty()) {
        editor->m_scrollToCursor = true;
        m_findWord = "";
        m_matches.clear();
        return;
    }

    if(findWord == m_findWord && !editor->m_textChanged && !m_optionsChanged)
        return;

    if (m_optionsChanged)
        m_optionsChanged = false;

    m_matches.clear();
    m_findWord = findWord;
    auto startingPos = editor->m_state.m_cursorPosition;
    auto state = editor->m_state;
    Coordinates begin = Coordinates(0,0);
    editor->m_state.m_cursorPosition = begin;

    if (!findNext(editor, false)) {
        editor->m_state = state;
        editor->m_scrollToCursor = true;
        return;
    }
    auto initialPos = editor->m_state.m_cursorPosition;
    m_matches.push_back(editor->m_state);

    while(editor->m_state.m_cursorPosition < startingPos) {
        if (!findNext(editor, false)) {
            editor->m_state = state;
            editor->m_scrollToCursor = true;
            return;
        }
        m_matches.push_back(editor->m_state);
    }

    while (findNext(editor, false))
        m_matches.push_back(editor->m_state);

    editor->m_state = state;
    editor->m_scrollToCursor = true;
    return;
}


bool TextEditor::FindReplaceHandler::replace(TextEditor *editor, bool next) {

    if (m_matches.empty() || m_findWord == m_replaceWord || m_findWord.empty())
        return false;


    auto state = editor->m_state;

    if (editor->m_state.m_cursorPosition <= editor->m_state.m_selectionEnd && editor->m_state.m_selectionEnd > editor->m_state.m_selectionStart && editor->m_state.m_cursorPosition > editor->m_state.m_selectionStart) {

        editor->m_state.m_cursorPosition = editor->m_state.m_selectionStart;
        if(editor->m_state.m_cursorPosition.m_column == 0) {
            editor->m_state.m_cursorPosition.m_line--;
            editor->m_state.m_cursorPosition.m_column = editor->getLineMaxColumn(
                    editor->m_state.m_cursorPosition.m_line);
        } else
            editor->m_state.m_cursorPosition.m_column--;
    }
    auto matchIndex = findMatch(editor, next);
    if(matchIndex != 0) {
        UndoRecord u;
        u.m_before = editor->m_state;

        auto selectionEnd = editor->m_state.m_selectionEnd;

        u.m_removed = editor->getSelectedText();
        u.m_removedStart = editor->m_state.m_selectionStart;
        u.m_removedEnd = editor->m_state.m_selectionEnd;

        editor->deleteSelection();
        if (getFindRegEx()) {
            std::string replacedText = std::regex_replace(editor->getText(), std::regex(m_findWord), m_replaceWord, std::regex_constants::format_first_only | std::regex_constants::format_no_copy);
            u.m_added = replacedText;
        } else
            u.m_added = m_replaceWord;

        u.m_addedStart = editor->getActualCursorCoordinates();

        editor->insertText(u.m_added);
        editor->setCursorPosition(editor->m_state.m_selectionEnd);

        u.m_addedEnd = editor->getActualCursorCoordinates();

        editor->m_scrollToCursor = true;
        ImGui::SetKeyboardFocusHere(0);

        u.m_after = editor->m_state;
        editor->addUndo(u);
        editor->m_textChanged = true;
        m_matches.erase(m_matches.begin() + matchIndex - 1);

        return true;
    }
    editor->m_state = state;
    return false;
}

bool TextEditor::FindReplaceHandler::replaceAll(TextEditor *editor) {
    uint32_t count = m_matches.size();

    for (uint32_t i = 0; i < count; i++)
        replace(editor,true);

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
    return getText(Coordinates(), Coordinates((int32_t)m_lines.size(), 0));
}

std::vector<std::string> TextEditor::getTextLines() const {
    std::vector<std::string> result;

    result.reserve(m_lines.size());

    for (auto &line : m_lines) {
        std::string text;

        text.resize(line.size());

        for (size_t i = 0; i < line.size(); ++i)
            text[i] = line[i].m_char;

        result.emplace_back(std::move(text));
    }

    return result;
}

std::string TextEditor::getSelectedText() const {
    return getText(m_state.m_selectionStart, m_state.m_selectionEnd);
}

std::string TextEditor::getCurrentLineText() const {
    auto lineLength = getLineMaxColumn(m_state.m_cursorPosition.m_line);
    return getText(
        Coordinates(m_state.m_cursorPosition.m_line, 0),
        Coordinates(m_state.m_cursorPosition.m_line, lineLength));
}

void TextEditor::processInputs() {
}

void TextEditor::colorize(int32_t aFromLine, int32_t aCount) {
    int32_t toLine     = aCount == -1 ? (int32_t)m_lines.size() : std::min((int32_t)m_lines.size(), aFromLine + aCount);
    m_colorRangeMin = std::min(m_colorRangeMin, aFromLine);
    m_colorRangeMax = std::max(m_colorRangeMax, toLine);
    m_colorRangeMin = std::max(0, m_colorRangeMin);
    m_colorRangeMax = std::max(m_colorRangeMin, m_colorRangeMax);
    m_checkComments = true;
}

void TextEditor::colorizeRange(int32_t aFromLine, int32_t aToLine) {
    if (m_lines.empty() || aFromLine >= aToLine)
        return;

    std::string buffer;
    std::cmatch results;
    std::string id;

    int32_t endLine = std::max(0, std::min((int32_t)m_lines.size(), aToLine));
    for (int32_t i = aFromLine; i < endLine; ++i) {
        auto &line = m_lines[i];

        if (line.empty())
            continue;

        buffer.resize(line.size());
        for (size_t j = 0; j < line.size(); ++j) {
            auto &col       = line[j];
            buffer[j]       = col.m_char;
            col.m_colorIndex = PaletteIndex::Default;
        }

        const char *bufferBegin = &buffer.front();
        const char *bufferEnd   = bufferBegin + buffer.size();

        auto last = bufferEnd;

        for (auto first = bufferBegin; first != last;) {
            const char *token_begin  = nullptr;
            const char *token_end    = nullptr;
            PaletteIndex token_color = PaletteIndex::Default;

            bool hasTokenizeResult = false;

            if (mL_languageDefinition.m_tokenize != nullptr) {
                if (mL_languageDefinition.m_tokenize(first, last, token_begin, token_end, token_color))
                    hasTokenizeResult = true;
            }

            if (hasTokenizeResult == false) {
                // todo : remove
                // printf("using regex for %.*s\n", first + 10 < last ? 10 : int32_t(last - first), first);

                for (auto &p : m_regexList) {
                    if (std::regex_search(first, last, results, p.first, std::regex_constants::match_continuous)) {
                        hasTokenizeResult = true;

                        auto &v     = *results.begin();
                        token_begin = v.first;
                        token_end   = v.second;
                        token_color = p.second;
                        break;
                    }
                }
            }

            if (hasTokenizeResult == false) {
                first++;
            } else {
                const size_t token_length = token_end - token_begin;

                if (token_color == PaletteIndex::Identifier) {
                    id.assign(token_begin, token_end);

                    // todo : allmost all language definitions use lower case to specify keywords, so shouldn't this use ::tolower ?
                    if (!mL_languageDefinition.m_caseSensitive)
                        std::transform(id.begin(), id.end(), id.begin(), ::toupper);

                    if (!line[first - bufferBegin].m_preprocessor) {
                        if (mL_languageDefinition.m_keywords.count(id) != 0)
                            token_color = PaletteIndex::Keyword;
                        else if (mL_languageDefinition.m_identifiers.count(id) != 0)
                            token_color = PaletteIndex::KnownIdentifier;
                        else if (mL_languageDefinition.m_preprocIdentifiers.count(id) != 0)
                            token_color = PaletteIndex::PreprocIdentifier;
                    } else {
                        if (mL_languageDefinition.m_preprocIdentifiers.count(id) != 0)
                            token_color = PaletteIndex::PreprocIdentifier;
                    }
                }

                for (size_t j = 0; j < token_length; ++j)
                    line[(token_begin - bufferBegin) + j].m_colorIndex = token_color;

                first = token_end;
            }
        }
    }
}

void TextEditor::colorizeInternal() {
    if (m_lines.empty() || !m_colorizerEnabled)
        return;

    if (m_checkComments) {
        auto endLine                 = m_lines.size();
        auto endIndex                = 0;
        auto commentStartLine        = endLine;
        auto commentStartIndex       = endIndex;
        auto withinGlobalDocComment  = false;
        auto withinDocComment        = false;
        auto withinComment           = false;
        auto withinString            = false;
        auto withinSingleLineComment = false;
        auto withinPreproc           = false;
        auto withinNotDef            = false;
        auto firstChar               = true;     // there is no other non-whitespace characters in the line before
        auto currentLine             = 0;
        auto currentIndex            = 0;
        auto commentLength           = 0;
        auto &startStr               = mL_languageDefinition.m_commentStart;
        auto &singleStartStr         = mL_languageDefinition.m_singleLineComment;
        auto &docStartStr            = mL_languageDefinition.m_docComment;
        auto &globalStartStr         = mL_languageDefinition.m_globalDocComment;

        std::vector<bool> ifDefs;
        ifDefs.push_back(true);

        while (currentLine < endLine || currentIndex < endIndex) {
            auto &line = m_lines[currentLine];

            auto setGlyphFlags = [&](int32_t index) {
                line[index].m_multiLineComment = withinComment;
                line[index].m_comment          = withinSingleLineComment;
                line[index].m_docComment       = withinDocComment;
                line[index].m_globalDocComment = withinGlobalDocComment;
                line[index].m_deactivated      = withinNotDef;
            };

            if (currentIndex == 0) {
                withinSingleLineComment = false;
                withinPreproc           = false;
                firstChar               = true;
            }

            if (!line.empty()) {
                auto &g = line[currentIndex];
                auto c  = g.m_char;

                if (c != mL_languageDefinition.m_preprocChar && !isspace(c))
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
                    if (firstChar && c == mL_languageDefinition.m_preprocChar) {
                        withinPreproc = true;
                        std::string directive;
                        auto start = currentIndex + 1;
                        while (start < (int32_t) line.size() && !isspace(line[start].m_char)) {
                            directive += line[start].m_char;
                            start++;
                        }

                        if (start < (int32_t) line.size()) {

                            if (isspace(line[start].m_char)) {
                                start += 1;
                                 if (directive == "define") {
                                     while (start < (int32_t) line.size() && isspace(line[start].m_char))
                                         start++;
                                     std::string identifier;
                                     while (start < (int32_t) line.size() && !isspace(line[start].m_char)) {
                                         identifier += line[start].m_char;
                                         start++;
                                     }
                                     if (identifier.size() > 0 && !withinNotDef && std::find(m_defines.begin(), m_defines.end(), identifier) == m_defines.end())
                                         m_defines.push_back(identifier);
                                    } else if (directive == "undef") {
                                         while (start < (int32_t) line.size() && isspace(line[start].m_char))
                                             start++;
                                         std::string identifier;
                                         while (start < (int32_t) line.size() && !isspace(line[start].m_char)) {
                                             identifier += line[start].m_char;
                                             start++;
                                         }
                                         if (identifier.size() > 0  && !withinNotDef)
                                             m_defines.erase(std::remove(m_defines.begin(), m_defines.end(), identifier), m_defines.end());
                                } else if (directive == "ifdef") {
                                    while (start < (int32_t) line.size() && isspace(line[start].m_char))
                                        start++;
                                    std::string identifier;
                                    while (start < (int32_t) line.size() && !isspace(line[start].m_char)) {
                                        identifier += line[start].m_char;
                                        start++;
                                    }
                                    if (!withinNotDef) {
                                        bool isConditionMet = std::find(m_defines.begin(), m_defines.end(), identifier) != m_defines.end();
                                        ifDefs.push_back(isConditionMet);
                                    } else
                                        ifDefs.push_back(false);
                                } else if (directive == "ifndef") {
                                    while (start < (int32_t) line.size() && isspace(line[start].m_char))
                                        start++;
                                    std::string identifier;
                                    while (start < (int32_t) line.size() && !isspace(line[start].m_char)) {
                                        identifier += line[start].m_char;
                                        start++;
                                    }
                                    if (!withinNotDef) {
                                        bool isConditionMet = std::find(m_defines.begin(), m_defines.end(), identifier) == m_defines.end();
                                        ifDefs.push_back(isConditionMet);
                                    } else
                                        ifDefs.push_back(false);
                                }
                            }
                        } else {
                            if (directive == "endif") {
                                if (ifDefs.size() > 1) {
                                    ifDefs.pop_back();
                                    withinNotDef = !ifDefs.back();
                                }
                            }
                        }
                    }

                    if (c == '\"') {
                        withinString                         = true;
                        setGlyphFlags(currentIndex);
                    } else {
                        auto pred            = [](const char &a, const Glyph &b) { return a == b.m_char; };

                        auto compareForth    = [&](const std::string &a, const std::vector<Glyph> &b) {
                            return !a.empty() && currentIndex + a.size() <= b.size() && equals(a.begin(), a.end(),
                                    b.begin() + currentIndex, b.begin() + currentIndex + a.size(), pred);
                        };

                        auto compareBack     = [&](const std::string &a, const std::vector<Glyph> &b) {
                            return !a.empty() && currentIndex + 1 >= (int32_t)a.size() && equals(a.begin(), a.end(),
                                    b.begin() + currentIndex + 1 - a.size(), b.begin() + currentIndex + 1, pred);
                        };

                        if (!inComment && !withinSingleLineComment && !withinPreproc) {
                            if (compareForth(singleStartStr, line)) {
                                withinSingleLineComment = !inComment;
                            } else {
                                bool isGlobalDocComment = compareForth(globalStartStr, line);
                                bool isDocComment = compareForth(docStartStr, line);
                                bool isComment = compareForth(startStr, line);
                                if (isGlobalDocComment || isDocComment || isComment) {
                                    commentStartLine = currentLine;
                                    commentStartIndex = currentIndex;
                                    if (isGlobalDocComment) {
                                        withinGlobalDocComment = true;
                                        commentLength = 3;
                                    } else if (isDocComment) {
                                        withinDocComment = true;
                                        commentLength = 3;
                                    } else {
                                        withinComment = true;
                                        commentLength = 2;
                                    }
                                }
                            }
                            inComment = (commentStartLine < currentLine || (commentStartLine == currentLine && commentStartIndex <= currentIndex));
                        }
                        setGlyphFlags(currentIndex);

                        auto &endStr = mL_languageDefinition.m_commentEnd;
                        if (compareBack(endStr, line) && ((commentStartLine != currentLine) || (commentStartIndex + commentLength < currentIndex))) {
                            withinComment          = false;
                            withinDocComment        = false;
                            withinGlobalDocComment  = false;
                            commentStartLine        = endLine;
                            commentStartIndex       = endIndex;
                            commentLength           = 0;
                        }
                    }
                }
                if (currentIndex < line.size())
                    line[currentIndex].m_preprocessor = withinPreproc;
                
                currentIndex += UTF8CharLength(c);
                if (currentIndex >= (int32_t)line.size()) {
                    withinNotDef = !ifDefs.back();
                    currentIndex = 0;
                    ++currentLine;
                }
            } else {
                currentIndex = 0;
                ++currentLine;
            }
        }
        m_defines.clear();
        m_checkComments = false;
    }

    if (m_colorRangeMin < m_colorRangeMax) {
        const int32_t increment = (mL_languageDefinition.m_tokenize == nullptr) ? 10 : 10000;
        const int32_t to        = std::min(m_colorRangeMin + increment, m_colorRangeMax);
        colorizeRange(m_colorRangeMin, to);
        m_colorRangeMin = to;

        if (m_colorRangeMax == m_colorRangeMin) {
            m_colorRangeMin = std::numeric_limits<int32_t>::max();
            m_colorRangeMax = 0;
        }
        return;
    }
}

float TextEditor::textDistanceToLineStart(const Coordinates &aFrom) const {
    auto &line      = m_lines[aFrom.m_line];
    float distance  = 0.0f;
    float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ", nullptr, nullptr).x;
    int32_t colIndex    = getCharacterIndex(aFrom);
    for (size_t it = 0u; it < line.size() && it < colIndex;) {
        if (line[it].m_char == '\t') {
            distance = (1.0f + std::floor((1.0f + distance) / (float(m_tabSize) * spaceSize))) * (float(m_tabSize) * spaceSize);
            ++it;
        } else {
            auto d = UTF8CharLength(line[it].m_char);
            char tempCString[7];
            int32_t i = 0;
            for (; i < 6 && d-- > 0 && it < (int32_t)line.size(); i++, it++)
                tempCString[i] = line[it].m_char;

            tempCString[i] = '\0';
            distance += ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, tempCString, nullptr, nullptr).x;
        }
    }

    return distance;
}

void TextEditor::ensureCursorVisible() {
    if (!m_withinRender) {
        m_scrollToCursor = true;
        return;
    }

    float scrollX = ImGui::GetScrollX();
    float scrollY = ImGui::GetScrollY();

    auto windowPadding = ImGui::GetStyle().WindowPadding * 2.0f;

    auto height = ImGui::GetWindowHeight() - m_topMargin - windowPadding.y;
    auto width  = ImGui::GetWindowWidth() - windowPadding.x;

    auto top    = (int32_t)ceil(scrollY / m_charAdvance.y);
    auto bottom = (int32_t)ceil((scrollY + height) / m_charAdvance.y);

    auto left  = scrollX;
    auto right = scrollX + width;

    auto pos = getActualCursorCoordinates();
    auto len = textDistanceToLineStart(pos);

    if (pos.m_line <= top + 1)
        ImGui::SetScrollY(std::max(0.0f, (pos.m_line - 1) * m_charAdvance.y));
    if (pos.m_line >= bottom - 2)
        ImGui::SetScrollY(std::max(0.0f, (pos.m_line + 2) * m_charAdvance.y - height));
    if (len == 0)
        ImGui::SetScrollX(0);
    else if (len + m_textStart <= left + 4)
        ImGui::SetScrollX(std::max(0.0f, len + m_textStart - 4));
    if (len + m_textStart + m_charAdvance.x * 2 >= right - 4)
        ImGui::SetScrollX(std::max(0.0f, len + m_textStart + 4 - width + m_charAdvance.x * 2));
}

int32_t TextEditor::getPageSize() const {
    auto height = ImGui::GetWindowHeight() - 20.0f - m_topMargin;
    return (int32_t)floor(height / m_charAdvance.y);
}

void TextEditor::resetCursorBlinkTime() {
    m_startTime = ImGui::GetTime() * 1000 - s_cursorBlinkOnTime;
}

TextEditor::UndoRecord::UndoRecord(
    const std::string &aAdded,
    const TextEditor::Coordinates aAddedStart,
    const TextEditor::Coordinates aAddedEnd,
    const std::string &aRemoved,
    const TextEditor::Coordinates aRemovedStart,
    const TextEditor::Coordinates aRemovedEnd,
    TextEditor::EditorState &aBefore,
    TextEditor::EditorState &aAfter)
    : m_added(aAdded), m_addedStart(aAddedStart), m_addedEnd(aAddedEnd), m_removed(aRemoved), m_removedStart(aRemovedStart), m_removedEnd(aRemovedEnd), m_before(aBefore), m_after(aAfter) {
    assert(m_addedStart <= m_addedEnd);
    assert(m_removedStart <= m_removedEnd);
}

void TextEditor::UndoRecord::undo(TextEditor *aEditor) {
    if (!m_added.empty()) {
        aEditor->deleteRange(m_addedStart, m_addedEnd);
        aEditor->colorize(m_addedStart.m_line - 1, m_addedEnd.m_line - m_addedStart.m_line + 2);
    }

    if (!m_removed.empty()) {
        auto start = m_removedStart;
        aEditor->insertTextAt(start, m_removed.c_str());
        aEditor->colorize(m_removedStart.m_line - 1, m_removedEnd.m_line - m_removedStart.m_line + 2);
    }

    aEditor->m_state = m_before;
    aEditor->ensureCursorVisible();
}

void TextEditor::UndoRecord::redo(TextEditor *aEditor) {
    if (!m_removed.empty()) {
        aEditor->deleteRange(m_removedStart, m_removedEnd);
        aEditor->colorize(m_removedStart.m_line - 1, m_removedEnd.m_line - m_removedStart.m_line + 1);
    }

    if (!m_added.empty()) {
        auto start = m_addedStart;
        aEditor->insertTextAt(start, m_added.c_str());
        aEditor->colorize(m_addedStart.m_line - 1, m_addedEnd.m_line - m_addedStart.m_line + 1);
    }

    aEditor->m_state = m_after;
    aEditor->ensureCursorVisible();
}

bool tokenizeCStyleString(const char *in_begin, const char *in_end, const char *&out_begin, const char *&out_end) {
    const char *p = in_begin;

    if (*p == '"') {
        p++;

        while (p < in_end) {
            // handle end of string
            if (*p == '"') {
                out_begin = in_begin;
                out_end   = p + 1;
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

bool tokenizeCStyleCharacterLiteral(const char *in_begin, const char *in_end, const char *&out_begin, const char *&out_end) {
    const char *p = in_begin;

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
            out_end   = p + 1;
            return true;
        }
    }

    return false;
}

bool tokenizeCStyleIdentifier(const char *in_begin, const char *in_end, const char *&out_begin, const char *&out_end) {
    const char *p = in_begin;

    if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_') {
        p++;

        while ((p < in_end) && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_'))
            p++;

        out_begin = in_begin;
        out_end   = p;
        return true;
    }

    return false;
}

bool tokenizeCStyleNumber(const char *in_begin, const char *in_end, const char *&out_begin, const char *&out_end) {
    const char *p = in_begin;

    const bool startsWithNumber = *p >= '0' && *p <= '9';

    if (*p != '+' && *p != '-' && !startsWithNumber)
        return false;

    p++;

    bool hasNumber = startsWithNumber;

    while (p < in_end && (*p >= '0' && *p <= '9')) {
        hasNumber = true;

        p++;
    }

    if (hasNumber == false)
        return false;

    bool isFloat  = false;
    bool isHex    = false;
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

    if (isHex == false && isBinary == false) {
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

            if (hasDigits == false)
                return false;
        }

        // single precision floating point type
        if (p < in_end && *p == 'f')
            p++;
    }

    if (isFloat == false) {
        // integer size type
        while (p < in_end && (*p == 'u' || *p == 'U' || *p == 'l' || *p == 'L'))
            p++;
    }

    out_begin = in_begin;
    out_end   = p;
    return true;
}

bool tokenizeCStylePunctuation(const char *in_begin, const char *in_end, const char *&out_begin, const char *&out_end) {
    (void)in_end;

    switch (*in_begin) {
        case '[':
        case ']':
        case '{':
        case '}':
        case '!':
        case '%':
        case '^':
        case '&':
        case '*':
        case '(':
        case ')':
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
        case ';':
        case ',':
        case '.':
            out_begin = in_begin;
            out_end   = in_begin + 1;
            return true;
    }

    return false;
}

const TextEditor::LanguageDefinition &TextEditor::LanguageDefinition::CPlusPlus() {
    static bool inited = false;
    static LanguageDefinition langDef;
    if (!inited) {
        static const char *const cppKeywords[] = {
            "alignas", "alignof", "and", "and_eq", "asm", "atomic_cancel", "atomic_commit", "atomic_noexcept", "auto", "bitand", "bitor", "bool", "break", "case", "catch", "char", "char16_t", "char32_t", "class", "compl", "concept", "const", "constexpr", "const_cast", "continue", "decltype", "default", "delete", "do", "double", "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false", "float", "for", "friend", "goto", "if", "import", "inline", "int", "long", "module", "mutable", "namespace", "new", "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private", "protected", "public", "register", "reinterpret_cast", "requires", "return", "short", "signed", "sizeof", "static", "static_assert", "static_cast", "struct", "switch", "synchronized", "template", "this", "thread_local", "throw", "true", "try", "typedef", "typeid", "typename", "union", "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq"
        };
        for (auto &k : cppKeywords)
            langDef.m_keywords.insert(k);

        static const char *const identifiers[] = {
            "abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh", "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha", "isdigit", "isgraph", "ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp", "modf", "pow", "printf", "sprintf", "snprintf", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand", "strcat", "strcmp", "strerror", "time", "tolower", "toupper", "std", "string", "vector", "map", "unordered_map", "set", "unordered_set", "min", "max"
        };
        for (auto &k : identifiers) {
            Identifier id;
            id.m_declaration = "Built-in function";
            langDef.m_identifiers.insert(std::make_pair(std::string(k), id));
        }

        langDef.m_tokenize = [](const char *in_begin, const char *in_end, const char *&out_begin, const char *&out_end, PaletteIndex &paletteIndex) -> bool {
            paletteIndex = PaletteIndex::Max;

            while (in_begin < in_end && isascii(*in_begin) && isblank(*in_begin))
                in_begin++;

            if (in_begin == in_end) {
                out_begin    = in_end;
                out_end      = in_end;
                paletteIndex = PaletteIndex::Default;
            } else if (tokenizeCStyleString(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::String;
            else if (tokenizeCStyleCharacterLiteral(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::CharLiteral;
            else if (tokenizeCStyleIdentifier(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::Identifier;
            else if (tokenizeCStyleNumber(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::Number;
            else if (tokenizeCStylePunctuation(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::Punctuation;

            return paletteIndex != PaletteIndex::Max;
        };

        langDef.m_commentStart      = "/*";
        langDef.m_commentEnd        = "*/";
        langDef.m_singleLineComment = "//";

        langDef.m_caseSensitive   = true;
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
        for (auto &k : keywords)
            langDef.m_keywords.insert(k);

        static const char *const identifiers[] = {
            "abort", "abs", "acos", "all", "AllMemoryBarrier", "AllMemoryBarrierWithGroupSync", "any", "asdouble", "asfloat", "asin", "asint", "asint", "asuint", "asuint", "atan", "atan2", "ceil", "CheckAccessFullyMapped", "clamp", "clip", "cos", "cosh", "countbits", "cross", "D3DCOLORtoUBYTE4", "ddx", "ddx_coarse", "ddx_fine", "ddy", "ddy_coarse", "ddy_fine", "degrees", "determinant", "DeviceMemoryBarrier", "DeviceMemoryBarrierWithGroupSync", "distance", "dot", "dst", "errorf", "EvaluateAttributeAtCentroid", "EvaluateAttributeAtSample", "EvaluateAttributeSnapped", "exp", "exp2", "f16tof32", "f32tof16", "faceforward", "firstbithigh", "firstbitlow", "floor", "fma", "fmod", "frac", "frexp", "fwidth", "GetRenderTargetSampleCount", "GetRenderTargetSamplePosition", "GroupMemoryBarrier", "GroupMemoryBarrierWithGroupSync", "InterlockedAdd", "InterlockedAnd", "InterlockedCompareExchange", "InterlockedCompareStore", "InterlockedExchange", "InterlockedMax", "InterlockedMin", "InterlockedOr", "InterlockedXor", "isfinite", "isinf", "isnan", "ldexp", "length", "lerp", "lit", "log", "log10", "log2", "mad", "max", "min", "modf", "msad4", "mul", "noise", "normalize", "pow", "printf", "Process2DQuadTessFactorsAvg", "Process2DQuadTessFactorsMax", "Process2DQuadTessFactorsMin", "ProcessIsolineTessFactors", "ProcessQuadTessFactorsAvg", "ProcessQuadTessFactorsMax", "ProcessQuadTessFactorsMin", "ProcessTriTessFactorsAvg", "ProcessTriTessFactorsMax", "ProcessTriTessFactorsMin", "radians", "rcp", "reflect", "refract", "reversebits", "round", "rsqrt", "saturate", "sign", "sin", "sincos", "sinh", "smoothstep", "sqrt", "step", "tan", "tanh", "tex1D", "tex1D", "tex1Dbias", "tex1Dgrad", "tex1Dlod", "tex1Dproj", "tex2D", "tex2D", "tex2Dbias", "tex2Dgrad", "tex2Dlod", "tex2Dproj", "tex3D", "tex3D", "tex3Dbias", "tex3Dgrad", "tex3Dlod", "tex3Dproj", "texCUBE", "texCUBE", "texCUBEbias", "texCUBEgrad", "texCUBElod", "texCUBEproj", "transpose", "trunc"
        };
        for (auto &k : identifiers) {
            Identifier id;
            id.m_declaration = "Built-in function";
            langDef.m_identifiers.insert(std::make_pair(std::string(k), id));
        }

        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Preprocessor));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

        langDef.m_commentStart      = "/*";
        langDef.m_commentEnd        = "*/";
        langDef.m_singleLineComment = "//";

        langDef.m_caseSensitive   = true;
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
            "auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum", "extern", "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return", "short", "signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic", "_Imaginary", "_Noreturn", "_Static_assert", "_Thread_local"
        };
        for (auto &k : keywords)
            langDef.m_keywords.insert(k);

        static const char *const identifiers[] = {
            "abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh", "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha", "isdigit", "isgraph", "ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp", "modf", "pow", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand", "strcat", "strcmp", "strerror", "time", "tolower", "toupper"
        };
        for (auto &k : identifiers) {
            Identifier id;
            id.m_declaration = "Built-in function";
            langDef.m_identifiers.insert(std::make_pair(std::string(k), id));
        }

        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Preprocessor));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

        langDef.m_commentStart      = "/*";
        langDef.m_commentEnd        = "*/";
        langDef.m_singleLineComment = "//";

        langDef.m_caseSensitive   = true;
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
            "auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum", "extern", "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return", "short", "signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic", "_Imaginary", "_Noreturn", "_Static_assert", "_Thread_local"
        };
        for (auto &k : keywords)
            langDef.m_keywords.insert(k);

        static const char *const identifiers[] = {
            "abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh", "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha", "isdigit", "isgraph", "ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp", "modf", "pow", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand", "strcat", "strcmp", "strerror", "time", "tolower", "toupper"
        };
        for (auto &k : identifiers) {
            Identifier id;
            id.m_declaration = "Built-in function";
            langDef.m_identifiers.insert(std::make_pair(std::string(k), id));
        }

        langDef.m_tokenize = [](const char *in_begin, const char *in_end, const char *&out_begin, const char *&out_end, PaletteIndex &paletteIndex) -> bool {
            paletteIndex = PaletteIndex::Max;

            while (in_begin < in_end && isascii(*in_begin) && isblank(*in_begin))
                in_begin++;

            if (in_begin == in_end) {
                out_begin    = in_end;
                out_end      = in_end;
                paletteIndex = PaletteIndex::Default;
            } else if (tokenizeCStyleString(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::String;
            else if (tokenizeCStyleCharacterLiteral(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::CharLiteral;
            else if (tokenizeCStyleIdentifier(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::Identifier;
            else if (tokenizeCStyleNumber(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::Number;
            else if (tokenizeCStylePunctuation(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::Punctuation;

            return paletteIndex != PaletteIndex::Max;
        };

        langDef.m_commentStart      = "/*";
        langDef.m_commentEnd        = "*/";
        langDef.m_singleLineComment = "//";

        langDef.m_caseSensitive   = true;
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
            "ADD", "EXCEPT", "PERCENT", "ALL", "EXEC", "PLAN", "ALTER", "EXECUTE", "PRECISION", "AND", "EXISTS", "PRIMARY", "ANY", "EXIT", "PRINT", "AS", "FETCH", "PROC", "ASC", "FILE", "PROCEDURE", "AUTHORIZATION", "FILLFACTOR", "PUBLIC", "BACKUP", "FOR", "RAISERROR", "BEGIN", "FOREIGN", "READ", "BETWEEN", "FREETEXT", "READTEXT", "BREAK", "FREETEXTTABLE", "RECONFIGURE", "BROWSE", "FROM", "REFERENCES", "BULK", "FULL", "REPLICATION", "BY", "FUNCTION", "RESTORE", "CASCADE", "GOTO", "RESTRICT", "CASE", "GRANT", "RETURN", "CHECK", "GROUP", "REVOKE", "CHECKPOINT", "HAVING", "RIGHT", "CLOSE", "HOLDLOCK", "ROLLBACK", "CLUSTERED", "IDENTITY", "ROWCOUNT", "COALESCE", "IDENTITY_INSERT", "ROWGUIDCOL", "COLLATE", "IDENTITYCOL", "RULE", "COLUMN", "IF", "SAVE", "COMMIT", "IN", "SCHEMA", "COMPUTE", "INDEX", "SELECT", "CONSTRAINT", "INNER", "SESSION_USER", "CONTAINS", "INSERT", "SET", "CONTAINSTABLE", "INTERSECT", "SETUSER", "CONTINUE", "INTO", "SHUTDOWN", "CONVERT", "IS", "SOME", "CREATE", "JOIN", "STATISTICS", "CROSS", "KEY", "SYSTEM_USER", "CURRENT", "KILL", "TABLE", "CURRENT_DATE", "LEFT", "TEXTSIZE", "CURRENT_TIME", "LIKE", "THEN", "CURRENT_TIMESTAMP", "LINENO", "TO", "CURRENT_USER", "LOAD", "TOP", "CURSOR", "NATIONAL", "TRAN", "DATABASE", "NOCHECK", "TRANSACTION", "DBCC", "NONCLUSTERED", "TRIGGER", "DEALLOCATE", "NOT", "TRUNCATE", "DECLARE", "NULL", "TSEQUAL", "DEFAULT", "NULLIF", "UNION", "DELETE", "OF", "UNIQUE", "DENY", "OFF", "UPDATE", "DESC", "OFFSETS", "UPDATETEXT", "DISK", "ON", "USE", "DISTINCT", "OPEN", "USER", "DISTRIBUTED", "OPENDATASOURCE", "VALUES", "DOUBLE", "OPENQUERY", "VARYING", "DROP", "OPENROWSET", "VIEW", "DUMMY", "OPENXML", "WAITFOR", "DUMP", "OPTION", "WHEN", "ELSE", "OR", "WHERE", "END", "ORDER", "WHILE", "ERRLVL", "OUTER", "WITH", "ESCAPE", "OVER", "WRITETEXT"
        };

        for (auto &k : keywords)
            langDef.m_keywords.insert(k);

        static const char *const identifiers[] = {
            "ABS", "ACOS", "ADD_MONTHS", "ASCII", "ASCIISTR", "ASIN", "ATAN", "ATAN2", "AVG", "BFILENAME", "BIN_TO_NUM", "BITAND", "CARDINALITY", "CASE", "CAST", "CEIL", "CHARTOROWID", "CHR", "COALESCE", "COMPOSE", "CONCAT", "CONVERT", "CORR", "COS", "COSH", "COUNT", "COVAR_POP", "COVAR_SAMP", "CUME_DIST", "CURRENT_DATE", "CURRENT_TIMESTAMP", "DBTIMEZONE", "DECODE", "DECOMPOSE", "DENSE_RANK", "DUMP", "EMPTY_BLOB", "EMPTY_CLOB", "EXP", "EXTRACT", "FIRST_VALUE", "FLOOR", "FROM_TZ", "GREATEST", "GROUP_ID", "HEXTORAW", "INITCAP", "INSTR", "INSTR2", "INSTR4", "INSTRB", "INSTRC", "LAG", "LAST_DAY", "LAST_VALUE", "LEAD", "LEAST", "LENGTH", "LENGTH2", "LENGTH4", "LENGTHB", "LENGTHC", "LISTAGG", "LN", "LNNVL", "LOCALTIMESTAMP", "LOG", "LOWER", "LPAD", "LTRIM", "MAX", "MEDIAN", "MIN", "MOD", "MONTHS_BETWEEN", "NANVL", "NCHR", "NEW_TIME", "NEXT_DAY", "NTH_VALUE", "NULLIF", "NUMTODSINTERVAL", "NUMTOYMINTERVAL", "NVL", "NVL2", "POWER", "RANK", "RAWTOHEX", "REGEXP_COUNT", "REGEXP_INSTR", "REGEXP_REPLACE", "REGEXP_SUBSTR", "REMAINDER", "REPLACE", "ROUND", "ROWNUM", "RPAD", "RTRIM", "SESSIONTIMEZONE", "SIGN", "SIN", "SINH", "SOUNDEX", "SQRT", "STDDEV", "SUBSTR", "SUM", "SYS_CONTEXT", "SYSDATE", "SYSTIMESTAMP", "TAN", "TANH", "TO_CHAR", "TO_CLOB", "TO_DATE", "TO_DSINTERVAL", "TO_LOB", "TO_MULTI_BYTE", "TO_NCLOB", "TO_NUMBER", "TO_SINGLE_BYTE", "TO_TIMESTAMP", "TO_TIMESTAMP_TZ", "TO_YMINTERVAL", "TRANSLATE", "TRIM", "TRUNC", "TZ_OFFSET", "UID", "UPPER", "USER", "USERENV", "VAR_POP", "VAR_SAMP", "VARIANCE", "VSIZE "
        };
        for (auto &k : identifiers) {
            Identifier id;
            id.m_declaration = "Built-in function";
            langDef.m_identifiers.insert(std::make_pair(std::string(k), id));
        }

        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\\'[^\\\']*\\\'", PaletteIndex::String));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

        langDef.m_commentStart      = "/*";
        langDef.m_commentEnd        = "*/";
        langDef.m_singleLineComment = "//";

        langDef.m_caseSensitive   = false;
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
            "and", "abstract", "auto", "bool", "break", "case", "cast", "class", "const", "continue", "default", "do", "double", "else", "enum", "false", "final", "float", "for", "from", "funcdef", "function", "get", "if", "import", "in", "inout", "int", "interface", "int8", "int16", "int32", "int64", "is", "mixin", "namespace", "not", "null", "or", "out", "override", "private", "protected", "return", "set", "shared", "super", "switch", "this ", "true", "typedef", "uint", "uint8", "uint16", "uint32", "uint64", "void", "while", "xor"
        };

        for (auto &k : keywords)
            langDef.m_keywords.insert(k);

        static const char *const identifiers[] = {
            "cos", "sin", "tab", "acos", "asin", "atan", "atan2", "cosh", "sinh", "tanh", "log", "log10", "pow", "sqrt", "abs", "ceil", "floor", "fraction", "closeTo", "fpFromIEEE", "fpToIEEE", "complex", "opEquals", "opAddAssign", "opSubAssign", "opMulAssign", "opDivAssign", "opAdd", "opSub", "opMul", "opDiv"
        };
        for (auto &k : identifiers) {
            Identifier id;
            id.m_declaration = "Built-in function";
            langDef.m_identifiers.insert(std::make_pair(std::string(k), id));
        }

        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\'\\\\?[^\\']\\'", PaletteIndex::String));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

        langDef.m_commentStart      = "/*";
        langDef.m_commentEnd        = "*/";
        langDef.m_singleLineComment = "//";

        langDef.m_caseSensitive   = true;
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
            "and", "break", "do", "", "else", "elseif", "end", "false", "for", "function", "if", "in", "", "local", "nil", "not", "or", "repeat", "return", "then", "true", "until", "while"
        };

        for (auto &k : keywords)
            langDef.m_keywords.insert(k);

        static const char *const identifiers[] = {
            "assert", "collectgarbage", "dofile", "error", "getmetatable", "ipairs", "loadfile", "load", "loadstring", "next", "pairs", "pcall", "print", "rawequal", "rawlen", "rawget", "rawset", "select", "setmetatable", "tonumber", "tostring", "type", "xpcall", "_G", "_VERSION", "arshift", "band", "bnot", "bor", "bxor", "btest", "extract", "lrotate", "lshift", "replace", "rrotate", "rshift", "create", "resume", "running", "status", "wrap", "yield", "isyieldable", "debug", "getuservalue", "gethook", "getinfo", "getlocal", "getregistry", "getmetatable", "getupvalue", "upvaluejoin", "upvalueid", "setuservalue", "sethook", "setlocal", "setmetatable", "setupvalue", "traceback", "close", "flush", "input", "lines", "open", "output", "popen", "read", "tmpfile", "type", "write", "close", "flush", "lines", "read", "seek", "setvbuf", "write", "__gc", "__tostring", "abs", "acos", "asin", "atan", "ceil", "cos", "deg", "exp", "tointeger", "floor", "fmod", "ult", "log", "max", "min", "modf", "rad", "random", "randomseed", "sin", "sqrt", "string", "tan", "type", "atan2", "cosh", "sinh", "tanh", "pow", "frexp", "ldexp", "log10", "pi", "huge", "maxinteger", "mininteger", "loadlib", "searchpath", "seeall", "preload", "cpath", "path", "searchers", "loaded", "module", "require", "clock", "date", "difftime", "execute", "exit", "getenv", "remove", "rename", "setlocale", "time", "tmpname", "byte", "char", "dump", "find", "format", "gmatch", "gsub", "len", "lower", "match", "rep", "reverse", "sub", "upper", "pack", "packsize", "unpack", "concat", "maxn", "insert", "pack", "unpack", "remove", "move", "sort", "offset", "codepoint", "char", "len", "codes", "charpattern", "coroutine", "table", "io", "os", "string", "utf8", "bit32", "math", "debug", "package"
        };
        for (auto &k : identifiers) {
            Identifier id;
            id.m_declaration = "Built-in function";
            langDef.m_identifiers.insert(std::make_pair(std::string(k), id));
        }

        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\\'[^\\\']*\\\'", PaletteIndex::String));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
        langDef.m_tokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

        langDef.m_commentStart      = "--[[";
        langDef.m_commentEnd        = "]]";
        langDef.m_singleLineComment = "--";

        langDef.m_caseSensitive   = true;
        langDef.m_autoIndentation = false;

        langDef.m_name = "Lua";

        inited = true;
    }
    return langDef;
}
