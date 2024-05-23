#include <algorithm>
#include <chrono>
#include <string>
#include <regex>
#include <cmath>

#include "TextEditor.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"    // for imGui::GetCurrentWindow()
#include "imgui_internal.h"



bool TextEditor::Coordinates::operator ==(const TextEditor::Coordinates& o) const
{
    return
            mLine == o.mLine &&
            mColumn == o.mColumn;
}

bool TextEditor::Coordinates::operator !=(const TextEditor::Coordinates& o) const
{
    return
            mLine != o.mLine ||
            mColumn != o.mColumn;
}

bool TextEditor::Coordinates::operator <(const TextEditor::Coordinates& o) const
{
    if (mLine != o.mLine)
        return mLine < o.mLine;
    return mColumn < o.mColumn;
}

bool TextEditor::Coordinates::operator >(const TextEditor::Coordinates& o) const
{
    if (mLine != o.mLine)
        return mLine > o.mLine;
    return mColumn > o.mColumn;
}

bool TextEditor::Coordinates::operator <=(const TextEditor::Coordinates& o) const
{
    if (mLine != o.mLine)
        return mLine < o.mLine;
    return mColumn <= o.mColumn;
}

bool TextEditor::Coordinates::operator >=(const TextEditor::Coordinates& o) const
{
    if (mLine != o.mLine)
        return mLine > o.mLine;
    return mColumn >= o.mColumn;
}

template<class InputIt1, class InputIt2, class BinaryPredicate>
bool equals(InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2, BinaryPredicate p) {
    for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
        if (!p(*first1, *first2))
            return false;
    }
    return first1 == last1 && first2 == last2;
}

const int TextEditor::sCursorBlinkInterval = 1200;
const int TextEditor::sCursorBlinkOnTime = 800;

TextEditor::Palette TextEditor::sPaletteBase = TextEditor::GetDarkPalette();

TextEditor::FindReplaceHandler::FindReplaceHandler() : mWholeWord(false),mFindRegEx(false),mMatchCase(false)  {}

TextEditor::TextEditor()
    : mLineSpacing(1.0f), mUndoIndex(0), mTabSize(4), mOverwrite(false), mReadOnly(false), mWithinRender(false), mScrollToCursor(false), mScrollToTop(false), mScrollToBottom(false), mTextChanged(false), mColorizerEnabled(true), mTextStart(20.0f), mLeftMargin(10), mTopMargin(0), mCursorPositionChanged(false), mSelectionMode(SelectionMode::Normal),mLastClick(-1.0f), mHandleKeyboardInputs(true), mHandleMouseInputs(true), mIgnoreImGuiChild(false), mShowWhitespaces(true), mShowCursor(true), mShowLineNumbers(true),mStartTime(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()) {
    mLines.push_back(Line());
}

TextEditor::~TextEditor() {
}

ImVec2 TextEditor::UnderSquiggles( ImVec2 pos ,uint32_t nChars, ImColor color, const ImVec2 &size_arg) {
    auto save = ImGui::GetStyle().AntiAliasedLines;
    ImGui::GetStyle().AntiAliasedLines = false;
    ImGuiWindow *window = ImGui::GetCurrentWindow();
    window->DC.CursorPos =pos;
    const ImVec2 label_size = ImGui::CalcTextSize("W", nullptr, true);
    ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x, label_size.y);
    float lineWidth = size.x / 3.0f + 0.5f;
    float halfLineW = lineWidth / 2.0f;

    for (uint32_t i = 0; i < nChars; i++) {
        pos = window->DC.CursorPos;
        float lineY = pos.y + size.y;

        ImVec2 pos1_1 = ImVec2(pos.x + 0*lineWidth, lineY + halfLineW);
        ImVec2 pos1_2 = ImVec2(pos.x + 1*lineWidth, lineY - halfLineW);
        ImVec2 pos2_1 = ImVec2(pos.x + 2*lineWidth, lineY + halfLineW);
        ImVec2 pos2_2 = ImVec2(pos.x + 3*lineWidth, lineY - halfLineW);

        ImGui::GetWindowDrawList()->AddLine(pos1_1, pos1_2, ImU32(color), 0.4f);
        ImGui::GetWindowDrawList()->AddLine(pos1_2, pos2_1, ImU32(color), 0.4f);
        ImGui::GetWindowDrawList()->AddLine(pos2_1, pos2_2, ImU32(color), 0.4f);

        window->DC.CursorPos = ImVec2(pos.x + size.x, pos.y);
    }
    auto ret = window->DC.CursorPos;
    ret.y += size.y;
    return ret;
}

void TextEditor::SetPalette(const Palette &aValue) {
    sPaletteBase = aValue;
}

std::string TextEditor::GetText(const Coordinates &aStart, const Coordinates &aEnd) const {
    std::string result;

    auto lstart = aStart.mLine;
    auto lend   = aEnd.mLine;
    auto istart = GetCharacterIndex(aStart);
    auto iend   = GetCharacterIndex(aEnd);
    size_t s    = 0;

    for (size_t i = lstart; i < lend; i++)
        s += mLines[i].size();

    result.reserve(s + s / 8);

    while (istart < iend || lstart < lend) {
        if (lstart >= (int32_t)mLines.size())
            break;

        auto &line = mLines[lstart];
        if (istart < (int32_t)line.size()) {
            result += line[istart].mChar;
            istart++;
        } else {
            istart = 0;
            ++lstart;
            result += '\n';
        }
    }

    return result;
}

TextEditor::Coordinates TextEditor::GetActualCursorCoordinates() const {
    return SanitizeCoordinates(mState.mCursorPosition);
}

TextEditor::Coordinates TextEditor::SanitizeCoordinates(const Coordinates &aValue) const {
    auto line   = aValue.mLine;
    auto column = aValue.mColumn;
    if (line >= (int32_t)mLines.size()) {
        if (mLines.empty()) {
            line   = 0;
            column = 0;
        } else {
            line   = (int32_t)mLines.size() - 1;
            column = GetLineMaxColumn(line);
        }
        return Coordinates(line, column);
    } else {
        column = mLines.empty() ? 0 : std::min(column, GetLineMaxColumn(line));
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
static inline int32_t ImTextCharToUtf8(char *buf, int32_t buf_size, uint32_t c) {
    if (c < 0x80) {
        buf[0] = (char)c;
        return 1;
    }
    if (c < 0x800) {
        if (buf_size < 2) return 0;
        buf[0] = (char)(0xc0 + (c >> 6));
        buf[1] = (char)(0x80 + (c & 0x3f));
        return 2;
    }
    if (c >= 0xdc00 && c < 0xe000) {
        return 0;
    }
    if (c >= 0xd800 && c < 0xdc00) {
        if (buf_size < 4) return 0;
        buf[0] = (char)(0xf0 + (c >> 18));
        buf[1] = (char)(0x80 + ((c >> 12) & 0x3f));
        buf[2] = (char)(0x80 + ((c >> 6) & 0x3f));
        buf[3] = (char)(0x80 + ((c)&0x3f));
        return 4;
    }
    // else if (c < 0x10000)
    {
        if (buf_size < 3) return 0;
        buf[0] = (char)(0xe0 + (c >> 12));
        buf[1] = (char)(0x80 + ((c >> 6) & 0x3f));
        buf[2] = (char)(0x80 + ((c)&0x3f));
        return 3;
    }
}

void TextEditor::Advance(Coordinates &aCoordinates) const {
    if (aCoordinates.mLine < (int32_t)mLines.size()) {
        auto &line  = mLines[aCoordinates.mLine];
        auto charIndex = GetCharacterIndex(aCoordinates);

        if (charIndex + 1 < (int32_t)line.size()) {
            auto delta = UTF8CharLength(line[charIndex].mChar);
            charIndex     = std::min(charIndex + delta, (int32_t)line.size() - 1);
        } else {
            ++aCoordinates.mLine;
            charIndex = 0;
        }
        aCoordinates.mColumn = GetCharacterColumn(aCoordinates.mLine, charIndex);
    }
}

void TextEditor::DeleteRange(const Coordinates &aStart, const Coordinates &aEnd) {
    assert(aEnd >= aStart);
    assert(!mReadOnly);

    // printf("D(%d.%d)-(%d.%d)\n", aStart.mLine, aStart.mColumn, aEnd.mLine, aEnd.mColumn);

    if (aEnd == aStart)
        return;

    auto start = GetCharacterIndex(aStart);
    auto end   = GetCharacterIndex(aEnd);

    if (aStart.mLine == aEnd.mLine) {
        auto &line = mLines[aStart.mLine];
        auto n     = GetLineMaxColumn(aStart.mLine);
        if (aEnd.mColumn >= n)
            line.erase(line.begin() + start, line.end());
        else
            line.erase(line.begin() + start, line.begin() + end);
    } else {
        auto &firstLine = mLines[aStart.mLine];
        auto &lastLine  = mLines[aEnd.mLine];

        firstLine.erase(firstLine.begin() + start, firstLine.end());
        lastLine.erase(lastLine.begin(), lastLine.begin() + end);

        if (aStart.mLine < aEnd.mLine)
            firstLine.insert(firstLine.end(), lastLine.begin(), lastLine.end());

        if (aStart.mLine < aEnd.mLine)
            RemoveLine(aStart.mLine + 1, aEnd.mLine + 1);
    }

    mTextChanged = true;
}

int32_t TextEditor::InsertTextAt(Coordinates & /* inout */ aWhere, const char *aValue) {
    int32_t charIndex  = GetCharacterIndex(aWhere);
    int32_t totalLines = 0;
    while (*aValue != '\0') {
        assert(!mLines.empty());

        if (*aValue == '\r') {
            // skip
            ++aValue;
        } else if (*aValue == '\n') {
            if (charIndex < (int32_t)mLines[aWhere.mLine].size()) {
                auto &newLine = InsertLine(aWhere.mLine + 1);
                auto &line    = mLines[aWhere.mLine];
                newLine.insert(newLine.begin(), line.begin() + charIndex, line.end());
                line.erase(line.begin() + charIndex, line.end());
            } else {
                InsertLine(aWhere.mLine + 1);
            }
            ++aWhere.mLine;
            aWhere.mColumn = 0;
            charIndex         = 0;
            ++totalLines;
            ++aValue;
        } else {
            auto &line = mLines[aWhere.mLine];
            auto d     = UTF8CharLength(*aValue);
            while (d-- > 0 && *aValue != '\0')
                line.insert(line.begin() + charIndex++, Glyph(*aValue++, PaletteIndex::Default));
            ++aWhere.mColumn;
        }

        mTextChanged = true;
    }

    return totalLines;
}

void TextEditor::AddUndo(UndoRecord &aValue) {
    assert(!mReadOnly);
    // printf("AddUndo: (@%d.%d) +\'%s' [%d.%d .. %d.%d], -\'%s', [%d.%d .. %d.%d] (@%d.%d)\n",
    //	aValue.mBefore.mCursorPosition.mLine, aValue.mBefore.mCursorPosition.mColumn,
    //	aValue.mAdded.c_str(), aValue.mAddedStart.mLine, aValue.mAddedStart.mColumn, aValue.mAddedEnd.mLine, aValue.mAddedEnd.mColumn,
    //	aValue.mRemoved.c_str(), aValue.mRemovedStart.mLine, aValue.mRemovedStart.mColumn, aValue.mRemovedEnd.mLine, aValue.mRemovedEnd.mColumn,
    //	aValue.mAfter.mCursorPosition.mLine, aValue.mAfter.mCursorPosition.mColumn
    //	);

    mUndoBuffer.resize((size_t)(mUndoIndex + 1));
    mUndoBuffer.back() = aValue;
    ++mUndoIndex;
}

TextEditor::Coordinates TextEditor::ScreenPosToCoordinates(const ImVec2 &aPosition) const {
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 local(aPosition.x - origin.x, aPosition.y - origin.y);

    int32_t lineNo = std::max(0, (int32_t)floor(local.y / mCharAdvance.y));

    int32_t columnCoord = 0;

    if (lineNo >= 0 && lineNo < (int32_t)mLines.size()) {
        auto &line = mLines.at(lineNo);

        int32_t columnIndex = 0;
        float columnX   = 0.0f;

        while ((size_t)columnIndex < line.size()) {
            float columnWidth;

            if (line[columnIndex].mChar == '\t') {
                float spaceSize  = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ").x;
                float oldX       = columnX;
                float newColumnX = (1.0f + std::floor((1.0f + columnX) / (float(mTabSize) * spaceSize))) * (float(mTabSize) * spaceSize);
                columnWidth      = newColumnX - oldX;
                if (mTextStart + columnX + columnWidth * 0.5f > local.x)
                    break;
                columnX     = newColumnX;
                columnCoord = (columnCoord / mTabSize) * mTabSize + mTabSize;
                columnIndex++;
            } else {
                char buf[7];
                auto d = UTF8CharLength(line[columnIndex].mChar);
                int32_t i  = 0;
                while (i < 6 && d-- > 0)
                    buf[i++] = line[columnIndex++].mChar;
                buf[i]      = '\0';
                columnWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf).x;
                if (mTextStart + columnX + columnWidth * 0.5f > local.x)
                    break;
                columnX += columnWidth;
                columnCoord++;
            }
        }
    }

    return SanitizeCoordinates(Coordinates(lineNo, columnCoord));
}


void TextEditor::DeleteWordLeft()  {
    const auto wordEnd = GetCursorPosition();
    MoveLeft();
    const auto wordStart = FindWordStart(GetCursorPosition());
    SetSelection(wordStart, wordEnd);
    Backspace();
}

void TextEditor::DeleteWordRight() {
    const auto wordStart = GetCursorPosition();
    MoveRight();
    const auto wordEnd = FindWordEnd(GetCursorPosition());
    SetSelection(wordStart, wordEnd);
    Backspace();
}


TextEditor::Coordinates TextEditor::FindWordStart(const Coordinates &aFrom) const {
    Coordinates at = aFrom;
    if (at.mLine >= (int32_t)mLines.size())
        return at;

    auto &line  = mLines[at.mLine];
    auto charIndex = GetCharacterIndex(at);

    if (charIndex >= (int32_t)line.size())
        return at;

    while (charIndex > 0 && isspace(line[charIndex].mChar))
        --charIndex;

    auto charStart = line[charIndex].mChar;
    while (charIndex > 0) {
        auto c = line[charIndex].mChar;
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
    return Coordinates(at.mLine, GetCharacterColumn(at.mLine, charIndex));
}

TextEditor::Coordinates TextEditor::FindWordEnd(const Coordinates &aFrom) const {
    Coordinates at = aFrom;
    if (at.mLine >= (int32_t)mLines.size())
        return at;

    auto &line  = mLines[at.mLine];
    auto charIndex = GetCharacterIndex(at);

    if (charIndex >= (int32_t)line.size())
        return at;

    bool prevspace = (bool)isspace(line[charIndex].mChar);
    auto charStart    = (PaletteIndex)line[charIndex].mColorIndex;
    while (charIndex < (int32_t)line.size()) {
        auto c = line[charIndex].mChar;
        auto d = UTF8CharLength(c);
        if (charStart != (PaletteIndex)line[charIndex].mColorIndex)
            break;

        if (prevspace != !!isspace(c)) {
            if (isspace(c))
                while (charIndex < (int32_t)line.size() && isspace(line[charIndex].mChar))
                    ++charIndex;
            break;
        }
        charIndex += d;
    }
    return Coordinates(aFrom.mLine, GetCharacterColumn(aFrom.mLine, charIndex));
}

TextEditor::Coordinates TextEditor::FindNextWord(const Coordinates &aFrom) const {
    Coordinates at = aFrom;
    if (at.mLine >= (int32_t)mLines.size())
        return at;

    // skip to the next non-word character
    auto charIndex = GetCharacterIndex(aFrom);
    bool isword = false;
    bool skip   = false;
    if (charIndex < (int32_t)mLines[at.mLine].size()) {
        auto &line = mLines[at.mLine];
        isword     = isalnum(line[charIndex].mChar);
        skip       = isword;
    }

    while (!isword || skip) {
        if (at.mLine >= mLines.size()) {
            auto l = std::max(0, (int32_t)mLines.size() - 1);
            return Coordinates(l, GetLineMaxColumn(l));
        }

        auto &line = mLines[at.mLine];
        if (charIndex < (int32_t)line.size()) {
            isword = isalnum(line[charIndex].mChar);

            if (isword && !skip)
                return Coordinates(at.mLine, GetCharacterColumn(at.mLine, charIndex));

            if (!isword)
                skip = false;

            charIndex++;
        } else {
            charIndex = 0;
            ++at.mLine;
            skip   = false;
            isword = false;
        }
    }

    return at;
}

int32_t TextEditor::GetCharacterIndex(const Coordinates &aCoordinates) const {
    if (aCoordinates.mLine >= mLines.size())
        return -1;
    auto &line = mLines[aCoordinates.mLine];
    int32_t col    = 0;
    int32_t i      = 0;
    while (i < line.size() && col < aCoordinates.mColumn) {
        auto c = line[i].mChar;
        i += UTF8CharLength(c);
        if (c == '\t')
            col = (col / mTabSize) * mTabSize + mTabSize;
        else
            col++;
    }
    return i;
}

int32_t TextEditor::GetCharacterColumn(int32_t aLine, int32_t aIndex) const {
    if (aLine >= mLines.size())
        return 0;
    auto &line = mLines[aLine];
    int32_t col    = 0;
    int32_t i      = 0;
    while (i < line.size() && i < aIndex) {
        auto c = line[i].mChar;
        i += UTF8CharLength(c);
        if (c == '\t')
            col = (col / mTabSize) * mTabSize + mTabSize;
        else
            col++;
    }
    return col;
}

int32_t TextEditor::GetStringCharacterCount(std::string str) const {
    if (str.empty())
        return 0;
    int32_t c      = 0;
    for (unsigned i = 0; i < str.size(); c++)
        i += UTF8CharLength(str[i]);
    return c;
}

int32_t TextEditor::GetLineCharacterCount(int32_t aLine) const {
    if (aLine >= mLines.size())
        return 0;
    auto &line = mLines[aLine];
    int32_t c      = 0;
    for (unsigned i = 0; i < line.size(); c++)
        i += UTF8CharLength(line[i].mChar);
    return c;
}

unsigned long long TextEditor::GetLineByteCount(int32_t aLine) const {
    if (aLine >= mLines.size())
        return 0;
    auto &line = mLines[aLine];
    return line.size();
}

int32_t TextEditor::GetLineMaxColumn(int32_t aLine) const {
    if (aLine >= mLines.size())
        return 0;
    auto &line = mLines[aLine];
    int32_t col    = 0;
    for (unsigned i = 0; i < line.size();) {
        auto c = line[i].mChar;
        if (c == '\t')
            col = (col / mTabSize) * mTabSize + mTabSize;
        else
            col++;
        i += UTF8CharLength(c);
    }
    return col;
}

bool TextEditor::IsOnWordBoundary(const Coordinates &aAt) const {
    if (aAt.mLine >= (int32_t)mLines.size() || aAt.mColumn == 0)
        return true;

    auto &line  = mLines[aAt.mLine];
    auto charIndex = GetCharacterIndex(aAt);
    if (charIndex >= (int32_t)line.size())
        return true;

    if (mColorizerEnabled)
        return line[charIndex].mColorIndex != line[size_t(charIndex - 1)].mColorIndex;

    return isspace(line[charIndex].mChar) != isspace(line[charIndex - 1].mChar);
}

void TextEditor::RemoveLine(int32_t aStart, int32_t aEnd) {
    assert(!mReadOnly);
    assert(aEnd >= aStart);
    assert(mLines.size() > (size_t)(aEnd - aStart));

    ErrorMarkers etmp;
    for (auto &i : mErrorMarkers) {
        ErrorMarkers::value_type e(i.first.mLine >= aStart ?  Coordinates(i.first.mLine - 1,i.first.mColumn ) : i.first, i.second);
        if (e.first.mLine >= aStart && e.first.mLine <= aEnd)
            continue;
        etmp.insert(e);
    }
    mErrorMarkers = std::move(etmp);

    Breakpoints btmp;
    for (auto i : mBreakpoints) {
        if (i >= aStart && i <= aEnd)
            continue;
        btmp.insert(i >= aStart ? i - 1 : i);
    }
    mBreakpoints = std::move(btmp);

    mLines.erase(mLines.begin() + aStart, mLines.begin() + aEnd);
    assert(!mLines.empty());

    mTextChanged = true;
}

void TextEditor::RemoveLine(int32_t aIndex) {
    assert(!mReadOnly);
    assert(mLines.size() > 1);

    ErrorMarkers etmp;
    for (auto &i : mErrorMarkers) {
        ErrorMarkers::value_type e(i.first.mLine > aIndex ? Coordinates(i.first.mLine - 1 ,i.first.mColumn) : i.first, i.second);
        if (e.first.mLine - 1 == aIndex)
            continue;
        etmp.insert(e);
    }
    mErrorMarkers = std::move(etmp);

    Breakpoints btmp;
    for (auto i : mBreakpoints) {
        if (i == aIndex)
            continue;
        btmp.insert(i >= aIndex ? i - 1 : i);
    }
    mBreakpoints = std::move(btmp);

    mLines.erase(mLines.begin() + aIndex);
    assert(!mLines.empty());

    mTextChanged = true;
}

TextEditor::Line &TextEditor::InsertLine(int32_t aIndex) {
    auto &result = *mLines.insert(mLines.begin() + aIndex, Line());

    ErrorMarkers etmp;
    for (auto &i : mErrorMarkers)
        etmp.insert(ErrorMarkers::value_type(i.first.mLine >= aIndex ? Coordinates(i.first.mLine + 1,i.first.mColumn) : i.first, i.second));


    mErrorMarkers = std::move(etmp);

  Breakpoints btmp;
    for (auto i : mBreakpoints)
        btmp.insert(i >= aIndex ? i + 1 : i);
    mBreakpoints = std::move(btmp);

    return result;
}

std::string TextEditor::GetWordUnderCursor() const {
    auto c = GetCursorPosition();
    return GetWordAt(c);
}

std::string TextEditor::GetWordAt(const Coordinates &aCoords) const {
    auto start = FindWordStart(aCoords);
    auto end   = FindWordEnd(aCoords);

    std::string r;

    auto istart = GetCharacterIndex(start);
    auto iend   = GetCharacterIndex(end);

    for (auto it = istart; it < iend; ++it)
        r.push_back(mLines[aCoords.mLine][it].mChar);

    return r;
}

void TextEditor::HandleKeyboardInputs() {
    ImGuiIO &io   = ImGui::GetIO();
    auto shift    = io.KeyShift;
    auto left     = ImGui::IsKeyPressed(ImGuiKey_LeftArrow);
    auto right    = ImGui::IsKeyPressed(ImGuiKey_RightArrow);
    auto up       = ImGui::IsKeyPressed(ImGuiKey_UpArrow);
    auto down     = ImGui::IsKeyPressed(ImGuiKey_DownArrow);
    auto ctrl     = io.ConfigMacOSXBehaviors ? io.KeyAlt : io.KeyCtrl;
    auto alt      = io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;
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

        if (!IsReadOnly() && !ctrl && !shift && !alt && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)))
            EnterCharacter('\n', false);
        else if (!IsReadOnly() && !ctrl && !alt && ImGui::IsKeyPressed(ImGuiKey_Tab))
            EnterCharacter('\t', shift);

        if (!IsReadOnly() && !io.InputQueueCharacters.empty()) {
            for (int32_t i = 0; i < io.InputQueueCharacters.Size; i++) {
                auto c = io.InputQueueCharacters[i];
                if (c != 0 && (c == '\n' || c >= 32)) {
                    EnterCharacter(c, shift);
                }
            }
            io.InputQueueCharacters.resize(0);
        }
    }
}

void TextEditor::HandleMouseInputs() {
    ImGuiIO &io = ImGui::GetIO();
    auto shift  = io.KeyShift;
    auto ctrl   = io.ConfigMacOSXBehaviors ? io.KeyAlt : io.KeyCtrl;
    auto alt    = io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;

    if (ImGui::IsWindowHovered()) {
        if (!alt) {
            auto click       = ImGui::IsMouseClicked(0);
            auto doubleClick = ImGui::IsMouseDoubleClicked(0);
            auto t           = ImGui::GetTime();
            auto tripleClick = click && !doubleClick && (mLastClick != -1.0f && (t - mLastClick) < io.MouseDoubleClickTime);
            bool resetBlinking = false;
            /*
            Left mouse button triple click
            */

            if (tripleClick) {
                if (!ctrl) {
                    mState.mCursorPosition = mInteractiveStart = mInteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos());
                    mSelectionMode                                               = SelectionMode::Line;
                    SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);
                }

                mLastClick = -1.0f;
                resetBlinking=true;
            }

            /*
            Left mouse button double click
            */

            else if (doubleClick) {
                if (!ctrl) {
                    mState.mCursorPosition = mInteractiveStart = mInteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos());
                    if (mSelectionMode == SelectionMode::Line)
                        mSelectionMode = SelectionMode::Normal;
                    else
                        mSelectionMode = SelectionMode::Word;
                    SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);
                }

                mLastClick = (float)ImGui::GetTime();
                resetBlinking=true;
            }

            /*
            Left mouse button click
            */
            else if (click) {
                if (ctrl) {
                    mState.mCursorPosition = mInteractiveStart = mInteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos());
                    mSelectionMode = SelectionMode::Word;
                } else if (shift) {
                    mSelectionMode = SelectionMode::Normal;
                    mInteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos());
                } else {
                    mState.mCursorPosition = mInteractiveStart = mInteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos());
                    mSelectionMode = SelectionMode::Normal;
                }
                SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);
                resetBlinking=true;

                mLastClick = (float)ImGui::GetTime();
            }
            // Mouse left button dragging (=> update selection)
            else if (ImGui::IsMouseDragging(0) && ImGui::IsMouseDown(0)) {
                io.WantCaptureMouse    = true;
                mState.mCursorPosition = mInteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos());
                SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);
                resetBlinking=true;
            }
            if (resetBlinking)
                ResetCursorBlinkTime();
        }
    }
}

void TextEditor::setColorRange(Coordinates start, Coordinates end, PaletteIndex aColorIndex) {
    auto startLineNumber = start.mLine;
    auto endLineNumber = end.mLine;
    if (startLineNumber > endLineNumber || endLineNumber >= mLines.size() || startLineNumber >= mLines.size() ||
        start.mColumn > mLines[startLineNumber].size() || end.mColumn > mLines[endLineNumber].size())
        return;
    for(auto lineNumber = startLineNumber; lineNumber <= endLineNumber; lineNumber++) {
        auto &line  = mLines[lineNumber];
        if (line.empty())
            continue;
        auto final = (lineNumber == endLineNumber) ? end.mColumn : line.size();
        for (auto i = start.mColumn; i <= final; i++)
            line[i].mColorIndex = aColorIndex;
    }
}

void TextEditor::Render() {
    /* Compute mCharAdvance regarding scaled font size (Ctrl + mouse wheel)*/
    const float fontSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, "#", nullptr, nullptr).x;
    mCharAdvance         = ImVec2(fontSize, ImGui::GetTextLineHeightWithSpacing() * mLineSpacing);

    /* Update palette with the current alpha from style */
    for (int32_t i = 0; i < (int32_t)PaletteIndex::Max; ++i) {
        auto color = ImGui::ColorConvertU32ToFloat4(sPaletteBase[i]);
        color.w *= ImGui::GetStyle().Alpha;
        mPalette[i] = ImGui::ColorConvertFloat4ToU32(color);
    }

    assert(mLineBuffer.empty());

    auto contentSize = ImGui::GetWindowContentRegionMax() - ImVec2(0,mTopMargin);
    auto drawList    = ImGui::GetWindowDrawList();
    float longest(mTextStart);

    if (mScrollToTop) {
        mScrollToTop = false;
        ImGui::SetScrollY(0.f);
    }

    if ( mScrollToBottom && ImGui::GetScrollMaxY() > ImGui::GetScrollY()) {
        mScrollToBottom = false;
        ImGui::SetScrollY(ImGui::GetScrollMaxY());
    }

    ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos() + ImVec2(0, mTopMargin);
    auto scrollX           = ImGui::GetScrollX();
    auto scrollY           = ImGui::GetScrollY();

    auto lineNo        = (int32_t)(std::floor(scrollY / mCharAdvance.y));// + linesAdded);
    auto globalLineMax = (int32_t)mLines.size();
    auto lineMax       = std::max(0, std::min((int32_t)mLines.size() - 1, lineNo + (int32_t)std::ceil((scrollY + contentSize.y) / mCharAdvance.y)));

    // Deduce mTextStart by evaluating mLines size (global lineMax) plus two spaces as text width
    char buf[16];

    if (mShowLineNumbers)
        snprintf(buf, 16, " %d ", globalLineMax);
    else
        buf[0] = '\0';
    mTextStart = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf, nullptr, nullptr).x + mLeftMargin;

    if (!mLines.empty()) {
        float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ", nullptr, nullptr).x;
        while (lineNo <= lineMax) {
            ImVec2 lineStartScreenPos = ImVec2(cursorScreenPos.x, cursorScreenPos.y + lineNo * mCharAdvance.y);
            ImVec2 textScreenPos      = ImVec2(lineStartScreenPos.x + mTextStart, lineStartScreenPos.y);

            auto &line    = mLines[lineNo];
            longest       = std::max(mTextStart + TextDistanceToLineStart(Coordinates(lineNo, GetLineMaxColumn(lineNo))), longest);
            auto columnNo = 0;
            Coordinates lineStartCoord(lineNo, 0);
            Coordinates lineEndCoord(lineNo, GetLineMaxColumn(lineNo));

            // Draw selection for the current line
            float sstart = -1.0f;
            float ssend  = -1.0f;

            assert(mState.mSelectionStart <= mState.mSelectionEnd);
            if (mState.mSelectionStart <= lineEndCoord)
                sstart = mState.mSelectionStart > lineStartCoord ? TextDistanceToLineStart(mState.mSelectionStart) : 0.0f;
            if (mState.mSelectionEnd > lineStartCoord)
                ssend = TextDistanceToLineStart(mState.mSelectionEnd < lineEndCoord ? mState.mSelectionEnd : lineEndCoord);

            if (mState.mSelectionEnd.mLine > lineNo)
                ssend += mCharAdvance.x;

            if (sstart != -1 && ssend != -1 && sstart < ssend) {
                ImVec2 vstart(lineStartScreenPos.x + mTextStart + sstart, lineStartScreenPos.y);
                ImVec2 vend(lineStartScreenPos.x + mTextStart + ssend, lineStartScreenPos.y + mCharAdvance.y);
                drawList->AddRectFilled(vstart, vend, mPalette[(int32_t)PaletteIndex::Selection]);
            }

            auto start = ImVec2(lineStartScreenPos.x + scrollX, lineStartScreenPos.y);

            // Draw line number (right aligned)
            snprintf(buf, 16, "%d  ", lineNo + 1);

            auto lineNoWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf, nullptr, nullptr).x;
            drawList->AddText(ImVec2(lineStartScreenPos.x + mTextStart - lineNoWidth, lineStartScreenPos.y), mPalette[(int32_t)PaletteIndex::LineNumber], buf);

            // Draw breakpoints
            if (mBreakpoints.count(lineNo + 1) != 0) {
                auto end = ImVec2(lineStartScreenPos.x + contentSize.x + 2.0f * scrollX, lineStartScreenPos.y + mCharAdvance.y);
                drawList->AddRectFilled(start + ImVec2(mTextStart, 0), end, mPalette[(int32_t)PaletteIndex::Breakpoint]);

                drawList->AddCircleFilled(start + ImVec2(0, mCharAdvance.y) / 2, mCharAdvance.y / 3, mPalette[(int32_t)PaletteIndex::Breakpoint]);
                drawList->AddCircle(start + ImVec2(0, mCharAdvance.y) / 2, mCharAdvance.y / 3, mPalette[(int32_t)PaletteIndex::Default]);
            }

            if (mState.mCursorPosition.mLine == lineNo && mShowCursor) {
                bool focused = ImGui::IsWindowFocused();
                ImGuiViewport *viewport = ImGui::GetWindowViewport();

                // Highlight the current line (where the cursor is)
                if (!HasSelection()) {
                    auto end = ImVec2(start.x + contentSize.x + scrollX, start.y + mCharAdvance.y);
                    drawList->AddRectFilled(start, end, mPalette[(int32_t)(focused ? PaletteIndex::CurrentLineFill : PaletteIndex::CurrentLineFillInactive)]);
                    drawList->AddRect(start, end, mPalette[(int32_t)PaletteIndex::CurrentLineEdge], 1.0f);
                }

                // Render the cursor
                if (focused) {
                    auto timeEnd = ImGui::GetTime() * 1000;
                    auto elapsed = timeEnd - mStartTime;
                    if (elapsed > sCursorBlinkOnTime) {
                        float width = 1.0f;
                        auto charIndex = GetCharacterIndex(mState.mCursorPosition);
                        float cx    = TextDistanceToLineStart(mState.mCursorPosition);

                        if (mOverwrite && charIndex < (int32_t)line.size()) {
                            auto c = line[charIndex].mChar;
                            if (c == '\t') {
                                auto x = (1.0f + std::floor((1.0f + cx) / (float(mTabSize) * spaceSize))) * (float(mTabSize) * spaceSize);
                                width  = x - cx;
                            } else {
                                char buf2[2];
                                buf2[0] = line[charIndex].mChar;
                                buf2[1] = '\0';
                                width   = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf2).x;
                            }
                        }
                        ImVec2 charStart(textScreenPos.x + cx, lineStartScreenPos.y);
                        ImVec2 charEnd(textScreenPos.x + cx + width, lineStartScreenPos.y + mCharAdvance.y);
                        drawList->AddRectFilled(charStart, charEnd, mPalette[(int32_t)PaletteIndex::Cursor]);
                        if (elapsed > sCursorBlinkInterval)
                            mStartTime = timeEnd;
                    }
                }
            }

            // Render colorized text
            auto prevColor = line.empty() ? mPalette[(int32_t)PaletteIndex::Default] : mPalette[(int32_t)line[0].mColorIndex];
            ImVec2 bufferOffset;
            std::string lineString;

            for (int32_t i = 0; i < line.size();) {

                auto &glyph = line[i];
                lineString += glyph.mChar;
                auto color  = mPalette[(int32_t)glyph.mColorIndex];
                bool underSquiggled = false;
                ErrorMarkers::iterator errorIt;
                if (mErrorMarkers.size() > 0) {
                    errorIt = mErrorMarkers.find(Coordinates(lineNo+1,i));
                    if (errorIt != mErrorMarkers.end()) {
                        underSquiggled = true;
                    }
                }
                if ((color != prevColor || glyph.mChar == '\t' || glyph.mChar == ' ') && !mLineBuffer.empty()) {
                    const ImVec2 newOffset(textScreenPos.x + bufferOffset.x, textScreenPos.y + bufferOffset.y);
                    drawList->AddText(newOffset, prevColor, mLineBuffer.c_str());
                    auto textSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f,
                                                                    mLineBuffer.c_str(), nullptr, nullptr);
                    bufferOffset.x += textSize.x;
                    mLineBuffer.clear();
                }
                if (underSquiggled) {
                    underSquiggled = false;
                    auto textStart = TextDistanceToLineStart(Coordinates(lineNo, i-1)) + mTextStart;
                    auto begin = ImVec2(lineStartScreenPos.x + textStart, lineStartScreenPos.y);
                    auto end = UnderSquiggles(begin, errorIt->second.first, mPalette[(int32_t) PaletteIndex::ErrorMarker]);
                    mErrorHoverBoxes[Coordinates(lineNo+1,i)]=std::make_pair(begin,end);
                }

                prevColor = color;

                if (glyph.mChar == '\t') {
                    auto oldX      = bufferOffset.x;
                    bufferOffset.x = (1.0f + std::floor((1.0f + bufferOffset.x) / (float(mTabSize) * spaceSize)));
                    bufferOffset.x *= (float(mTabSize) * spaceSize);
                    ++i;

                    if (mShowWhitespaces) {
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
                } else if (glyph.mChar == ' ') {
                    if (mShowWhitespaces) {
                        const auto s = ImGui::GetFontSize();
                        const auto x = textScreenPos.x + bufferOffset.x + spaceSize * 0.5f;
                        const auto y = textScreenPos.y + bufferOffset.y + s * 0.5f;
                        drawList->AddCircleFilled(ImVec2(x, y), 1.5f, 0x80808080, 4);
                    }
                    bufferOffset.x += spaceSize;
                    i++;
                } else {
                    auto l = UTF8CharLength(glyph.mChar);
                    while (l-- > 0)
                        mLineBuffer.push_back(line[i++].mChar);
                }
                ++columnNo;
            }
            bool underSquiggled = false;
            ErrorMarkers::iterator errorIt;
            if (mErrorMarkers.size() > 0) {
                errorIt = mErrorMarkers.find(Coordinates(lineNo + 1, line.size()));
                if (errorIt != mErrorMarkers.end())
                    underSquiggled = true;
            }

            if (!mLineBuffer.empty()) {
                const ImVec2 newOffset(textScreenPos.x + bufferOffset.x, textScreenPos.y + bufferOffset.y);
                drawList->AddText(newOffset, prevColor, mLineBuffer.c_str());
                mLineBuffer.clear();
            }
            if (underSquiggled) {
                underSquiggled = false;
                auto textStart = TextDistanceToLineStart(Coordinates(lineNo, line.size()-1)) + mTextStart;
                auto begin = ImVec2(lineStartScreenPos.x + textStart, lineStartScreenPos.y);
                auto end = UnderSquiggles(begin, errorIt->second.first,mPalette[(int32_t) PaletteIndex::ErrorMarker]);
                mErrorHoverBoxes[Coordinates(lineNo+1,line.size())]=std::make_pair(begin,end);
            }

            ++lineNo;
        }
        if (lineNo < mLines.size() && ImGui::GetScrollMaxX() > 0.0f)
            longest       = std::max(mTextStart + TextDistanceToLineStart(Coordinates(lineNo, GetLineMaxColumn(lineNo))), longest);

    }

    ImGui::Dummy(ImVec2((longest + 2), mLines.size() * mCharAdvance.y));

    if (mScrollToCursor) {
        EnsureCursorVisible();
        mScrollToCursor = false;
    }

    for (auto [key,value] : mErrorMarkers) {
        auto start = mErrorHoverBoxes[key].first;
        auto end = mErrorHoverBoxes[key].second;
        if (ImGui::IsMouseHoveringRect(start, end)) {
            ImGui::BeginTooltip();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
            ImGui::Text("Error at line %d:", key.mLine);
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.2f, 1.0f));
            ImGui::Text("%s", value.second.c_str());
            ImGui::PopStyleColor();
            ImGui::EndTooltip();
        }
    }

    ImGuiPopupFlags_ popup_flags = ImGuiPopupFlags_None;
    ImGuiContext& g = *GImGui;
    auto oldTopMargin = mTopMargin;
    auto popupStack = g.OpenPopupStack;
    if (popupStack.Size > 0) {
        for (int32_t n = 0; n < popupStack.Size; n++){
            auto window = popupStack[n].Window;
            if (window != nullptr) {
                if (window->Size.x == mFindReplaceHandler.GetFindWindowSize().x &&
                    window->Size.y == mFindReplaceHandler.GetFindWindowSize().y &&
                    window->Pos.x == mFindReplaceHandler.GetFindWindowPos().x &&
                    window->Pos.y == mFindReplaceHandler.GetFindWindowPos().y) {
                    mTopMargin = mFindReplaceHandler.GetFindWindowSize().y;
                }
            }
        }
    } else {
        mTopMargin = 0;
    }

    static float linesAdded = 0;
    static float pixelsAdded = 0;
    static float savedScrollY = 0;
    static float shiftedScrollY = 0;
    if (mTopMargin != oldTopMargin) {
        if (oldTopMargin == 0)
            savedScrollY = ImGui::GetScrollY();
        auto window = ImGui::GetCurrentWindow();
        auto maxScroll = window->ScrollMax.y;
        if (maxScroll > 0) {
            float lineCount;
            float pixelCount;
            if (mTopMargin > oldTopMargin) {
                pixelCount = mTopMargin - oldTopMargin;
                lineCount = pixelCount / mCharAdvance.y;
            } else if (mTopMargin > 0) {
                pixelCount = oldTopMargin - mTopMargin;
                lineCount = pixelCount / mCharAdvance.y;
            } else {
                pixelCount = oldTopMargin;
                lineCount = std::round(linesAdded);
            }
            auto state = mState;
            auto oldScrollY = ImGui::GetScrollY();
            int32_t lineCountInt;

            if (mTopMargin > oldTopMargin) {
                lineCountInt = std::round(lineCount + linesAdded - std::floor(linesAdded));
            } else
                lineCountInt = std::round(lineCount);
            for (int32_t i = 0; i < lineCountInt; i++) {
                if (mTopMargin > oldTopMargin)
                    mLines.insert(mLines.begin() + mLines.size(), Line());
                else
                    mLines.erase(mLines.begin() + mLines.size() - 1);
            }
            if (mTopMargin > oldTopMargin) {
                linesAdded += lineCount;
                pixelsAdded += pixelCount;
            } else if (mTopMargin > 0) {
                linesAdded -= lineCount;
                pixelsAdded -= pixelCount;
            } else {
                linesAdded = 0;
                pixelsAdded = 0;
            }
            if (oldScrollY + pixelCount < maxScroll) {
                if (mTopMargin > oldTopMargin)
                    shiftedScrollY = oldScrollY + pixelCount;
                else if (mTopMargin > 0)
                    shiftedScrollY = oldScrollY - pixelCount;
                else if (ImGui::GetScrollY() == shiftedScrollY)
                    shiftedScrollY = savedScrollY;
                else
                    shiftedScrollY = ImGui::GetScrollY() - pixelCount;
                ImGui::SetScrollY(shiftedScrollY);
            } else {
                if (mTopMargin > oldTopMargin)
                    mScrollToBottom = true;
            }
            mState = state;
        }
    }
}

void TextEditor::Render(const char *aTitle, const ImVec2 &aSize, bool aBorder) {
    mWithinRender          = true;
    //mTextChanged           = false;
    mCursorPositionChanged = false;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(mPalette[(int32_t)PaletteIndex::Background]));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    if (!mIgnoreImGuiChild)
        ImGui::BeginChild(aTitle, aSize, aBorder, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoMove);

    if (mHandleKeyboardInputs) {
        HandleKeyboardInputs();
        ImGui::PushTabStop(true);
    }

    if (mHandleMouseInputs)
        HandleMouseInputs();

    Render();

    if (mHandleKeyboardInputs)
        ImGui::PopTabStop();

    if (!mIgnoreImGuiChild)
        ImGui::EndChild();

    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    mWithinRender = false;
}

void TextEditor::clearLines(){
    for (auto line : mLines)
        line.clear();
    mLines.clear();
}

void TextEditor::SetText(const std::string &aText) {
    clearLines();
    mLines.emplace_back(Line());
    for (auto chr : aText) {
        if (chr == '\r') {
            // ignore the carriage return character
        } else if (chr == '\n')
            mLines.emplace_back(Line());
        else {
            mLines.back().emplace_back(Glyph(chr, PaletteIndex::Default));
        }
    }

    mTextChanged = true;
    mScrollToTop = true;

    mUndoBuffer.clear();
    mUndoIndex = 0;
}

void TextEditor::SetTextLines(const std::vector<std::string> &aLines) {
    mLines.clear();

    if (aLines.empty()) {
        mLines.emplace_back(Line());
    } else {
        mLines.resize(aLines.size());

        for (size_t i = 0; i < aLines.size(); ++i) {
            const std::string &aLine = aLines[i];

            mLines[i].reserve(aLine.size());
            for (size_t j = 0; j < aLine.size(); ++j)
                mLines[i].emplace_back(Glyph(aLine[j], PaletteIndex::Default));
        }
    }

    mTextChanged = true;
    mScrollToTop = true;

    mUndoBuffer.clear();
    mUndoIndex = 0;
}

void TextEditor::EnterCharacter(ImWchar aChar, bool aShift) {
    assert(!mReadOnly);
    mFindReplaceHandler.GetMatches().clear();
    UndoRecord u;

    u.mBefore = mState;

    ResetCursorBlinkTime();

    if (HasSelection()) {
        if (aChar == '\t' && mState.mSelectionStart.mLine != mState.mSelectionEnd.mLine) {

            auto start       = mState.mSelectionStart;
            auto end         = mState.mSelectionEnd;
            auto originalEnd = end;

            if (start > end)
                std::swap(start, end);
            start.mColumn = 0;

            if (end.mColumn == 0 && end.mLine > 0)
                --end.mLine;
            if (end.mLine >= (int32_t)mLines.size())
                end.mLine = mLines.empty() ? 0 : (int32_t)mLines.size() - 1;
            end.mColumn = GetLineMaxColumn(end.mLine);

            u.mRemovedStart = start;
            u.mRemovedEnd   = end;
            u.mRemoved      = GetText(start, end);

            bool modified = false;

            for (int32_t i = start.mLine; i <= end.mLine; i++) {
                auto &line = mLines[i];
                if (aShift) {
                    if (!line.empty()) {
                        if (line.front().mChar == '\t') {
                            line.erase(line.begin());
                            modified = true;
                        } else {
                            for (int32_t j = 0; j < mTabSize && !line.empty() && line.front().mChar == ' '; j++) {
                                line.erase(line.begin());
                                modified = true;
                            }
                        }
                    }
                } else {
                    for (int32_t j = start.mColumn % mTabSize; j < mTabSize; j++)
                        line.insert(line.begin(), Glyph(' ', PaletteIndex::Background));
                    modified = true;
                }
            }

            if (modified) {
                start = Coordinates(start.mLine, GetCharacterColumn(start.mLine, 0));
                Coordinates rangeEnd;
                if (originalEnd.mColumn != 0) {
                    end      = Coordinates(end.mLine, GetLineMaxColumn(end.mLine));
                    rangeEnd = end;
                    u.mAdded = GetText(start, end);
                } else {
                    end      = Coordinates(originalEnd.mLine, 0);
                    rangeEnd = Coordinates(end.mLine - 1, GetLineMaxColumn(end.mLine - 1));
                    u.mAdded = GetText(start, rangeEnd);
                }

                u.mAddedStart = start;
                u.mAddedEnd   = rangeEnd;
                u.mAfter      = mState;

                mState.mSelectionStart = start;
                mState.mSelectionEnd   = end;
                AddUndo(u);

                mTextChanged = true;

                EnsureCursorVisible();
            }

            return;
        }    // c == '\t'
        else {
            u.mRemoved      = GetSelectedText();
            u.mRemovedStart = mState.mSelectionStart;
            u.mRemovedEnd   = mState.mSelectionEnd;
            DeleteSelection();
        }
    }    // HasSelection

    auto coord    = GetActualCursorCoordinates();
    u.mAddedStart = coord;

    assert(!mLines.empty());

    if (aChar == '\n') {
        InsertLine(coord.mLine + 1);
        auto &line    = mLines[coord.mLine];
        auto &newLine = mLines[coord.mLine + 1];

       // if (mLanguageDefinition.mAutoIndentation)
       for (size_t it = 0; it < line.size() && isascii(line[it].mChar) && isblank(line[it].mChar); ++it)
           newLine.push_back(line[it]);

        const size_t whitespaceSize = newLine.size();
        int32_t cstart              = 0;
        int32_t cpos                = 0;
        auto cindex                 = GetCharacterIndex(coord);
        if (cindex < whitespaceSize) {// && mLanguageDefinition.mAutoIndentation) {
            cstart = (int32_t) whitespaceSize;
            cpos = cindex;
        } else {
            cstart = cindex;
            cpos = (int32_t) whitespaceSize;
        }
        newLine.insert(newLine.end(), line.begin() + cstart, line.end());
        line.erase(line.begin() + cstart, line.begin() + line.size());
        SetCursorPosition(Coordinates(coord.mLine + 1, GetCharacterColumn(coord.mLine + 1, cpos)));
        u.mAdded = (char)aChar;
    } else if (aChar == '\t') {
        auto &line  = mLines[coord.mLine];
        auto cindex = GetCharacterIndex(coord);

        if (!aShift) {
            auto spacesToInsert = mTabSize - (cindex % mTabSize);
            for (int32_t j = 0; j < spacesToInsert; j++)
                line.insert(line.begin() + cindex, Glyph(' ', PaletteIndex::Background));
            SetCursorPosition(Coordinates(coord.mLine, GetCharacterColumn(coord.mLine, cindex + spacesToInsert)));
        } else {
            auto spacesToRemove = (cindex % mTabSize);
            if (spacesToRemove == 0) spacesToRemove = 4;
            spacesToRemove = std::min(spacesToRemove, (int32_t) line.size());
            for (int32_t j = 0; j < spacesToRemove; j++) {
                if ((line.begin() + cindex - 1)->mChar == ' ') {
                    line.erase(line.begin() + cindex - 1);
                    cindex -= 1;
                }
            }

            SetCursorPosition(Coordinates(coord.mLine, GetCharacterColumn(coord.mLine, std::max(0, cindex))));
        }

    } else {
        char buf[7];
        int32_t e = ImTextCharToUtf8(buf, 7, aChar);
        if (e > 0) {
            buf[e]      = '\0';
            auto &line  = mLines[coord.mLine];
            auto cindex = GetCharacterIndex(coord);

            if (mOverwrite && cindex < (int32_t)line.size()) {
                auto d = UTF8CharLength(line[cindex].mChar);

                u.mRemovedStart = mState.mCursorPosition;
                u.mRemovedEnd   = Coordinates(coord.mLine, GetCharacterColumn(coord.mLine, cindex + d));

                while (d-- > 0 && cindex < (int32_t)line.size()) {
                    u.mRemoved += line[cindex].mChar;
                    line.erase(line.begin() + cindex);
                }
            }

            //line.resize(line.size() + e);
            for (auto p = buf; *p != '\0'; p++, ++cindex)
                line.insert(line.begin() + cindex, Glyph(*p, PaletteIndex::Default));
            u.mAdded = buf;

            SetCursorPosition(Coordinates(coord.mLine, GetCharacterColumn(coord.mLine, cindex)));
        } else
            return;
    }

    mTextChanged = true;

    u.mAddedEnd = GetActualCursorCoordinates();
    u.mAfter    = mState;

    AddUndo(u);

    EnsureCursorVisible();
}

void TextEditor::SetReadOnly(bool aValue) {
    mReadOnly = aValue;
}

void TextEditor::SetCursorPosition(const Coordinates &aPosition) {
    if (mState.mCursorPosition != aPosition) {
        mState.mCursorPosition = aPosition;
        mCursorPositionChanged = true;
        EnsureCursorVisible();
    }
}

void TextEditor::SetSelectionStart(const Coordinates &aPosition) {
    mState.mSelectionStart = SanitizeCoordinates(aPosition);
    if (mState.mSelectionStart > mState.mSelectionEnd)
        std::swap(mState.mSelectionStart, mState.mSelectionEnd);
}

void TextEditor::SetSelectionEnd(const Coordinates &aPosition) {
    mState.mSelectionEnd = SanitizeCoordinates(aPosition);
    if (mState.mSelectionStart > mState.mSelectionEnd)
        std::swap(mState.mSelectionStart, mState.mSelectionEnd);
}

void TextEditor::SetSelection(const Coordinates &aStart, const Coordinates &aEnd, SelectionMode aMode) {
    auto oldSelStart = mState.mSelectionStart;
    auto oldSelEnd   = mState.mSelectionEnd;

    mState.mSelectionStart = SanitizeCoordinates(aStart);
    mState.mSelectionEnd   = SanitizeCoordinates(aEnd);
    if (mState.mSelectionStart > mState.mSelectionEnd)
        std::swap(mState.mSelectionStart, mState.mSelectionEnd);

    switch (aMode) {
        case TextEditor::SelectionMode::Normal:
            break;
        case TextEditor::SelectionMode::Word:
            {
                mState.mSelectionStart = FindWordStart(mState.mSelectionStart);
                if (!IsOnWordBoundary(mState.mSelectionEnd))
                    mState.mSelectionEnd = FindWordEnd(FindWordStart(mState.mSelectionEnd));
                break;
            }
        case TextEditor::SelectionMode::Line:
            {
                const auto lineNo      = mState.mSelectionEnd.mLine;
                const auto lineSize    = (size_t)lineNo < mLines.size() ? mLines[lineNo].size() : 0;
                mState.mSelectionStart = Coordinates(mState.mSelectionStart.mLine, 0);
                mState.mSelectionEnd   = Coordinates(lineNo, GetLineMaxColumn(lineNo));
                break;
            }
        default:
            break;
    }

    if (mState.mSelectionStart != oldSelStart ||
        mState.mSelectionEnd != oldSelEnd)
        mCursorPositionChanged = true;
}

void TextEditor::SetTabSize(int32_t aValue) {
    mTabSize = std::max(0, std::min(32, aValue));
}

void TextEditor::InsertColoredText(const std::string &aValue, TextEditor::PaletteIndex aColorIndex) {
    if (aValue == "")
        return;

    auto pos       = GetActualCursorCoordinates();
    auto start     = std::min(pos, mState.mSelectionStart);
    int32_t totalLines = pos.mLine - start.mLine;

    totalLines += InsertTextAt(pos, aValue.c_str());

    SetSelection(pos, pos);
    SetCursorPosition(pos);
    for (auto lineNumber = start.mLine; lineNumber <= start.mLine + totalLines; lineNumber++) {
        auto &line = mLines[lineNumber];
        for (auto &glyph : line)
            glyph.mColorIndex = aColorIndex;
    }
}

void TextEditor::InsertText(const std::string &aValue) {
    InsertText(aValue.c_str());
}

void TextEditor::InsertText(const char *aValue) {
    mFindReplaceHandler.GetMatches().clear();
    if (aValue == nullptr)
        return;

    auto pos       = GetActualCursorCoordinates();
    auto start     = std::min(pos, mState.mSelectionStart);

    InsertTextAt(pos, aValue);

    SetSelection(pos, pos);
    SetCursorPosition(pos);
}

void TextEditor::DeleteSelection() {
    mFindReplaceHandler.GetMatches().clear();
    assert(mState.mSelectionEnd >= mState.mSelectionStart);

    if (mState.mSelectionEnd == mState.mSelectionStart)
        return;

    DeleteRange(mState.mSelectionStart, mState.mSelectionEnd);

    SetSelection(mState.mSelectionStart, mState.mSelectionStart);
    SetCursorPosition(mState.mSelectionStart);
}

void TextEditor::MoveUp(int32_t aAmount, bool aSelect) {
    ResetCursorBlinkTime();
    auto oldPos                  = mState.mCursorPosition;
    mState.mCursorPosition.mLine = std::max(0, mState.mCursorPosition.mLine - aAmount);
    if (oldPos != mState.mCursorPosition) {
        if (aSelect) {
            if (oldPos == mInteractiveStart)
                mInteractiveStart = mState.mCursorPosition;
            else if (oldPos == mInteractiveEnd)
                mInteractiveEnd = mState.mCursorPosition;
            else {
                mInteractiveStart = mState.mCursorPosition;
                mInteractiveEnd   = oldPos;
            }
        } else
            mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
        SetSelection(mInteractiveStart, mInteractiveEnd);

        EnsureCursorVisible();
    }
}

void TextEditor::MoveDown(int32_t aAmount, bool aSelect) {
    assert(mState.mCursorPosition.mColumn >= 0);
    ResetCursorBlinkTime();
    auto oldPos                  = mState.mCursorPosition;
    mState.mCursorPosition.mLine = std::max(0, std::min((int32_t)mLines.size() - 1, mState.mCursorPosition.mLine + aAmount));

    if (mState.mCursorPosition != oldPos) {
        if (aSelect) {
            if (oldPos == mInteractiveEnd)
                mInteractiveEnd = mState.mCursorPosition;
            else if (oldPos == mInteractiveStart)
                mInteractiveStart = mState.mCursorPosition;
            else {
                mInteractiveStart = oldPos;
                mInteractiveEnd   = mState.mCursorPosition;
            }
        } else
            mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
        SetSelection(mInteractiveStart, mInteractiveEnd);

        EnsureCursorVisible();
    }
}

static bool IsUTFSequence(char c) {
    return (c & 0xC0) == 0x80;
}

void TextEditor::MoveLeft(int32_t aAmount, bool aSelect, bool aWordMode) {
    ResetCursorBlinkTime();
    if (mLines.empty())
        return;

    auto oldPos            = mState.mCursorPosition;
    mState.mCursorPosition = GetActualCursorCoordinates();
    auto line              = mState.mCursorPosition.mLine;
    auto cindex            = GetCharacterIndex(mState.mCursorPosition);

    while (aAmount-- > 0) {
        if (cindex == 0) {
            if (line > 0) {
                --line;
                if ((int32_t)mLines.size() > line)
                    cindex = (int32_t)mLines[line].size();
                else
                    cindex = 0;
            }
        } else {
            --cindex;
            if (cindex > 0) {
                if ((int32_t)mLines.size() > line) {
                    while (cindex > 0 && IsUTFSequence(mLines[line][cindex].mChar))
                        --cindex;
                }
            }
        }

        mState.mCursorPosition = Coordinates(line, GetCharacterColumn(line, cindex));
        if (aWordMode) {
            mState.mCursorPosition = FindWordStart(mState.mCursorPosition);
            cindex                 = GetCharacterIndex(mState.mCursorPosition);
        }
    }

    mState.mCursorPosition = Coordinates(line, GetCharacterColumn(line, cindex));

    assert(mState.mCursorPosition.mColumn >= 0);
    if (aSelect) {
        if (oldPos == mInteractiveStart)
            mInteractiveStart = mState.mCursorPosition;
        else if (oldPos == mInteractiveEnd)
            mInteractiveEnd = mState.mCursorPosition;
        else {
            mInteractiveStart = mState.mCursorPosition;
            mInteractiveEnd   = oldPos;
        }
    } else
        mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
    SetSelection(mInteractiveStart, mInteractiveEnd, aSelect && aWordMode ? SelectionMode::Word : SelectionMode::Normal);

    EnsureCursorVisible();
}

void TextEditor::MoveRight(int32_t aAmount, bool aSelect, bool aWordMode) {
    ResetCursorBlinkTime();
    auto oldPos = mState.mCursorPosition;

    if (mLines.empty() || oldPos.mLine >= mLines.size())
        return;

    auto cindex = GetCharacterIndex(mState.mCursorPosition);
    while (aAmount-- > 0) {
        auto lindex = mState.mCursorPosition.mLine;
        auto &line  = mLines[lindex];

        if (cindex >= line.size()) {
            if (mState.mCursorPosition.mLine < mLines.size() - 1) {
                mState.mCursorPosition.mLine   = std::max(0, std::min((int32_t)mLines.size() - 1, mState.mCursorPosition.mLine + 1));
                mState.mCursorPosition.mColumn = 0;
            } else
                return;
        } else {
            cindex += UTF8CharLength(line[cindex].mChar);
            mState.mCursorPosition = Coordinates(lindex, GetCharacterColumn(lindex, cindex));
            if (aWordMode) {
                auto save = mState.mCursorPosition;
                mState.mCursorPosition = FindWordEnd(mState.mCursorPosition);
                auto previous = mState.mCursorPosition;
                MoveLeft(1, false, true);
                while (mState.mCursorPosition >= save){
                    previous = mState.mCursorPosition;
                    MoveLeft(1, false, true);
                }
                mState.mCursorPosition = previous;
                cindex = GetCharacterIndex(mState.mCursorPosition);
            }
        }
    }

    if (aSelect) {
        if (oldPos == mInteractiveEnd)
            mInteractiveEnd = SanitizeCoordinates(mState.mCursorPosition);
        else if (oldPos == mInteractiveStart)
            mInteractiveStart = mState.mCursorPosition;
        else {
            mInteractiveStart = oldPos;
            mInteractiveEnd   = mState.mCursorPosition;
        }
    } else
        mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
    SetSelection(mInteractiveStart, mInteractiveEnd, aSelect && aWordMode ? SelectionMode::Word : SelectionMode::Normal);

    EnsureCursorVisible();
}

void TextEditor::MoveTop(bool aSelect) {
    ResetCursorBlinkTime();
    auto oldPos = mState.mCursorPosition;
    SetCursorPosition(Coordinates(0, 0));

    if (mState.mCursorPosition != oldPos && aSelect) {
        if (oldPos == mInteractiveStart)
            mInteractiveStart = mState.mCursorPosition;
        else if (oldPos == mInteractiveEnd)
            mInteractiveEnd = mState.mCursorPosition;
        else {
            mInteractiveStart = mState.mCursorPosition;
            mInteractiveEnd = oldPos;
        }
    } else
        mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
    SetSelection(mInteractiveStart, mInteractiveEnd);

}

void TextEditor::TextEditor::MoveBottom(bool aSelect) {
    ResetCursorBlinkTime();
    auto oldPos = GetCursorPosition();
    auto newPos = Coordinates((int32_t)mLines.size() - 1, GetLineMaxColumn((int32_t)mLines.size() - 1));
    SetCursorPosition(newPos);
    if (mState.mCursorPosition != oldPos && aSelect) {
        if (oldPos == mInteractiveEnd)
            mInteractiveEnd = mState.mCursorPosition;
        else if (oldPos == mInteractiveStart)
            mInteractiveStart = mState.mCursorPosition;
        else {
            mInteractiveStart = oldPos;
            mInteractiveEnd = mState.mCursorPosition;
        }
    } else
        mInteractiveStart = mInteractiveEnd = newPos;
    SetSelection(mInteractiveStart, mInteractiveEnd);
}

void TextEditor::MoveHome(bool aSelect) {
    ResetCursorBlinkTime();
    auto oldPos = mState.mCursorPosition;
    SetCursorPosition(Coordinates(mState.mCursorPosition.mLine, 0));

    if (mState.mCursorPosition != oldPos && aSelect) {
        if (oldPos == mInteractiveStart)
            mInteractiveStart = mState.mCursorPosition;
        else if (oldPos == mInteractiveEnd)
            mInteractiveEnd = mState.mCursorPosition;
        else {
            mInteractiveStart = mState.mCursorPosition;
            mInteractiveEnd = oldPos;
        }
    } else
        mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
    SetSelection(mInteractiveStart, mInteractiveEnd);
}

void TextEditor::MoveEnd(bool aSelect) {
    ResetCursorBlinkTime();
    auto oldPos = mState.mCursorPosition;
    SetCursorPosition(Coordinates(mState.mCursorPosition.mLine, GetLineMaxColumn(oldPos.mLine)));

    if (mState.mCursorPosition != oldPos && aSelect) {
        if (oldPos == mInteractiveEnd)
            mInteractiveEnd = mState.mCursorPosition;
        else if (oldPos == mInteractiveStart)
            mInteractiveStart = mState.mCursorPosition;
        else {
            mInteractiveStart = oldPos;
            mInteractiveEnd = mState.mCursorPosition;
        }
    } else
        mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
    SetSelection(mInteractiveStart, mInteractiveEnd);
}

void TextEditor::Delete() {
    mFindReplaceHandler.GetMatches().clear();
    ResetCursorBlinkTime();
    assert(!mReadOnly);

    if (mLines.empty())
        return;

    UndoRecord u;
    u.mBefore = mState;

    if (HasSelection()) {
        u.mRemoved      = GetSelectedText();
        u.mRemovedStart = mState.mSelectionStart;
        u.mRemovedEnd   = mState.mSelectionEnd;

        DeleteSelection();
    } else {
        auto pos = GetActualCursorCoordinates();
        SetCursorPosition(pos);
        auto &line = mLines[pos.mLine];

        if (pos.mColumn == GetLineMaxColumn(pos.mLine)) {
            if (pos.mLine == (int32_t)mLines.size() - 1)
                return;

            u.mRemoved      = '\n';
            u.mRemovedStart = u.mRemovedEnd = GetActualCursorCoordinates();
            Advance(u.mRemovedEnd);

            auto &nextLine = mLines[pos.mLine + 1];
            line.insert(line.end(), nextLine.begin(), nextLine.end());
            RemoveLine(pos.mLine + 1);
        } else {
            auto cindex     = GetCharacterIndex(pos);
            u.mRemovedStart = u.mRemovedEnd = GetActualCursorCoordinates();
            u.mRemovedEnd.mColumn++;
            u.mRemoved = GetText(u.mRemovedStart, u.mRemovedEnd);

            auto d = UTF8CharLength(line[cindex].mChar);
            while (d-- > 0 && cindex < (int32_t)line.size())
                line.erase(line.begin() + cindex);
        }

        mTextChanged = true;
    }

    u.mAfter = mState;
    AddUndo(u);
}

void TextEditor::Backspace() {
    mFindReplaceHandler.GetMatches().clear();
    ResetCursorBlinkTime();
    assert(!mReadOnly);

    if (mLines.empty())
        return;

    UndoRecord u;
    u.mBefore = mState;

    if (HasSelection()) {
        u.mRemoved      = GetSelectedText();
        u.mRemovedStart = mState.mSelectionStart;
        u.mRemovedEnd   = mState.mSelectionEnd;

        DeleteSelection();
    } else {
        auto pos = GetActualCursorCoordinates();
        SetCursorPosition(pos);

        if (mState.mCursorPosition.mColumn == 0) {
            if (mState.mCursorPosition.mLine == 0)
                return;

            u.mRemoved      = '\n';
            u.mRemovedStart = u.mRemovedEnd = Coordinates(pos.mLine - 1, GetLineMaxColumn(pos.mLine - 1));
            Advance(u.mRemovedEnd);

            auto &line     = mLines[mState.mCursorPosition.mLine];
            auto &prevLine = mLines[mState.mCursorPosition.mLine - 1];
            auto prevSize  = GetLineMaxColumn(mState.mCursorPosition.mLine - 1);
            prevLine.insert(prevLine.end(), line.begin(), line.end());

            ErrorMarkers etmp;
            for (auto &i : mErrorMarkers)
                etmp.insert(ErrorMarkers::value_type(i.first.mLine - 1 == mState.mCursorPosition.mLine ? Coordinates(i.first.mLine - 1,i.first.mColumn) : i.first, i.second));
            mErrorMarkers = std::move(etmp);

            RemoveLine(mState.mCursorPosition.mLine);
            --mState.mCursorPosition.mLine;
            mState.mCursorPosition.mColumn = prevSize;
        } else {
            auto &line  = mLines[mState.mCursorPosition.mLine];
            auto cindex = GetCharacterIndex(pos) - 1;
            auto cend   = cindex + 1;
            while (cindex > 0 && IsUTFSequence(line[cindex].mChar))
                --cindex;

            u.mRemovedStart = u.mRemovedEnd = GetActualCursorCoordinates();
            --u.mRemovedStart.mColumn;
            mState.mCursorPosition.mColumn = GetCharacterColumn(mState.mCursorPosition.mLine, cindex);

            while (cindex < line.size() && cend-- > cindex) {
                u.mRemoved += line[cindex].mChar;
                line.erase(line.begin() + cindex);
            }
        }

        mTextChanged = true;

        EnsureCursorVisible();
    }

    u.mAfter = mState;
    AddUndo(u);
}

void TextEditor::SelectWordUnderCursor() {
    auto c = GetCursorPosition();
    SetSelection(FindWordStart(c), FindWordEnd(c));
}

void TextEditor::SelectAll() {
    SetSelection(Coordinates(0, 0), Coordinates((int32_t)mLines.size(), 0));
}

bool TextEditor::HasSelection() const {
    return mState.mSelectionEnd > mState.mSelectionStart;
}

void TextEditor::Copy() {
    if (HasSelection()) {
        ImGui::SetClipboardText(GetSelectedText().c_str());
    } else {
        if (!mLines.empty()) {
            std::string str;
            auto &line = mLines[GetActualCursorCoordinates().mLine];
            for (auto &g : line)
                str.push_back(g.mChar);
            ImGui::SetClipboardText(str.c_str());
        }
    }
}

void TextEditor::Cut() {
    mFindReplaceHandler.GetMatches().clear();
    if (IsReadOnly()) {
        Copy();
    } else {
        if (HasSelection()) {
            UndoRecord u;
            u.mBefore       = mState;
            u.mRemoved      = GetSelectedText();
            u.mRemovedStart = mState.mSelectionStart;
            u.mRemovedEnd   = mState.mSelectionEnd;

            Copy();
            DeleteSelection();

            u.mAfter = mState;
            AddUndo(u);
        }
    }
}

void TextEditor::Paste() {
    mFindReplaceHandler.GetMatches().clear();
    if (IsReadOnly())
        return;

    auto clipText = ImGui::GetClipboardText();
    if (clipText != nullptr && strlen(clipText) > 0) {
        UndoRecord u;
        u.mBefore = mState;

        if (HasSelection()) {
            u.mRemoved      = GetSelectedText();
            u.mRemovedStart = mState.mSelectionStart;
            u.mRemovedEnd   = mState.mSelectionEnd;
            DeleteSelection();
        }

        u.mAdded      = clipText;
        u.mAddedStart = GetActualCursorCoordinates();

        InsertText(clipText);

        u.mAddedEnd = GetActualCursorCoordinates();
        u.mAfter    = mState;
        AddUndo(u);
    }
}

bool TextEditor::CanUndo() const {
    return !mReadOnly && mUndoIndex > 0;
}

bool TextEditor::CanRedo() const {
    return !mReadOnly && mUndoIndex < (int32_t)mUndoBuffer.size();
}

void TextEditor::Undo(int32_t aSteps) {
    while (CanUndo() && aSteps-- > 0)
        mUndoBuffer[--mUndoIndex].Undo(this);
}

void TextEditor::Redo(int32_t aSteps) {
    while (CanRedo() && aSteps-- > 0)
        mUndoBuffer[mUndoIndex++].Redo(this);
}

// the index here is array index so zero based
void TextEditor::FindReplaceHandler::SelectFound(TextEditor *editor, int32_t index) {
    assert(index >= 0 && index < mMatches.size());
    auto selectionStart = mMatches[index].mSelectionStart;
    auto selectionEnd = mMatches[index].mSelectionEnd;
    editor->SetSelection(selectionStart, selectionEnd);
    editor->SetCursorPosition(selectionEnd);
    editor->mScrollToCursor = true;
}

// The returned index is shown in the form
//  'index of count' so 1 based
unsigned TextEditor::FindReplaceHandler::FindMatch(TextEditor *editor, bool isNext) {

    if ( editor->mTextChanged || mOptionsChanged) {
        std::string findWord = GetFindWord();
        if (findWord.empty())
            return 0;
        resetMatches();
        FindAllMatches(editor, findWord);
    }

    auto targetPos = editor->mState.mCursorPosition;
    auto count = mMatches.size();

    if (count == 0) {
        editor->SetCursorPosition(targetPos);
        return 0;
    }

    for (unsigned i=0; i < count; i++) {
        if (targetPos >= mMatches[i].mSelectionStart && targetPos <= mMatches[i].mSelectionEnd) {
            if (isNext) {
                if (i == count - 1) {
                    SelectFound(editor, 0);
                    return 1;
                } else {
                    SelectFound(editor, i + 1);
                    return (i + 2);
                }
            } else {
                if (i == 0) {
                    SelectFound(editor, count - 1);
                    return count;
                } else {
                    SelectFound(editor, i - 1);
                    return i;
                }
            }
        }
    }

    if ((targetPos > mMatches[count - 1].mSelectionEnd) || (targetPos < mMatches[0].mSelectionStart)) {
        if (isNext) {
            SelectFound(editor,0);
            return 1;
        } else {
            SelectFound(editor,count - 1);
            return count;
        }
    }

    for (unsigned i=1;i < count;i++) {

        if (mMatches[i - 1].mSelectionEnd <= targetPos &&
            mMatches[i].mSelectionStart >= targetPos ) {
            if (isNext) {
                SelectFound(editor,i);
                return i + 1;
            } else {
                SelectFound(editor,i - 1);
                return i;
            }
        }
    }

    return 0;
}

// returns 1 based index
unsigned TextEditor::FindReplaceHandler::FindPosition( TextEditor *editor, TextEditor::Coordinates targetPos, bool isNext) {
    if ( editor->mTextChanged || mOptionsChanged) {
        std::string findWord = GetFindWord();
        if (findWord.empty())
            return 0;
        resetMatches();
        FindAllMatches(editor,findWord);
    }

    int32_t count = mMatches.size();
    if (count == 0)
        return 0;
    if( isNext) {
        if (targetPos > mMatches[count - 1].mSelectionEnd || targetPos <= mMatches[0].mSelectionEnd)
            return 1;
        for (unsigned i = 1; i < count; i++) {
            if (targetPos > mMatches[i-1].mSelectionEnd && targetPos <= mMatches[i].mSelectionEnd)
                return i+1;
        }
    } else {
        if (targetPos >= mMatches[count - 1].mSelectionStart || targetPos < mMatches[0].mSelectionStart)
            return count;
        for (unsigned i = 1; i < count; i++) {
            if (targetPos >= mMatches[i-1].mSelectionStart && targetPos < mMatches[i].mSelectionStart)
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

// Performs actual search to fill mMatches
bool TextEditor::FindReplaceHandler::FindNext(TextEditor *editor, bool wrapAround) {
    auto curPos = editor->mState.mCursorPosition;
    unsigned long selectionLength = editor->GetStringCharacterCount(mFindWord);
    size_t byteIndex = 0;

    for (size_t ln = 0; ln < curPos.mLine; ln++)
        byteIndex += editor->GetLineByteCount(ln) + 1;
    byteIndex += curPos.mColumn;

    std::string wordLower = mFindWord;
    if (!GetMatchCase())
        std::transform(wordLower.begin(), wordLower.end(), wordLower.begin(), ::tolower);

    std::string textSrc = editor->GetText();
    if (!GetMatchCase())
        std::transform(textSrc.begin(), textSrc.end(), textSrc.begin(), ::tolower);

    size_t textLoc;
    // TODO: use regexp find iterator in all cases
    //  to find all matches for FindAllMatches.
    //  That should make things faster (no need
    //  to call FindNext many times) and remove
    //  clunky match case code
    if (GetWholeWord() || GetFindRegEx()) {
        std::regex regularExpression;
        if (GetFindRegEx()) {
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
        unsigned long firstLength = iter->length();

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
        curPos.mLine = curPos.mColumn = 0;
        byteIndex = 0;

        for (size_t ln = 0; ln < editor->mLines.size(); ln++) {
            auto byteCount = editor->GetLineByteCount(ln) + 1;

            if (byteIndex + byteCount > textLoc) {
                curPos.mLine = ln;
                curPos.mColumn = textLoc - byteIndex;

                auto &line = editor->mLines[curPos.mLine];
                int32_t lineSize = line.size();
                for (int32_t i = 0; i < std::min(lineSize,curPos.mColumn); i++)
                    if (line[i].mChar == '\t')
                        curPos.mColumn += (editor->mTabSize - 1);
                break;
            } else {// just keep adding
                byteIndex += byteCount;
            }
        }
    } else
        return false;

    auto selStart = curPos, selEnd = curPos;
    selEnd.mColumn += selectionLength;
    editor->SetSelection(selStart, selEnd);
    editor->SetCursorPosition(selEnd);
    editor->mScrollToCursor = true;
    return true;
}

void TextEditor::FindReplaceHandler::FindAllMatches(TextEditor *editor,std::string findWord) {

    if (findWord.empty()) {
        editor->mScrollToCursor = true;
        mFindWord = "";
        mMatches.clear();
        return;
    }

    if(findWord == mFindWord && !editor->mTextChanged && !mOptionsChanged)
        return;

    if (mOptionsChanged)
        mOptionsChanged = false;

    mMatches.clear();
    mFindWord = findWord;
    auto startingPos = editor->mState.mCursorPosition;
    auto state = editor->mState;
    Coordinates begin = Coordinates(0,0);
    editor->mState.mCursorPosition = begin;

    if (!FindNext(editor,false)) {
        editor->mState = state;
        editor->mScrollToCursor = true;
        return;
    }
    auto initialPos = editor->mState.mCursorPosition;
    mMatches.push_back(editor->mState);

    while( editor->mState.mCursorPosition < startingPos) {
        if (!FindNext(editor,false)) {
            editor->mState = state;
            editor->mScrollToCursor = true;
            return;
        }
        mMatches.push_back(editor->mState);
    }

    while (FindNext(editor,false))
        mMatches.push_back(editor->mState);

    editor->mState = state;
    editor->mScrollToCursor = true;
    return;
}


bool TextEditor::FindReplaceHandler::Replace(TextEditor *editor, bool next) {

    if (mMatches.empty() || mFindWord == mReplaceWord || mFindWord.empty())
        return false;


    auto state = editor->mState;

    if (editor->mState.mCursorPosition <= editor->mState.mSelectionEnd && editor->mState.mSelectionEnd > editor->mState.mSelectionStart && editor->mState.mCursorPosition > editor->mState.mSelectionStart) {

        editor->mState.mCursorPosition = editor->mState.mSelectionStart;
        if(editor->mState.mCursorPosition.mColumn == 0) {
            editor->mState.mCursorPosition.mLine--;
            editor->mState.mCursorPosition.mColumn = editor->GetLineMaxColumn(editor->mState.mCursorPosition.mLine);
        } else
            editor->mState.mCursorPosition.mColumn--;
    }
    auto matchIndex = FindMatch(editor,next);
    if(matchIndex != 0) {
        UndoRecord u;
        u.mBefore = editor->mState;

        auto selectionEnd = editor->mState.mSelectionEnd;

        u.mRemoved = editor->GetSelectedText();
        u.mRemovedStart = editor->mState.mSelectionStart;
        u.mRemovedEnd = editor->mState.mSelectionEnd;
        auto removedCount = editor->GetStringCharacterCount(u.mRemoved);

        editor->DeleteSelection();
        if (GetFindRegEx()) {
            std::string replacedText = std::regex_replace(editor->GetText(), std::regex(mFindWord), mReplaceWord, std::regex_constants::format_first_only | std::regex_constants::format_no_copy);
            u.mAdded = replacedText;
        } else
            u.mAdded = mReplaceWord;

        u.mAddedStart = editor->GetActualCursorCoordinates();

        editor->InsertText(u.mAdded);
        editor->SetCursorPosition(editor->mState.mSelectionEnd);

        u.mAddedEnd = editor->GetActualCursorCoordinates();
        auto addedCount = editor->GetStringCharacterCount(u.mAdded);
        editor->mScrollToCursor = true;
        ImGui::SetKeyboardFocusHere(0);

        u.mAfter = editor->mState;
        editor->AddUndo(u);
        editor->mTextChanged = true;
        mMatches.erase(mMatches.begin() + matchIndex - 1);
        int32_t correction = addedCount - removedCount;
        if (correction != 0 ) {
            for (unsigned i = matchIndex - 1; i < mMatches.size(); i++) {
                 if (mMatches[i].mSelectionStart.mLine > selectionEnd.mLine)
                     break;
                 mMatches[i].mSelectionStart.mColumn += correction;
                 mMatches[i].mSelectionEnd.mColumn += correction;
                 mMatches[i].mCursorPosition.mColumn += correction;
            }
        }

        return true;
    }
    editor->mState = state;
    return false;
}

bool TextEditor::FindReplaceHandler::ReplaceAll(TextEditor *editor) {
    unsigned count = mMatches.size();

    for (unsigned i = 0; i < count; i++)
        Replace(editor,true);

    return true;
}


const TextEditor::Palette &TextEditor::GetDarkPalette() {
    const static Palette p = {
        {
            0xfff0f0f0, // Default
            0xffd69c56, // Keyword
            0xff00ff00, // Number
            0xff7070e0, // String
            0xff70a0e0, // Char literal
            0xffffffff, // Operator
            0xff408080, // Separator
            0xff400a00, // Preproc identifier
            0xff679794, // Builtin type
            0xff765437, // User Defined type
            0xff408080, // Directive
            0xff586820, // Doc Comment
            0xff708020, // Block Doc Comment
            0xff90a030, // Global Doc Comment
            0xff206020, // Comment (single line)
            0xff406020, // Comment (multi line)
            0xff004545, // Preprocessor deactivated
            0xffe06b5d, // Function
            0xff569cd6, // Attribute
            0xffb250dc, // Namespace
            0xff806906, // Typedef
            0xff4b760d, // Pattern Variable
            0xff9bc64d, // Local Variable
            0xff6b961d, // Pattern Placed Variable
            0xffbde66d, // Template Variable
            0xffdb068d, // Placed Variable
            0xff8bb61d, // Function Variable
            0xff7ba62d, // Function Parameter
            0xff9bcB2d, // Unknown Identifier
            0xffbbe66d, // Global Variable
            0xff151515, // Background
            0xffe0e0e0, // Cursor
            0x80a06020, // Selection
            0x800020ff, // ErrorMarker
            0x40f08000, // Breakpoint
            0xff707000, // Line number
            0x800020ff, // Error text
            0xff408080, // Warning text
            0xff206020, // Debug text
            0xfff0f0f0, // Default text
            0x40000000, // Current line fill
            0x40808080, // Current line fill (inactive)
            0x40a0a0a0 // Current line edge
          }
      };
      return p;
}

const TextEditor::Palette &TextEditor::GetLightPalette() {
    const static Palette p = {
        {
            0xff7f7f7f, // default
            0xffff0c06, // Keyword
            0xff008000, // Number
            0xff2020a0, // String
            0xff304070, // Char literal
            0xff000000, // Operator
            0xff101010, // Separator
            0xff404040, // Identifier
            0xffc040a0, // Preproc identifier
            0xff679794, // Builtin type
            0xff765437, // User Defined type
            0xff406060, // Directive
            0xff707820, // Global Doc Comment
            0xff889020, // Block Doc Comment
            0xff586020, // Doc Comment
            0xff205020, // Comment (single line)
            0xff405020, // Comment (multi line)
            0xffa7cccc, // Preprocessor deactivated
            0xff1f94a2, // Function
            0xff060cff, // Attribute
            0xff401070, // Namespace
            0xff806906, // Typedef
            0xff606010, // Pattern Variable
            0xff808030, // Local Variable
            0xffffffff, // Pattern Placed Variable
            0xff000000, // Placed Variable
            0x80600000, // Function Variable
            0xa00010ff, // Function Parameter
            0x80f08000, // Unknown Identifier
            0xff505000, // Global Variable
            0x40000000,  // Background
            0x40808080,  // Cursor
            0x40000000,  // Selection
                         // ErrorMarker
                         // Breakpoint
                         // Line number
                         // Current line fill
                         // Current line fill
                         // Current line edge
        }
    };
    return p;
}

/*!
 * @brief Get the palette for the Solarized Dark
 *
 * /// Solarized Dark palette
 *
 * @return const TextEditor::Palette&
 */


const TextEditor::Palette &TextEditor::GetRetroBluePalette() {
    const static Palette p = {
        {
            0xff00ffff, // None
            0xffffff00, // Keyword
            0xff00ff00, // Number
            0xff808000, // String
            0xff808000, // Char literal
            0xffffffff, // Punctuation
            0xff00ffff, // Identifier
            0xffff00ff, // Preproc identifier
            0xff679794, // Builtin type
            0xff765437, // User Defined type
            0xff008000, // Directive
            0xff101010, // Global Doc Comment
            0xff181818, // Block Doc Comment
            0xff202020, // Doc Comment
            0xff808080, // Comment (single line)
            0xff404040, // Comment (multi line)
            0xff004000, // Preprocessor deactivated
            0xff00ff00, // Function
            0xff00ffff, // Attribute
            0xff00ffff, // Namespace
            0xff806906, // Typedef
            0xffdddddd, // Local Variable
            0xffffffff, // Global Variable
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


std::string TextEditor::GetText() const {
    return GetText(Coordinates(), Coordinates((int32_t)mLines.size(), 0));
}

std::vector<std::string> TextEditor::GetTextLines() const {
    std::vector<std::string> result;

    result.reserve(mLines.size());

    for (auto &line : mLines) {
        std::string text;

        text.resize(line.size());

        for (size_t i = 0; i < line.size(); ++i)
            text[i] = line[i].mChar;

        result.emplace_back(std::move(text));
    }

    return result;
}

std::string TextEditor::GetSelectedText() const {
    return GetText(mState.mSelectionStart, mState.mSelectionEnd);
}

std::string TextEditor::GetCurrentLineText() const {
    auto lineLength = GetLineMaxColumn(mState.mCursorPosition.mLine);
    return GetText(
        Coordinates(mState.mCursorPosition.mLine, 0),
        Coordinates(mState.mCursorPosition.mLine, lineLength));
}

void TextEditor::ProcessInputs() {
}

float TextEditor::TextDistanceToLineStart(const Coordinates &aFrom) const {
    auto &line      = mLines[aFrom.mLine];
    float distance  = 0.0f;
    float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ", nullptr, nullptr).x;
    int32_t colIndex    = GetCharacterIndex(aFrom);
    for (size_t it = 0u; it < line.size() && it < colIndex;) {
        if (line[it].mChar == '\t') {
            distance = (1.0f + std::floor((1.0f + distance) / (float(mTabSize) * spaceSize))) * (float(mTabSize) * spaceSize);
            ++it;
        } else {
            auto d = UTF8CharLength(line[it].mChar);
            char tempCString[7];
            int32_t i = 0;
            for (; i < 6 && d-- > 0 && it < (int32_t)line.size(); i++, it++)
                tempCString[i] = line[it].mChar;

            tempCString[i] = '\0';
            distance += ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, tempCString, nullptr, nullptr).x;
        }
    }

    return distance;
}

void TextEditor::EnsureCursorVisible() {
    if (!mWithinRender) {
        mScrollToCursor = true;
        return;
    }

    float scrollX = ImGui::GetScrollX();
    float scrollY = ImGui::GetScrollY();

    auto windowPadding = ImGui::GetStyle().WindowPadding * 2.0f;

    auto height = ImGui::GetWindowHeight() - mTopMargin - windowPadding.y;
    auto width  = ImGui::GetWindowWidth() - windowPadding.x;

    auto top    = (int32_t)ceil(scrollY / mCharAdvance.y);
    auto bottom = (int32_t)ceil((scrollY + height) / mCharAdvance.y);

    auto left  = scrollX;
    auto right = scrollX + width;

    auto pos = GetActualCursorCoordinates();
    auto len = TextDistanceToLineStart(pos);

    if (pos.mLine <= top + 1)
        ImGui::SetScrollY(std::max(0.0f, (pos.mLine - 1) * mCharAdvance.y));
    if (pos.mLine >= bottom - 2)
        ImGui::SetScrollY(std::max(0.0f, (pos.mLine + 2) * mCharAdvance.y - height));
    if (len == 0)
        ImGui::SetScrollX(0);
    else if (len + mTextStart <= left + 4)
        ImGui::SetScrollX(std::max(0.0f, len + mTextStart - 4));
    if (len + mTextStart + mCharAdvance.x * 2 >= right - 4)
        ImGui::SetScrollX(std::max(0.0f, len + mTextStart + 4 - width + mCharAdvance.x * 2));
}

int32_t TextEditor::GetPageSize() const {
    auto height = ImGui::GetWindowHeight() - 20.0f - mTopMargin;
    return (int32_t)floor(height / mCharAdvance.y);
}

template<> TextEditor::PaletteIndex TextEditor::getPaletteIndex<TextEditor::PatternLanguageTypes>(TextEditor::PatternLanguageTypes val) {
    switch (val) {
        case TextEditor::PatternLanguageTypes::Keyword:
            return TextEditor::PaletteIndex::Keyword;
        case TextEditor::PatternLanguageTypes::ValueType:
            return TextEditor::PaletteIndex::BuiltInType;
        case TextEditor::PatternLanguageTypes::Operator:
            return TextEditor::PaletteIndex::Operator;
        case TextEditor::PatternLanguageTypes::Integer:
            return TextEditor::PaletteIndex::NumericLiteral;
        case TextEditor::PatternLanguageTypes::String:
            return TextEditor::PaletteIndex::StringLiteral;
        case TextEditor::PatternLanguageTypes::Identifier:
            return TextEditor::PaletteIndex::UnkIdentifier;
        case TextEditor::PatternLanguageTypes::Separator:
            return TextEditor::PaletteIndex::Separator;
        case TextEditor::PatternLanguageTypes::Comment:
            return TextEditor::PaletteIndex::Comment;
        case TextEditor::PatternLanguageTypes::BlockComment:
            return TextEditor::PaletteIndex::BlockComment;
        case TextEditor::PatternLanguageTypes::DocBlockComment:
            return TextEditor::PaletteIndex::DocBlockComment;
        case TextEditor::PatternLanguageTypes::DocGlobalComment:
            return TextEditor::PaletteIndex::DocGlobalComment;
        case TextEditor::PatternLanguageTypes::DocComment:
            return TextEditor::PaletteIndex::DocComment;
        case TextEditor::PatternLanguageTypes::Directive:
            return TextEditor::PaletteIndex::Directive;
        default:
            return TextEditor::PaletteIndex::Default;
    }
}

template <> TextEditor::PaletteIndex TextEditor::getPaletteIndex<TextEditor::ConsoleTypes>(TextEditor::ConsoleTypes val) {
    switch (val) {
        case TextEditor::ConsoleTypes::Error:
            return TextEditor::PaletteIndex::ErrorText;
        case TextEditor::ConsoleTypes::Warning:
            return TextEditor::PaletteIndex::WarningText;
        case TextEditor::ConsoleTypes::Debug:
            return TextEditor::PaletteIndex::DebugText;
        default:
            return TextEditor::PaletteIndex::DefaultText;
    }
}

void TextEditor::ResetCursorBlinkTime() {
    mStartTime = ImGui::GetTime() * 1000 - sCursorBlinkOnTime;
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
    : mAdded(aAdded), mAddedStart(aAddedStart), mAddedEnd(aAddedEnd), mRemoved(aRemoved), mRemovedStart(aRemovedStart), mRemovedEnd(aRemovedEnd), mBefore(aBefore), mAfter(aAfter) {
    assert(mAddedStart <= mAddedEnd);
    assert(mRemovedStart <= mRemovedEnd);
}

void TextEditor::UndoRecord::Undo(TextEditor *aEditor) {
    if (!mAdded.empty()) {
        aEditor->DeleteRange(mAddedStart, mAddedEnd);
    }

    if (!mRemoved.empty()) {
        auto start = mRemovedStart;
        aEditor->InsertTextAt(start, mRemoved.c_str());
    }

    aEditor->mState = mBefore;
    aEditor->EnsureCursorVisible();
}

void TextEditor::UndoRecord::Redo(TextEditor *aEditor) {
    if (!mRemoved.empty()) {
        aEditor->DeleteRange(mRemovedStart, mRemovedEnd);
    }

    if (!mAdded.empty()) {
        auto start = mAddedStart;
        aEditor->InsertTextAt(start, mAdded.c_str());
    }

    aEditor->mState = mAfter;
    aEditor->EnsureCursorVisible();
}
