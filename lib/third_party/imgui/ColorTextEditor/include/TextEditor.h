#pragma once

#include <string>
#include <vector>
#include <array>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <regex>
#include "imgui.h"

class TextEditor
{
public:
	enum class PaletteIndex
	{
		Default,
		Keyword,
		Number,
		String,
		CharLiteral,
		Punctuation,
		Preprocessor,
		Identifier,
		KnownIdentifier,
		PreprocIdentifier,
        GlobalDocComment,
		DocComment,
		Comment,
		MultiLineComment,
		PreprocessorDeactivated,
		Background,
		Cursor,
		Selection,
		ErrorMarker,
		Breakpoint,
		LineNumber,
		CurrentLineFill,
		CurrentLineFillInactive,
		CurrentLineEdge,
		Max
	};

	enum class SelectionMode
	{
		Normal,
		Word,
		Line
	};

	struct Breakpoint
	{
		int32_t m_line;
		bool m_enabled;
		std::string m_condition;

		Breakpoint()
			: m_line(-1)
			, m_enabled(false)
		{}
	};

	// Represents a character coordinate from the user's point of view,
	// i. e. consider an uniform grid (assuming fixed-width font) on the
	// screen as it is rendered, and each cell has its own coordinate, starting from 0.
	// Tabs are counted as [1..m_tabSize] count empty spaces, depending on
	// how many space is necessary to reach the next tab stop.
	// For example, coordinate (1, 5) represents the character 'B' in a line "\tABC", when m_tabSize = 4,
	// because it is rendered as "    ABC" on the screen.
	struct Coordinates
	{
		int32_t m_line, m_column;
		Coordinates() : m_line(0), m_column(0) {}
		Coordinates(int32_t aLine, int32_t aColumn) : m_line(aLine), m_column(aColumn)
		{
			assert(aLine >= 0);
			assert(aColumn >= 0);
		}
		static Coordinates invalid() { static Coordinates invalid(-1, -1); return invalid; }

		bool operator ==(const Coordinates& o) const
		{
			return
                    m_line == o.m_line &&
                    m_column == o.m_column;
		}

		bool operator !=(const Coordinates& o) const
		{
			return
                    m_line != o.m_line ||
                    m_column != o.m_column;
		}

		bool operator <(const Coordinates& o) const
		{
			if (m_line != o.m_line)
				return m_line < o.m_line;
			return m_column < o.m_column;
		}

		bool operator >(const Coordinates& o) const
		{
			if (m_line != o.m_line)
				return m_line > o.m_line;
			return m_column > o.m_column;
		}

		bool operator <=(const Coordinates& o) const
		{
			if (m_line != o.m_line)
				return m_line < o.m_line;
			return m_column <= o.m_column;
		}

		bool operator >=(const Coordinates& o) const
		{
			if (m_line != o.m_line)
				return m_line > o.m_line;
			return m_column >= o.m_column;
		}
	};

	struct Identifier
	{
		Coordinates m_location;
		std::string m_declaration;
	};

	typedef std::string String;
	typedef std::unordered_map<std::string, Identifier> Identifiers;
	typedef std::unordered_set<std::string> Keywords;
	typedef std::map<int32_t, std::string> ErrorMarkers;
	typedef std::unordered_set<int32_t> Breakpoints;
	typedef std::array<ImU32, (uint32_t)PaletteIndex::Max> Palette;
	typedef uint8_t Char;

	struct Glyph
	{
		Char m_char;
		PaletteIndex m_colorIndex = PaletteIndex::Default;
		bool m_comment : 1;
		bool m_multiLineComment : 1;
		bool m_preprocessor : 1;
		bool m_docComment : 1;
        bool m_globalDocComment : 1;
		bool m_deactivated : 1;

		Glyph(Char aChar, PaletteIndex aColorIndex) : m_char(aChar), m_colorIndex(aColorIndex), m_comment(false),
                                                      m_multiLineComment(false), m_preprocessor(false), m_docComment(false), m_globalDocComment(false), m_deactivated(false) {}
	};

	typedef std::vector<Glyph> Line;
	typedef std::vector<Line> Lines;

	struct LanguageDefinition
	{
		typedef std::pair<std::string, PaletteIndex> TokenRegexString;
		typedef std::vector<TokenRegexString> TokenRegexStrings;
		typedef bool(*TokenizeCallback)(const char * in_begin, const char * in_end, const char *& out_begin, const char *& out_end, PaletteIndex & paletteIndex);

		std::string m_name;
		Keywords m_keywords;
		Identifiers m_identifiers;
		Identifiers m_preprocIdentifiers;
		std::string m_commentStart, m_commentEnd, m_singleLineComment, m_globalDocComment, m_docComment;
		char m_preprocChar;
		bool m_autoIndentation;

		TokenizeCallback m_tokenize;

		TokenRegexStrings m_tokenRegexStrings;

		bool m_caseSensitive;

		LanguageDefinition()
			: m_preprocChar('#'), m_autoIndentation(true), m_tokenize(nullptr), m_caseSensitive(true)
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

	TextEditor();
	~TextEditor();

	void setLanguageDefinition(const LanguageDefinition& aLanguageDef);
	const LanguageDefinition& getLanguageDefinition() const { return mL_languageDefinition; }

	static const Palette& getPalette() { return s_paletteBase; }
	static void setPalette(const Palette& aValue);

	void setErrorMarkers(const ErrorMarkers& aMarkers) { m_errorMarkers = aMarkers; }
	void setBreakpoints(const Breakpoints& aMarkers) { m_breakpoints = aMarkers; }

	void render(const char* aTitle, const ImVec2& aSize = ImVec2(), bool aBorder = false);
	void setText(const std::string& aText);
	std::string getText() const;

	void setTextLines(const std::vector<std::string>& aLines);
	std::vector<std::string> getTextLines() const;

	std::string getSelectedText() const;
	std::string getCurrentLineText()const;
    class FindReplaceHandler;

public:
    FindReplaceHandler *getFindReplaceHandler() { return &m_findReplaceHandler; }
	int32_t getTotalLines() const { return (int32_t)m_lines.size(); }
	bool isOverwrite() const { return m_overwrite; }

	void setReadOnly(bool aValue);
	bool isReadOnly() const { return m_readOnly; }
	bool isTextChanged() const { return m_textChanged; }
	bool isCursorPositionChanged() const { return m_cursorPositionChanged; }

    void setShowCursor(bool aValue) { m_showCursor = aValue; }
    void setShowLineNumbers(bool aValue) { m_showLineNumbers = aValue; }

	bool isColorizerEnabled() const { return m_colorizerEnabled; }
	void setColorizerEnable(bool aValue);

	Coordinates getCursorPosition() const { return getActualCursorCoordinates(); }
	void setCursorPosition(const Coordinates& aPosition);

	inline void setHandleMouseInputs    (bool aValue){ m_handleMouseInputs    = aValue;}
	inline bool isHandleMouseInputsEnabled() const { return m_handleKeyboardInputs; }

	inline void setHandleKeyboardInputs (bool aValue){ m_handleKeyboardInputs = aValue;}
	inline bool isHandleKeyboardInputsEnabled() const { return m_handleKeyboardInputs; }

	inline void setImGuiChildIgnored    (bool aValue){ m_ignoreImGuiChild     = aValue;}
	inline bool isImGuiChildIgnored() const { return m_ignoreImGuiChild; }

	inline void setShowWhitespaces(bool aValue) { m_showWhitespaces = aValue; }
	inline bool isShowingWhitespaces() const { return m_showWhitespaces; }

	void setTabSize(int32_t aValue);
	inline int32_t getTabSize() const { return m_tabSize; }

	void insertText(const std::string& aValue);
	void insertText(const char* aValue);

	void moveUp(int32_t aAmount = 1, bool aSelect = false);
	void moveDown(int32_t aAmount = 1, bool aSelect = false);
	void moveLeft(int32_t aAmount = 1, bool aSelect = false, bool aWordMode = false);
	void moveRight(int32_t aAmount = 1, bool aSelect = false, bool aWordMode = false);
	void moveTop(bool aSelect = false);
	void moveBottom(bool aSelect = false);
	void moveHome(bool aSelect = false);
	void moveEnd(bool aSelect = false);

	void setSelectionStart(const Coordinates& aPosition);
	void setSelectionEnd(const Coordinates& aPosition);
	void setSelection(const Coordinates& aStart, const Coordinates& aEnd, SelectionMode aMode = SelectionMode::Normal);
	void selectWordUnderCursor();
	void selectAll();
	bool hasSelection() const;

	void copy();
	void cut();
	void paste();
	void doDelete();

	ImVec2 &getCharAdvance() { return m_charAdvance; }

	bool canUndo() const;
	bool canRedo() const;
	void undo(int32_t aSteps = 1);
	void redo(int32_t aSteps = 1);

	static const Palette& getDarkPalette();
	static const Palette& getLightPalette();
	static const Palette& getRetroBluePalette();

private:
	typedef std::vector<std::pair<std::regex, PaletteIndex>> RegexList;

	struct EditorState
	{
		Coordinates m_selectionStart;
		Coordinates m_selectionEnd;
		Coordinates m_cursorPosition;
	};


public:
    class FindReplaceHandler {
    public:
        FindReplaceHandler();
        typedef std::vector<EditorState> Matches;
        Matches &getMatches() { return m_matches; }
        bool findNext(TextEditor *editor, bool wrapAround);
        uint32_t findMatch(TextEditor *editor, bool isNex);
        bool replace(TextEditor *editor,bool right);
        bool replaceAll(TextEditor *editor);
        std::string &getFindWord()  { return m_findWord; }
        void setFindWord(TextEditor *editor, const std::string &aFindWord) {
            if (aFindWord != m_findWord) {
                findAllMatches(editor, aFindWord);
                m_findWord = aFindWord;
            }
        }
        std::string &getReplaceWord()  { return m_replaceWord; }
        void setReplaceWord(const std::string &aReplaceWord) { m_replaceWord = aReplaceWord; }
        void selectFound(TextEditor *editor, int32_t found);
        void findAllMatches(TextEditor *editor, std::string findWord);
        uint32_t findPosition(TextEditor *editor, Coordinates pos, bool isNext);
        bool getMatchCase() const { return m_matchCase; }
        void setMatchCase(TextEditor *editor, bool matchCase)  {
            if (matchCase != m_matchCase) {
                m_matchCase = matchCase;
                m_optionsChanged = true;
                findAllMatches(editor, m_findWord);
            }
        }
        bool getWholeWord()  const { return m_wholeWord; }
        void setWholeWord(TextEditor *editor, bool wholeWord)  {
            if (wholeWord != m_wholeWord) {
                m_wholeWord = wholeWord;
                m_optionsChanged = true;
                findAllMatches(editor, m_findWord);
            }
        }
        bool getFindRegEx()  const { return m_findRegEx; }
        void setFindRegEx(TextEditor *editor, bool findRegEx)  {
            if (findRegEx != m_findRegEx) {
                m_findRegEx = findRegEx;
                m_optionsChanged = true;
                findAllMatches(editor, m_findWord);
            }
        }
        void resetMatches() {
            m_matches.clear();
            m_findWord = "";
        }

        void setFindWindowPos(const ImVec2 &pos) { m_findWindowPos = pos; }
        void setFindWindowSize(const ImVec2 &size) { m_findWindowSize = size; }
        ImVec2 getFindWindowPos() const { return m_findWindowPos; }
        ImVec2 getFindWindowSize() const { return m_findWindowSize; }
    private:
        std::string m_findWord;
        std::string m_replaceWord;
        bool m_matchCase;
        bool m_wholeWord;
        bool m_findRegEx;
        bool m_optionsChanged;
        Matches m_matches;
        ImVec2 m_findWindowPos;
        ImVec2 m_findWindowSize;
    };
    FindReplaceHandler m_findReplaceHandler;
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

		void undo(TextEditor* aEditor);
		void redo(TextEditor* aEditor);

		std::string m_added;
		Coordinates m_addedStart;
		Coordinates m_addedEnd;

		std::string m_removed;
		Coordinates m_removedStart;
		Coordinates m_removedEnd;

		EditorState m_before;
		EditorState m_after;
	};

	typedef std::vector<UndoRecord> UndoBuffer;

	void processInputs();
	void colorize(int32_t aFromLine = 0, int32_t aCount = -1);
	void colorizeRange(int32_t aFromLine = 0, int32_t aToLine = 0);
	void colorizeInternal();
	float textDistanceToLineStart(const Coordinates& aFrom) const;
	void ensureCursorVisible();
	int32_t getPageSize() const;
	std::string getText(const Coordinates& aStart, const Coordinates& aEnd) const;
	Coordinates getActualCursorCoordinates() const;
	Coordinates sanitizeCoordinates(const Coordinates& aValue) const;
	void advance(Coordinates& aCoordinates) const;
	void deleteRange(const Coordinates& aStart, const Coordinates& aEnd);
	int32_t insertTextAt(Coordinates &aWhere, const char* aValue);
	void addUndo(UndoRecord& aValue);
	Coordinates screenPosToCoordinates(const ImVec2& aPosition) const;
	Coordinates findWordStart(const Coordinates& aFrom) const;
	Coordinates findWordEnd(const Coordinates& aFrom) const;
	Coordinates findNextWord(const Coordinates& aFrom) const;
	int32_t getCharacterIndex(const Coordinates& aCoordinates) const;
	int32_t getCharacterColumn(int32_t aLine, int32_t aIndex) const;
	int32_t getLineCharacterCount(int32_t aLine) const;
	uint64_t getLineByteCount(int32_t aLine) const;
	int32_t getStringCharacterCount(std::string str) const;
	int32_t getLineMaxColumn(int32_t aLine) const;
	bool isOnWordBoundary(const Coordinates& aAt) const;
	void removeLine(int32_t aStart, int32_t aEnd);
	void removeLine(int32_t aIndex);
	Line& insertLine(int32_t aIndex);
	void enterCharacter(ImWchar aChar, bool aShift);
	void backspace();
	void deleteSelection();
	std::string getWordUnderCursor() const;
	std::string getWordAt(const Coordinates& aCoords) const;
	ImU32 getGlyphColor(const Glyph& aGlyph) const;
    void resetCursorBlinkTime();

	void handleKeyboardInputs();
	void handleMouseInputs();
	void Render();

	float m_lineSpacing;
	Lines m_lines;
	EditorState m_state;
	UndoBuffer m_undoBuffer;
	int32_t m_undoIndex;
    bool m_scrollToBottom;
    float m_topMargin;

	int32_t m_tabSize;
	bool m_overwrite;
	bool m_readOnly;
	bool m_withinRender;
	bool m_scrollToCursor;
	bool m_scrollToTop;
	bool m_textChanged;
	bool m_colorizerEnabled;
	float m_textStart;                   // position (in pixels) where a code line starts relative to the left of the TextEditor.
	int32_t  m_leftMargin;
	bool m_cursorPositionChanged;
	int32_t m_colorRangeMin, m_colorRangeMax;
	SelectionMode m_selectionMode;
	bool m_handleKeyboardInputs;
	bool m_handleMouseInputs;
	bool m_ignoreImGuiChild;
	bool m_showWhitespaces;

	static Palette s_paletteBase;
	Palette m_palette;
	LanguageDefinition mL_languageDefinition;
	RegexList m_regexList;
    bool m_checkComments;
	Breakpoints m_breakpoints;
	ErrorMarkers m_errorMarkers;
	ImVec2 m_charAdvance;
	Coordinates m_interactiveStart, m_interactiveEnd;
	std::string m_lineBuffer;
	uint64_t m_startTime;
	std::vector<std::string> m_defines;

	float m_lastClick;
    bool m_showCursor;
    bool m_showLineNumbers;

    static const int32_t s_cursorBlinkInterval;
    static const int32_t s_cursorBlinkOnTime;
};

bool tokenizeCStyleString(const char * in_begin, const char * in_end, const char *& out_begin, const char *& out_end);
bool tokenizeCStyleCharacterLiteral(const char * in_begin, const char * in_end, const char *& out_begin, const char *& out_end);
bool tokenizeCStyleIdentifier(const char * in_begin, const char * in_end, const char *& out_begin, const char *& out_end);
bool tokenizeCStyleNumber(const char * in_begin, const char * in_end, const char *& out_begin, const char *& out_end);
bool tokenizeCStylePunctuation(const char * in_begin, const char * in_end, const char *& out_begin, const char *& out_end);