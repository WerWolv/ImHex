#pragma once

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <functional>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <regex>
#include <chrono>
#include "imgui.h"
#include "imgui_internal.h"
using strConstIter = std::string::const_iterator;
class TextEditor
{
public:
	enum class PaletteIndex
	{
		Default,
		Identifier,
		Directive,
		Operator,
		Separator,
		BuiltInType,
		Keyword,
		NumericLiteral,
		StringLiteral,
		CharLiteral,
		Cursor,
		Background,
		LineNumber,
		Selection,
		Breakpoint,
		ErrorMarker,
		PreprocessorDeactivated,
		CurrentLineFill,
		CurrentLineFillInactive,
		CurrentLineEdge,
		ErrorText,
		WarningText,
		DebugText,
		DefaultText,
		Attribute,
		PatternVariable,
		LocalVariable,
		CalculatedPointer,
		TemplateArgument,
		Function,
		View,
		FunctionVariable,
		FunctionParameter,
		UserDefinedType,
		PlacedVariable,
		GlobalVariable,
		NameSpace,
		TypeDef,
		UnkIdentifier,
		DocComment,
		DocBlockComment,
		BlockComment,
		GlobalDocComment,
		Comment,
		PreprocIdentifier,
		Max
    };

	// Represents a character coordinate from the user's point of view,
	// i. e. consider an uniform grid (assuming fixed-width font) on the
	// screen as it is rendered, and each cell has its own coordinate, starting from 0.
	// Tabs are counted as [1..mTabSize] count empty spaces, depending on
	// how many space is necessary to reach the next tab stop.
	// For example, coordinate (1, 5) represents the character 'B' in a line "\tABC", when mTabSize = 4,
	// because it is rendered as "    ABC" on the screen.
	struct Coordinates
	{
		int mLine, mColumn;
		Coordinates() : mLine(0), mColumn(0) {}
		Coordinates(int aLine, int aColumn) : mLine(aLine), mColumn(aColumn)
		{
			IM_ASSERT(aLine >= 0);
			IM_ASSERT(aColumn >= 0);
		}
		static Coordinates Invalid() { static Coordinates invalid(-1, -1); return invalid; }

		bool operator ==(const Coordinates& o) const
		{
			return
				mLine == o.mLine &&
				mColumn == o.mColumn;
		}

		bool operator !=(const Coordinates& o) const
		{
			return
				mLine != o.mLine ||
				mColumn != o.mColumn;
		}

		bool operator <(const Coordinates& o) const
		{
			if (mLine != o.mLine)
				return mLine < o.mLine;
			return mColumn < o.mColumn;
		}

		bool operator >(const Coordinates& o) const
		{
			if (mLine != o.mLine)
				return mLine > o.mLine;
			return mColumn > o.mColumn;
		}

		bool operator <=(const Coordinates& o) const
		{
			if (mLine != o.mLine)
				return mLine < o.mLine;
			return mColumn <= o.mColumn;
		}

		bool operator >=(const Coordinates& o) const
		{
			if (mLine != o.mLine)
				return mLine > o.mLine;
			return mColumn >= o.mColumn;
		}
	};

	struct Identifier
	{
		Coordinates mLocation;
		std::string mDeclaration;
	};

    using String = std::string;
	using Identifiers = std::unordered_map<std::string, Identifier>;
	using Keywords = std::unordered_set<std::string> ;
    using ErrorMarkers = std::map<Coordinates, std::pair<uint32_t ,std::string>>;
    using Breakpoints = std::unordered_set<uint32_t>;
    using Palette = std::array<ImU32, (uint64_t )PaletteIndex::Max>;
    using Glyph = uint8_t ;

    class ActionableBox {

        ImRect mBox;
    public:
        ActionableBox()=default;
        explicit ActionableBox(const ImRect &box) : mBox(box) {}
        virtual bool trigger() {
            return ImGui::IsMouseHoveringRect(mBox.Min,mBox.Max);
        }

        virtual void callback() {}
    };

    class CursorChangeBox : public ActionableBox {
    public:
        CursorChangeBox()=default;
        explicit CursorChangeBox(const ImRect &box) : ActionableBox(box) {

        }

        void callback() override {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
    };

    class ErrorGotoBox : public ActionableBox {
        Coordinates mPos;
    public:
        ErrorGotoBox()=default;
        ErrorGotoBox(const ImRect &box, const Coordinates &pos, TextEditor *editor) : ActionableBox(box), mPos(pos), mEditor(editor) {

        }

        bool trigger() override {
            return ActionableBox::trigger() && ImGui::IsMouseClicked(0);
        }

        void callback() override {
            mEditor->JumpToCoords(mPos);
        }

    private:
        TextEditor *mEditor;
    };

    using ErrorGotoBoxes = std::map<Coordinates, ErrorGotoBox>;
    using CursorBoxes = std::map<Coordinates, CursorChangeBox>;

    class ErrorHoverBox : public ActionableBox {
        Coordinates mPos;
        std::string mErrorText;
    public:
        ErrorHoverBox()=default;
        ErrorHoverBox(const ImRect &box, const Coordinates &pos,const char *errorText) : ActionableBox(box), mPos(pos), mErrorText(errorText) {

        }

        void callback() override {
            ImGui::BeginTooltip();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
            ImGui::Text("Error at line %d:", mPos.mLine);
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.2f, 1.0f));
            ImGui::TextUnformatted(mErrorText.c_str());
            ImGui::PopStyleColor();
            ImGui::EndTooltip();
        }
    };
    using ErrorHoverBoxes = std::map<Coordinates, ErrorHoverBox>;

    class Line {
    public:
        struct FlagBits {
		bool mComment : 1;
		bool mBlockComment : 1;
		bool mPreprocessor : 1;
		bool mDocComment : 1;
        bool mGlobalDocComment : 1;
		bool mDeactivated : 1;
        bool mBlockDocComment : 1;
        };
        union Flags {
            Flags(char value) : mValue(value) {}
            Flags(FlagBits bits) : mBits(bits) {}
            FlagBits mBits;
            char mValue;
        };

        class LineIterator {
        public:
            strConstIter mLineIter;
            strConstIter mColorizedIter;
            strConstIter mFlagsIter;

            LineIterator() = default;

            char operator*() {
                return *mLineIter;
            }

            LineIterator operator++() {
                LineIterator iter = *this;
                ++iter.mLineIter;
                ++iter.mColorizedIter;
                ++iter.mFlagsIter;
                return iter;
            }

            bool operator!=(const LineIterator &other) const {
                return mLineIter != other.mLineIter || mColorizedIter != other.mColorizedIter || mFlagsIter != other.mFlagsIter;
            }

            bool operator==(const LineIterator &other) const {
                return mLineIter == other.mLineIter && mColorizedIter == other.mColorizedIter && mFlagsIter == other.mFlagsIter;
            }

            LineIterator operator+(int n) {
                LineIterator iter = *this;
                iter.mLineIter += n;
                iter.mColorizedIter += n;
                iter.mFlagsIter += n;
                return iter;
            }

            int operator-(LineIterator l) {
                return mLineIter - l.mLineIter;
            }
        };

        LineIterator begin() const {
            LineIterator iter;
            iter.mLineIter = mLine.begin();
            iter.mColorizedIter = mColorizedLine.begin();
            iter.mFlagsIter = mFlags.begin();
            return iter;
        }

        LineIterator end() const {
            LineIterator iter;
            iter.mLineIter = mLine.end();
            iter.mColorizedIter = mColorizedLine.end();
            iter.mFlagsIter = mFlags.end();
            return iter;
        }

        std::string mLine;
        std::string mColorizedLine;
        std::string mFlags;
        bool mColorized = false;
        Line() : mLine(), mColorizedLine(), mFlags(), mColorized(false) {}

        explicit Line(const char *line) {
            Line(std::string(line));
        }

        explicit Line(const std::string &line) : mLine(line), mColorizedLine(std::string(line.size(), 0x00)), mFlags(std::string(line.size(), 0x00)), mColorized(false) {}
        Line(const Line &line) : mLine(line.mLine), mColorizedLine(line.mColorizedLine), mFlags(line.mFlags),  mColorized(line.mColorized) {}

        Line &operator=(const Line &line) {
            mLine = line.mLine;
            mColorizedLine = line.mColorizedLine;
            mFlags = line.mFlags;
            mColorized = line.mColorized;
            return *this;
        }

        Line &operator=(Line &&line) noexcept {
            mLine = std::move(line.mLine);
            mColorizedLine = std::move(line.mColorizedLine);
            mFlags = std::move(line.mFlags);
            mColorized = line.mColorized;
            return *this;
        }

        size_t size() const {
            return mLine.size();
        }
        enum class LinePart {
            Line,
            Colorized,
            Flags
        };

        char front(LinePart part = LinePart::Line) const {
            if (part == LinePart::Line && !mLine.empty())
                return mLine.front();
            if (part == LinePart::Colorized && !mColorizedLine.empty())
                return mColorizedLine.front();
            if (part == LinePart::Flags && !mFlags.empty())
                return mFlags.front();
            return 0x00;
        }

        void push_back(char c) {
            mLine.push_back(c);
            mColorizedLine.push_back(0x00);
            mFlags.push_back(0x00);
            mColorized = false;
        }

        bool empty() const {
            return mLine.empty();
        }


        std::string substr(size_t start, size_t length = (size_t)-1, LinePart part = LinePart::Line ) const {
            if (length == (size_t)-1)
                length = mLine.size() - start;
            if (start >= mLine.size())
                return "";
            if (start + length >= mLine.size())
                length = mLine.size() - start;
            if (part == LinePart::Line && length > 0)
                return mLine.substr(start, length);
            if (part == LinePart::Colorized && length > 0)
                return mColorizedLine.substr(start, length);
            if (part == LinePart::Flags && length > 0)
                return mFlags.substr(start, length);
            return "";
        }

        template<LinePart part=LinePart::Line>
        char operator[](size_t index) const {
            if (part == LinePart::Line)
                return mLine[index];
            if (part == LinePart::Colorized)
                return mColorizedLine[index];
            return mFlags[index];
        }

        template<LinePart part=LinePart::Line>
        const char &operator[](size_t index) {
            if (part == LinePart::Line)
                return mLine[index];
            if (part == LinePart::Colorized)
                return mColorizedLine[index];
            return mFlags[index];
        }

        void SetNeedsUpdate(bool needsUpdate) {
            mColorized = !needsUpdate;
        }

        void append(const char *text) {
            append(std::string(text));
        }

        void append(const char text) {
            append(std::string(1, text));
        }

        void append(const std::string &text) {
            mLine.append(text);
            mColorized = false;
        }

        void append(const Line &line) {
            append(line.begin(), line.end());
        }

        void append(LineIterator begin, LineIterator end) {
            if (begin.mLineIter < end.mLineIter)
                mLine.append(begin.mLineIter, end.mLineIter);
            if (begin.mColorizedIter < end.mColorizedIter)
                mColorizedLine.append(begin.mColorizedIter, end.mColorizedIter);
            if (begin.mFlagsIter < end.mFlagsIter)
                mFlags.append(begin.mFlagsIter, end.mFlagsIter);
            mColorized = false;
        }

        void insert(LineIterator iter, const std::string &text) {
            if (iter == end())
                append(text);
            else
                insert(iter, text.begin(), text.end());
        }

        void insert(LineIterator iter, const char text) {
            if (iter == end())
                append(text);
            else
                insert(iter,std::string(1, text));
        }

        void insert(LineIterator iter, strConstIter beginString, strConstIter endString) {
            if (iter == end())
                append(std::string(beginString, endString));
            else {
                mLine.insert(iter.mLineIter, beginString, endString);
                mColorized = false;
            }
        }

        void insert(LineIterator iter,const Line &line) {
            if (iter == end())
                append(line.begin(), line.end());
            else
                insert(iter, line.begin(), line.end());
        }

        void insert(LineIterator iter,LineIterator beginLine, LineIterator endLine) {
            if (iter == end())
                append(beginLine, endLine);
            else {
                mLine.insert(iter.mLineIter, beginLine.mLineIter, endLine.mLineIter);
                mColorized = false;
            }
        }

        void erase(LineIterator begin) {
            mLine.erase(begin.mLineIter);
            mColorized = false;
        }

        void erase(LineIterator begin, size_t count) {
            if (count == (size_t) -1)
                count = mLine.size() - (begin.mLineIter - mLine.begin());
            mLine.erase(begin.mLineIter, begin.mLineIter + count);
            mColorized = false;
        }

        void SetLine(const std::string &text) {
            mLine = text;
            mColorized = false;
        }

        void SetLine(const Line &text) {
            mLine = text.mLine;
            mColorizedLine = text.mColorizedLine;
            mFlags = text.mFlags;
            mColorized = text.mColorized;
        }


        bool NeedsUpdate() const {
            return !mColorized;
        }

    };

    using Lines = std::vector<Line>;

	struct LanguageDefinition
	{
		typedef std::pair<std::string, PaletteIndex> TokenRegexString;
		typedef std::vector<TokenRegexString> TokenRegexStrings;
		typedef bool(*TokenizeCallback)(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end, PaletteIndex &paletteIndex);

		std::string mName;
		Keywords mKeywords;
		Identifiers mIdentifiers;
		Identifiers mPreprocIdentifiers;
		std::string mSingleLineComment, mCommentEnd, mCommentStart, mGlobalDocComment, mDocComment, mBlockDocComment;
		char mPreprocChar;
		bool mAutoIndentation;

		TokenizeCallback mTokenize;

		TokenRegexStrings mTokenRegexStrings;

		bool mCaseSensitive;

		LanguageDefinition()
			: mPreprocChar('#'), mAutoIndentation(true), mTokenize(nullptr), mCaseSensitive(true)
		{
		}

		static const LanguageDefinition& CPlusPlus();
		static const LanguageDefinition& HLSL();
		static const LanguageDefinition& GLSL();
		static const LanguageDefinition& C();
		static const LanguageDefinition& SQL();
		static const LanguageDefinition& AngelScript();
		static const LanguageDefinition& Lua();
	};
    void ClearErrorMarkers() {
        mErrorMarkers.clear();
        mErrorHoverBoxes.clear();
    }

    void ClearGotoBoxes() {
        mErrorGotoBoxes.clear();
    }

    void ClearCursorBoxes() {
        mCursorBoxes.clear();
    }
    void ClearActionables() {
        ClearErrorMarkers();
        ClearGotoBoxes();
        ClearCursorBoxes();
    }

    struct Selection {
        Coordinates mStart;
        Coordinates mEnd;
    };

	TextEditor();
	~TextEditor();

	void SetLanguageDefinition(const LanguageDefinition& aLanguageDef);
	const LanguageDefinition& GetLanguageDefinition() const { return mLanguageDefinition; }

	static const Palette& GetPalette();
	static void SetPalette(const Palette& aValue);

	void SetErrorMarkers(const ErrorMarkers& aMarkers) { mErrorMarkers = aMarkers; }
    Breakpoints &GetBreakpoints() { return mBreakpoints; }
	void SetBreakpoints(const Breakpoints& aMarkers) { mBreakpoints = aMarkers; }
    ImVec2 Underwaves( ImVec2 pos, uint32_t nChars, ImColor color= ImGui::GetStyleColorVec4(ImGuiCol_Text), const ImVec2 &size_arg= ImVec2(0, 0));

	void Render(const char* aTitle, const ImVec2& aSize = ImVec2(), bool aBorder = false);
	void SetText(const std::string& aText);
    void JumpToLine(int line=-1);
    void JumpToCoords(const Coordinates &coords);
    void SetLongestLineLength(size_t line) {
        mLongestLineLength = line;
    }
	std::string GetText() const;
    bool isEmpty() const {
        if (mLines.empty())
            return true;
        if (mLines.size() == 1) {
            if (mLines[0].empty())
                return true;
            if (mLines[0].size() == 1 && mLines[0].front() == '\n')
                return true;
        }
        return false;
    }

    void SetTopLine();
    uint32_t GetTopLine() const {
        return static_cast<uint32_t>(std::floor(mTopLine));
    }
    uint32_t GetBottomLine() const {
        return static_cast<uint32_t>(std::ceil(mTopLine+mNumberOfLinesDisplayed));
    }
    void SetNeedsUpdate (uint32_t line, bool needsUpdate) {
        if (line < mLines.size())
            mLines[line].SetNeedsUpdate(needsUpdate);
    }

    void SetColorizedLineSize(size_t line) {
        if (line < mLines.size()) {
            const auto &size = mLines[line].mLine.size();
            if (mLines[line].mColorizedLine.size() != size) {
                mLines[line].mColorizedLine.resize(size);
                std::fill(mLines[line].mColorizedLine.begin(), mLines[line].mColorizedLine.end(), 0x00);
            }
        }
    }

    void SetColorizedLine(size_t line, const std::string &tokens) {
        if (line < mLines.size()) {
            auto &lineTokens = mLines[line].mColorizedLine;
            if (lineTokens.size() != tokens.size()) {
                lineTokens.resize(tokens.size());
                std::fill(lineTokens.begin(), lineTokens.end(), 0x00);
            }
            size_t i = 0;
            while (i < tokens.size()) {
                if (tokens[i] < 0) {
                    auto bytes = -tokens[i];
                    int tokenLength=0;
                    for (int j = 0; j < bytes; j++)
                        tokenLength += tokens[i + j + 2] << (j * 8);
                    for (int j = 0; j < tokenLength; j++)
                        lineTokens[i + j] = tokens[i + j];
                    i += tokenLength;
                } else if (tokens[i] > 0) {
                    lineTokens[i] = tokens[i];
                    if (i < lineTokens.size() && tokens[i] == tokens[i+1]) {
                        lineTokens[i + 1] = tokens[i + 1];
                        i+=2;
                    } else
                        i++;
                } else
                    i++;
            }
        }
        SetNeedsUpdate(line, false);
    }

    void SetScrollY();
	void SetTextLines(const std::vector<std::string>& aLines);
	std::vector<std::string> GetTextLines() const;

	std::string GetSelectedText() const;
	std::string GetCurrentLineText()const;

    std::string GetLineText(int line)const;
    void SetSourceCodeEditor(TextEditor *editor) { mSourceCodeEditor = editor; }
    TextEditor *GetSourceCodeEditor() {
        if(mSourceCodeEditor!=nullptr)
            return mSourceCodeEditor;
        return this;
    }

    class FindReplaceHandler;

public:
    void AddClickableText(std::string text) {
        mClickableText.push_back(text);
    }
    void ClearClickableText() {
        mClickableText.clear();
    }
    FindReplaceHandler *GetFindReplaceHandler() { return &mFindReplaceHandler; }
	int GetTotalLines() const { return (int)mLines.size(); }
	bool IsOverwrite() const { return mOverwrite; }
    void SetTopMarginChanged(int newMargin) {
        mNewTopMargin = newMargin;
        mTopMarginChanged = true;
    }
    void setFocusAtCoords(const Coordinates &coords) {
        mFocusAtCoords = coords;
        mUpdateFocus = true;
    }
    void SetOverwrite(bool aValue) { mOverwrite = aValue; }

    std::string ReplaceStrings(std::string string, const std::string &search, const std::string &replace);
    std::vector<std::string> SplitString(const std::string &string, const std::string &delimiter, bool removeEmpty);
    std::string ReplaceTabsWithSpaces(const std::string& string, uint32_t tabSize);
    std::string PreprocessText(const std::string &code);

	void SetReadOnly(bool aValue);
    bool IsEndOfLine(const Coordinates &aCoordinates) const;
    bool IsEndOfFile(const Coordinates &aCoordinates) const;
	bool IsReadOnly() const { return mReadOnly; }
	bool IsTextChanged() const { return mTextChanged; }
    void SetTextChanged(bool aValue) { mTextChanged = aValue; }
    void SetTimeStamp(int code) {
        auto now = std::chrono::high_resolution_clock::now();
        if (code == 0)
            mLinesTimestamp = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
        else
            mColorizedLinesTimestamp = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
    }
	bool IsCursorPositionChanged() const { return mCursorPositionChanged; }
    bool IsBreakpointsChanged() const { return mBreakPointsChanged; }
    void ClearBreakpointsChanged() { mBreakPointsChanged = false; }

    void SetShowCursor(bool aValue) { mShowCursor = aValue; }
    void SetShowLineNumbers(bool aValue) { mShowLineNumbers = aValue; }

	bool IsColorizerEnabled() const { return mColorizerEnabled; }
	void SetColorizerEnable(bool aValue);

	Coordinates GetCursorPosition() const { return GetActualCursorCoordinates(); }
	void SetCursorPosition(const Coordinates& aPosition);

    bool RaiseContextMenu() { return mRaiseContextMenu; }
    void ClearRaiseContextMenu() { mRaiseContextMenu = false; }

	inline void SetHandleMouseInputs    (bool aValue){ mHandleMouseInputs    = aValue;}
	inline bool IsHandleMouseInputsEnabled() const { return mHandleKeyboardInputs; }

	inline void SetHandleKeyboardInputs (bool aValue){ mHandleKeyboardInputs = aValue;}
	inline bool IsHandleKeyboardInputsEnabled() const { return mHandleKeyboardInputs; }

	inline void SetImGuiChildIgnored    (bool aValue){ mIgnoreImGuiChild     = aValue;}
	inline bool IsImGuiChildIgnored() const { return mIgnoreImGuiChild; }

	inline void SetShowWhitespaces(bool aValue) { mShowWhitespaces = aValue; }
	inline bool IsShowingWhitespaces() const { return mShowWhitespaces; }

	void SetTabSize(int aValue);
	inline int GetTabSize() const { return mTabSize; }

	void InsertText(const std::string& aValue);
	void InsertText(const char* aValue);

	void MoveUp(int aAmount = 1, bool aSelect = false);
	void MoveDown(int aAmount = 1, bool aSelect = false);
	void MoveLeft(int aAmount = 1, bool aSelect = false, bool aWordMode = false);
	void MoveRight(int aAmount = 1, bool aSelect = false, bool aWordMode = false);
	void MoveTop(bool aSelect = false);
	void MoveBottom(bool aSelect = false);
	void MoveHome(bool aSelect = false);
	void MoveEnd(bool aSelect = false);

	void SetSelectionStart(const Coordinates& aPosition);
	void SetSelectionEnd(const Coordinates& aPosition);
	void SetSelection(const Coordinates& aStart, const Coordinates& aEnd);
    Selection GetSelection() const;
	void SelectWordUnderCursor();
	void SelectAll();
	bool HasSelection() const;

	void Copy();
	void Cut();
	void Paste();
	void Delete();
    float GetPageSize() const;

	ImVec2 &GetCharAdvance() { return mCharAdvance; }

	bool CanUndo();
	bool CanRedo() const;
	void Undo(int aSteps = 1);
	void Redo(int aSteps = 1);

    void DeleteWordLeft();
    void DeleteWordRight();
    void Backspace();

	static const Palette& GetDarkPalette();
	static const Palette& GetLightPalette();
	static const Palette& GetRetroBluePalette();

private:
	typedef std::vector<std::pair<std::regex, PaletteIndex>> RegexList;

	struct EditorState
	{
		Coordinates mSelectionStart;
		Coordinates mSelectionEnd;
		Coordinates mCursorPosition;
	};


public:
    class FindReplaceHandler {
    public:
        FindReplaceHandler();
        typedef std::vector<EditorState> Matches;
        Matches &GetMatches() { return mMatches; }
        bool FindNext(TextEditor *editor);
        unsigned FindMatch(TextEditor *editor,bool isNex);
        bool Replace(TextEditor *editor,bool right);
        bool ReplaceAll(TextEditor *editor);
        std::string &GetFindWord()  { return mFindWord; }
        void SetFindWord(TextEditor *editor, const std::string &aFindWord) {
            if (aFindWord != mFindWord) {
                FindAllMatches(editor, aFindWord);
                mFindWord = aFindWord;
            }
        }
        std::string &GetReplaceWord()  { return mReplaceWord; }
        void SetReplaceWord(const std::string &aReplaceWord) { mReplaceWord = aReplaceWord; }
        void SelectFound(TextEditor *editor, int found);
        void FindAllMatches(TextEditor *editor,std::string findWord);
        unsigned FindPosition( TextEditor *editor, Coordinates pos, bool isNext);
        bool GetMatchCase() const { return mMatchCase; }
        void SetMatchCase(TextEditor *editor, bool matchCase)  {
            if (matchCase != mMatchCase) {
                mMatchCase = matchCase;
                mOptionsChanged = true;
                FindAllMatches(editor, mFindWord);
            }
        }
        bool GetWholeWord()  const { return mWholeWord; }
        void SetWholeWord(TextEditor *editor, bool wholeWord)  {
            if (wholeWord != mWholeWord) {
                mWholeWord = wholeWord;
                mOptionsChanged = true;
                FindAllMatches(editor, mFindWord);
            }
        }
        bool GetFindRegEx()  const { return mFindRegEx; }
        void SetFindRegEx(TextEditor *editor, bool findRegEx)  {
            if (findRegEx != mFindRegEx) {
                mFindRegEx = findRegEx;
                mOptionsChanged = true;
                FindAllMatches(editor, mFindWord);
            }
        }
        void resetMatches() {
            mMatches.clear();
            mFindWord = "";
        }

        void SetFindWindowPos(const ImVec2 &pos) { mFindWindowPos = pos; }
        void SetFindWindowSize(const ImVec2 &size) { mFindWindowSize = size; }
        ImVec2 GetFindWindowPos() const { return mFindWindowPos; }
        ImVec2 GetFindWindowSize() const { return mFindWindowSize; }
    private:
        std::string mFindWord;
        std::string mReplaceWord;
        bool mMatchCase;
        bool mWholeWord;
        bool mFindRegEx;
        bool mOptionsChanged;
        Matches mMatches;
        ImVec2 mFindWindowPos;
        ImVec2 mFindWindowSize;
    };
    FindReplaceHandler mFindReplaceHandler;
private:
	class UndoRecord
	{
	public:
		UndoRecord() {}
		~UndoRecord() {}

		UndoRecord(
			const std::string& aAdded,
			const TextEditor::Coordinates aAddedStart,
			const TextEditor::Coordinates aAddedEnd,

			const std::string& aRemoved,
			const TextEditor::Coordinates aRemovedStart,
			const TextEditor::Coordinates aRemovedEnd,

			TextEditor::EditorState& aBefore,
			TextEditor::EditorState& aAfter);

		void Undo(TextEditor* aEditor);
		void Redo(TextEditor* aEditor);

		std::string mAdded;
		Coordinates mAddedStart;
		Coordinates mAddedEnd;

		std::string mRemoved;
		Coordinates mRemovedStart;
		Coordinates mRemovedEnd;

		EditorState mBefore;
		EditorState mAfter;
	};

	typedef std::vector<UndoRecord> UndoBuffer;

	void ProcessInputs();
	void Colorize(int aFromLine = 0, int aCount = -1);
	void ColorizeRange(int aFromLine = 0, int aToLine = 0);
	void ColorizeInternal();
	float TextDistanceToLineStart(const Coordinates& aFrom) const;
	void EnsureCursorVisible();
	std::string GetText(const Coordinates& aStart, const Coordinates& aEnd) const;
	Coordinates GetActualCursorCoordinates() const;
	Coordinates SanitizeCoordinates(const Coordinates& aValue) const;
	void Advance(Coordinates& aCoordinates) const;
	void DeleteRange(const Coordinates& aStart, const Coordinates& aEnd);
	int InsertTextAt(Coordinates& aWhere, const char* aValue);
	void AddUndo(UndoRecord& aValue);
  	Coordinates ScreenPosToCoordinates(const ImVec2& aPosition) const;
	Coordinates FindWordStart(const Coordinates& aFrom) const;
	Coordinates FindWordEnd(const Coordinates& aFrom) const;
    Coordinates FindPreviousWord(const Coordinates& aFrom) const;
	Coordinates FindNextWord(const Coordinates& aFrom) const;
    Coordinates StringIndexToCoordinates(int aIndex, const std::string &str) const;
	int GetCharacterIndex(const Coordinates& aCoordinates) const;
	int GetCharacterColumn(int aLine, int aIndex) const;
	int GetLineCharacterCount(int aLine) const;
    int Utf8CharsToBytes(const Coordinates &aCoordinates) const;
    static int Utf8CharsToBytes(std::string line, uint32_t start, uint32_t numChars);
    unsigned long long GetLineByteCount(int aLine) const;
	int GetStringCharacterCount(std::string str) const;
	int GetLineMaxColumn(int aLine) const;
	bool IsOnWordBoundary(const Coordinates& aAt) const;
	void RemoveLine(int aStart, int aEnd);
	void RemoveLine(int aIndex);
	Line& InsertLine(int aIndex);
    void InsertLine(int aIndex, const std::string &aText);
    void EnterCharacter(ImWchar aChar, bool aShift);
	void DeleteSelection();
	std::string GetWordUnderCursor() const;
	std::string GetWordAt(const Coordinates& aCoords) const;
    TextEditor::PaletteIndex GetColorIndexFromFlags(Line::Flags flags);
    void ResetCursorBlinkTime();
    uint32_t SkipSpaces(const Coordinates &aFrom) const;
	void HandleKeyboardInputs();
	void HandleMouseInputs();
	void RenderText(const char *aTitle, const ImVec2 &lineNumbersStartPos, const ImVec2 &textEditorSize);
    void SetFocus();
	float mLineSpacing = 1.0F;
	Lines mLines;
    uint64_t mLinesTimestamp = 0;
    uint64_t mColorizedLinesTimestamp = 0;
	EditorState mState = {};
	UndoBuffer mUndoBuffer;
	int mUndoIndex = 0;
    bool mScrollToBottom = false;
    float mTopMargin = 0.0F;
    float mNewTopMargin = 0.0F;
    float mOldTopMargin = 0.0F;
    bool mTopMarginChanged = false;

	int mTabSize = 4;
	bool mOverwrite = false;
	bool mReadOnly = false;
	bool mWithinRender = false;
	bool mScrollToCursor = false;
	bool mScrollToTop = false;
	bool mTextChanged = false;
	bool mColorizerEnabled = true;
    float mLineNumberFieldWidth = 0.0F;
    size_t mLongestLineLength = 0;
	float mTextStart = 20.0F;                   // position (in pixels) where a code line starts relative to the left of the TextEditor.
	float  mLeftMargin = 10.0;
    float mTopLine = 0.0F;
    bool mSetTopLine = false;
	bool mCursorPositionChanged = false;
    bool mBreakPointsChanged = false;
	int mColorRangeMin = 0, mColorRangeMax = 0;
	bool mHandleKeyboardInputs = true;
	bool mHandleMouseInputs = true;
	bool mIgnoreImGuiChild = false;
	bool mShowWhitespaces = true;

	Palette mPalette = {};
	LanguageDefinition mLanguageDefinition = {};
	RegexList mRegexList;
    bool mCheckComments = true;
	Breakpoints mBreakpoints = {};
	ErrorMarkers mErrorMarkers = {};
    ErrorHoverBoxes mErrorHoverBoxes = {};
    ErrorGotoBoxes mErrorGotoBoxes = {};
    CursorBoxes mCursorBoxes = {};
	ImVec2 mCharAdvance = {};
	Coordinates mInteractiveStart = {}, mInteractiveEnd = {};
	std::string mLineBuffer;
	uint64_t mStartTime = 0;
	std::vector<std::string> mDefines;
    TextEditor *mSourceCodeEditor = nullptr;
    float mShiftedScrollY = 0;
    float mScrollY = 0;
    float mScrollYIncrement = 0.0F;
    bool mSetScrollY = false;
    float mNumberOfLinesDisplayed = 0;
	float mLastClick = -1.0F;
    bool mShowCursor = true;
    bool mShowLineNumbers = true;
    bool mRaiseContextMenu = false;
    Coordinates mFocusAtCoords = {};
    bool mUpdateFocus = false;

    std::vector<std::string>  mClickableText;

    static const int sCursorBlinkInterval;
    static const int sCursorBlinkOnTime;
};

bool TokenizeCStyleString(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end);
bool TokenizeCStyleCharacterLiteral(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end);
bool TokenizeCStyleIdentifier(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end);
bool TokenizeCStyleNumber(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end);
bool TokenizeCStyleOperator(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end);
bool TokenizeCStyleSeparator(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end);