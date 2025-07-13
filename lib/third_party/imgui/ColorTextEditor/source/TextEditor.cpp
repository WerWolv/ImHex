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

TextEditor::Palette sPaletteBase = TextEditor::GetDarkPalette();

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

const TextEditor::Palette& TextEditor::GetPalette() { return sPaletteBase; }

void TextEditor::SetPalette(const Palette &aValue) {
    sPaletteBase = aValue;
}

bool TextEditor::IsEndOfLine(const Coordinates &aCoordinates) const {
    if (aCoordinates.mLine < (int)mLines.size()) {
        auto &line = mLines[aCoordinates.mLine];
        auto cindex = GetCharacterIndex(aCoordinates);
        return cindex >= (int)line.size();
    }
    return true;
}

bool TextEditor::IsEndOfFile(const Coordinates &aCoordinates) const {
    if (aCoordinates.mLine < (int)mLines.size()) {
        auto &line = mLines[aCoordinates.mLine];
        auto cindex = GetCharacterIndex(aCoordinates);
        return cindex >= (int)line.size() && aCoordinates.mLine == (int)mLines.size() - 1;
    }
    return true;
}

std::string TextEditor::GetText(const Coordinates &aStart, const Coordinates &aEnd) const {
    std::string result="";
    auto start = SanitizeCoordinates(aStart);
    auto end   = SanitizeCoordinates(aEnd);
    auto lineStart = start.mLine;
    auto lineEnd   = end.mLine;
    auto startIndex = GetCharacterIndex(start);
    auto endIndex   = GetCharacterIndex(end);
    if (lineStart == lineEnd)
        result = mLines[lineStart].substr(startIndex,endIndex-startIndex);
    else {
        result = mLines[lineStart].substr(startIndex) + '\n';
        for (size_t i = lineStart+1; i < lineEnd; i++) {
            result += mLines[i].mChars + '\n';
        }
        result += mLines[lineEnd].substr(0, endIndex);
    }

    return result;
}

TextEditor::Coordinates TextEditor::GetActualCursorCoordinates() const {
    return SanitizeCoordinates(mState.mCursorPosition);
}

TextEditor::Coordinates TextEditor::SanitizeCoordinates(const Coordinates &aValue) const {
    Coordinates result = aValue;
    if (aValue.mLine < 0)
        result.mLine = mLines.size() + aValue.mLine;
    if (aValue.mColumn < 0)
        result.mColumn = GetLineMaxColumn(result.mLine) + aValue.mColumn + 1;

    result.mLine  = std::clamp(result.mLine, 0, (int)mLines.size()-1);
    result.mColumn = std::clamp(result.mColumn, 0, GetLineMaxColumn(result.mLine));
    return result;
}

// "Borrowed" from ImGui source
static inline void ImTextCharToUtf8(std::string &buffer, unsigned int c) {
    if (c < 0x80) {
        buffer += (char) c;
        return;
    }
    if (c < 0x800) {
        buffer += (char)(0xc0 + (c >> 6));
        buffer += (char)(0x80 + (c & 0x3f));
        return;
    }
    if (c >= 0xdc00 && c < 0xe000)
        return;
    if (c >= 0xd800 && c < 0xdc00) {
        buffer += (char)(0xf0 + (c >> 18));
        buffer += (char)(0x80 + ((c >> 12) & 0x3f));
        buffer += (char)(0x80 + ((c >> 6) & 0x3f));
        buffer += (char)(0x80 + ((c)&0x3f));
        return;
    }
    // else if (c < 0x10000)
    {
        buffer += (char)(0xe0 + (c >> 12));
        buffer += (char)(0x80 + ((c >> 6) & 0x3f));
        buffer += (char)(0x80 + ((c)&0x3f));
        return;
    }
}

static inline int ImTextCharToUtf8(char *buffer, int buf_size, unsigned int c) {
    std::string input;
    ImTextCharToUtf8(input,c);
    auto size = input.size();
    int i=0;
    for (;i<size;i++)
        buffer[i] = input[i];
    buffer[i]=0;
    return size;
}

void TextEditor::Advance(Coordinates &aCoordinates) const {
    if (IsEndOfFile(aCoordinates))
        return;
    if (IsEndOfLine(aCoordinates)) {
        if (aCoordinates.mLine + 1 < (int) mLines.size()) {
            ++aCoordinates.mLine;
            aCoordinates.mColumn = 0;
            return;
        }
        return;
    }
    auto &line = mLines[aCoordinates.mLine];
    auto characterIndex = GetCharacterIndex(aCoordinates);
    int maxDelta = line.size() - characterIndex;
    characterIndex += std::min(UTF8CharLength(line[characterIndex]), maxDelta);
    aCoordinates.mColumn = GetCharacterColumn(aCoordinates.mLine, characterIndex);
}

void TextEditor::DeleteRange(const Coordinates &aStart, const Coordinates &aEnd) {
    if (aEnd <= aStart || mReadOnly)
        return;
    auto start = SanitizeCoordinates(aStart);
    auto end   = SanitizeCoordinates(aEnd);
    auto startIndex = GetCharacterIndex(start);
    auto endIndex   = GetCharacterIndex(end);
    if (start.mLine == end.mLine) {
        auto &line = mLines[start.mLine];
        if (endIndex >= GetLineMaxColumn(start.mLine))
            line.erase(line.begin() + startIndex, -1);
        else
            line.erase(line.begin() + startIndex, endIndex-startIndex);
        line.mColorized = false;
    } else {
        auto &firstLine = mLines[start.mLine];
        auto &lastLine  = mLines[end.mLine];

        firstLine.erase(firstLine.begin() + startIndex, -1);
        lastLine.erase(lastLine.begin(), endIndex);

        if (aStart.mLine < aEnd.mLine) {
            firstLine.insert(firstLine.end(), lastLine.begin(), lastLine.end());
            firstLine.mColorized = false;
            RemoveLine(aStart.mLine + 1, aEnd.mLine);
        }
    }

    mTextChanged = true;
}

void TextEditor::AppendLine(const std::string &aValue) {
    auto text = PreprocessText(aValue);
    if (text.empty())
        return;
    if (isEmpty()) {
        mLines[0].mChars = text;
        mLines[0].mColors = std::string(text.size(),0);
        mLines[0].mFlags = std::string(text.size(),0);
    } else
        mLines.push_back(Line(text));
    SetCursorPosition(Coordinates((int)mLines.size() - 1, 0));
    mLines.back().mColorized = false;
    EnsureCursorVisible();
    mTextChanged = true;
}


int TextEditor::InsertTextAt(Coordinates & /* inout */ aWhere, const std::string &aValue) {
    auto text = PreprocessText(aValue);
    if (text.empty())
        return 0;
    auto start = SanitizeCoordinates(aWhere);
    auto &line = mLines[start.mLine];
    auto stringVector = SplitString(text, "\n", false);
    auto lineCount = (int)stringVector.size();
    aWhere.mLine += lineCount - 1;
    aWhere.mColumn += stringVector[lineCount-1].size();
    stringVector[lineCount - 1].append(line.substr(GetCharacterIndex(start)));
    if (GetCharacterIndex(start) < (int)line.size())
        line.erase(line.begin() + GetCharacterIndex(start),-1);

    line.append(stringVector[0]);
    line.mColorized = false;
    for (int i = 1; i < lineCount; i++) {
        InsertLine(start.mLine + i, stringVector[i]);
        mLines[start.mLine + i].mColorized = false;
    }
    mTextChanged = true;
    return lineCount;
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
    ImVec2 local = aPosition - ImGui::GetCursorScreenPos();

    int lineNo = std::max(0, (int)floor(local.y / mCharAdvance.y));
    if (local.x < (mLeftMargin - 2) || lineNo >= (int)mLines.size() || mLines[lineNo].empty())
        return Coordinates(std::min(lineNo,(int)mLines.size()-1), 0);

    local.x -= (mLeftMargin - 2);
    auto column = (int)floor(local.x / mCharAdvance.x);
    auto rem = static_cast<uint32_t>(local.x) % static_cast<uint32_t>(mCharAdvance.x);
    if (rem > mCharAdvance.x - 2)
        column++;

    return SanitizeCoordinates(Coordinates(lineNo, column));
}

void TextEditor::DeleteWordLeft()  {
    const auto wordEnd = GetCursorPosition();
    const auto wordStart = FindPreviousWord(GetCursorPosition());
    SetSelection(wordStart, wordEnd);
    Backspace();
}

void TextEditor::DeleteWordRight() {
    const auto wordStart = GetCursorPosition();
    const auto wordEnd = FindNextWord(GetCursorPosition());
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

    if (isWordChar(line.mChars[cindex])) {
        while (cindex > 0 && isWordChar(line.mChars[cindex-1]))
            --cindex;
    } else if (ispunct(line.mChars[cindex])) {
        while (cindex > 0 && ispunct(line.mChars[cindex-1]))
            --cindex;
    } else if (isspace(line.mChars[cindex])) {
        while (cindex > 0 && isspace(line.mChars[cindex-1]))
            --cindex;
    }
    return Coordinates(at.mLine, GetCharacterColumn(at.mLine, cindex));
}

TextEditor::Coordinates TextEditor::FindWordEnd(const Coordinates &aFrom) const {
    Coordinates at = aFrom;
    if (at.mLine >= (int)mLines.size())
        return at;

    auto &line  = mLines[at.mLine];
    auto cindex = GetCharacterIndex(at);

    if (isWordChar(line.mChars[cindex])) {
        while (cindex < (int)line.mChars.size() && isWordChar(line.mChars[cindex]))
            ++cindex;
    } else if (ispunct(line.mChars[cindex])) {
        while (cindex < (int)line.mChars.size() && ispunct(line.mChars[cindex]))
            ++cindex;
    } else if (isspace(line.mChars[cindex])) {
        while (cindex < (int)line.mChars.size() && isspace(line.mChars[cindex]))
            ++cindex;
    }
    return Coordinates(aFrom.mLine, GetCharacterColumn(aFrom.mLine, cindex));
}

TextEditor::Coordinates TextEditor::FindNextWord(const Coordinates &aFrom) const {
    Coordinates at = aFrom;
    if (at.mLine >= (int)mLines.size())
        return at;

    auto &line  = mLines[at.mLine];
    auto cindex = GetCharacterIndex(at);

    if (isspace(line.mChars[cindex])) {
        while (cindex < (int)line.mChars.size() && isspace(line.mChars[cindex]))
            ++cindex;
    }
    if (isWordChar(line.mChars[cindex])) {
        while (cindex < (int)line.mChars.size() && (isWordChar(line.mChars[cindex])))
            ++cindex;
    } else if (ispunct(line.mChars[cindex])) {
        while (cindex < (int)line.mChars.size() && (ispunct(line.mChars[cindex])))
            ++cindex;
    }
    return Coordinates(aFrom.mLine, GetCharacterColumn(aFrom.mLine, cindex));
}

TextEditor::Coordinates TextEditor::FindPreviousWord(const Coordinates &aFrom) const {
    Coordinates at = aFrom;
    if (at.mLine >= (int)mLines.size())
        return at;

    auto &line  = mLines[at.mLine];
    auto cindex = GetCharacterIndex(at);

    if (isspace(line.mChars[cindex-1])) {
        while (cindex > 0 && isspace(line.mChars[cindex-1]))
            --cindex;
    }
    if (isWordChar(line.mChars[cindex-1])) {
        while (cindex > 0 && isWordChar(line.mChars[cindex-1]))
            --cindex;
    } else if (ispunct(line.mChars[cindex-1])) {
        while (cindex > 0 &&  ispunct(line.mChars[cindex-1]))
            --cindex;
    }
    return Coordinates(at.mLine, GetCharacterColumn(at.mLine, cindex));
}



int TextEditor::Utf8CharsToBytes(std::string line, uint32_t start, uint32_t numChars) {
    if (line.empty())
        return 0;
    int column      = 0;
    int index      = 0;
    while (start + index < line.size() && column < numChars) {
        index += UTF8CharLength(line[start + index]);
        ++column;
    }
    return index;
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
        i += UTF8CharLength(line[i]);
        if (line[i] == '\t')
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
        const auto character = line[index];
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
    if (aLine < 0)
        return 0;
    auto &line = mLines[aLine];
    int col    = 0;
    int i      = 0;
    while (i < aIndex && i < (int)line.size()) {
        auto c = line[i];
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
    if (aLine < 0)
        return 0;
    auto &line = mLines[aLine];
    int c      = 0;
    for (unsigned i = 0; i < line.size(); c++)
        i += UTF8CharLength(line[i]);
    return c;
}

unsigned long long TextEditor::GetLineByteCount(int aLine) const {
    if (aLine >= mLines.size())
        return 0;
    if (aLine < 0)
        return 0;
    auto &line = mLines[aLine];
    return line.size();
}

int TextEditor::GetLineMaxColumn(int aLine) const {
    if (aLine >= mLines.size())
        return 0;
    if (aLine < 0)
        return 0;
    auto &line = mLines[aLine];
    int col    = 0;
    for (unsigned i = 0; i < line.size();) {
        auto c = line[i];
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

    if (mColorizerEnabled) {
        int wordStart     = 0;
        int newWordStart  = 0;
        int wordEnd       = 0;
        while (wordEnd < cindex) {
            wordStart = newWordStart;
            auto color = line.mColors[wordStart];
            int tokenLength = line.mColors.find_first_not_of(color, wordStart)-wordStart;
            wordEnd = wordStart + tokenLength;
            newWordStart = wordEnd + 1;
        }
        return cindex == wordStart || cindex == wordEnd;
    }
    return isspace(line[cindex]) != isspace(line[cindex - 1]);
}

void TextEditor::RemoveLine(int aStart, int aEnd) {

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
    if (start > end)
        std::swap(start, end);

    mLines.erase(mLines.begin() + aStart, mLines.begin() + aEnd + 1);

    mTextChanged = true;
}

void TextEditor::RemoveLine(int aIndex) {
    RemoveLine(aIndex, aIndex);
}

void TextEditor::InsertLine(int aIndex, const std::string &aText) {
    if (aIndex < 0 || aIndex > (int)mLines.size())
        return;
    auto &line = InsertLine(aIndex);
    line.append(aText);
    line.mColorized = false;
    mTextChanged = true;
}

TextEditor::Line &TextEditor::InsertLine(int aIndex) {

    if (isEmpty())
        return *mLines.insert(mLines.begin(), Line());

    if (aIndex == mLines.size())
        return *mLines.insert(mLines.end(), Line());

    auto newLine = Line();

    TextEditor::Line &result = *mLines.insert(mLines.begin() + aIndex, newLine);
    result.mColorized = false;

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
        r.push_back(mLines[aCoords.mLine][it]);

    return r;
}

uint8_t ByteCount(uint32_t number, std::string &bytes) {
    bytes[0] = static_cast<uint8_t>(number);
    if (!(number & 0xFFFFFF00))
        return 1;
    bytes[1] = static_cast<uint8_t>(number >> 8);
    if (!(number & 0xFFFF0000))
        return 2;
    bytes[2] = static_cast<uint8_t>(number >> 16);
    bytes[3] = static_cast<uint8_t>(number >> 24);
    return 4;
}

TextEditor::PaletteIndex TextEditor::GetColorIndexFromFlags(Line::Flags flags) {
    if (flags.mBits.mGlobalDocComment)
        return PaletteIndex::GlobalDocComment;
    if (flags.mBits.mBlockDocComment )
        return PaletteIndex::DocBlockComment;
    if (flags.mBits.mDocComment)
        return PaletteIndex::DocComment;
    if (flags.mBits.mBlockComment)
        return PaletteIndex::BlockComment;
    if (flags.mBits.mComment)
        return PaletteIndex::Comment;
    if (flags.mBits.mDeactivated)
        return PaletteIndex::PreprocessorDeactivated;
    if (flags.mBits.mPreprocessor)
        return PaletteIndex::Directive;
    return PaletteIndex::Default;
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
                    mState.mCursorPosition = ScreenPosToCoordinates(ImGui::GetMousePos());
                    auto line = mState.mCursorPosition.mLine;
                    mState.mSelectionStart = Coordinates(line, 0);
                    mState.mSelectionEnd = Coordinates(line, GetLineMaxColumn(line));
                }

                mLastClick = -1.0f;
                resetBlinking=true;
            }

            /*
            Left mouse button double click
            */

            else if (doubleClick) {
                if (!ctrl) {
                    mState.mCursorPosition = ScreenPosToCoordinates(ImGui::GetMousePos());
                    mState.mSelectionStart = FindWordStart(mState.mCursorPosition);
                    mState.mSelectionEnd = FindWordEnd(mState.mCursorPosition);
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
                    SelectWordUnderCursor();
                } else if (shift) {
                    mInteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos());
                    mState.mCursorPosition = mInteractiveEnd;
                    SetSelection(mInteractiveStart, mInteractiveEnd);
                } else {
                    mState.mCursorPosition = mInteractiveStart = mInteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos());
                    SetSelection(mInteractiveStart, mInteractiveEnd);
                }
                ResetCursorBlinkTime();
              
                EnsureCursorVisible();
                mLastClick = (float)ImGui::GetTime();
            } else if (rightClick) {
                auto cursorPosition = ScreenPosToCoordinates(ImGui::GetMousePos());

                if (!HasSelection() || mState.mSelectionStart > cursorPosition || cursorPosition > mState.mSelectionEnd) {
                    mState.mCursorPosition = mInteractiveStart = mInteractiveEnd = cursorPosition;
                    SetSelection(mInteractiveStart, mInteractiveEnd);
                }
                ResetCursorBlinkTime();
                mRaiseContextMenu = true;
            }
            // Mouse left button dragging (=> update selection)
            else if (ImGui::IsMouseDragging(0) && ImGui::IsMouseDown(0)) {
                io.WantCaptureMouse    = true;
                mState.mCursorPosition = mInteractiveEnd = ScreenPosToCoordinates(ImGui::GetMousePos());
                SetSelection(mInteractiveStart, mInteractiveEnd);
                resetBlinking=true;
            }
            if (resetBlinking)
                ResetCursorBlinkTime();
        }
    }
}

inline void TextUnformattedColoredAt(const ImVec2 &pos, const ImU32 &color, const char *text) {
    ImGui::SetCursorScreenPos(pos);
    ImGui::PushStyleColor(ImGuiCol_Text,color);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
}
uint32_t TextEditor::SkipSpaces(const Coordinates &aFrom) {
    auto line = aFrom.mLine;
    if (line >= mLines.size())
        return 0;
    auto &lines = mLines[line].mChars;
    auto &colors = mLines[line].mColors;
    auto cindex = GetCharacterIndex(aFrom);
    uint32_t s = 0;
    while (cindex < (int)lines.size() && lines[cindex] == ' ' && colors[cindex] == 0x00) {
        ++s;
        ++cindex;
    }
    if (mUpdateFocus)
        SetFocus();
    return s;
}

void TextEditor::SetFocus() {
    mState.mCursorPosition = mFocusAtCoords;
    ResetCursorBlinkTime();
    EnsureCursorVisible();
    if (!this->mReadOnly)
        ImGui::SetKeyboardFocusHere(0);
    mUpdateFocus = false;
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

    char buf[16];


    if (!mLines.empty()) {
        float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ", nullptr, nullptr).x;

        while (lineNo <= lineMax) {
            ImVec2 lineStartScreenPos = ImVec2(cursorScreenPos.x + mLeftMargin, mTopMargin + cursorScreenPos.y + std::floor(lineNo) * mCharAdvance.y);
            ImVec2 textScreenPos      = lineStartScreenPos;

            auto &line    = mLines[lineNo];
            auto colors = mLines[lineNo].mColors;
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
                ImGui::SetCursorScreenPos(ImVec2(lineNumbersStartPos.x, lineStartScreenPos.y));
                if (ImGui::InvisibleButton(lineNoStr.c_str(),ImVec2(mLineNumberFieldWidth,mCharAdvance.y))) {
                    if (mBreakpoints.contains(lineNo + 1))
                        mBreakpoints.erase(lineNo + 1);
                    else
                        mBreakpoints.insert(lineNo + 1);
                    mBreakPointsChanged = true;
                    mState.mCursorPosition = Coordinates(lineNo, 0);
                    JumpToCoords(mState.mCursorPosition);
                }

                // Draw breakpoints
                if (mBreakpoints.count(lineNo + 1) != 0) {
                    auto end = ImVec2(lineNoStartScreenPos.x + contentSize.x + mLineNumberFieldWidth, lineStartScreenPos.y + mCharAdvance.y);
                    drawList->AddRectFilled(ImVec2(lineNumbersStartPos.x, lineStartScreenPos.y), end, mPalette[(int)PaletteIndex::Breakpoint]);

                    drawList->AddCircleFilled(start + ImVec2(0, mCharAdvance.y) / 2, mCharAdvance.y / 3, mPalette[(int)PaletteIndex::Breakpoint]);
                    drawList->AddCircle(start + ImVec2(0, mCharAdvance.y) / 2, mCharAdvance.y / 3, mPalette[(int)PaletteIndex::Default]);
                    drawList->AddText(ImVec2(lineNoStartScreenPos.x + mLeftMargin, lineStartScreenPos.y),mPalette[(int) PaletteIndex::LineNumber], lineNoStr.c_str());
                }

                if (mState.mCursorPosition.mLine == lineNo && mShowCursor) {

                    // Highlight the current line (where the cursor is)
                    if (!HasSelection()) {
                        auto end = ImVec2(lineNoStartScreenPos.x + contentSize.x + mLineNumberFieldWidth, lineStartScreenPos.y + mCharAdvance.y);
                        drawList->AddRectFilled(ImVec2(lineNumbersStartPos.x, lineStartScreenPos.y), end, mPalette[(int)(focused ? PaletteIndex::CurrentLineFill : PaletteIndex::CurrentLineFillInactive)]);
                        drawList->AddRect(ImVec2(lineNumbersStartPos.x, lineStartScreenPos.y), end, mPalette[(int)PaletteIndex::CurrentLineEdge], 1.0f);
                    }
                }

                TextUnformattedColoredAt(ImVec2(mLeftMargin + lineNoStartScreenPos.x, lineStartScreenPos.y), mPalette[(int) PaletteIndex::LineNumber], lineNoStr.c_str());
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
                            auto c = line[cindex];
                            if (c == '\t') {
                                auto x = (1.0f + std::floor((1.0f + cx) / (float(mTabSize) * spaceSize))) * (float(mTabSize) * spaceSize);
                                width  = x - cx;
                            } else {
                                char buf2[2];
                                buf2[0] = line[cindex];
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
            if (line.empty()) {
                ImGui::Dummy(mCharAdvance);
                lineNo = std::floor(lineNo + 1.0F);
                if (mUpdateFocus)
                    SetFocus();
                continue;
            }
            int i = 0;
            auto colorsSize = static_cast<uint32_t >(colors.size());
            i += SkipSpaces(Coordinates(lineNo, i));
            while (i < colorsSize) {
                char color = colors[i];
                uint32_t tokenLength = colors.find_first_not_of(color, i) - i;
                if (mUpdateFocus)
                    SetFocus();
                color = std::clamp(color, (char)PaletteIndex::Default, (char)((uint8_t)PaletteIndex::Max-1));
                tokenLength = std::clamp(tokenLength, 1u, colorsSize - i);
                bool underwaved = false;
                ErrorMarkers::iterator errorIt;

                if (errorIt = mErrorMarkers.find(Coordinates(lineNo+1,i+1)); errorIt != mErrorMarkers.end()) {
                    underwaved = true;
                }

                mLineBuffer = line.substr(i, tokenLength);
                ImGui::PushStyleColor(ImGuiCol_Text, mPalette[(uint64_t) color]);
                auto charsBefore = ImGui::CalcTextSize(line.mChars.substr(0, i).c_str()).x;
                const ImVec2 textScreenPosition(lineStartScreenPos.x + charsBefore, lineStartScreenPos.y);
                ImGui::SetCursorScreenPos(textScreenPosition);
                ImGui::TextUnformatted(mLineBuffer.c_str());
                ImGui::PopStyleColor();
                mLineBuffer.clear();
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


                i += tokenLength;
                i += SkipSpaces(Coordinates(lineNo, i));

            }

            lineNo = std::floor(lineNo + 1.0F);
        }
    }
    ImVec2 lineStartScreenPos = ImVec2(cursorScreenPos.x + mLeftMargin, mTopMargin + cursorScreenPos.y + std::floor(lineNo) * mCharAdvance.y);
    if (!mIgnoreImGuiChild)
        ImGui::EndChild();

    if (mShowLineNumbers && !mIgnoreImGuiChild) {
            ImGui::BeginChild("##lineNumbers");
            ImGui::SetCursorScreenPos(ImVec2(lineNumbersStartPos.x, lineStartScreenPos.y));
        ImGui::Dummy(ImVec2(mLineNumberFieldWidth, (globalLineMax - lineMax - 1) * mCharAdvance.y + ImGui::GetCurrentWindow()->InnerClipRect.GetHeight() - mCharAdvance.y));
        ImGui::EndChild();
    }
    if (!mIgnoreImGuiChild)
        ImGui::BeginChild(aTitle);

    ImGui::SetCursorScreenPos(lineStartScreenPos);
    if (mShowLineNumbers)
        ImGui::Dummy(ImVec2(mLongestLineLength * mCharAdvance.x + mCharAdvance.x, (globalLineMax - lineMax - 2.0F) * mCharAdvance.y + ImGui::GetCurrentWindow()->InnerClipRect.GetHeight()));
    else
        ImGui::Dummy(ImVec2(mLongestLineLength * mCharAdvance.x + mCharAdvance.x, (globalLineMax - lineMax - 3.0F) * mCharAdvance.y + ImGui::GetCurrentWindow()->InnerClipRect.GetHeight() - 1.0f));

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
    mCursorPositionChanged = false;

    if (mLines.capacity() < 2*mLines.size())
        mLines.reserve(2*mLines.size());

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

    bool scroll_x = mLongestLineLength * mCharAdvance.x >= textEditorSize.x;

    bool scroll_y = mLines.size() > 1;
    if (!aBorder)
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
        ImGui::GetCurrentWindow()->ScrollbarY = true;
        ImGui::Scrollbar(ImGuiAxis_Y);
        ImGui::GetCurrentWindow()->ScrollbarY = false;
    }
    if (scroll_x) {
        ImGui::GetCurrentWindow()->ScrollbarX = true;
        ImGui::Scrollbar(ImGuiAxis_X);
        ImGui::GetCurrentWindow()->ScrollbarX = false;
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

void TextEditor::SetText(const std::string &aText, bool aUndo) {
    UndoRecord u;
    if (!mReadOnly && aUndo) {
        u.mBefore = mState;
        u.mRemoved = GetText();
        u.mRemovedStart = Coordinates(0, 0);
        u.mRemovedEnd = Coordinates((int) mLines.size() - 1, GetLineMaxColumn((int) mLines.size() - 1));
    }
    mLines.resize(1);
    mLines[0] = Line();
    mLines[0].mColorized = false;
    std::string text = PreprocessText(aText);
    for (auto chr : text) {
        if (chr == '\n') {
            mLines.push_back(Line());
            mLines.back().mColorized = false;
        } else
            mLines.back().push_back(Glyph(chr));
    }
    if (!mReadOnly && aUndo) {
        u.mAdded = text;
        u.mAddedStart = Coordinates(0, 0);
        u.mAddedEnd = Coordinates((int) mLines.size() - 1, GetLineMaxColumn((int) mLines.size() - 1));
    }
    mTextChanged = true;
    mScrollToTop = true;
    if (!mReadOnly && aUndo) {
        u.mAfter = mState;

        AddUndo(u);
    }

    Colorize();
}

void TextEditor::SetTextLines(const std::vector<std::string> &aLines) {
    mLines.clear();

    if (aLines.empty()) {
        mLines.emplace_back(Line());
        mLines[0].mColorized = false;
    } else {
         auto lineCount = aLines.size();
         mLines.resize(lineCount);
         for (size_t i = 0; i < lineCount; i++) {
             mLines[i].SetLine(PreprocessText(aLines[i]));
             mLines[i].mColorized = false;
         }
    }

    mTextChanged = true;
    mScrollToTop = true;
    SetTimeStamp(0);
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
                        if (line.front() == '\t') {
                            line.erase(line.begin());
                            modified = true;
                        } else {
                            for (int j = 0; j < mTabSize && !line.empty() && line.front() == ' '; j++) {
                                line.erase(line.begin());
                                modified = true;
                            }
                        }
                    }
                } else {
                    auto spacesToInsert = mTabSize - (start.mColumn % mTabSize);
                    std::string spaces(spacesToInsert, ' ');
                    line.insert(line.begin(), spaces.begin(), spaces.end());
                    line.mColorized = false;
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
            for (size_t it = 0; it < line.size() && isascii(line[it]) && isblank(line[it]); ++it)
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
        line.erase(line.begin() + cstart,-1);
        line.mColorized = false;
        SetCursorPosition(Coordinates(coord.mLine + 1, GetCharacterColumn(coord.mLine + 1, cpos)));
        u.mAdded = (char)aChar;
    } else if (aChar == '\t') {
        auto &line  = mLines[coord.mLine];
        auto cindex = GetCharacterIndex(coord);

        if (!aShift) {
            auto spacesToInsert = mTabSize - (cindex % mTabSize);
            std::string spaces(spacesToInsert, ' ');
            line.insert(line.begin() + cindex, spaces.begin(), spaces.end());
            line.mColorized = false;
            SetCursorPosition(Coordinates(coord.mLine, GetCharacterColumn(coord.mLine, cindex + spacesToInsert)));
        } else {
            auto spacesToRemove = (cindex % mTabSize);
            if (spacesToRemove == 0) spacesToRemove = mTabSize;
            spacesToRemove = std::min(spacesToRemove, (int32_t) line.size());
            for (int j = 0; j < spacesToRemove; j++) {
                if (*(line.begin() + (cindex - 1)) == ' ') {
                    line.erase(line.begin() + (cindex - 1));
                    cindex -= 1;
                }
            }
            line.mColorized = false;
            SetCursorPosition(Coordinates(coord.mLine, GetCharacterColumn(coord.mLine, std::max(0, cindex))));
        }

    } else {
        std::string buf = "";
        ImTextCharToUtf8(buf, aChar);
        if (buf.size() > 0) {
            auto &line  = mLines[coord.mLine];
            auto cindex = GetCharacterIndex(coord);

            if (mOverwrite && cindex < (int)line.size()) {
                auto d = UTF8CharLength(line[cindex]);

                u.mRemovedStart = mState.mCursorPosition;
                                                                                                                                                        u.mRemovedEnd   = Coordinates(coord.mLine, GetCharacterColumn(coord.mLine, cindex + d));
                u.mRemoved      = std::string(line.mChars.begin() + cindex, line.mChars.begin() + cindex + d);
                line.erase(line.begin() + cindex, d);
                line.mColorized = false;
            }
            line.insert(line.begin() + cindex, buf.begin(), buf.end());
            line.mColorized = false;
            u.mAdded = buf;
            auto charCount = GetStringCharacterCount(buf);
            SetCursorPosition(Coordinates(coord.mLine, GetCharacterColumn(coord.mLine, cindex + charCount)));
        } else
            return;
    }

    u.mAddedEnd = GetActualCursorCoordinates();
    u.mAfter    = mState;

    mTextChanged = true;

    AddUndo(u);

    Colorize();

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

void TextEditor::SetSelection(const Coordinates &aStart, const Coordinates &aEnd) {
    auto oldSelStart = mState.mSelectionStart;
    auto oldSelEnd   = mState.mSelectionEnd;

    mState.mSelectionStart = SanitizeCoordinates(aStart);
    mState.mSelectionEnd   = SanitizeCoordinates(aEnd);
    if (mState.mSelectionStart > mState.mSelectionEnd)
        std::swap(mState.mSelectionStart, mState.mSelectionEnd);

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
    auto text = PreprocessText(aValue);

    InsertTextAt(pos, text);
    mLines[pos.mLine].mColorized = false;

    SetSelection(pos, pos);
    SetCursorPosition(pos);

    std::string findWord = mFindReplaceHandler.GetFindWord();
    if (!findWord.empty()) {
        mFindReplaceHandler.resetMatches();
        mFindReplaceHandler.FindAllMatches(this, findWord);
    }
    Colorize();
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
    Colorize();
}

void TextEditor::JumpToLine(int line) {
    auto newPos = mState.mCursorPosition;
    if (line != -1) {
        newPos = Coordinates(line , 0);
    }
    JumpToCoords(newPos);
}

void TextEditor::JumpToCoords(const Coordinates &aNewPos) {
    SetSelection(aNewPos, aNewPos);
    SetCursorPosition(aNewPos);
    EnsureCursorVisible();

    SetFocusAtCoords(aNewPos);
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
        } else if (aWordMode) {
            mState.mCursorPosition = FindPreviousWord(mState.mCursorPosition);
            cindex                 = GetCharacterIndex(mState.mCursorPosition);
        } else {
            --cindex;
            if (cindex > 0) {
                if ((int)mLines.size() > lindex) {
                    while (cindex > 0 && IsUTFSequence(line[cindex]))
                        --cindex;
                }
            }
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

    SetSelection(mInteractiveStart, mInteractiveEnd);

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
        } else if (aWordMode) {
            mState.mCursorPosition = FindNextWord(mState.mCursorPosition);
            cindex = GetCharacterIndex(mState.mCursorPosition);
        } else {
            ++cindex;
            if (cindex < (int)line.size()) {
                if ((int)mLines.size() > lindex) {
                    while (cindex < (int)line.size() && IsUTFSequence(line[cindex]))
                        ++cindex;
                }
            }
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

    SetSelection(mInteractiveStart, mInteractiveEnd);

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

    auto &line = mLines[oldPos.mLine];
    auto prefix = line.substr(0, oldPos.mColumn);
    auto postfix = line.substr(oldPos.mColumn);
    if (prefix.empty() && postfix.empty())
        return;
    auto home=0;
    if (!prefix.empty()) {
        auto idx = prefix.find_first_not_of(" ");
        if (idx == std::string::npos) {
            auto postIdx = postfix.find_first_of(" ");
            if (postIdx == std::string::npos || postIdx == 0)
                home=0;
            else {
                postIdx = postfix.find_first_not_of(" ");
                if (postIdx == std::string::npos)
                    home =  GetLineMaxColumn(oldPos.mLine);
                else if (postIdx == 0)
                    home = 0;
                else
                    home = oldPos.mColumn + postIdx;
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
                home = GetLineMaxColumn(oldPos.mLine);
            else
                home = oldPos.mColumn + postIdx;
        }
    }



    //while (home < line.size() && isspace(line[home]))
    //    home++;
    //if (home == oldPos.mColumn)
    //    home = 0;
    SetCursorPosition(Coordinates(mState.mCursorPosition.mLine, home));
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
            line.mColorized = false;
            RemoveLine(pos.mLine + 1);

        } else {
            auto cindex     = GetCharacterIndex(pos);
            u.mRemovedStart = u.mRemovedEnd = GetActualCursorCoordinates();
            u.mRemovedEnd.mColumn++;
            u.mRemoved = GetText(u.mRemovedStart, u.mRemovedEnd);

            auto d = UTF8CharLength(line[cindex]);
            line.erase(line.begin() + cindex, d);
            line.mColorized = false;
        }

        mTextChanged = true;

        Colorize();
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
    if (isEmpty() || mReadOnly)
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
        auto &line     = mLines[pos.mLine];

        if (pos.mColumn == 0) {
            if (pos.mLine == 0)
                return;

            u.mRemoved      = '\n';
            u.mRemovedStart = u.mRemovedEnd = Coordinates(pos.mLine - 1, GetLineMaxColumn(pos.mLine - 1));
            Advance(u.mRemovedEnd);

            auto &prevLine = mLines[pos.mLine - 1];
            auto prevSize  = GetLineMaxColumn(pos.mLine - 1);
            if (prevSize == 0)
                prevLine = line;
            else
                prevLine.insert(prevLine.end(), line.begin(), line.end());
            prevLine.mColorized = false;


            ErrorMarkers etmp;
            for (auto &i : mErrorMarkers)
                etmp.insert(ErrorMarkers::value_type(i.first.mLine - 1 == mState.mCursorPosition.mLine ? Coordinates(i.first.mLine - 1,i.first.mColumn) : i.first, i.second));
            mErrorMarkers = std::move(etmp);
            RemoveLine(mState.mCursorPosition.mLine);
            --mState.mCursorPosition.mLine;
            mState.mCursorPosition.mColumn = prevSize;
        } else {
            auto cindex = GetCharacterIndex(pos) - 1;
            auto cend   = cindex + 1;
            while (cindex > 0 && IsUTFSequence(line[cindex]))
                --cindex;

            u.mRemovedStart = u.mRemovedEnd = GetActualCursorCoordinates();
            --u.mRemovedStart.mColumn;
            mState.mCursorPosition.mColumn = GetCharacterColumn(mState.mCursorPosition.mLine, cindex);
            u.mRemoved = GetText(u.mRemovedStart, u.mRemovedEnd);
            if (cend > cindex && cend < (int) line.size())
                line.erase(line.begin() + cindex, cend-cindex);
            else
                line.erase(line.begin() + cindex, -1);
            line.mColorized = false;
        }

        mTextChanged = true;

        EnsureCursorVisible();
        Colorize();
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
    auto wordStart = FindWordStart(GetCursorPosition());
    SetSelection(wordStart,FindWordEnd(wordStart));
}

void TextEditor::SelectAll() {
    if (isEmpty())
        return;

    SetSelection(Coordinates(0, 0), Coordinates((int)mLines.size(), mLines.empty() ? 0 : GetLineMaxColumn((int)mLines.size() - 1)));
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
            const auto &line = mLines[GetActualCursorCoordinates().mLine];
            std::copy(line.mChars.begin(), line.mChars.end(), std::back_inserter(str));
            ImGui::SetClipboardText(str.c_str());
        }
    }
}

void TextEditor::Cut() {
    if (IsReadOnly()) {
        Copy();
    } else {
        if (!HasSelection()) {
            auto lineIndex = GetActualCursorCoordinates().mLine;
            if (lineIndex < 0 || lineIndex >= (int)mLines.size())
                return;
            SetSelection(Coordinates(lineIndex, 0), Coordinates(lineIndex+1, 0));
        }
        UndoRecord u;
        u.mBefore = mState;
        u.mRemoved = GetSelectedText();
        u.mRemovedStart = mState.mSelectionStart;
        u.mRemovedEnd = mState.mSelectionEnd;

        Copy();
        DeleteSelection();

        u.mAfter = mState;
        AddUndo(u);
    }
    std::string findWord = mFindReplaceHandler.GetFindWord();
    if (!findWord.empty()) {
        mFindReplaceHandler.resetMatches();
        mFindReplaceHandler.FindAllMatches(this, findWord);
    }
}

std::string TextEditor::ReplaceStrings(std::string string, const std::string &search, const std::string &replace) {
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
        std::size_t pos = 0;
        while ((pos = result.find(search, pos)) != std::string::npos) {
            result.replace(pos, search.size(), replace);
            pos += replace.size();
        }
    }

    return result;
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
            pos += spaces - 1;
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
    result = ReplaceStrings(result, "\000", ".");
    result = ReplaceTabsWithSpaces(result, 4);

    return result;
}

void TextEditor::Paste() {
    if (IsReadOnly())
        return;

    auto clipText = ImGui::GetClipboardText();
    if (clipText != nullptr ) {
        auto len = strlen(clipText);
        if (len > 0 ) {
            std::string text = PreprocessText(clipText);
            UndoRecord u;
            u.mBefore = mState;

            if (HasSelection()) {
                u.mRemoved = GetSelectedText();
                u.mRemovedStart = mState.mSelectionStart;
                u.mRemovedEnd = mState.mSelectionEnd;
                DeleteSelection();
            }

            u.mAdded = text;
            u.mAddedStart = GetActualCursorCoordinates();
            InsertText(text);

            u.mAddedEnd = GetActualCursorCoordinates();
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
    editor->EnsureCursorVisible();
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
    curPos.mColumn = mMatches.empty() ? editor->mState.mCursorPosition.mColumn : editor->Utf8CharsToBytes(mMatches.back().mCursorPosition);

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
    return GetText(Coordinates(), Coordinates(-1, -1));
}

std::vector<std::string> TextEditor::GetTextLines() const {
    std::vector<std::string> result;

    result.reserve(mLines.size());

    for (auto &line : mLines) {
        std::string text = line.mChars;
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

void TextEditor::Colorize() {
    mUpdateFlags = true;
}

void TextEditor::ColorizeRange() {

    if (isEmpty())
        return;

    std::smatch results;
    std::string id;
    if (mLanguageDefinition.mTokenize == nullptr)
        mLanguageDefinition.mTokenize = []( strConstIter, strConstIter, strConstIter &, strConstIter &, PaletteIndex &) { return false; };
    auto linesSize = mLines.size();
    for (int i = 0; i < linesSize; ++i) {
        auto &line = mLines[i];
        auto size = line.size();

        if (line.mColors.size() != size) {
            line.mColors.resize(size, 0);
            line.mColorized = false;
        }

        if (line.mColorized || line.empty())
            continue;

        auto last = line.end();

        auto first = line.begin();
        for (auto current = first;(current-first) < size;) {
            strConstIter token_begin;
            strConstIter token_end;
            PaletteIndex token_color = PaletteIndex::Default;

            bool hasTokenizeResult = mLanguageDefinition.mTokenize(current.mCharsIter, last.mCharsIter, token_begin, token_end, token_color);
            auto token_offset = token_begin - first.mCharsIter;

            if (!hasTokenizeResult) {
                // todo : remove
                // printf("using regex for %.*s\n", first + 10 < last ? 10 : int(last - first), first);

                for (auto &p : mRegexList) {
                    if (std::regex_search(first.mCharsIter, last.mCharsIter, results, p.first, std::regex_constants::match_continuous)) {
                        hasTokenizeResult = true;

                        const auto &v     = results.begin();
                        token_begin = v->first;
                        token_end   = v->second;
                        token_color = p.second;
                        break;
                    }
                }
            }

            if (!hasTokenizeResult)
                current=current + 1;
            else {
                current = first + token_offset;
                size_t token_length=0;
                Line::Flags flags(0);
                flags.mValue = line.mFlags[token_offset];
                if (flags.mValue == 0) {
                    token_length = token_end - token_begin;
                    if (token_color == PaletteIndex::Identifier) {
                        id.assign(token_begin, token_end);

                        // todo : almost all language definitions use lower case to specify keywords, so shouldn't this use ::tolower ?
                        if (!mLanguageDefinition.mCaseSensitive)
                            std::transform(id.begin(), id.end(), id.begin(), ::toupper);
                        else if (mLanguageDefinition.mKeywords.count(id) != 0)
                            token_color = PaletteIndex::Keyword;
                        else if (mLanguageDefinition.mIdentifiers.count(id) != 0)
                            token_color = PaletteIndex::BuiltInType;
                        else if (id == "$")
                            token_color = PaletteIndex::GlobalVariable;
                    }
                } else {
                    if ((token_color == PaletteIndex::Identifier || flags.mBits.mPreprocessor) && !flags.mBits.mDeactivated && !(flags.mValue & Line::InComment)) {
                        id.assign(token_begin, token_end);
                        if (mLanguageDefinition.mPreprocIdentifiers.count(id) != 0) {
                            token_color = PaletteIndex::Directive;
                            token_begin -= 1;
                            token_length = token_end - token_begin;
                            token_offset -= 1;
                        } else if (flags.mBits.mPreprocessor) {
                            token_color = PaletteIndex::PreprocIdentifier;
                            token_length = token_end - token_begin;
                        }
                    }
                    if ((token_color != PaletteIndex::Directive && token_color != PaletteIndex::PreprocIdentifier) || flags.mBits.mDeactivated) {
                        if (flags.mBits.mDeactivated && flags.mBits.mPreprocessor) {
                            token_color = PaletteIndex::PreprocessorDeactivated;
                            token_begin -= 1;
                            token_offset -= 1;
                        } else if (id.assign(token_begin, token_end);flags.mValue & Line::InComment && mLanguageDefinition.mPreprocIdentifiers.count(id) != 0) {
                            token_color = GetColorIndexFromFlags(flags);
                            token_begin -= 1;
                            token_offset -= 1;
                        }

                        auto flag = line.mFlags[token_offset];
                       token_length = line.mFlags.find_first_not_of(flag, token_offset + 1);
                       if (token_length == std::string::npos)
                           token_length = line.size() - token_offset;
                        else
                            token_length -= token_offset;

                        token_end = token_begin + token_length;
                        if (!flags.mBits.mPreprocessor || flags.mBits.mDeactivated)
                            token_color = GetColorIndexFromFlags(flags);
                    }
                }

                if (token_color != PaletteIndex::Identifier || *current.mColorsIter == static_cast<char>(PaletteIndex::Identifier)) {
                    if (token_offset + token_length >= (int)line.mColors.size()) {
                       auto colors = line.mColors;
                       line.mColors.resize(token_offset + token_length, static_cast<char>(PaletteIndex::Default));
                       std::copy(colors.begin(), colors.end(), line.mColors.begin());
                    }
                    try {
                        line.mColors.replace(token_offset, token_length, token_length, static_cast<char>(token_color));
                    } catch (const std::exception &e) {
                        std::cerr << "Error replacing color: " << e.what() << std::endl;
                        return;
                    }
                }
                current = current + token_length;
            }
        }
        line.mColorized = true;
    }
}

void TextEditor::ColorizeInternal() {
    if (isEmpty() || !mColorizerEnabled)
        return;

    if (mUpdateFlags) {
        auto endLine = mLines.size();
        auto commentStartLine = endLine;
        auto commentStartIndex = 0;
        auto withinGlobalDocComment = false;
        auto withinBlockDocComment = false;
        auto withinString = false;
        auto withinBlockComment = false;
        auto withinNotDef = false;
        auto currentLine = 0;
        auto commentLength = 0;

        std::vector<bool> ifDefs;
        ifDefs.push_back(true);
        mDefines.push_back("__IMHEX__");
        for (currentLine = 0; currentLine < endLine; currentLine++) {
            auto &line = mLines[currentLine];
            auto lineLength = line.size();

            if (line.mFlags.size() != lineLength) {
                line.mFlags.resize(lineLength, 0);
                line.mColorized = false;
            }
            //if (!line.mColorized) {

                auto withinComment = false;
                auto withinDocComment = false;
                auto withinPreproc = false;
                auto firstChar = true;   // there is no other non-whitespace characters in the line before

                auto setGlyphFlags = [&](int index) {
                    Line::Flags flags(0);
                    flags.mBits.mComment = withinComment;
                    flags.mBits.mBlockComment = withinBlockComment;
                    flags.mBits.mDocComment = withinDocComment;
                    flags.mBits.mGlobalDocComment = withinGlobalDocComment;
                    flags.mBits.mBlockDocComment = withinBlockDocComment;
                    flags.mBits.mDeactivated = withinNotDef;
                    if (mLines[currentLine].mFlags[index] != flags.mValue) {
                        mLines[currentLine].mColorized = false;
                        mLines[currentLine].mFlags[index] = flags.mValue;
                    }
                };

                auto currentIndex = 0;
                if (line.empty())
                    continue;
                while (currentIndex < lineLength) {

                    auto &g = line[currentIndex];
                    auto c = g;

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
                        if (firstChar && c == mLanguageDefinition.mPreprocChar && !inComment && !withinComment && !withinDocComment && !withinString) {
                            withinPreproc = true;
                            std::string directive;
                            auto start = currentIndex + 1;
                            while (start < (int) line.size() && !isspace(line[start])) {
                                directive += line[start];
                                start++;
                            }

                            while (start < (int) line.size() && isspace(line[start]))
                                start++;

                            if (directive == "endif" && !ifDefs.empty()) {
                                ifDefs.pop_back();
                                withinNotDef = !ifDefs.back();
                            } else {
                                std::string identifier;
                                while (start < (int) line.size() && !isspace(line[start])) {
                                    identifier += line[start];
                                    start++;
                                }
                                if (directive == "define") {
                                    if (identifier.size() > 0 && !withinNotDef && std::find(mDefines.begin(), mDefines.end(), identifier) == mDefines.end())
                                        mDefines.push_back(identifier);
                                } else if (directive == "undef") {
                                    if (identifier.size() > 0 && !withinNotDef)
                                        mDefines.erase(std::remove(mDefines.begin(), mDefines.end(), identifier), mDefines.end());
                                } else if (directive == "ifdef") {
                                    if (!withinNotDef) {
                                        bool isConditionMet = std::find(mDefines.begin(), mDefines.end(), identifier) != mDefines.end();
                                        ifDefs.push_back(isConditionMet);
                                    } else
                                        ifDefs.push_back(false);
                                } else if (directive == "ifndef") {
                                    if (!withinNotDef) {
                                        bool isConditionMet = std::find(mDefines.begin(), mDefines.end(), identifier) == mDefines.end();
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
                                return !a.empty() && currentIndex + 1 >= (int) a.size() && equals(a.begin(), a.end(), b.begin() + (currentIndex + 1 - a.size()), b.begin() + (currentIndex + 1), pred);
                            };

                            if (!inComment && !withinComment && !withinDocComment && !withinPreproc && !withinString) {
                                if (compareForth(mLanguageDefinition.mDocComment, line.mChars)) {
                                    withinDocComment = !inComment;
                                    commentLength = 3;
                                } else if (compareForth(mLanguageDefinition.mSingleLineComment, line.mChars)) {
                                    withinComment = !inComment;
                                    commentLength = 2;
                                } else {
                                    bool isGlobalDocComment = compareForth(mLanguageDefinition.mGlobalDocComment, line.mChars);
                                    bool isBlockDocComment = compareForth(mLanguageDefinition.mBlockDocComment, line.mChars);
                                    bool isBlockComment = compareForth(mLanguageDefinition.mCommentStart, line.mChars);
                                    if (isGlobalDocComment || isBlockDocComment || isBlockComment) {
                                        commentStartLine = currentLine;
                                        commentStartIndex = currentIndex;
                                        if (currentIndex < line.size() - 4 && isBlockComment &&
                                            line.mChars[currentIndex + 2] == '*' &&
                                            line.mChars[currentIndex + 3] == '/') {
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

                            if (compareBack(mLanguageDefinition.mCommentEnd, line.mChars) && ((commentStartLine != currentLine) || (commentStartIndex + commentLength < currentIndex))) {
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
                        flags.mValue = mLines[currentLine].mFlags[currentIndex];
                        flags.mBits.mPreprocessor = withinPreproc;
                        mLines[currentLine].mFlags[currentIndex] = flags.mValue;
                    }
                    auto utf8CharLength = UTF8CharLength(c);
                    if (utf8CharLength > 1) {
                        Line::Flags flags(0);
                        flags.mValue = mLines[currentLine].mFlags[currentIndex];
                        for (int j = 1; j < utf8CharLength; j++) {
                            currentIndex++;
                            mLines[currentLine].mFlags[currentIndex] = flags.mValue;
                        }
                    }
                    currentIndex++;
                }
                withinNotDef = !ifDefs.back();
           // }
            // mUpdateFlags = false;
        }
        mDefines.clear();
    }
    ColorizeRange();
}

float TextEditor::TextDistanceToLineStart(const Coordinates &aFrom) const {
    auto &line      = mLines[aFrom.mLine];
    float distance  = 0.0f;
    float spaceSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ", nullptr, nullptr).x;
    int colIndex    = GetCharacterIndex(aFrom);
    for (size_t it = 0u; it < line.size() && it < colIndex;) {
        if (line[it] == '\t') {
            distance = (1.0f + std::floor((1.0f + distance) / (float(mTabSize) * spaceSize))) * (float(mTabSize) * spaceSize);
            ++it;
        } else {
            auto d = UTF8CharLength(line[it]);
            char tempCString[7];
            int i = 0;
            for (; i < 6 && d-- > 0 && it < (int)line.size(); i++, it++)
                tempCString[i] = line[it];

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
        aEditor->Colorize();
    }

    if (!mRemoved.empty()) {
        auto start = mRemovedStart;
        aEditor->InsertTextAt(start, mRemoved.c_str());
        aEditor->Colorize();
    }

    aEditor->mState = mBefore;
    aEditor->EnsureCursorVisible();
}

void TextEditor::UndoRecord::Redo(TextEditor *aEditor) {
    if (!mRemoved.empty()) {
        aEditor->DeleteRange(mRemovedStart, mRemovedEnd);
       aEditor->Colorize();
    }

    if (!mAdded.empty()) {
        auto start = mAddedStart;
        aEditor->InsertTextAt(start, mAdded.c_str());
       aEditor->Colorize();
    }

    aEditor->mState = mAfter;
    aEditor->EnsureCursorVisible();

}

bool TokenizeCStyleString(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end) {
    auto p = in_begin;

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

bool TokenizeCStyleCharacterLiteral(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end) {
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
            out_end   = p + 1;
            return true;
        }
    }

    return false;
}

bool TokenizeCStyleIdentifier(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end) {
    auto p = in_begin;

    if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_' || *p == '$') {
        p++;

        while ((p < in_end) && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_'))
            p++;

        out_begin = in_begin;
        out_end   = p;
        return true;
    }

    return false;
}

bool TokenizeCStyleNumber(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end) {
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

        // single and double precision floating point type
        if (p < in_end && (*p == 'f' || *p == 'F' || *p == 'd' || *p == 'D'))
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

bool TokenizeCStyleOperator(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end) {
    (void)in_end;

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
            out_end   = in_begin + 1;
            return true;
    }

    return false;
}

bool TokenizeCStyleSeparator(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end) {
    (void)in_end;

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

        langDef.mTokenize = [](strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end, PaletteIndex &paletteIndex) -> bool {
            paletteIndex = PaletteIndex::Max;

            while (in_begin < in_end && isascii(*in_begin) && isblank(*in_begin))
                in_begin++;

            if (in_begin == in_end) {
                out_begin    = in_end;
                out_end      = in_end;
                paletteIndex = PaletteIndex::Default;
            } else if (TokenizeCStyleString(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::StringLiteral;
            else if (TokenizeCStyleCharacterLiteral(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::CharLiteral;
            else if (TokenizeCStyleIdentifier(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::Identifier;
            else if (TokenizeCStyleNumber(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::NumericLiteral;
            else if (TokenizeCStyleSeparator(in_begin, in_end, out_begin, out_end))
                paletteIndex= PaletteIndex::Separator;
            else if (TokenizeCStyleOperator(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::Operator;

            return paletteIndex != PaletteIndex::Max;
        };

        langDef.mCommentStart      = "/*";
        langDef.mCommentEnd        = "*/";
        langDef.mSingleLineComment           = "//";

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

        langDef.mTokenRegexStrings.push_back(std::make_pair("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Directive));
        langDef.mTokenRegexStrings.push_back(std::make_pair("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::StringLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::NumericLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
        langDef.mTokenRegexStrings.push_back(std::make_pair("[\\!\\%\\^\\&\\*\\-\\+\\=\\~\\|\\<\\>\\?\\/]", PaletteIndex::Operator));
        langDef.mTokenRegexStrings.push_back(std::make_pair("[\\[\\]\\{\\}\\(\\)\\;\\,\\.]", PaletteIndex::Separator));
        langDef.mCommentStart      = "/*";
        langDef.mCommentEnd        = "*/";
        langDef.mSingleLineComment           = "//";

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

        langDef.mTokenRegexStrings.push_back(std::make_pair("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Directive));
        langDef.mTokenRegexStrings.push_back(std::make_pair("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::StringLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::NumericLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
        langDef.mTokenRegexStrings.push_back(std::make_pair("[\\!\\%\\^\\&\\*\\-\\+\\=\\~\\|\\<\\>\\?\\/]", PaletteIndex::Operator));
        langDef.mTokenRegexStrings.push_back(std::make_pair("[\\[\\]\\{\\}\\(\\)\\;\\,\\.]", PaletteIndex::Separator));
        langDef.mCommentStart      = "/*";
        langDef.mCommentEnd        = "*/";
        langDef.mSingleLineComment           = "//";

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

        langDef.mTokenize = [](strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end, PaletteIndex &paletteIndex) -> bool {
            paletteIndex = PaletteIndex::Max;

            while (in_begin < in_end && isascii(*in_begin) && isblank(*in_begin))
                in_begin++;

            if (in_begin == in_end) {
                out_begin    = in_end;
                out_end      = in_end;
                paletteIndex = PaletteIndex::Default;
            } else if (TokenizeCStyleString(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::StringLiteral;
            else if (TokenizeCStyleCharacterLiteral(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::CharLiteral;
            else if (TokenizeCStyleIdentifier(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::Identifier;
            else if (TokenizeCStyleNumber(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::NumericLiteral;
            else if (TokenizeCStyleOperator(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::Operator;
            else if (TokenizeCStyleSeparator(in_begin, in_end, out_begin, out_end))
                paletteIndex = PaletteIndex::Separator;

            return paletteIndex != PaletteIndex::Max;
        };

        langDef.mCommentStart      = "/*";
        langDef.mCommentEnd        = "*/";
        langDef.mSingleLineComment           = "//";

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

        langDef.mTokenRegexStrings.push_back(std::make_pair("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Directive));
        langDef.mTokenRegexStrings.push_back(std::make_pair("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::StringLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::NumericLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
        langDef.mTokenRegexStrings.push_back(std::make_pair("[\\!\\%\\^\\&\\*\\-\\+\\=\\~\\|\\<\\>\\?\\/]", PaletteIndex::Operator));
        langDef.mTokenRegexStrings.push_back(std::make_pair("[\\[\\]\\{\\}\\(\\)\\;\\,\\.]", PaletteIndex::Separator));

        langDef.mCommentStart      = "/*";
        langDef.mCommentEnd        = "*/";
        langDef.mSingleLineComment           = "//";

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

        langDef.mTokenRegexStrings.push_back(std::make_pair("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Directive));
        langDef.mTokenRegexStrings.push_back(std::make_pair("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::StringLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::NumericLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
        langDef.mTokenRegexStrings.push_back(std::make_pair("[\\!\\%\\^\\&\\*\\-\\+\\=\\~\\|\\<\\>\\?\\/]", PaletteIndex::Operator));
        langDef.mTokenRegexStrings.push_back(std::make_pair("[\\[\\]\\{\\}\\(\\)\\;\\,\\.]", PaletteIndex::Separator));

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

        langDef.mTokenRegexStrings.push_back(std::make_pair("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Directive));
        langDef.mTokenRegexStrings.push_back(std::make_pair("L?\\\"(\\\\.|[^\\\"])*\\\"", PaletteIndex::StringLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("\\'\\\\?[^\\']\\'", PaletteIndex::CharLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::NumericLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::NumericLiteral));
        langDef.mTokenRegexStrings.push_back(std::make_pair("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier));
        langDef.mTokenRegexStrings.push_back(std::make_pair("[\\!\\%\\^\\&\\*\\-\\+\\=\\~\\|\\<\\>\\?\\/]", PaletteIndex::Operator));
        langDef.mTokenRegexStrings.push_back(std::make_pair("[\\[\\]\\{\\}\\(\\)\\;\\,\\.]", PaletteIndex::Separator));

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
