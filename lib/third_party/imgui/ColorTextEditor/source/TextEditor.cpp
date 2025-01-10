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

const int TextEditor::sCursorBlinkInterval = 1200;
const int TextEditor::sCursorBlinkOnTime = 800;

TextEditor::Palette TextEditor::sPaletteBase = TextEditor::GetDarkPalette();

TextEditor::FindReplaceHandler::FindReplaceHandler() : mWholeWord(false),mFindRegEx(false),mMatchCase(false)  {}

TextEditor::TextEditor() {
    mStartTime = ImGui::GetTime() * 1000;
    SetLanguageDefinition(LanguageDefinition::HLSL());
    mLines.push_back(Line());
}

TextEditor::~TextEditor() {
}


ImVec2 TextEditor::Underwaves( ImVec2 pos ,uint32_t nChars, ImColor color, const ImVec2 &size_arg) {
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

void TextEditor::SetLanguageDefinition(const LanguageDefinition &aLanguageDef) {
    mLanguageDefinition = aLanguageDef;
    mRegexList.clear();

    for (auto &r : mLanguageDefinition.mTokenRegexStrings)
        mRegexList.push_back(std::make_pair(std::regex(r.first, std::regex_constants::optimize), r.second));

    Colorize();
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
        if (lstart >= (int)mLines.size())
            break;

        auto &line = mLines[lstart];
        if (istart < (int)line.size()) {
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
    if (line >= (int)mLines.size()) {
        if (isEmpty()) {
            line   = 0;
            column = 0;
        } else {
            line   = (int)mLines.size() - 1;
            column = GetLineMaxColumn(line);
        }
        return Coordinates(line, column);
    } else {
        column = isEmpty() ? 0 : std::min(column, GetLineMaxColumn(line));
        return Coordinates(line, column);
    }
}

// https://en.wikipedia.org/wiki/UTF-8
// We assume that the char is a standalone character (<128) or a leading byte of an UTF-8 code sequence (non-10xxxxxx code)
static int UTF8CharLength(TextEditor::Char c) {
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
static inline int ImTextCharToUtf8(char *buf, int buf_size, unsigned int c) {
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
    if (aCoordinates.mLine < (int)mLines.size()) {
        auto &line  = mLines[aCoordinates.mLine];
        auto cindex = GetCharacterIndex(aCoordinates);

        if (cindex + 1 < (int)line.size()) {
            auto delta = UTF8CharLength(line[cindex].mChar);
            cindex     = std::min(cindex + delta, (int)line.size() - 1);
        } else {
            ++aCoordinates.mLine;
            cindex = 0;
        }
        aCoordinates.mColumn = GetCharacterColumn(aCoordinates.mLine, cindex);
    }
}

void TextEditor::DeleteRange(const Coordinates &aStart, const Coordinates &aEnd) {
    IM_ASSERT(aEnd >= aStart);
    IM_ASSERT(!mReadOnly);

    // printf("D(%d.%d)-(%d.%d)\n", aStart.mLine, aStart.mColumn, aEnd.mLine, aEnd.mColumn);

    if (aEnd == aStart)
        return;

    auto start = GetCharacterIndex(aStart);
    auto end   = GetCharacterIndex(aEnd);
    if (start == -1 || end == -1)
        return;

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
            RemoveLine(aStart.mLine + 1, aEnd.mLine);
    }

    mTextChanged = true;
}

int TextEditor::InsertTextAt(Coordinates & /* inout */ aWhere, const char *aValue) {
    int cindex     = GetCharacterIndex(aWhere);
    int totalLines = 0;
    while (*aValue != '\0') {
        if (mLines.empty()) {
            mLines.push_back(Line());
            mTextChanged = true;
        }

        if (*aValue == '\r') {
            // skip
            ++aValue;
        } else if (*aValue == '\t') {
            auto &line = mLines[aWhere.mLine];
            auto c = GetCharacterColumn(aWhere.mLine, cindex);
            auto r = c % mTabSize;
            auto d = mTabSize - r;
            auto i = d;
            while (i-- > 0)
                line.insert(line.begin() + cindex++, Glyph(' ', PaletteIndex::Default));

            cindex += d;
            aWhere.mColumn += d;
            aValue++;
        } else if (*aValue == '\n') {
            if (cindex < (int)mLines[aWhere.mLine].size()) {
                auto &newLine = InsertLine(aWhere.mLine + 1);
                auto &line    = mLines[aWhere.mLine];
                newLine.insert(newLine.begin(), line.begin() + cindex, line.end());
                line.erase(line.begin() + cindex, line.end());
            } else {
                InsertLine(aWhere.mLine + 1);
            }
            ++aWhere.mLine;
            aWhere.mColumn = 0;
            cindex         = 0;
            ++totalLines;
            ++aValue;
        } else {
            auto &line = mLines[aWhere.mLine];
            auto d     = UTF8CharLength(*aValue);
            while (d-- > 0 && *aValue != '\0')
                line.insert(line.begin() + cindex++, Glyph(*aValue++, PaletteIndex::Default));
            ++aWhere.mColumn;
        }

        mTextChanged = true;
    }

    return totalLines;
}

void TextEditor::AddUndo(UndoRecord &aValue) {
    IM_ASSERT(!mReadOnly);
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

    int lineNo = std::max(0, (int)floor(local.y / mCharAdvance.y));
    if (local.x < mCharAdvance.x)
        return Coordinates(lineNo, 0);
    local.x -= mCharAdvance.x;

    int columnCoord = 0;

    if (lineNo >= 0 && lineNo < (int)mLines.size()) {
        auto &line = mLines.at(lineNo);

        int columnIndex = 0;
        float columnX   = 0.0f;

        while ((size_t)columnIndex < line.size()) {
            float columnWidth = 0.0f;

            if (line[columnIndex].mChar == '\t') {
                float spaceSize  = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ").x;
                float oldX       = columnX;
                float newColumnX = (1.0f + std::floor((1.0f + columnX) / (float(mTabSize) * spaceSize))) * (float(mTabSize) * spaceSize);
                columnWidth      = newColumnX - oldX;
                if (columnX + columnWidth > local.x)
                    break;
                columnX     = newColumnX;
                columnCoord = (columnCoord / mTabSize) * mTabSize + mTabSize;
                columnIndex++;
            } else {
                char buf[7];
                auto d = UTF8CharLength(line[columnIndex].mChar);
                int i  = 0;
                while (i < 6 && d-- > 0)
                    buf[i++] = line[columnIndex++].mChar;
                buf[i]      = '\0';
                columnWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf).x;
                if (columnX + columnWidth > local.x)
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

bool isWordChar(char c) {
    auto asUChar = static_cast<unsigned char>(c);
    return std::isalnum(asUChar) || c == '_' || asUChar > 0x7F;
}

TextEditor::Coordinates TextEditor::FindWordStart(const Coordinates &aFrom) const {
    Coordinates at = aFrom;
    if (at.mLine >= (int)mLines.size())
        return at;

    auto &line  = mLines[at.mLine];
    auto cindex = GetCharacterIndex(at);

    if (cindex >= (int)line.size())
        return at;

    while (cindex > 0 && !isWordChar(line[cindex-1].mChar))
        --cindex;

    while (cindex > 0 && isWordChar(line[cindex - 1].mChar))
        --cindex;

    if (cindex==0 && line[cindex].mChar == '\"')
        ++cindex;
    return Coordinates(at.mLine, GetCharacterColumn(at.mLine, cindex));
}

TextEditor::Coordinates TextEditor::FindWordEnd(const Coordinates &aFrom) const {
    Coordinates at = aFrom;
    if (at.mLine >= (int)mLines.size())
        return at;

    auto &line  = mLines[at.mLine];
    auto cindex = GetCharacterIndex(at);

    if (cindex >= (int)line.size())
        return at;

    while (cindex < (line.size()) && !isWordChar(line[cindex].mChar))
        ++cindex;
    while (cindex < (line.size()) && isWordChar(line[cindex].mChar))
        ++cindex;

    if (line[cindex-1].mChar == '\"')
        --cindex;

    return Coordinates(aFrom.mLine, GetCharacterColumn(aFrom.mLine, cindex));
}

TextEditor::Coordinates TextEditor::FindNextWord(const Coordinates &aFrom) const {
    Coordinates at = aFrom;
    if (at.mLine >= (int)mLines.size())
        return at;

    // skip to the next non-word character
    auto cindex = GetCharacterIndex(aFrom);
    bool isword = false;
    bool skip   = false;
    if (cindex < (int)mLines[at.mLine].size()) {
        const auto &line = mLines[at.mLine];
        isword     = isalnum(line[cindex].mChar);
        skip       = isword;
    }

    while (!isword || skip) {
        if (at.mLine >= mLines.size()) {
            auto l = std::max(0, (int)mLines.size() - 1);
            return Coordinates(l, GetLineMaxColumn(l));
        }

        auto &line = mLines[at.mLine];
        if (cindex < (int)line.size()) {
            isword = isalnum(line[cindex].mChar);

            if (isword && !skip)
                return Coordinates(at.mLine, GetCharacterColumn(at.mLine, cindex));

            if (!isword)
                skip = false;

            cindex++;
        } else {
            cindex = 0;
            ++at.mLine;
            skip   = false;
            isword = false;
        }
    }

    return at;
}

int TextEditor::Utf8CharsToBytes(const Coordinates &aCoordinates) const {
    if (aCoordinates.mLine >= mLines.size())
        return -1;
    auto &line = mLines[aCoordinates.mLine];
    if (line.empty())
        return 0;
    int c      = 0;
    int i      = 0;
    while (i < line.size() && c < aCoordinates.mColumn) {
        i += UTF8CharLength(line[i].mChar);
        if (line[i].mChar == '\t')
            c = (c / mTabSize) * mTabSize + mTabSize;
        else
            ++c;
    }
    return i;
}

TextEditor::Coordinates TextEditor::StringIndexToCoordinates(int aIndex, const std::string &input ) const {
    if (aIndex < 0 || aIndex > (int)input.size())
        return Coordinates(0, 0);
    std::string str = input.substr(0, aIndex);
    auto line = std::count(str.begin(),str.end(),'\n');
    auto index = str.find_last_of('\n');
    str = str.substr(index+1);
    auto col = GetStringCharacterCount(str);

    return Coordinates(line, col);
}

int TextEditor::GetCharacterIndex(const Coordinates &aCoordinates) const {
    if (aCoordinates.mLine >= mLines.size())
        return -1;

    const auto &line = mLines[aCoordinates.mLine];
    int column = 0;
    int index  = 0;
    while (index < line.size() && column < aCoordinates.mColumn) {
        const auto character = line[index].mChar;
        index += UTF8CharLength(character);
        if (character == '\t')
            column = (column / mTabSize) * mTabSize + mTabSize;
        else
            ++column;
    }

    return index;
}

int TextEditor::GetCharacterColumn(int aLine, int aIndex) const {
    if (aLine >= mLines.size())
        return 0;
    auto &line = mLines[aLine];
    int col    = 0;
    int i      = 0;
    while (i < aIndex && i < (int)line.size()) {
        auto c = line[i].mChar;
        i += UTF8CharLength(c);
        if (c == '\t')
            col = (col / mTabSize) * mTabSize + mTabSize;
        else
            col++;
    }
    return col;
}

int TextEditor::GetStringCharacterCount(std::string str) const {
    if (str.empty())
        return 0;
    int c      = 0;
    for (unsigned i = 0; i < str.size(); c++)
        i += UTF8CharLength(str[i]);
    return c;
}

int TextEditor::GetLineCharacterCount(int aLine) const {
    if (aLine >= mLines.size())
        return 0;
    auto &line = mLines[aLine];
    int c      = 0;
    for (unsigned i = 0; i < line.size(); c++)
        i += UTF8CharLength(line[i].mChar);
    return c;
}

unsigned long long TextEditor::GetLineByteCount(int aLine) const {
    if (aLine >= mLines.size())
        return 0;
    auto &line = mLines[aLine];
    return line.size();
}

int TextEditor::GetLineMaxColumn(int aLine) const {
    if (aLine >= mLines.size())
        return 0;
    auto &line = mLines[aLine];
    int col    = 0;
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
    if (aAt.mLine >= (int)mLines.size() || aAt.mColumn == 0)
        return true;

    auto &line  = mLines[aAt.mLine];
    auto cindex = GetCharacterIndex(aAt);
    if (cindex >= (int)line.size())
        return true;

    if (mColorizerEnabled)
        return line[cindex].mColorIndex != line[size_t(cindex - 1)].mColorIndex;

    return isspace(line[cindex].mChar) != isspace(line[cindex - 1].mChar);
}

void TextEditor::RemoveLine(int aStart, int aEnd) {
    IM_ASSERT(!mReadOnly);
    IM_ASSERT(aEnd >= aStart);

    ErrorMarkers etmp;
    for (auto &i : mErrorMarkers) {
        ErrorMarkers::value_type e(i.first.mLine >= aStart ?  Coordinates(i.first.mLine - 1,i.first.mColumn ) : i.first, i.second);
        if (e.first.mLine >= aStart && e.first.mLine <= aEnd)
            continue;
        etmp.insert(e);
    }
    mErrorMarkers = std::move(etmp);

    Breakpoints btmp;
    for (auto breakpoint : mBreakpoints) {
        if (breakpoint <= aStart || breakpoint >= aEnd) {
            if (breakpoint >= aEnd) {
                btmp.insert(breakpoint - 1);
                mBreakPointsChanged = true;
            } else
                btmp.insert(breakpoint);
        }
    }

    mBreakpoints = std::move(btmp);
    // use clamp to ensure valid results instead of assert.
    auto start = std::clamp(aStart, 0, (int)mLines.size()-1);
    auto end   = std::clamp(aEnd, 0, (int)mLines.size());
    mLines.erase(mLines.begin() + aStart, mLines.begin() + aEnd + 1);

    mTextChanged = true;
}

void TextEditor::RemoveLine(int aIndex) {
    IM_ASSERT(!mReadOnly);
    IM_ASSERT(mLines.size() > 1);

    ErrorMarkers etmp;
    for (auto &i : mErrorMarkers) {
        ErrorMarkers::value_type e(i.first.mLine > aIndex ? Coordinates(i.first.mLine - 1 ,i.first.mColumn) : i.first, i.second);
        if (e.first.mLine - 1 == aIndex)
            continue;
        etmp.insert(e);
    }
    mErrorMarkers = std::move(etmp);

    Breakpoints btmp;
    for (auto breakpoint : mBreakpoints) {
        if (breakpoint > aIndex) {
            btmp.insert(breakpoint - 1);
            mBreakPointsChanged = true;
        }else
            btmp.insert(breakpoint);
    }
    if (mBreakPointsChanged)
        mBreakpoints = std::move(btmp);

    mLines.erase(mLines.begin() + aIndex);
    IM_ASSERT(!mLines.empty());

    mTextChanged = true;
}

TextEditor::Line &TextEditor::InsertLine(int aIndex) {

    if (isEmpty())
        return *mLines.insert(mLines.begin(), Line());

    if (aIndex == mLines.size())
        return *mLines.insert(mLines.end(), Line());

    auto newLine = Line();

    TextEditor::Line &result = *mLines.insert(mLines.begin() + aIndex, newLine);

    ErrorMarkers etmp;
    for (auto &i : mErrorMarkers)
        etmp.insert(ErrorMarkers::value_type(i.first.mLine >= aIndex ? Coordinates(i.first.mLine + 1,i.first.mColumn) : i.first, i.second));
    mErrorMarkers = std::move(etmp);

    Breakpoints btmp;
    for (auto breakpoint : mBreakpoints) {
        if (breakpoint >= aIndex) {
            btmp.insert(breakpoint + 1);
            mBreakPointsChanged = true;
        } else
            btmp.insert(breakpoint);
    }
    if (mBreakPointsChanged)
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

ImU32 TextEditor::GetGlyphColor(const Glyph &aGlyph) const {
    if (!mColorizerEnabled)
        return mPalette[(int)PaletteIndex::Default];
    if (aGlyph.mGlobalDocComment)
        return mPalette[(int)PaletteIndex::GlobalDocComment];
    if (aGlyph.mDocComment)
        return mPalette[(int)PaletteIndex::DocComment];
    if (aGlyph.mComment)
        return mPalette[(int)PaletteIndex::Comment];
    if (aGlyph.mMultiLineComment)
        return mPalette[(int)PaletteIndex::MultiLineComment];
    if (aGlyph.mDeactivated)
        return mPalette[(int)PaletteIndex::PreprocessorDeactivated];
    const auto color = mPalette[(int)aGlyph.mColorIndex];
    if (aGlyph.mPreprocessor) {
        const auto ppcolor = mPalette[(int)PaletteIndex::Preprocessor];
        const int c0       = ((ppcolor & 0xff) + (color & 0xff)) / 2;
        const int c1       = (((ppcolor >> 8) & 0xff) + ((color >> 8) & 0xff)) / 2;
        const int c2       = (((ppcolor >> 16) & 0xff) + ((color >> 16) & 0xff)) / 2;
        const int c3       = (((ppcolor >> 24) & 0xff) + ((color >> 24) & 0xff)) / 2;
        return ImU32(c0 | (c1 << 8) | (c2 << 16) | (c3 << 24));
    }
    return color;
}

void TextEditor::HandleKeyboardInputs() {
    ImGuiIO &io   = ImGui::GetIO();

    // command => Ctrl
    // control => Super
    // option  => Alt
    auto ctrl     = io.KeyCtrl;
    auto alt      = io.KeyAlt;
    auto shift    = io.KeyShift;

    if (ImGui::IsWindowFocused()) {
        if (ImGui::IsWindowHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);

        io.WantCaptureKeyboard = true;
        io.WantTextInput       = true;

        if (!IsReadOnly() && !ctrl && !shift && !alt && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)))
            EnterCharacter('\n', false);
        else if (!IsReadOnly() && !ctrl && !alt && ImGui::IsKeyPressed(ImGuiKey_Tab))
            EnterCharacter('\t', shift);

        if (!IsReadOnly() && !io.InputQueueCharacters.empty()) {
            for (int i = 0; i < io.InputQueueCharacters.Size; i++) {
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
            auto click         = ImGui::IsMouseClicked(0);
            auto doubleClick   = ImGui::IsMouseDoubleClicked(0);
            auto rightClick    = ImGui::IsMouseClicked(1);
            auto t     = ImGui::GetTime();
            auto tripleClick   = click && !doubleClick && (mLastClick != -1.0f && (t - mLastClick) < io.MouseDoubleClickTime);
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
                ResetCursorBlinkTime();
              
                EnsureCursorVisible();
                mLastClick = (float)ImGui::GetTime();
            } else if (rightClick) {
                auto cursorPosition = ScreenPosToCoordinates(ImGui::GetMousePos());

                if (!HasSelection() || mState.mSelectionStart > cursorPosition || cursorPosition > mState.mSelectionEnd) {
                    mState.mCursorPosition = mInteractiveStart = mInteractiveEnd = cursorPosition;
                    mSelectionMode = SelectionMode::Normal;
                    SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);
                }
                ResetCursorBlinkTime();
                mRaiseContextMenu = true;
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

int TextEditor::GetLongestLineLength() const {
    int result = 0;
    for (int i = 0; i < (int)mLines.size(); i++)
        result = std::max(result, GetLineCharacterCount(i));
    return result;
}

inline void TextUnformattedColoredAt(const ImVec2 &pos, const ImU32 &color, const char *text) {
    ImGui::SetCursorScreenPos(pos);
    ImGui::PushStyleColor(ImGuiCol_Text,color);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}

void TextEditor::RenderText(const char *aTitle, const ImVec2 &lineNumbersStartPos, const ImVec2 &textEditorSize) {
    /* Compute mCharAdvance regarding scaled font size (Ctrl + mouse wheel)*/
    const float fontSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, "#", nullptr, nullptr).x;
    mCharAdvance         = ImVec2(fontSize, ImGui::GetTextLineHeightWithSpacing() * mLineSpacing);

    /* Update palette with the current alpha from style */
    for (int i = 0; i < (int)PaletteIndex::Max; ++i) {
        auto color = ImGui::ColorConvertU32ToFloat4(sPaletteBase[i]);
        color.w *= ImGui::GetStyle().Alpha;
        mPalette[i] = ImGui::ColorConvertFloat4ToU32(color);
    }

    IM_ASSERT(mLineBuffer.empty());

    auto contentSize = textEditorSize;
    auto drawList    = ImGui::GetWindowDrawList();
    mNumberOfLinesDisplayed = GetPageSize();

    if (mScrollToTop) {
        mScrollToTop = false;
        ImGui::SetScrollY(0.f);
    }

    if ( mScrollToBottom && ImGui::GetScrollMaxY() >= ImGui::GetScrollY()) {
        mScrollToBottom = false;
        ImGui::SetScrollY(ImGui::GetScrollMaxY());
    }

    ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
    ImVec2 position       = lineNumbersStartPos;
    auto scrollX           = ImGui::GetScrollX();
    if (mSetScrollY)
        SetScrollY();
    auto scrollY           = ImGui::GetScrollY();
    if (mSetTopLine)
        SetTopLine();
    else
        mTopLine = std::max<float>(0.0F, (scrollY-mTopMargin) / mCharAdvance.y);
    auto lineNo        = mTopLine;
    float  globalLineMax    = mLines.size();
    auto lineMax       = std::clamp(lineNo + mNumberOfLinesDisplayed, 0.0F, globalLineMax-1.0F);
    int totalDigitCount = std::floor(std::log10(globalLineMax)) + 1;
    mLongest = GetLongestLineLength() * mCharAdvance.x;

    // Deduce mTextStart by evaluating mLines size (global lineMax) plus two spaces as text width
    char buf[16];

    if (mShowLineNumbers)
        snprintf(buf, 16, " %d ", int(globalLineMax));
    else
        buf[0] = '\0';
    mTextStart = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf, nullptr, nullptr).x + mLeftMargin;

    if (!mLines.empty()) {
        float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ", nullptr, nullptr).x;

        while (lineNo <= lineMax) {
            ImVec2 lineStartScreenPos = ImVec2(cursorScreenPos.x + mLeftMargin, mTopMargin + cursorScreenPos.y + std::floor(lineNo) * mCharAdvance.y);
            ImVec2 textScreenPos      = lineStartScreenPos;

            auto &line    = mLines[lineNo];
            auto columnNo = 0;
            Coordinates lineStartCoord(lineNo, 0);
            Coordinates lineEndCoord(lineNo, GetLineMaxColumn(lineNo));

            // Draw selection for the current line
            float sstart = -1.0f;
            float ssend  = -1.0f;

            IM_ASSERT(mState.mSelectionStart <= mState.mSelectionEnd);
            if (mState.mSelectionStart <= lineEndCoord)
                sstart = mState.mSelectionStart > lineStartCoord ? TextDistanceToLineStart(mState.mSelectionStart) : 0.0f;
            if (mState.mSelectionEnd > lineStartCoord)
                ssend = TextDistanceToLineStart(mState.mSelectionEnd < lineEndCoord ? mState.mSelectionEnd : lineEndCoord);

            if (mState.mSelectionEnd.mLine > lineNo)
                ssend += mCharAdvance.x;

            if (sstart != -1 && ssend != -1 && sstart < ssend) {
                ImVec2 vstart(lineStartScreenPos.x + sstart, lineStartScreenPos.y);
                ImVec2 vend(lineStartScreenPos.x + ssend, lineStartScreenPos.y + mCharAdvance.y);
                drawList->AddRectFilled(vstart, vend, mPalette[(int)PaletteIndex::Selection]);
            }
            ImVec2 lineNoStartScreenPos = ImVec2(position.x, mTopMargin + cursorScreenPos.y + std::floor(lineNo) * mCharAdvance.y);
            auto start = ImVec2(lineNoStartScreenPos.x + mLineNumberFieldWidth, lineStartScreenPos.y);
            bool focused = ImGui::IsWindowFocused();
            if (!mIgnoreImGuiChild)
                ImGui::EndChild();
            // Draw line number (right aligned)
            if (mShowLineNumbers) {
                ImGui::SetCursorScreenPos(position);
                if (!mIgnoreImGuiChild)
                    ImGui::BeginChild("##lineNumbers");

                int padding = totalDigitCount - std::floor(std::log10(lineNo + 1)) - 1;
                std::string space = " ";
                while (padding-- > 0) {
                    space += " ";
                }
                std::string lineNoStr = space + std::to_string((int)(lineNo + 1));
                TextUnformattedColoredAt(ImVec2(mLeftMargin + lineNoStartScreenPos.x, lineStartScreenPos.y), mPalette[(int) PaletteIndex::LineNumber], lineNoStr.c_str());
            }

            // Draw breakpoints
            if (mBreakpoints.count(lineNo + 1) != 0) {
                auto end = ImVec2(lineNoStartScreenPos.x + contentSize.x + mLineNumberFieldWidth, lineStartScreenPos.y + mCharAdvance.y);
                drawList->AddRectFilled(ImVec2(lineNumbersStartPos.x, lineStartScreenPos.y), end, mPalette[(int)PaletteIndex::Breakpoint]);

                drawList->AddCircleFilled(start + ImVec2(0, mCharAdvance.y) / 2, mCharAdvance.y / 3, mPalette[(int)PaletteIndex::Breakpoint]);
                drawList->AddCircle(start + ImVec2(0, mCharAdvance.y) / 2, mCharAdvance.y / 3, mPalette[(int)PaletteIndex::Default]);
            }

            if (mState.mCursorPosition.mLine == lineNo && mShowCursor) {

                // Highlight the current line (where the cursor is)
                if (!HasSelection()) {
                    auto end = ImVec2(lineNoStartScreenPos.x + contentSize.x + mLineNumberFieldWidth, lineStartScreenPos.y + mCharAdvance.y);
                    drawList->AddRectFilled(ImVec2(lineNumbersStartPos.x, lineStartScreenPos.y), end, mPalette[(int)(focused ? PaletteIndex::CurrentLineFill : PaletteIndex::CurrentLineFillInactive)]);
                    drawList->AddRect(ImVec2(lineNumbersStartPos.x, lineStartScreenPos.y), end, mPalette[(int)PaletteIndex::CurrentLineEdge], 1.0f);
                }
            }
            if (mShowLineNumbers && !mIgnoreImGuiChild)
                ImGui::EndChild();

            if (!mIgnoreImGuiChild)
                ImGui::BeginChild(aTitle);
            if (mState.mCursorPosition.mLine == lineNo && mShowCursor) {
                // Render the cursor
                if (focused) {
                    auto timeEnd = ImGui::GetTime() * 1000;
                    auto elapsed = timeEnd - mStartTime;
                    if (elapsed > sCursorBlinkOnTime) {
                        float width = 1.0f;
                        auto cindex = GetCharacterIndex(mState.mCursorPosition);
                        float cx    = TextDistanceToLineStart(mState.mCursorPosition);

                        if (mOverwrite && cindex < (int)line.size()) {
                            auto c = line[cindex].mChar;
                            if (c == '\t') {
                                auto x = (1.0f + std::floor((1.0f + cx) / (float(mTabSize) * spaceSize))) * (float(mTabSize) * spaceSize);
                                width  = x - cx;
                            } else {
                                char buf2[2];
                                buf2[0] = line[cindex].mChar;
                                buf2[1] = '\0';
                                width   = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf2).x;
                            }
                        }
                        ImVec2 cstart(lineStartScreenPos.x + cx, lineStartScreenPos.y);
                        ImVec2 cend(lineStartScreenPos.x + cx + width, lineStartScreenPos.y + mCharAdvance.y);
                        drawList->AddRectFilled(cstart, cend, mPalette[(int)PaletteIndex::Cursor]);
                        if (elapsed > sCursorBlinkInterval)
                            mStartTime = timeEnd;
                    }
                }
            }

            // Render goto buttons
            auto lineText = GetLineText(lineNo);
            Coordinates gotoKey = Coordinates(lineNo + 1, 0);
            std::string errorLineColumn;
            bool found = false;
            for (auto text : mClickableText) {
                if (lineText.find(text) == 0) {
                    errorLineColumn = lineText.substr(text.size());
                    if (!errorLineColumn.empty()) {
                        found = true;
                        break;
                    }
                }
            }
            if (found) {
                int currLine = 0, currColumn = 0;
                if (auto idx = errorLineColumn.find(":"); idx != std::string::npos) {
                    auto errorLine = errorLineColumn.substr(0, idx);
                    if (!errorLine.empty())
                        currLine = std::stoi(errorLine) - 1;
                    auto errorColumn = errorLineColumn.substr(idx + 1);
                    if (!errorColumn.empty())
                        currColumn = std::stoi(errorColumn) - 1;
                }
                TextEditor::Coordinates errorPos = {currLine, currColumn};
                ImVec2 errorStart = ImVec2(lineStartScreenPos.x, lineStartScreenPos.y);
                ImVec2 errorEnd = ImVec2( lineStartScreenPos.x + TextDistanceToLineStart(Coordinates(lineNo, GetLineCharacterCount(lineNo))), lineStartScreenPos.y + mCharAdvance.y);
                ErrorGotoBox box = ErrorGotoBox(ImRect({errorStart, errorEnd}), errorPos, GetSourceCodeEditor());
                mErrorGotoBoxes[gotoKey] = box;
                CursorChangeBox cursorBox = CursorChangeBox(ImRect({errorStart, errorEnd}));
                mCursorBoxes[gotoKey] = cursorBox;
            }
            if (mCursorBoxes.find(gotoKey) != mCursorBoxes.end()) {
                auto box = mCursorBoxes[gotoKey];
                if (box.trigger()) box.callback();
            }

            if (mErrorGotoBoxes.find(gotoKey) != mErrorGotoBoxes.end()) {
                auto box = mErrorGotoBoxes[gotoKey];
                if (box.trigger()) box.callback();
            }

            // Render colorized text
            auto prevColor = line.empty() ? mPalette[(int)PaletteIndex::Default] : GetGlyphColor(line[0]);
            ImVec2 bufferOffset;

            for (int i = 0; i < line.size();) {
                auto &glyph = line[i];
                auto color  = GetGlyphColor(glyph);
                bool underwaved = false;
                ErrorMarkers::iterator errorIt;

                if (errorIt = mErrorMarkers.find(Coordinates(lineNo+1,i+1)); errorIt != mErrorMarkers.end()) {
                    underwaved = true;
                }

                if ((color != prevColor || glyph.mChar == '\t' || glyph.mChar == ' ') && !mLineBuffer.empty()) {
                    const ImVec2 newOffset(textScreenPos.x + bufferOffset.x, textScreenPos.y + bufferOffset.y);
                    TextUnformattedColoredAt(newOffset, prevColor, mLineBuffer.c_str());
                    auto textSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, mLineBuffer.c_str(), nullptr, nullptr);
                    bufferOffset.x += textSize.x;
                    mLineBuffer.clear();
                }
                if (underwaved) {
                    auto textStart = TextDistanceToLineStart(Coordinates(lineNo, i));
                    auto begin = ImVec2(lineStartScreenPos.x + textStart, lineStartScreenPos.y);
                    auto errorLength = errorIt->second.first;
                    auto errorMessage = errorIt->second.second;
                    if (errorLength == 0)
                        errorLength = line.size() - i - 1;
                    auto end = Underwaves(begin, errorLength, mPalette[(int32_t) PaletteIndex::ErrorMarker]);
                    Coordinates key = Coordinates(lineNo+1,i+1);
                    ErrorHoverBox box = ErrorHoverBox(ImRect({begin, end}), key, errorMessage.c_str());
                    mErrorHoverBoxes[key] = box;
                }
                Coordinates key = Coordinates(lineNo + 1, i + 1);
                if (mErrorHoverBoxes.find(key) != mErrorHoverBoxes.end()) {
                    auto box = mErrorHoverBoxes[key];
                    if (box.trigger()) box.callback();
                }

                prevColor = color;

                if (mUpdateFocus && mFocusAtCoords == Coordinates(lineNo, i)) {
                    mState.mCursorPosition = mInteractiveStart = mInteractiveEnd = mFocusAtCoords;
                    mSelectionMode = SelectionMode::Normal;
                    SetSelection(mInteractiveStart, mInteractiveEnd, mSelectionMode);
                    ResetCursorBlinkTime();
                    EnsureCursorVisible();
                    ImGui::SetKeyboardFocusHere(-1);
                    mUpdateFocus = false;
                }

                if (glyph.mChar == '\t') {
                    auto oldX      = bufferOffset.x;
                    bufferOffset.x = (1.0f + std::floor((1.0f + bufferOffset.x) / (float(mTabSize) * spaceSize))) * (float(mTabSize) * spaceSize);
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

            if (!mLineBuffer.empty()) {
                const ImVec2 newOffset(textScreenPos.x + bufferOffset.x, textScreenPos.y + bufferOffset.y);
                TextUnformattedColoredAt(newOffset, prevColor, mLineBuffer.c_str());
                mLineBuffer.clear();
            }

            lineNo = std::floor(lineNo + 1.0F);
        }
    }
    if (!mIgnoreImGuiChild)
        ImGui::EndChild();

    if (mShowLineNumbers && !mIgnoreImGuiChild) {
            ImGui::BeginChild("##lineNumbers");
        ImGui::Dummy(ImVec2(mLineNumberFieldWidth, (globalLineMax - lineMax - 1) * mCharAdvance.y + ImGui::GetCurrentWindow()->InnerClipRect.GetHeight() - mCharAdvance.y));
        ImGui::EndChild();
    }
    if (!mIgnoreImGuiChild)
        ImGui::BeginChild(aTitle);

    if (mShowLineNumbers)
        ImGui::Dummy(ImVec2(mLongest, (globalLineMax - lineMax - 2.0F) * mCharAdvance.y + ImGui::GetCurrentWindow()->InnerClipRect.GetHeight()));
    else
        ImGui::Dummy(ImVec2(mLongest, (globalLineMax - 1.0f - lineMax + GetPageSize() - 1.0f ) * mCharAdvance.y - 2 * ImGuiStyle().WindowPadding.y));

    if (mScrollToCursor)
        EnsureCursorVisible();


    if (mTopMarginChanged) {
        mTopMarginChanged = false;
        auto window = ImGui::GetCurrentWindow();
        auto maxScroll = window->ScrollMax.y;
        if (maxScroll > 0) {
            float pixelCount;
            if (mNewTopMargin > mTopMargin) {
                pixelCount = mNewTopMargin - mTopMargin;
            } else if (mNewTopMargin > 0) {
                pixelCount = mTopMargin - mNewTopMargin;
            } else {
                pixelCount = mTopMargin;
            }
            auto oldScrollY = ImGui::GetScrollY();

            if (mNewTopMargin > mTopMargin)
                mShiftedScrollY = oldScrollY + pixelCount;
            else
                mShiftedScrollY = oldScrollY - pixelCount;
            ImGui::SetScrollY(mShiftedScrollY);
            mTopMargin = mNewTopMargin;
        }
    }
}

void TextEditor::Render(const char *aTitle, const ImVec2 &aSize, bool aBorder) {
    mWithinRender          = true;
    mTextChanged           = false;
    mCursorPositionChanged = false;

    auto scrollBg = ImGui::GetStyleColorVec4(ImGuiCol_ScrollbarBg);
    scrollBg.w = 0.0f;
    auto scrollBarSize = ImGui::GetStyle().ScrollbarSize;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(mPalette[(int) PaletteIndex::Background]));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImGui::ColorConvertFloat4ToU32(scrollBg));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding,0);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize,scrollBarSize);

    auto position = ImGui::GetCursorScreenPos();
    if (mShowLineNumbers ) {
        std::string lineNumber = " " + std::to_string(mLines.size()) + " ";
        mLineNumberFieldWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, lineNumber.c_str(), nullptr, nullptr).x + mLeftMargin;
        ImGui::SetNextWindowPos(position);
        ImGui::SetCursorScreenPos(position);
        auto lineNoSize = ImVec2(mLineNumberFieldWidth, aSize.y);
        if (!mIgnoreImGuiChild) {
            ImGui::BeginChild("##lineNumbers", lineNoSize, false, ImGuiWindowFlags_NoScrollbar);
            ImGui::EndChild();
        }
    }  else {
        mLineNumberFieldWidth = 0;
    }

    ImVec2 textEditorSize = aSize;
    textEditorSize.x -=  mLineNumberFieldWidth;
    mLongest        = GetLongestLineLength() * mCharAdvance.x;
    bool scroll_x = mLongest > textEditorSize.x;
    bool scroll_y = mLines.size() > 1;
    if (!aBorder && scroll_y)
        textEditorSize.x -= scrollBarSize;
    ImGui::SetCursorScreenPos(ImVec2(position.x + mLineNumberFieldWidth, position.y));
    ImGuiChildFlags childFlags  = aBorder ? ImGuiChildFlags_Borders : ImGuiChildFlags_None;
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove;
    if (!mIgnoreImGuiChild)
        ImGui::BeginChild(aTitle, textEditorSize, childFlags, windowFlags);
    auto window = ImGui::GetCurrentWindow();
    window->ScrollbarSizes = ImVec2(scrollBarSize * scroll_x, scrollBarSize * scroll_y);
    ImGui::GetCurrentWindowRead()->ScrollbarSizes = ImVec2(scrollBarSize * scroll_y, scrollBarSize * scroll_x);
    if (scroll_y) {
        ImGui::GetCurrentWindow()->ScrollbarY= true;
        ImGui::Scrollbar(ImGuiAxis_Y);
    }
    if (scroll_x) {
        ImGui::GetCurrentWindow()->ScrollbarX= true;
        ImGui::Scrollbar(ImGuiAxis_X);
    }

    if (mHandleKeyboardInputs) {
        HandleKeyboardInputs();
    }

    if (mHandleMouseInputs)
        HandleMouseInputs();

    ColorizeInternal();
    RenderText(aTitle, position, textEditorSize);

    if (!mIgnoreImGuiChild)
        ImGui::EndChild();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);

    mWithinRender = false;
    ImGui::SetCursorScreenPos(ImVec2(position.x,position.y+aSize.y-1));
    ImGui::Dummy({});
}

void TextEditor::SetText(const std::string &aText) {
    mLines.resize(1);
    mLines[0].clear();
    std::string text = PreprocessText(aText);
    for (auto chr : text) {
        if (chr == '\r') {
            // ignore the carriage return character
        } else if (chr == '\n')
            mLines.push_back(Line());
        else {
            mLines.back().push_back(Glyph(chr, PaletteIndex::Default));
        }
    }

    mTextChanged = true;
    mScrollToTop = true;

    mUndoBuffer.clear();
    mUndoIndex = 0;

    Colorize();
}

void TextEditor::SetTextLines(const std::vector<std::string> &aLines) {
    mLines.clear();

    if (isEmpty()) {
        mLines.emplace_back(Line());
    } else {
        mLines.resize(aLines.size());

        for (size_t i = 0; i < aLines.size(); ++i) {
            const std::string &aLine = PreprocessText(aLines[i]);

            mLines[i].reserve(aLine.size());
            for (size_t j = 0; j < aLine.size(); ++j)
                mLines[i].emplace_back(Glyph(aLine[j], PaletteIndex::Default));
        }
    }

    mTextChanged = true;
    mScrollToTop = true;

    mUndoBuffer.clear();
    mUndoIndex = 0;

    Colorize();
}

void TextEditor::EnterCharacter(ImWchar aChar, bool aShift) {
    IM_ASSERT(!mReadOnly);

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
            if (end.mLine >= (int)mLines.size())
                end.mLine = isEmpty() ? 0 : (int)mLines.size() - 1;
            end.mColumn = GetLineMaxColumn(end.mLine);

            u.mRemovedStart = start;
            u.mRemovedEnd   = end;
            u.mRemoved      = GetText(start, end);

            bool modified = false;

            for (int i = start.mLine; i <= end.mLine; i++) {
                auto &line = mLines[i];
                if (aShift) {
                    if (!line.empty()) {
                        if (line.front().mChar == '\t') {
                            line.erase(line.begin());
                            modified = true;
                        } else {
                            for (int j = 0; j < mTabSize && !line.empty() && line.front().mChar == ' '; j++) {
                                line.erase(line.begin());
                                modified = true;
                            }
                        }
                    }
                } else {
                    for (int j = start.mColumn % mTabSize; j < mTabSize; j++)
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


    if (mLines.empty())
        mLines.push_back(Line());

    if (aChar == '\n') {
        InsertLine(coord.mLine + 1);
        auto &line    = mLines[coord.mLine];
        auto &newLine = mLines[coord.mLine + 1];

        if (mLanguageDefinition.mAutoIndentation)
            for (size_t it = 0; it < line.size() && isascii(line[it].mChar) && isblank(line[it].mChar); ++it)
                newLine.push_back(line[it]);

        const size_t whitespaceSize = newLine.size();
        int cstart                  = 0;
        int cpos                    = 0;
        auto cindex                 = GetCharacterIndex(coord);
        if (cindex < whitespaceSize && mLanguageDefinition.mAutoIndentation) {
            cstart = (int) whitespaceSize;
            cpos = cindex;
        } else {
            cstart = cindex;
            cpos = (int) whitespaceSize;
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
            for (int j = 0; j < spacesToInsert; j++)
                line.insert(line.begin() + cindex, Glyph(' ', PaletteIndex::Background));
            SetCursorPosition(Coordinates(coord.mLine, GetCharacterColumn(coord.mLine, cindex + spacesToInsert)));
        } else {
            auto spacesToRemove = (cindex % mTabSize);
            if (spacesToRemove == 0) spacesToRemove = 4;

            for (int j = 0; j < spacesToRemove; j++) {
                if ((line.begin() + cindex - 1)->mChar == ' ') {
                    line.erase(line.begin() + cindex - 1);
                    cindex -= 1;
                }
            }

            SetCursorPosition(Coordinates(coord.mLine, GetCharacterColumn(coord.mLine, std::max(0, cindex))));
        }

    } else {
        char buf[7];
        int e = ImTextCharToUtf8(buf, 7, aChar);
        if (e > 0) {
            buf[e]      = '\0';
            auto &line  = mLines[coord.mLine];
            auto cindex = GetCharacterIndex(coord);

            if (mOverwrite && cindex < (int)line.size()) {
                auto d = UTF8CharLength(line[cindex].mChar);

                u.mRemovedStart = mState.mCursorPosition;
                u.mRemovedEnd   = Coordinates(coord.mLine, GetCharacterColumn(coord.mLine, cindex + d));

                while (d-- > 0 && cindex < (int)line.size()) {
                    u.mRemoved += line[cindex].mChar;
                    line.erase(line.begin() + cindex);
                }
            }

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

    Colorize(coord.mLine - 1, 3);

    std::string findWord = mFindReplaceHandler.GetFindWord();
    if (!findWord.empty()) {
        mFindReplaceHandler.resetMatches();
        mFindReplaceHandler.FindAllMatches(this, findWord);
    }

    EnsureCursorVisible();
}

void TextEditor::SetReadOnly(bool aValue) {
    mReadOnly = aValue;
}

void TextEditor::SetColorizerEnable(bool aValue) {
    mColorizerEnabled = aValue;
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

TextEditor::Selection TextEditor::GetSelection() const {
    return {mState.mSelectionStart, mState.mSelectionEnd};
}

void TextEditor::SetTabSize(int aValue) {
    mTabSize = std::max(0, std::min(32, aValue));
}

void TextEditor::InsertText(const std::string &aValue) {
    InsertText(aValue.c_str());
}

void TextEditor::InsertText(const char *aValue) {
    if (aValue == nullptr)
        return;

    auto pos       = GetActualCursorCoordinates();
    auto start     = std::min(pos, mState.mSelectionStart);
    int totalLines = pos.mLine - start.mLine;
    auto text = PreprocessText(aValue);

    totalLines += InsertTextAt(pos, text.c_str());

    SetSelection(pos, pos);
    SetCursorPosition(pos);

    std::string findWord = mFindReplaceHandler.GetFindWord();
    if (!findWord.empty()) {
        mFindReplaceHandler.resetMatches();
        mFindReplaceHandler.FindAllMatches(this, findWord);
    }
    Colorize(start.mLine - 1, totalLines + 2);
}

void TextEditor::DeleteSelection() {
    IM_ASSERT(mState.mSelectionEnd >= mState.mSelectionStart);

    if (mState.mSelectionEnd == mState.mSelectionStart)
        return;

    DeleteRange(mState.mSelectionStart, mState.mSelectionEnd);

    SetSelection(mState.mSelectionStart, mState.mSelectionStart);
    SetCursorPosition(mState.mSelectionStart);
    std::string findWord = mFindReplaceHandler.GetFindWord();
    if (!findWord.empty()) {
        mFindReplaceHandler.resetMatches();
        mFindReplaceHandler.FindAllMatches(this, findWord);
    }
    Colorize(mState.mSelectionStart.mLine, 1);
}

void TextEditor::JumpToLine(int line) {
    auto newPos = Coordinates(line, 0);
    JumpToCoords(newPos);
}

void TextEditor::JumpToCoords(const Coordinates &aNewPos) {
    SetSelection(aNewPos, aNewPos);
    SetCursorPosition(aNewPos);
    EnsureCursorVisible();

    setFocusAtCoords(aNewPos);
}

void TextEditor::MoveUp(int aAmount, bool aSelect) {
    ResetCursorBlinkTime();
    auto oldPos                  = mState.mCursorPosition;
    if (aAmount < 0) {
        mScrollYIncrement = -1.0;
        SetScrollY();
        return;
    }
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

void TextEditor::MoveDown(int aAmount, bool aSelect) {
    IM_ASSERT(mState.mCursorPosition.mColumn >= 0);
    ResetCursorBlinkTime();
    auto oldPos                  = mState.mCursorPosition;
    if (aAmount < 0) {
        mScrollYIncrement = 1.0;
        SetScrollY();
        return;
    }

    mState.mCursorPosition.mLine = std::clamp(mState.mCursorPosition.mLine + aAmount, 0, (int)mLines.size() - 1);
    if (oldPos.mLine == (mLines.size() - 1)) {
        mTopLine += aAmount;
        mTopLine = std::clamp(mTopLine, 0.0F, mLines.size() - 1.0F);
        SetTopLine();
        EnsureCursorVisible();
        return;
    }

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

void TextEditor::MoveLeft(int aAmount, bool aSelect, bool aWordMode) {
    auto oldPos = mState.mCursorPosition;

    ResetCursorBlinkTime();
    if (isEmpty() || oldPos.mLine >= mLines.size())
        return;

    mState.mCursorPosition = GetActualCursorCoordinates();
    auto lindex            = mState.mCursorPosition.mLine;
    auto cindex            = GetCharacterIndex(mState.mCursorPosition);

    while (aAmount-- > 0) {
        const auto &line  = mLines[lindex];
        if (cindex == 0) {
            if (lindex > 0) {
                --lindex;
                if ((int)mLines.size() > lindex)
                    cindex = (int)mLines[lindex].size();
                else
                    cindex = 0;
            }
        } else {
            --cindex;
            if (cindex > 0) {
                if ((int)mLines.size() > lindex) {
                    while (cindex > 0 && IsUTFSequence(line[cindex].mChar))
                        --cindex;
                }
            }
        }

        mState.mCursorPosition = Coordinates(lindex, GetCharacterColumn(lindex, cindex));
        if (aWordMode) {
            mState.mCursorPosition = FindWordStart(mState.mCursorPosition);
            cindex                 = GetCharacterIndex(mState.mCursorPosition);
        }
    }

    mState.mCursorPosition = Coordinates(lindex, GetCharacterColumn(lindex, cindex));

    IM_ASSERT(mState.mCursorPosition.mColumn >= 0);
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

void TextEditor::MoveRight(int aAmount, bool aSelect, bool aWordMode) {
    ResetCursorBlinkTime();
    auto oldPos = mState.mCursorPosition;

    if (isEmpty() || oldPos.mLine >= mLines.size())
        return;

    mState.mCursorPosition = GetActualCursorCoordinates();
    auto cindex = GetCharacterIndex(mState.mCursorPosition);
    auto lindex = mState.mCursorPosition.mLine;

    while (aAmount-- > 0) {
        auto &line  = mLines[lindex];

        if (cindex >= line.size()) {
            if (lindex < mLines.size() - 1) {
                ++lindex;
                cindex = 0;
            }
        } else {
            ++cindex;
            if (cindex < (int)line.size()) {
                if ((int)mLines.size() > lindex) {
                    while (cindex < (int)line.size() && IsUTFSequence(line[cindex].mChar))
                        ++cindex;
                }
            }
        }

        mState.mCursorPosition = Coordinates(lindex, GetCharacterColumn(lindex, cindex));

        if (aWordMode) {
            mState.mCursorPosition = FindWordEnd(mState.mCursorPosition);
            cindex = GetCharacterIndex(mState.mCursorPosition);
        }
    }

    mState.mCursorPosition = Coordinates(lindex, GetCharacterColumn(lindex, cindex));

    IM_ASSERT(mState.mCursorPosition.mColumn >= 0);
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

    if (mState.mCursorPosition != oldPos) {
        if (aSelect) {
            mInteractiveEnd   = oldPos;
            mInteractiveStart = mState.mCursorPosition;
        } else
            mInteractiveStart = mInteractiveEnd = mState.mCursorPosition;
        SetSelection(mInteractiveStart, mInteractiveEnd);
    }
}

void TextEditor::TextEditor::MoveBottom(bool aSelect) {
    ResetCursorBlinkTime();
    auto oldPos = GetCursorPosition();
    auto newPos = Coordinates((int)mLines.size() - 1, GetLineMaxColumn((int)mLines.size() - 1));
    SetCursorPosition(newPos);
    if (aSelect) {
        mInteractiveStart = oldPos;
        mInteractiveEnd   = newPos;
    } else
        mInteractiveStart = mInteractiveEnd = newPos;
    SetSelection(mInteractiveStart, mInteractiveEnd);
}

void TextEditor::MoveHome(bool aSelect) {
    ResetCursorBlinkTime();
    auto oldPos = mState.mCursorPosition;
    SetCursorPosition(Coordinates(mState.mCursorPosition.mLine, 0));

    if (mState.mCursorPosition != oldPos) {
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
    }
}

void TextEditor::MoveEnd(bool aSelect) {
    ResetCursorBlinkTime();
    auto oldPos = mState.mCursorPosition;
    SetCursorPosition(Coordinates(mState.mCursorPosition.mLine, GetLineMaxColumn(oldPos.mLine)));

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
    }
}

void TextEditor::Delete() {
    ResetCursorBlinkTime();
    IM_ASSERT(!mReadOnly);

    if (isEmpty())
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
            if (pos.mLine == (int)mLines.size() - 1)
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
            while (d-- > 0 && cindex < (int)line.size())
                line.erase(line.begin() + cindex);
        }

        mTextChanged = true;

        Colorize(pos.mLine, 1);
    }

    u.mAfter = mState;
    AddUndo(u);
    std::string findWord = mFindReplaceHandler.GetFindWord();
    if (!findWord.empty()) {
        mFindReplaceHandler.resetMatches();
        mFindReplaceHandler.FindAllMatches(this, findWord);
    }
}

void TextEditor::Backspace() {
    ResetCursorBlinkTime();
    IM_ASSERT(!mReadOnly);

    if (isEmpty())
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
        Colorize(mState.mCursorPosition.mLine, 1);
    }

    u.mAfter = mState;
    AddUndo(u);
    std::string findWord = mFindReplaceHandler.GetFindWord();
    if (!findWord.empty()) {
        mFindReplaceHandler.resetMatches();
        mFindReplaceHandler.FindAllMatches(this, findWord);
    }
}

void TextEditor::SelectWordUnderCursor() {
    auto c = GetCursorPosition();
    SetSelection(FindWordStart(c), FindWordEnd(c));
}

void TextEditor::SelectAll() {
    SetSelection(Coordinates(0, 0), Coordinates((int)mLines.size(), 0));
}

bool TextEditor::HasSelection() const {
    return !isEmpty() && mState.mSelectionEnd > mState.mSelectionStart;
}

void TextEditor::Copy() {
    if (HasSelection()) {
        ImGui::SetClipboardText(GetSelectedText().c_str());
    } else {
        if (!isEmpty()) {
            std::string str;
            auto &line = mLines[GetActualCursorCoordinates().mLine];
            for (auto &g : line)
                str.push_back(g.mChar);
            ImGui::SetClipboardText(str.c_str());
        }
    }
}

void TextEditor::Cut() {
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
    std::string findWord = mFindReplaceHandler.GetFindWord();
    if (!findWord.empty()) {
        mFindReplaceHandler.resetMatches();
        mFindReplaceHandler.FindAllMatches(this, findWord);
    }
}

std::string TextEditor::ReplaceStrings(std::string string, const std::string &search, const std::string &replace) {
    if (search.empty())
        return string;

    std::size_t pos = 0;
    while ((pos = string.find(search, pos)) != std::string::npos) {
        string.replace(pos, search.size(), replace);
        pos += replace.size();
    }

    return string;
}

std::vector<std::string> TextEditor::SplitString(const std::string &string, const std::string &delimiter, bool removeEmpty) {
    if (delimiter.empty()) {
        return { string };
    }

    std::vector<std::string> result;

    size_t start = 0, end = 0;
    while ((end = string.find(delimiter, start)) != std::string::npos) {
        size_t size = end - start;
        if (start + size > string.length())
            break;

        auto token = string.substr(start, end - start);
        start = end + delimiter.length();
        result.emplace_back(std::move(token));
    }

    result.emplace_back(string.substr(start));

    if (removeEmpty)
        std::erase_if(result, [](const auto &string) { return string.empty(); });

    return result;
}


std::string TextEditor::ReplaceTabsWithSpaces(const std::string& string, uint32_t tabSize) {
    if (tabSize == 0 || string.empty() || string.find('\t') == std::string::npos)
        return string;

    auto stringVector = SplitString(string, "\n", false);
    auto size = stringVector.size();
    std::string result;
    for (size_t i = 0; i < size; i++) {
        auto &line = stringVector[i];
        std::size_t pos = 0;
        while ((pos = line.find('\t', pos)) != std::string::npos) {
            auto spaces = tabSize - (pos % tabSize);
            line.replace(pos, 1, std::string(spaces, ' '));
            pos += tabSize - 1;
        }
        result += line;
        if (i < size - 1)
            result += "\n";
    }
    return result;
}


std::string TextEditor::PreprocessText(const std::string &code) {
    std::string result = ReplaceStrings(code, "\r\n", "\n");
    result = ReplaceStrings(result, "\r", "\n");
    result = ReplaceTabsWithSpaces(result, 4);

    return result;
}

void TextEditor::Paste() {
    if (IsReadOnly())
        return;

    auto clipText = ImGui::GetClipboardText();
    if (clipText != nullptr && strlen(clipText) > 0) {
        std::string text = PreprocessText(clipText);

        UndoRecord u;
        u.mBefore = mState;

        if (HasSelection()) {
            u.mRemoved      = GetSelectedText();
            u.mRemovedStart = mState.mSelectionStart;
            u.mRemovedEnd   = mState.mSelectionEnd;
            DeleteSelection();
        }

        u.mAdded      = text;
        u.mAddedStart = GetActualCursorCoordinates();

        InsertText(text);

        u.mAddedEnd = GetActualCursorCoordinates();
        u.mAfter    = mState;
        AddUndo(u);
    }
    std::string findWord = mFindReplaceHandler.GetFindWord();
    if (!findWord.empty()) {
        mFindReplaceHandler.resetMatches();
        mFindReplaceHandler.FindAllMatches(this, findWord);
    }
}

bool TextEditor::CanUndo() {
    return !mReadOnly && mUndoIndex > 0;
}

bool TextEditor::CanRedo() const {
    return !mReadOnly && mUndoIndex < (int)mUndoBuffer.size();
}

void TextEditor::Undo(int aSteps) {
    while (CanUndo() && aSteps-- > 0)
        mUndoBuffer[--mUndoIndex].Undo(this);
    std::string findWord = mFindReplaceHandler.GetFindWord();
    if (!findWord.empty()) {
        mFindReplaceHandler.resetMatches();
        mFindReplaceHandler.FindAllMatches(this, findWord);
    }
}

void TextEditor::Redo(int aSteps) {
    while (CanRedo() && aSteps-- > 0)
        mUndoBuffer[mUndoIndex++].Redo(this);
    std::string findWord = mFindReplaceHandler.GetFindWord();
    if (!findWord.empty()) {
        mFindReplaceHandler.resetMatches();
        mFindReplaceHandler.FindAllMatches(this, findWord);
    }
}

// the index here is array index so zero based
void TextEditor::FindReplaceHandler::SelectFound(TextEditor *editor, int index) {
    IM_ASSERT(index >= 0 && index < mMatches.size());
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

    int count = mMatches.size();
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
bool TextEditor::FindReplaceHandler::FindNext(TextEditor *editor) {
    Coordinates curPos;
    curPos.mLine = mMatches.empty() ? editor->mState.mCursorPosition.mLine : mMatches.back().mCursorPosition.mLine;
    curPos.mColumn = mMatches.empty() ? editor->mState.mCursorPosition.mColumn : editor->Utf8CharsToBytes(
            mMatches.back().mCursorPosition);

    unsigned long matchLength = editor->GetStringCharacterCount(mFindWord);
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

        size_t pos=0;
        std::sregex_iterator iter = std::sregex_iterator(textSrc.begin(), textSrc.end(), regularExpression);
        std::sregex_iterator end;
        if (!iter->ready())
            return false;
        size_t firstLoc = iter->position();
        unsigned long firstLength = iter->length();

        if(firstLoc > byteIndex) {
            pos = firstLoc;
            matchLength = firstLength;
        }  else {

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
    state.mSelectionStart = editor->StringIndexToCoordinates(textLoc,textSrc);
    state.mSelectionEnd = editor->StringIndexToCoordinates(textLoc + matchLength,textSrc);
    state.mCursorPosition = state.mSelectionEnd;
    mMatches.push_back(state);
    return true;
}

void TextEditor::FindReplaceHandler::FindAllMatches(TextEditor *editor,std::string findWord) {

    if (findWord.empty()) {
        editor->EnsureCursorVisible();
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
    auto saveState = editor->mState;
    Coordinates begin = Coordinates(0,0);
    editor->mState.mCursorPosition = begin;

    if (!FindNext(editor)) {
        editor->mState = saveState;
        editor->EnsureCursorVisible();
        return;
    }
    TextEditor::EditorState state = mMatches.back();

    while( state.mCursorPosition < startingPos) {
        if (!FindNext(editor)) {
            editor->mState = saveState;
            editor->EnsureCursorVisible();
            return;
        }
        state = mMatches.back();
    }

    while (FindNext(editor));

    editor->mState = saveState;
    editor->EnsureCursorVisible();
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

        editor->EnsureCursorVisible();
        ImGui::SetKeyboardFocusHere(0);

        u.mAfter = editor->mState;
        editor->AddUndo(u);
        editor->mTextChanged = true;

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

const TextEditor::Palette &TextEditor::GetLightPalette() {
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

const TextEditor::Palette &TextEditor::GetRetroBluePalette() {
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


std::string TextEditor::GetText() const {
    return GetText(Coordinates(), Coordinates((int)mLines.size(), 0));
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
    return GetLineText(mState.mCursorPosition.mLine);
}

std::string TextEditor::GetLineText(int line) const {
    auto lineLength = GetLineCharacterCount(line);
    return GetText(Coordinates(line, 0),Coordinates(line, lineLength));
}

void TextEditor::ProcessInputs() {
}

void TextEditor::Colorize(int aFromLine, int aLines) {
    int toLine     = aLines == -1 ? (int)mLines.size() : std::min((int)mLines.size(), aFromLine + aLines);
    mColorRangeMin = std::min(mColorRangeMin, aFromLine);
    mColorRangeMax = std::max(mColorRangeMax, toLine);
    mColorRangeMin = std::max(0, mColorRangeMin);
    mColorRangeMax = std::max(mColorRangeMin, mColorRangeMax);
    mCheckComments = true;
}

void TextEditor::ColorizeRange(int aFromLine, int aToLine) {
    if (isEmpty() || aFromLine >= aToLine)
        return;

    std::string buffer;
    std::cmatch results;
    std::string id;

    int endLine = std::max(0, std::min((int)mLines.size(), aToLine));
    for (int i = aFromLine; i < endLine; ++i) {
        auto &line = mLines[i];

        if (line.empty())
            continue;

        buffer.resize(line.size());
        for (size_t j = 0; j < line.size(); ++j) {
            auto &col       = line[j];
            buffer[j]       = col.mChar;
            col.mColorIndex = PaletteIndex::Default;
        }

        const char *bufferBegin = &buffer.front();
        const char *bufferEnd   = bufferBegin + buffer.size();

        auto last = bufferEnd;

        for (auto first = bufferBegin; first != last;) {
            const char *token_begin  = nullptr;
            const char *token_end    = nullptr;
            PaletteIndex token_color = PaletteIndex::Default;

            bool hasTokenizeResult = false;

            if (mLanguageDefinition.mTokenize != nullptr) {
                if (mLanguageDefinition.mTokenize(first, last, token_begin, token_end, token_color))
                    hasTokenizeResult = true;
            }

            if (!hasTokenizeResult) {
                // todo : remove
                // printf("using regex for %.*s\n", first + 10 < last ? 10 : int(last - first), first);

                for (auto &p : mRegexList) {
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
                    if (!mLanguageDefinition.mCaseSensitive)
                        std::transform(id.begin(), id.end(), id.begin(), ::toupper);

                    if (!line[first - bufferBegin].mPreprocessor) {
                        if (mLanguageDefinition.mKeywords.count(id) != 0)
                            token_color = PaletteIndex::Keyword;
                        else if (mLanguageDefinition.mIdentifiers.count(id) != 0)
                            token_color = PaletteIndex::KnownIdentifier;
                        else if (mLanguageDefinition.mPreprocIdentifiers.count(id) != 0)
                            token_color = PaletteIndex::PreprocIdentifier;
                    } else {
                        if (mLanguageDefinition.mPreprocIdentifiers.count(id) != 0)
                            token_color = PaletteIndex::PreprocIdentifier;
                    }
                }

                for (size_t j = 0; j < token_length; ++j)
                    line[(token_begin - bufferBegin) + j].mColorIndex = token_color;

                first = token_end;
            }
        }
    }
}

void TextEditor::ColorizeInternal() {
    if (isEmpty() || !mColorizerEnabled)
        return;

    if (mCheckComments) {
        auto endLine                 = mLines.size();
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
        auto &startStr               = mLanguageDefinition.mCommentStart;
        auto &singleStartStr         = mLanguageDefinition.mSingleLineComment;
        auto &docStartStr            = mLanguageDefinition.mDocComment;
        auto &globalStartStr         = mLanguageDefinition.mGlobalDocComment;

        std::vector<bool> ifDefs;
        ifDefs.push_back(true);

        while (currentLine < endLine || currentIndex < endIndex) {
            auto &line = mLines[currentLine];

            auto setGlyphFlags = [&](int index) {
                line[index].mMultiLineComment = withinComment;
                line[index].mComment          = withinSingleLineComment;
                line[index].mDocComment       = withinDocComment;
                line[index].mGlobalDocComment = withinGlobalDocComment;
                line[index].mDeactivated      = withinNotDef;
            };

            if (currentIndex == 0) {
                withinSingleLineComment = false;
                withinPreproc           = false;
                firstChar               = true;
            }

            if (!line.empty()) {
                auto &g = line[currentIndex];
                auto c  = g.mChar;

                if (c != mLanguageDefinition.mPreprocChar && !isspace(c))
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
                    if (firstChar && c == mLanguageDefinition.mPreprocChar) {
                        withinPreproc = true;
                        std::string directive;
                        auto start = currentIndex + 1;
                        while (start < (int) line.size() && !isspace(line[start].mChar)) {
                            directive += line[start].mChar;
                            start++;
                        }

                        if (start < (int) line.size()) {

                            if (isspace(line[start].mChar)) {
                                start += 1;
                                 if (directive == "define") {
                                     while (start < (int) line.size() && isspace(line[start].mChar))
                                         start++;
                                     std::string identifier;
                                     while (start < (int) line.size() && !isspace(line[start].mChar)) {
                                         identifier += line[start].mChar;
                                         start++;
                                     }
                                     if (identifier.size() > 0 && !withinNotDef && std::find(mDefines.begin(),mDefines.end(),identifier) == mDefines.end())
                                         mDefines.push_back(identifier);
                                    } else if (directive == "undef") {
                                         while (start < (int) line.size() && isspace(line[start].mChar))
                                             start++;
                                         std::string identifier;
                                         while (start < (int) line.size() && !isspace(line[start].mChar)) {
                                             identifier += line[start].mChar;
                                             start++;
                                         }
                                         if (identifier.size() > 0  && !withinNotDef)
                                             mDefines.erase(std::remove(mDefines.begin(), mDefines.end(), identifier), mDefines.end());
                                } else if (directive == "ifdef") {
                                    while (start < (int) line.size() && isspace(line[start].mChar))
                                        start++;
                                    std::string identifier;
                                    while (start < (int) line.size() && !isspace(line[start].mChar)) {
                                        identifier += line[start].mChar;
                                        start++;
                                    }
                                    if (!withinNotDef) {
                                        bool isConditionMet = std::find(mDefines.begin(),mDefines.end(),identifier) != mDefines.end();
                                        ifDefs.push_back(isConditionMet);
                                    } else
                                        ifDefs.push_back(false);
                                } else if (directive == "ifndef") {
                                    while (start < (int) line.size() && isspace(line[start].mChar))
                                        start++;
                                    std::string identifier;
                                    while (start < (int) line.size() && !isspace(line[start].mChar)) {
                                        identifier += line[start].mChar;
                                        start++;
                                    }
                                    if (!withinNotDef) {
                                        bool isConditionMet =  std::find(mDefines.begin(),mDefines.end(),identifier) == mDefines.end();
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
                        auto pred            = [](const char &a, const Glyph &b) { return a == b.mChar; };

                        auto compareForth    = [&](const std::string &a, const std::vector<Glyph> &b) {
                            return !a.empty() && currentIndex + a.size() <= b.size() && equals(a.begin(), a.end(),
                                    b.begin() + currentIndex, b.begin() + currentIndex + a.size(), pred);
                        };

                        auto compareBack     = [&](const std::string &a, const std::vector<Glyph> &b) {
                            return !a.empty() && currentIndex + 1 >= (int)a.size() && equals(a.begin(), a.end(),
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

                        auto &endStr = mLanguageDefinition.mCommentEnd;
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
                    line[currentIndex].mPreprocessor = withinPreproc;
                
                currentIndex += UTF8CharLength(c);
                if (currentIndex >= (int)line.size()) {
                    withinNotDef = !ifDefs.back();
                    currentIndex = 0;
                    ++currentLine;
                }
            } else {
                currentIndex = 0;
                ++currentLine;
            }
        }
        mDefines.clear();
        mCheckComments = false;
    }

    if (mColorRangeMin < mColorRangeMax) {
        const int increment = (mLanguageDefinition.mTokenize == nullptr) ? 10 : 10000;
        const int to        = std::min(mColorRangeMin + increment, mColorRangeMax);
        ColorizeRange(mColorRangeMin, to);
        mColorRangeMin = to;

        if (mColorRangeMax == mColorRangeMin) {
            mColorRangeMin = std::numeric_limits<int>::max();
            mColorRangeMax = 0;
        }
        return;
    }
}

float TextEditor::TextDistanceToLineStart(const Coordinates &aFrom) const {
    auto &line      = mLines[aFrom.mLine];
    float distance  = 0.0f;
    float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ", nullptr, nullptr).x;
    int colIndex    = GetCharacterIndex(aFrom);
    for (size_t it = 0u; it < line.size() && it < colIndex;) {
        if (line[it].mChar == '\t') {
            distance = (1.0f + std::floor((1.0f + distance) / (float(mTabSize) * spaceSize))) * (float(mTabSize) * spaceSize);
            ++it;
        } else {
            auto d = UTF8CharLength(line[it].mChar);
            char tempCString[7];
            int i = 0;
            for (; i < 6 && d-- > 0 && it < (int)line.size(); i++, it++)
                tempCString[i] = line[it].mChar;

            tempCString[i] = '\0';
            distance += ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, tempCString, nullptr, nullptr).x;
        }
    }

    return distance;
}

void TextEditor::SetScrollY() {
    if (!mWithinRender) {
        mSetScrollY = true;
        return;
    } else {
        mSetScrollY = false;
        auto scrollY = ImGui::GetScrollY();
        ImGui::SetScrollY(std::clamp(scrollY+mScrollYIncrement,0.0f,ImGui::GetScrollMaxY()));
    }
}

void TextEditor::SetTopLine() {
    if (!mWithinRender) {
        mSetTopLine = true;
        return;
    } else {
        mSetTopLine = false;
        ImGui::SetScrollY(mTopLine * mCharAdvance.y);
        EnsureCursorVisible();
    }
}

void TextEditor::EnsureCursorVisible() {
    if (!mWithinRender) {
        mScrollToCursor = true;
        return;
    }

    auto scrollBarSize = ImGui::GetStyle().ScrollbarSize;
    float scrollX = ImGui::GetScrollX();
    float scrollY = ImGui::GetScrollY();

    auto windowPadding = ImGui::GetStyle().FramePadding * 2.0f;

    auto height = ImGui::GetWindowHeight() - mTopMargin - scrollBarSize - mCharAdvance.y;
    auto width  = ImGui::GetWindowWidth() - windowPadding.x - scrollBarSize;

    auto top    = (int)rint((mTopMargin > scrollY ? mTopMargin -scrollY : scrollY) / mCharAdvance.y);
    auto bottom = top + (int)rint(height / mCharAdvance.y);

    auto left  = (int)rint(scrollX / mCharAdvance.x);
    auto right = left + (int)rint(width / mCharAdvance.x);

    auto pos = GetActualCursorCoordinates();
    pos.mColumn = (int)rint(TextDistanceToLineStart(pos) / mCharAdvance.x);

    bool mScrollToCursorX = true;
    bool mScrollToCursorY = true;

    if (pos.mLine >= top && pos.mLine <= bottom)
        mScrollToCursorY = false;
    if ((pos.mColumn >= left) && (pos.mColumn  <= right))
        mScrollToCursorX = false;
    if (!mScrollToCursorX && !mScrollToCursorY && mOldTopMargin == mTopMargin) {
        mScrollToCursor = false;
        mOldTopMargin = mTopMargin;
        return;
    }

    if (mScrollToCursorY) {
        if (pos.mLine < top)
            ImGui::SetScrollY(std::max(0.0f, pos.mLine * mCharAdvance.y));
        if (pos.mLine > bottom)
            ImGui::SetScrollY(std::max(0.0f, pos.mLine * mCharAdvance.y - height));
    }
    if (mScrollToCursorX) {
        if (pos.mColumn < left)
            ImGui::SetScrollX(std::max(0.0f, pos.mColumn * mCharAdvance.x ));
        if (pos.mColumn > right)
            ImGui::SetScrollX(std::max(0.0f, pos.mColumn * mCharAdvance.x - width));
    }
    mOldTopMargin = mTopMargin;
}

float TextEditor::GetPageSize() const {
    auto height = ImGui::GetCurrentWindow()->InnerClipRect.GetHeight();
    return height / mCharAdvance.y;
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
    IM_ASSERT(mAddedStart <= mAddedEnd);
    IM_ASSERT(mRemovedStart <= mRemovedEnd);
}

void TextEditor::UndoRecord::Undo(TextEditor *aEditor) {
    if (!mAdded.empty()) {
        aEditor->DeleteRange(mAddedStart, mAddedEnd);
        aEditor->Colorize(mAddedStart.mLine - 1, mAddedEnd.mLine - mAddedStart.mLine + 2);
    }

    if (!mRemoved.empty()) {
        auto start = mRemovedStart;
        aEditor->InsertTextAt(start, mRemoved.c_str());
        aEditor->Colorize(mRemovedStart.mLine - 1, mRemovedEnd.mLine - mRemovedStart.mLine + 2);
    }

    aEditor->mState = mBefore;
    aEditor->EnsureCursorVisible();
}

void TextEditor::UndoRecord::Redo(TextEditor *aEditor) {
    if (!mRemoved.empty()) {
        aEditor->DeleteRange(mRemovedStart, mRemovedEnd);
        aEditor->Colorize(mRemovedStart.mLine - 1, mRemovedEnd.mLine - mRemovedStart.mLine + 1);
    }

    if (!mAdded.empty()) {
        auto start = mAddedStart;
        aEditor->InsertTextAt(start, mAdded.c_str());
        aEditor->Colorize(mAddedStart.mLine - 1, mAddedEnd.mLine - mAddedStart.mLine + 1);
    }

    aEditor->mState = mAfter;
    aEditor->EnsureCursorVisible();

}

bool TokenizeCStyleString(const char *in_begin, const char *in_end, const char *&out_begin, const char *&out_end) {
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

bool TokenizeCStyleCharacterLiteral(const char *in_begin, const char *in_end, const char *&out_begin, const char *&out_end) {
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

bool TokenizeCStyleIdentifier(const char *in_begin, const char *in_end, const char *&out_begin, const char *&out_end) {
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

bool TokenizeCStyleNumber(const char *in_begin, const char *in_end, const char *&out_begin, const char *&out_end) {
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

bool TokenizeCStylePunctuation(const char *in_begin, const char *in_end, const char *&out_begin, const char *&out_end) {
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
            langDef.mKeywords.insert(k);

        static const char *const identifiers[] = {
            "abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh", "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha", "isdigit", "isgraph", "ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp", "modf", "pow", "printf", "sprintf", "snprintf", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand", "strcat", "strcmp", "strerror", "time", "tolower", "toupper", "std", "string", "vector", "map", "unordered_map", "set", "unordered_set", "min", "max"
        };
        for (auto &k : identifiers) {
            Identifier id;
            id.mDeclaration = "Built-in function";
            langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
        }

        langDef.mTokenize = [](const char *in_begin, const char *in_end, const char *&out_begin, const char *&out_end, PaletteIndex &paletteIndex) -> bool {
            paletteIndex = PaletteIndex::Max;

            while (in_begin < in_end && isascii(*in_begin) && isblank(*in_begin))
                in_begin++;

            if (in_begin == in_end) {
                out_begin    = in_end;
                out_end      = in_end;
                paletteIndex = PaletteIndex::Default;
            } else if (TokenizeCStyleString(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::String;
            else if (TokenizeCStyleCharacterLiteral(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::CharLiteral;
            else if (TokenizeCStyleIdentifier(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::Identifier;
            else if (TokenizeCStyleNumber(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::Number;
            else if (TokenizeCStylePunctuation(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::Punctuation;

            return paletteIndex != PaletteIndex::Max;
        };

        langDef.mCommentStart      = "/*";
        langDef.mCommentEnd        = "*/";
        langDef.mSingleLineComment = "//";

        langDef.mCaseSensitive   = true;
        langDef.mAutoIndentation = true;

        langDef.mName = "C++";

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
            langDef.mKeywords.insert(k);

        static const char *const identifiers[] = {
            "abort", "abs", "acos", "all", "AllMemoryBarrier", "AllMemoryBarrierWithGroupSync", "any", "asdouble", "asfloat", "asin", "asint", "asint", "asuint", "asuint", "atan", "atan2", "ceil", "CheckAccessFullyMapped", "clamp", "clip", "cos", "cosh", "countbits", "cross", "D3DCOLORtoUBYTE4", "ddx", "ddx_coarse", "ddx_fine", "ddy", "ddy_coarse", "ddy_fine", "degrees", "determinant", "DeviceMemoryBarrier", "DeviceMemoryBarrierWithGroupSync", "distance", "dot", "dst", "errorf", "EvaluateAttributeAtCentroid", "EvaluateAttributeAtSample", "EvaluateAttributeSnapped", "exp", "exp2", "f16tof32", "f32tof16", "faceforward", "firstbithigh", "firstbitlow", "floor", "fma", "fmod", "frac", "frexp", "fwidth", "GetRenderTargetSampleCount", "GetRenderTargetSamplePosition", "GroupMemoryBarrier", "GroupMemoryBarrierWithGroupSync", "InterlockedAdd", "InterlockedAnd", "InterlockedCompareExchange", "InterlockedCompareStore", "InterlockedExchange", "InterlockedMax", "InterlockedMin", "InterlockedOr", "InterlockedXor", "isfinite", "isinf", "isnan", "ldexp", "length", "lerp", "lit", "log", "log10", "log2", "mad", "max", "min", "modf", "msad4", "mul", "noise", "normalize", "pow", "printf", "Process2DQuadTessFactorsAvg", "Process2DQuadTessFactorsMax", "Process2DQuadTessFactorsMin", "ProcessIsolineTessFactors", "ProcessQuadTessFactorsAvg", "ProcessQuadTessFactorsMax", "ProcessQuadTessFactorsMin", "ProcessTriTessFactorsAvg", "ProcessTriTessFactorsMax", "ProcessTriTessFactorsMin", "radians", "rcp", "reflect", "refract", "reversebits", "round", "rsqrt", "saturate", "sign", "sin", "sincos", "sinh", "smoothstep", "sqrt", "step", "tan", "tanh", "tex1D", "tex1D", "tex1Dbias", "tex1Dgrad", "tex1Dlod", "tex1Dproj", "tex2D", "tex2D", "tex2Dbias", "tex2Dgrad", "tex2Dlod", "tex2Dproj", "tex3D", "tex3D", "tex3Dbias", "tex3Dgrad", "tex3Dlod", "tex3Dproj", "texCUBE", "texCUBE", "texCUBEbias", "texCUBEgrad", "texCUBElod", "texCUBEproj", "transpose", "trunc"
        };
        for (auto &k : identifiers) {
            Identifier id;
            id.mDeclaration = "Built-in function";
            langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
        }

        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Preprocessor));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

        langDef.mCommentStart      = "/*";
        langDef.mCommentEnd        = "*/";
        langDef.mSingleLineComment = "//";

        langDef.mCaseSensitive   = true;
        langDef.mAutoIndentation = true;

        langDef.mName = "HLSL";

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
            langDef.mKeywords.insert(k);

        static const char *const identifiers[] = {
            "abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh", "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha", "isdigit", "isgraph", "ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp", "modf", "pow", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand", "strcat", "strcmp", "strerror", "time", "tolower", "toupper"
        };
        for (auto &k : identifiers) {
            Identifier id;
            id.mDeclaration = "Built-in function";
            langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
        }

        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Preprocessor));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

        langDef.mCommentStart      = "/*";
        langDef.mCommentEnd        = "*/";
        langDef.mSingleLineComment = "//";

        langDef.mCaseSensitive   = true;
        langDef.mAutoIndentation = true;

        langDef.mName = "GLSL";

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
            langDef.mKeywords.insert(k);

        static const char *const identifiers[] = {
            "abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh", "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha", "isdigit", "isgraph", "ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp", "modf", "pow", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand", "strcat", "strcmp", "strerror", "time", "tolower", "toupper"
        };
        for (auto &k : identifiers) {
            Identifier id;
            id.mDeclaration = "Built-in function";
            langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
        }

        langDef.mTokenize = [](const char *in_begin, const char *in_end, const char *&out_begin, const char *&out_end, PaletteIndex &paletteIndex) -> bool {
            paletteIndex = PaletteIndex::Max;

            while (in_begin < in_end && isascii(*in_begin) && isblank(*in_begin))
                in_begin++;

            if (in_begin == in_end) {
                out_begin    = in_end;
                out_end      = in_end;
                paletteIndex = PaletteIndex::Default;
            } else if (TokenizeCStyleString(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::String;
            else if (TokenizeCStyleCharacterLiteral(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::CharLiteral;
            else if (TokenizeCStyleIdentifier(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::Identifier;
            else if (TokenizeCStyleNumber(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::Number;
            else if (TokenizeCStylePunctuation(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::Punctuation;

            return paletteIndex != PaletteIndex::Max;
        };

        langDef.mCommentStart      = "/*";
        langDef.mCommentEnd        = "*/";
        langDef.mSingleLineComment = "//";

        langDef.mCaseSensitive   = true;
        langDef.mAutoIndentation = true;

        langDef.mName = "C";

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
            langDef.mKeywords.insert(k);

        static const char *const identifiers[] = {
            "ABS", "ACOS", "ADD_MONTHS", "ASCII", "ASCIISTR", "ASIN", "ATAN", "ATAN2", "AVG", "BFILENAME", "BIN_TO_NUM", "BITAND", "CARDINALITY", "CASE", "CAST", "CEIL", "CHARTOROWID", "CHR", "COALESCE", "COMPOSE", "CONCAT", "CONVERT", "CORR", "COS", "COSH", "COUNT", "COVAR_POP", "COVAR_SAMP", "CUME_DIST", "CURRENT_DATE", "CURRENT_TIMESTAMP", "DBTIMEZONE", "DECODE", "DECOMPOSE", "DENSE_RANK", "DUMP", "EMPTY_BLOB", "EMPTY_CLOB", "EXP", "EXTRACT", "FIRST_VALUE", "FLOOR", "FROM_TZ", "GREATEST", "GROUP_ID", "HEXTORAW", "INITCAP", "INSTR", "INSTR2", "INSTR4", "INSTRB", "INSTRC", "LAG", "LAST_DAY", "LAST_VALUE", "LEAD", "LEAST", "LENGTH", "LENGTH2", "LENGTH4", "LENGTHB", "LENGTHC", "LISTAGG", "LN", "LNNVL", "LOCALTIMESTAMP", "LOG", "LOWER", "LPAD", "LTRIM", "MAX", "MEDIAN", "MIN", "MOD", "MONTHS_BETWEEN", "NANVL", "NCHR", "NEW_TIME", "NEXT_DAY", "NTH_VALUE", "NULLIF", "NUMTODSINTERVAL", "NUMTOYMINTERVAL", "NVL", "NVL2", "POWER", "RANK", "RAWTOHEX", "REGEXP_COUNT", "REGEXP_INSTR", "REGEXP_REPLACE", "REGEXP_SUBSTR", "REMAINDER", "REPLACE", "ROUND", "ROWNUM", "RPAD", "RTRIM", "SESSIONTIMEZONE", "SIGN", "SIN", "SINH", "SOUNDEX", "SQRT", "STDDEV", "SUBSTR", "SUM", "SYS_CONTEXT", "SYSDATE", "SYSTIMESTAMP", "TAN", "TANH", "TO_CHAR", "TO_CLOB", "TO_DATE", "TO_DSINTERVAL", "TO_LOB", "TO_MULTI_BYTE", "TO_NCLOB", "TO_NUMBER", "TO_SINGLE_BYTE", "TO_TIMESTAMP", "TO_TIMESTAMP_TZ", "TO_YMINTERVAL", "TRANSLATE", "TRIM", "TRUNC", "TZ_OFFSET", "UID", "UPPER", "USER", "USERENV", "VAR_POP", "VAR_SAMP", "VARIANCE", "VSIZE "
        };
        for (auto &k : identifiers) {
            Identifier id;
            id.mDeclaration = "Built-in function";
            langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
        }

        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\\'[^\\\']*\\\'", PaletteIndex::String));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

        langDef.mCommentStart      = "/*";
        langDef.mCommentEnd        = "*/";
        langDef.mSingleLineComment = "//";

        langDef.mCaseSensitive   = false;
        langDef.mAutoIndentation = false;

        langDef.mName = "SQL";

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
            langDef.mKeywords.insert(k);

        static const char *const identifiers[] = {
            "cos", "sin", "tab", "acos", "asin", "atan", "atan2", "cosh", "sinh", "tanh", "log", "log10", "pow", "sqrt", "abs", "ceil", "floor", "fraction", "closeTo", "fpFromIEEE", "fpToIEEE", "complex", "opEquals", "opAddAssign", "opSubAssign", "opMulAssign", "opDivAssign", "opAdd", "opSub", "opMul", "opDiv"
        };
        for (auto &k : identifiers) {
            Identifier id;
            id.mDeclaration = "Built-in function";
            langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
        }

        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\'\\\\?[^\\']\\'", PaletteIndex::String));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

        langDef.mCommentStart      = "/*";
        langDef.mCommentEnd        = "*/";
        langDef.mSingleLineComment = "//";

        langDef.mCaseSensitive   = true;
        langDef.mAutoIndentation = true;

        langDef.mName = "AngelScript";

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
            langDef.mKeywords.insert(k);

        static const char *const identifiers[] = {
            "assert", "collectgarbage", "dofile", "error", "getmetatable", "ipairs", "loadfile", "load", "loadstring", "next", "pairs", "pcall", "print", "rawequal", "rawlen", "rawget", "rawset", "select", "setmetatable", "tonumber", "tostring", "type", "xpcall", "_G", "_VERSION", "arshift", "band", "bnot", "bor", "bxor", "btest", "extract", "lrotate", "lshift", "replace", "rrotate", "rshift", "create", "resume", "running", "status", "wrap", "yield", "isyieldable", "debug", "getuservalue", "gethook", "getinfo", "getlocal", "getregistry", "getmetatable", "getupvalue", "upvaluejoin", "upvalueid", "setuservalue", "sethook", "setlocal", "setmetatable", "setupvalue", "traceback", "close", "flush", "input", "lines", "open", "output", "popen", "read", "tmpfile", "type", "write", "close", "flush", "lines", "read", "seek", "setvbuf", "write", "__gc", "__tostring", "abs", "acos", "asin", "atan", "ceil", "cos", "deg", "exp", "tointeger", "floor", "fmod", "ult", "log", "max", "min", "modf", "rad", "random", "randomseed", "sin", "sqrt", "string", "tan", "type", "atan2", "cosh", "sinh", "tanh", "pow", "frexp", "ldexp", "log10", "pi", "huge", "maxinteger", "mininteger", "loadlib", "searchpath", "seeall", "preload", "cpath", "path", "searchers", "loaded", "module", "require", "clock", "date", "difftime", "execute", "exit", "getenv", "remove", "rename", "setlocale", "time", "tmpname", "byte", "char", "dump", "find", "format", "gmatch", "gsub", "len", "lower", "match", "rep", "reverse", "sub", "upper", "pack", "packsize", "unpack", "concat", "maxn", "insert", "pack", "unpack", "remove", "move", "sort", "offset", "codepoint", "char", "len", "codes", "charpattern", "coroutine", "table", "io", "os", "string", "utf8", "bit32", "math", "debug", "package"
        };
        for (auto &k : identifiers) {
            Identifier id;
            id.mDeclaration = "Built-in function";
            langDef.mIdentifiers.insert(std::make_pair(std::string(k), id));
        }

        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::String));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("\\\'[^\\\']*\\\'", PaletteIndex::String));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::Number));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
        langDef.mTokenRegexStrings.push_back(std::make_pair<std::string, PaletteIndex>("[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/\\;\\,\\.]", PaletteIndex::Punctuation));

        langDef.mCommentStart      = "--[[";
        langDef.mCommentEnd        = "]]";
        langDef.mSingleLineComment = "--";

        langDef.mCaseSensitive   = true;
        langDef.mAutoIndentation = false;

        langDef.mName = "Lua";

        inited = true;
    }
    return langDef;
}
