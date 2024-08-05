#pragma once

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <mutex>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <regex>
#include "imgui.h"

class TextEditor
{
public:
    /// Represents a character coordinate from the user's point of view,
    /// i.e. consider a uniform grid (assuming monospaced font) on the
    /// screen as it is rendered, and each cell has its own coordinate, starting from 0.
    /// Tabs are counted as [1:mTabSize] count empty spaces, depending on
    /// how many space is necessary to reach the next tab stop.
    /// For example, coordinate (0, 5) represents the character 'B' in the first line "\\tABC", where mTabSize = 4,
    /// because it is rendered as "    ABC" on the screen.
    ///
    /// This is similar to the Location class in ImHex except that it is zero based
    /// and it doesnt contain a pointer to the text.
    struct Coordinates
    {
        int32_t mLine, mColumn;
        Coordinates() : mLine(0), mColumn(0) {}
        Coordinates(int32_t aLine, int32_t aColumn) : mLine(aLine), mColumn(aColumn)
        {
            if(aLine<0)
                assert(aLine >= 0);
            if(aColumn<0)
                assert(aColumn >= 0);
        }
        static Coordinates Invalid() { static Coordinates invalid(-1, -1); return invalid; }
		bool operator ==(const Coordinates& o) const;
		bool operator !=(const Coordinates& o) const;
		bool operator <(const Coordinates& o) const;
		bool operator >(const Coordinates& o) const;
		bool operator <=(const Coordinates& o) const;
		bool operator >=(const Coordinates& o) const;
    };

    /**
     * @brief Colorizer API
     * PaletteIndex is an enum class that represents the color index of the text editor.
     * All the colors that can be used in the text editor are defined in this enum class.
     */

	enum class PaletteIndex
	{
        Default,
        UserDefinedType,
		PatternVariable,
		LocalVariable,
		CalculatedPointer,
		TemplateArgument,
		View,
		FunctionVariable,
		FunctionParameter,
		PlacedVariable,
        GlobalVariable,
		PreprocIdentifier,
        NameSpace,
        TypeDef,
        Keyword,
        BuiltInType,
        Attribute,
        Directive,
        Function,
        UnkIdentifier,
        NumericLiteral,
        StringLiteral,
        CharLiteral,
        Operator,
        Separator,
        Comment,
		BlockComment,
        DocComment,
		DocBlockComment,
		DocGlobalComment,
		PreprocessorDeactivated,
		Background,
		Cursor,
		Selection,
		ErrorMarker,
		Breakpoint,
		ErrorText,
		WarningText,
		DebugText,
		DefaultText,
		LineNumber,
		CurrentLineFill,
		CurrentLineFillInactive,
		CurrentLineEdge,
		Max
	};

    /**
     * @brief sets the color of a range of text
     * @param startCoord
     * @param endCoord
     * @param aColorIndex
     * This function and the next one are the ones used to set the colors of the text.
     */

    void setColorRange(Coordinates startCoord, Coordinates endCoord, PaletteIndex aColorIndex);

    /**
    * @brief Inserts a string at the current cursor position with a specific color.
    * @param aValue
    * @param aColorIndex
    * Typically used to insert lines of text but the only restriction is that the string
    * is of the same color.
    */

    void InsertColoredText(const std::string& aValue, PaletteIndex aColorIndex);

	typedef uint8_t Char;

	/**
	 * @brief Glyph
	 * A glyph is a character and its color index.
	 * A line is a vector of glyphs and a document is a vector of lines.
	 */

	struct Glyph
	{
		[[maybe_unused]] Char mChar = 0x00;
		[[maybe_unused]] PaletteIndex mColorIndex = PaletteIndex::Default;
        Glyph()=default;
		Glyph(Char aChar, PaletteIndex aColorIndex) : mChar(aChar), mColorIndex(aColorIndex) {}
	};

	using Line = std::vector<Glyph>;
    using Lines = std::vector<Line>;
    using LineColors = std::vector<PaletteIndex>;
    using LinesOfColors = std::vector<LineColors>;

    class ColorStaging {
        std::mutex *mMutex;
        LinesOfColors mLinesOfColors;
    public:
        ColorStaging();

        void stage(const LinesOfColors &linesOfColors) {
            std::scoped_lock lock(*mMutex);

            mLinesOfColors = linesOfColors;
        }

        void commit(Lines &lines) {
            std::scoped_lock lock(*mMutex);

            auto lineCount = lines.size();
            if (lineCount != mLinesOfColors.size())
                return;
            for (uint32_t i = 0; i < lineCount; i++) {
                auto &line = lines[i];
                auto &lineColors = mLinesOfColors[i];
                auto glyphCount = line.size();
                if (glyphCount != lineColors.size())
                    return;
                for (uint32_t j = 0; j < glyphCount; j++)
                    line[j].mColorIndex = lineColors[j];
            }
        }
    };

    enum class SelectionMode
    {
        Normal,
        Word,
        Line
    };

	using String = std::string;
    using ErrorMarkers = std::map<Coordinates, std::pair<uint32_t ,std::string>>;
    using ErrorHoverBoxes = std::map<Coordinates, std::pair<ImVec2,ImVec2>>;
	using Breakpoints = std::unordered_set<int32_t>;
	using Palette = std::array<ImU32, (uint32_t)PaletteIndex::Max>;

	TextEditor();
	~TextEditor();

	static const Palette& GetPalette() { return sPaletteBase; }
	static void SetPalette(const Palette& aValue);

	void SetErrorMarkers(const ErrorMarkers& aMarkers) { mErrorMarkers = aMarkers; }
	void ClearErrorMarkers() {
		mErrorMarkers.clear();
		mErrorHoverBoxes.clear();
	}

    auto getColorsStaging() { return &mColorStaging; }
	void SetBreakpoints(const Breakpoints& aMarkers) { mBreakpoints = aMarkers; }
    ImVec2 UnderSquiggles( ImVec2 pos, uint32_t nChars, ImColor color= ImGui::GetStyleColorVec4(ImGuiCol_Text), const ImVec2 &size_arg= ImVec2(0, 0));

	void Render(const char* aTitle, const ImVec2& aSize = ImVec2(), bool aBorder = false);
	void SetText(const std::string& aText);
	std::string GetText() const;
    Lines &GetLines() {
        return mLines;
    }
	void clearLines();
	[[maybe_unused]] void SetTextLines(const Lines& aLines);
	std::vector<std::string> GetTextLines() const;

	std::string GetSelectedText() const;
	[[maybe_unused]] std::string GetCurrentLineText()const;
    class FindReplaceHandler;

    FindReplaceHandler *GetFindReplaceHandler() { return &mFindReplaceHandler; }
	int32_t GetTotalLines() const { return (int32_t)mLines.size(); }
	[[maybe_unused]] bool IsOverwrite() const { return mOverwrite; }
    void SetOverwrite(bool aValue) { mOverwrite = aValue; }

	void SetReadOnly(bool aValue);
	bool IsReadOnly() const { return mReadOnly; }
	bool IsTextChanged() const { return mTextChanged; }
    void SetTextChanged(bool aValue) { mTextChanged = aValue; }
	[[maybe_unused]] bool IsCursorPositionChanged() const { return mCursorPositionChanged; }

    void SetShowCursor(bool aValue) { mShowCursor = aValue; }
    void SetShowLineNumbers(bool aValue) { mShowLineNumbers = aValue; }

	Coordinates GetCursorPosition() const { return GetActualCursorCoordinates(); }
	void SetCursorPosition(const Coordinates& aPosition);

	[[maybe_unused]] inline void SetHandleMouseInputs    (bool aValue){ mHandleMouseInputs    = aValue;}
	[[maybe_unused]] inline bool IsHandleMouseInputsEnabled() const { return mHandleKeyboardInputs; }

	[[maybe_unused]]inline void SetHandleKeyboardInputs (bool aValue){ mHandleKeyboardInputs = aValue;}
	[[maybe_unused]] inline bool IsHandleKeyboardInputsEnabled() const { return mHandleKeyboardInputs; }

	inline void SetImGuiChildIgnored    (bool aValue){ mIgnoreImGuiChild     = aValue;}
	[[maybe_unused]] inline bool IsImGuiChildIgnored() const { return mIgnoreImGuiChild; }

	inline void SetShowWhitespaces(bool aValue) { mShowWhitespaces = aValue; }
	[[maybe_unused]] inline bool IsShowingWhitespaces() const { return mShowWhitespaces; }

	[[maybe_unused]] void SetTabSize(int32_t aValue);
	[[maybe_unused]] inline int32_t GetTabSize() const { return mTabSize; }

	void InsertText(const std::string& aValue);
	void InsertText(const char* aValue);


	void MoveUp(int32_t aAmount = 1, bool aSelect = false);
	void MoveDown(int32_t aAmount = 1, bool aSelect = false);
	void MoveLeft(int32_t aAmount = 1, bool aSelect = false, bool aWordMode = false);
	void MoveRight(int32_t aAmount = 1, bool aSelect = false, bool aWordMode = false);
	void MoveTop(bool aSelect = false);
	void MoveBottom(bool aSelect = false);
	void MoveHome(bool aSelect = false);
	void MoveEnd(bool aSelect = false);

[[maybe_unused]]	void SetSelectionStart(const Coordinates& aPosition);
[[maybe_unused]]	void SetSelectionEnd(const Coordinates& aPosition);
	void SetSelection(const Coordinates& aStart, const Coordinates& aEnd, SelectionMode aMode = SelectionMode::Normal);
	void SelectWordUnderCursor();
	void SelectAll();
	bool HasSelection() const;


	void Copy();
	void Cut();
	void Paste();
	void Delete();
    int32_t GetPageSize() const;

	ImVec2 &GetCharAdvance() { return mCharAdvance; }

	bool CanUndo() const;
	bool CanRedo() const;
	void Undo(int32_t aSteps = 1);
	void Redo(int32_t aSteps = 1);

    void DeleteWordLeft();
    void DeleteWordRight();
    void Backspace();

					static const Palette& GetDarkPalette();
[[maybe_unused]]	static const Palette& GetLightPalette();
[[maybe_unused]]	static const Palette& GetRetroBluePalette();

private:

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
        bool FindNext(TextEditor *editor,bool wrapAround);
        uint32_t FindMatch(TextEditor *editor,bool isNex);
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
        void SelectFound(TextEditor *editor, int32_t found);
        void FindAllMatches(TextEditor *editor,std::string findWord);
        uint32_t FindPosition( TextEditor *editor, Coordinates pos, bool isNext);
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

	using UndoBuffer = std::vector<UndoRecord>;

	[[maybe_unused]] void ProcessInputs();
	float TextDistanceToLineStart(const Coordinates& aFrom) const;
	void EnsureCursorVisible();
	std::string GetText(const Coordinates& aStart, const Coordinates& aEnd) const;
	Coordinates GetActualCursorCoordinates() const;
	Coordinates SanitizeCoordinates(const Coordinates& aValue) const;
	void Advance(Coordinates& aCoordinates) const;
	void DeleteRange(const Coordinates& aStart, const Coordinates& aEnd);
	int32_t InsertTextAt(Coordinates& aWhere, const char* aValue);
	void AddUndo(UndoRecord& aValue);
	Coordinates ScreenPosToCoordinates(const ImVec2& aPosition) const;
	Coordinates FindWordStart(const Coordinates& aFrom) const;
	Coordinates FindWordEnd(const Coordinates& aFrom) const;
	[[maybe_unused]] Coordinates FindNextWord(const Coordinates& aFrom) const;
	int32_t GetCharacterIndex(const Coordinates& aCoordinates) const;
	int32_t GetCharacterColumn(int32_t aLine, int32_t aIndex) const;
	[[maybe_unused]] int32_t GetLineCharacterCount(int32_t aLine) const;
	uint64_t GetLineByteCount(int32_t aLine) const;
	int32_t GetStringCharacterCount(std::string str) const;
	int32_t GetLineMaxColumn(int32_t aLine) const;
	bool IsOnWordBoundary(const Coordinates& aAt) const;
	void RemoveLine(int32_t aStart, int32_t aEnd);
	void RemoveLine(int32_t aIndex);
	Line& InsertLine(int32_t aIndex);
	void EnterCharacter(ImWchar aChar, bool aShift);
	void DeleteSelection();
	[[maybe_unused]] std::string GetWordUnderCursor() const;
	std::string GetWordAt(const Coordinates& aCoords) const;
    void ResetCursorBlinkTime();
	void HandleKeyboardInputs();
	void HandleMouseInputs();
	void Render();

	float mLineSpacing;
	Lines mLines;
    ColorStaging mColorStaging;
	EditorState mState;
	UndoBuffer mUndoBuffer;
	int32_t mUndoIndex;
    bool mScrollToBottom;
    float mTopMargin;

	int32_t mTabSize;
	bool mOverwrite;
	bool mReadOnly;
	bool mWithinRender;
	bool mScrollToCursor;
	bool mScrollToTop;
	bool mTextChanged;
	bool mColorizerEnabled;
	float mTextStart;                   // position (in pixels) where a code line starts relative to the left of the TextEditor.
	int32_t  mLeftMargin;
	bool mCursorPositionChanged;
	SelectionMode mSelectionMode;
	bool mHandleKeyboardInputs;
	bool mHandleMouseInputs;
	bool mIgnoreImGuiChild;
	bool mShowWhitespaces;

	static Palette sPaletteBase;
	Palette mPalette;
	Breakpoints mBreakpoints;
	ErrorMarkers mErrorMarkers;
    ErrorHoverBoxes mErrorHoverBoxes;
	ImVec2 mCharAdvance;
	Coordinates mInteractiveStart, mInteractiveEnd;
	std::string mLineBuffer;
	uint64_t mStartTime;

	float mLastClick;
    bool mShowCursor;
    bool mShowLineNumbers;

    static const int sCursorBlinkInterval;
    static const int sCursorBlinkOnTime;
};

