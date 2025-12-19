#pragma once

#include <string>
#include <vector>
#include <array>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <regex>
#include "imgui.h"
#include "imgui_internal.h"
#include <hex/helpers/utils.hpp>
#include <pl/core/location.hpp>

namespace hex::ui {
    using strConstIter = std::string::const_iterator;

    class TextEditor {
    public:

        class Range {
        public:
            friend class TextEditor;
            friend class Lines;

            // Coordinates represent 2-dimensional points used to identify locations in the pattern editor.
            // as line number and column number pairs. Coordinates can be folded and unfolded. Folded
            // lines are called rows. Columns keep their name. You can convert lines to rows and vice versa or
            // the supplied functions to convert whole coordinates. Unfolded and folded coordinates come in two types.
            // The first one referred to as (plain) coordinates correspond to the line number for the y component
            // and the utf8 character index within the line for the x coordinate. The  second kind are given the
            // name of Index coordinate or just plain index. These correspond directly
            // to the indices of the c++ containers holding the editor data. Negative values are used to index
            // from the end of the respective c++ container. In any document with N lines and M_N columns on each line
            // first character can be described equally by the Coordinates (0,0) or (-M, -N_0) and the last one as either
            // (M-1, N_(M-1)-1) or  just (-1,-1).
            class Coordinates {
            public:
                friend class TextEditor;
                friend class ViewPatternEditor;
                Coordinates() : m_line(0), m_column(0) {}
                explicit Coordinates(pl::core::Location location) : m_line(location.line - 1), m_column(location.column - 1) {}
                Coordinates(i32 lineIndex, i32 column) : m_line(lineIndex), m_column(column) {}
                Coordinates(TextEditor *editor, i32 lineIndex, i32 column);
                Coordinates sanitize(TextEditor *editor);
                bool isValid(TextEditor *editor) const;
                bool operator==(const Coordinates &o) const;
                bool operator!=(const Coordinates &o) const;
                bool operator<(const Coordinates &o) const;
                bool operator>(const Coordinates &o) const;
                bool operator<=(const Coordinates &o) const;
                bool operator>=(const Coordinates &o) const;
                Coordinates operator+(const Coordinates &o) const;
                Coordinates operator-(const Coordinates &o) const;
                i32 getLine() const { return m_line; }
                i32 getColumn() const { return m_column; }

            private:
                i32 m_line, m_column;
            };

            Range() = default;
            explicit Range(std::pair<Coordinates,Coordinates> coords) : m_start(coords.first), m_end(coords.second) {
                if (m_start > m_end) { std::swap(m_start, m_end); }}
            Range(Coordinates start, Coordinates end) : m_start(start), m_end(end) {
                if (m_start > m_end) { std::swap(m_start, m_end); }}



            Coordinates getSelectedLines();
            Coordinates getSelectedColumns();
            Coordinates getStart() { return m_start; }
            Coordinates getEnd() { return m_end; }
            bool isSingleLine();
            enum class EndsInclusive : u8 { None = 0, Start = 2, End = 1, Both = 3 };
            bool contains(const Coordinates &coordinates, EndsInclusive endsInclusive = EndsInclusive::Both) const;
            bool contains(const Range &range, EndsInclusive endsInclusive = EndsInclusive::Both) const;
            bool containsLine(i32 value, EndsInclusive endsInclusive = EndsInclusive::Both) const;
            bool containsColumn(i32 value, EndsInclusive endsInclusive = EndsInclusive::Both) const;
            bool overlaps(const Range &o, EndsInclusive endsInclusive = EndsInclusive::Both) const;
            bool operator==(const Range &o) const;
            bool operator!=(const Range &o) const;
            bool operator<(const Range &o) const {
                return m_end < o.m_end;
            }
            bool operator>(const Range &o) const {
                return m_end > o.m_end;
            }
            bool operator<=(const Range &o) const {
                return !(*this > o);
            }
            bool operator>=(const Range &o) const {
                return !(*this < o);
            }

        private:
            Coordinates m_start;
            Coordinates m_end;
        };

        using Coordinates = Range::Coordinates;
        class EditorState {
        public:
            friend class TextEditor;
            bool operator==(const EditorState &o) const;
            EditorState() = default;
            EditorState(const Range &selection, const Coordinates &cursorPosition) : m_selection(selection), m_cursorPosition(cursorPosition) {}
        private:
            Range m_selection;
            Coordinates m_cursorPosition;
        };

        class UndoRecord;
        class UndoAction;
        using UndoBuffer = std::vector<UndoAction>;
        using UndoRecords = std::vector<UndoRecord>;

        class FindReplaceHandler {
        public:
            FindReplaceHandler();
            using Matches = std::vector<EditorState>;
            Matches &getMatches() { return m_matches; }
            bool findNext(TextEditor *editor, u64 &byteIndex);
            u32 findMatch(TextEditor *editor, i32 index);
            bool replace(TextEditor *editor, bool right);
            bool replaceAll(TextEditor *editor);
            std::string &getFindWord() { return m_findWord; }

            void setFindWord(TextEditor *editor, const std::string &findWord);

            std::string &getReplaceWord() { return m_replaceWord; }
            void setReplaceWord(const std::string &replaceWord) { m_replaceWord = replaceWord; }
            void selectFound(TextEditor *editor, i32 found);
            void findAllMatches(TextEditor *editor, std::string findWord);
            u32 findPosition(TextEditor *editor, Coordinates pos, bool isNext);
            bool getMatchCase() const { return m_matchCase; }

            void setMatchCase(TextEditor *editor, bool matchCase);

            bool getWholeWord() const { return m_wholeWord; }
            void setWholeWord(TextEditor *editor, bool wholeWord);

            bool getFindRegEx() const { return m_findRegEx; }
            void setFindRegEx(TextEditor *editor, bool findRegEx);

            void resetMatches();
            UndoRecords m_undoBuffer;
        private:
            std::string m_findWord;
            std::string m_replaceWord;
            bool m_matchCase;
            bool m_wholeWord;
            bool m_findRegEx;
            bool m_optionsChanged;
            Matches m_matches;
        };

        enum class PaletteIndex {
            Default, Identifier, Directive, Operator, Separator, BuiltInType, Keyword, NumericLiteral, StringLiteral, CharLiteral, Cursor, Background, LineNumber, Selection, Breakpoint, ErrorMarker, PreprocessorDeactivated,
            CurrentLineFill, CurrentLineFillInactive, CurrentLineEdge, ErrorText, WarningText, DebugText, DefaultText, Attribute, PatternVariable, LocalVariable, CalculatedPointer, TemplateArgument, Function, View,
            FunctionVariable, FunctionParameter, UserDefinedType, PlacedVariable, GlobalVariable, NameSpace, TypeDef, UnkIdentifier, DocComment, DocBlockComment, BlockComment, GlobalDocComment, Comment, PreprocIdentifier, Max
        };

        using RegexList     = std::vector<std::pair<std::regex, PaletteIndex>>;
        using Keywords      = std::unordered_set<std::string>;
        using ErrorMarkers  = std::map<Coordinates, std::pair<i32, std::string>>;
        using Breakpoints   = std::unordered_set<u32>;
        using Palette       = std::array<ImU32, (u64) PaletteIndex::Max>;
        using Glyph         = u8;

        struct Identifier { Coordinates m_location; std::string m_declaration;};
        using Identifiers   = std::unordered_map<std::string, Identifier>;

        class ActionableBox {
        public:
            ActionableBox() : m_box(ImRect(ImVec2(0, 0), ImVec2(0, 0))) {};
            explicit ActionableBox(const ImRect &box) : m_box(box) {}

            ImRect &getBox() const { return const_cast<ImRect &>(m_box);}
            virtual bool trigger();
            virtual void callback() {}
            virtual ~ActionableBox() = default;
            void shiftBoxVertically(float lineCount, float lineHeight);
        private:
            ImRect m_box;
        };

        class CursorChangeBox : public ActionableBox {
        public:
            CursorChangeBox() : ActionableBox(ImRect(ImVec2(0, 0), ImVec2(0, 0))) {}
            explicit CursorChangeBox(const ImRect &box) : ActionableBox(box) {}

            void callback() override { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);}
        };

        class ErrorGotoBox : public ActionableBox {
        public:
            ErrorGotoBox() = default;
            ErrorGotoBox(const ImRect &box, const Coordinates &pos, TextEditor *editor) : ActionableBox(box), m_pos(pos), m_editor(editor) {}

            bool trigger() override { return ActionableBox::trigger() && ImGui::IsMouseClicked(0);}
            void callback() override { m_editor->jumpToCoords(m_pos);}
        private:
            Coordinates m_pos;
            TextEditor *m_editor = nullptr;
        };

        class ErrorHoverBox : public ActionableBox {
        public:
            ErrorHoverBox() = default;
            ErrorHoverBox(const ImRect &box, const Coordinates &pos, const char *errorText) : ActionableBox(box), m_pos(pos), m_errorText(errorText) {}

            void callback() override;
        private:
            Coordinates m_pos;
            std::string m_errorText;
        };

        using ErrorGotoBoxes        = std::map<Coordinates, ErrorGotoBox>;
        using CursorBoxes           = std::map<Coordinates, CursorChangeBox>;
        using ErrorHoverBoxes       = std::map<Coordinates, ErrorHoverBox>;
        enum class TrimMode : u8 { TrimNone = 0, TrimEnd = 1, TrimStart = 2, TrimBoth = 3 };

        // A line of text in the pattern editor consists of three strings; the character encoding, the color encoding and the flags.
        // The char encoding is utf-8, the color encoding are indices to the color palette and the flags are used to override the colors
        // depending on priorities; e.g. comments, strings, etc.

        class LineIterator {
        public:
            friend class hex::ui::TextEditor;
            LineIterator(const LineIterator &other) : m_charsIter(other.m_charsIter), m_colorsIter(other.m_colorsIter), m_flagsIter(other.m_flagsIter) {}
            LineIterator() = default;

            char operator*();
            LineIterator operator++();
            LineIterator operator=(const LineIterator &other);
            bool operator!=(const LineIterator &other) const;
            bool operator==(const LineIterator &other) const;
            LineIterator operator+(i32 n);
            i32 operator-(LineIterator l);
        private:
            strConstIter m_charsIter;
            strConstIter m_colorsIter;
            strConstIter m_flagsIter;
        };

        class Line {
        public:
            friend class TextEditor;
            enum class Comments : u8 {
                NoComment = 0,
                Doc = 0b0001,
                Block = 0b0010,
                BlockDoc = 0b0011,
                Line = 0b0100,
                Global = 0b0101,
            };
            struct FlagBits {
                bool doc: 1;
                bool block: 1;
                bool global: 1;
                bool deactivated: 1;
                bool preprocessor: 1;
                bool matchedDelimiter: 1;
            };

            union Flags {
                Flags(char value) : m_value(value) {}
                Flags(FlagBits bits) : m_bits(bits) {}
                FlagBits m_bits;
                char m_value;
            };

            enum class LinePart { Chars, Utf8, Colors, Flags };

            Line() : m_lineMaxColumn(-1) {}
            explicit Line(const char *line) : Line(std::string(line)) {}
            explicit Line(const std::string &line) : m_chars(line), m_colors(std::string(line.size(), 0x00)), m_flags(std::string(line.size(), 0x00)), m_lineMaxColumn(maxColumn()) {}
            Line(const Line &line) : m_chars(std::string(line.m_chars)), m_colors(std::string(line.m_colors)), m_flags(std::string(line.m_flags)), m_colorized(line.m_colorized), m_lineMaxColumn(line.m_lineMaxColumn) {}
            Line(Line &&line) noexcept : m_chars(std::move(line.m_chars)), m_colors(std::move(line.m_colors)), m_flags(std::move(line.m_flags)), m_colorized(line.m_colorized), m_lineMaxColumn(line.m_lineMaxColumn) {}
            Line(std::string chars, std::string colors, std::string flags) : m_chars(std::move(chars)), m_colors(std::move(colors)), m_flags(std::move(flags)), m_lineMaxColumn(maxColumn()) {}

            bool operator==(const Line &line) const;
            bool operator!=(const Line &line) const;
            [[nodiscard]] i32 indexColumn(i32 stringIndex) const;
            [[nodiscard]] i32 maxColumn();
            [[nodiscard]] i32 maxColumn() const;
            [[nodiscard]] i32 columnIndex(i32 column) const;
            [[nodiscard]] i32 textSize() const;
            [[nodiscard]] i32 textSize(u32 index) const;
            i32 lineTextSize(TrimMode trimMode = TrimMode::TrimNone);
            [[nodiscard]] i32 stringTextSize(const std::string &str) const;
            i32 textSizeIndex(float textSize, i32 position);
            [[nodiscard]] LineIterator begin() const;
            [[nodiscard]] LineIterator end() const;
            LineIterator begin();
            LineIterator end();
            Line &operator=(const Line &line);
            Line &operator=(Line &&line) noexcept;
            [[nodiscard]] u64 size() const;
            TextEditor::Line trim(TrimMode trimMode);
            [[nodiscard]] char front(LinePart part = LinePart::Chars) const;
            [[nodiscard]] std::string frontUtf8(LinePart part = LinePart::Chars) const;
            void push_back(char c);
            [[nodiscard]] bool empty() const;
            [[nodiscard]] std::string substr(u64 start, u64 length = (u64) -1, LinePart part = LinePart::Chars) const;
            Line subLine(u64 start, u64 length = (u64) -1);
            char operator[](u64 index) const;
            // C++ can't overload functions based on return type, so use any type other
            // than u64 to avoid ambiguity.
            std::string operator[](i64 column) const;
            void setNeedsUpdate(bool needsUpdate);
            void append(const char *text);
            void append(char text);
            void append(const std::string &text);
            void append(const Line &line);
            void append(LineIterator begin, LineIterator end);
            void insert(LineIterator iter, const std::string &text);
            void insert(LineIterator iter, char text);
            void insert(LineIterator iter, strConstIter beginString, strConstIter endString);
            void insert(LineIterator iter, const Line &line);
            void insert(LineIterator iter, LineIterator beginLine, LineIterator endLine);
            void erase(LineIterator begin);
            void erase(LineIterator begin, u64 count);
            void erase(u64 start, i64 length = -1);
            void clear();
            void setLine(const std::string &text);
            void setLine(const Line &text);
            [[nodiscard]] bool needsUpdate() const;
            bool isEndOfLine(i32 column);
        private:
           std::string m_chars;
           std::string m_colors;
           std::string m_flags;
           bool m_colorized = false;
           i32 m_lineMaxColumn;
        };

        using Lines = std::vector<Line>;

        struct LanguageDefinition {
            typedef std::pair<std::string, PaletteIndex> TokenRegexString;
            typedef std::vector<TokenRegexString> TokenRegexStrings;
            typedef bool(*TokenizeCallback)(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end, PaletteIndex &paletteIndex);

            std::string m_name;
            Keywords m_keywords;
            Identifiers m_identifiers;
            Identifiers m_preprocIdentifiers;
            std::string m_singleLineComment, m_commentEnd, m_commentStart, m_globalDocComment, m_docComment, m_blockDocComment;
            char m_preprocChar = '#';
            bool m_autoIndentation = true;
            TokenizeCallback m_tokenize;
            TokenRegexStrings m_tokenRegexStrings;
            bool m_caseSensitive = true;

            LanguageDefinition() : m_keywords({}), m_identifiers({}), m_preprocIdentifiers({}),
                                    m_tokenize(nullptr), m_tokenRegexStrings({}) {}

            static const LanguageDefinition &CPlusPlus();
            static const LanguageDefinition &HLSL();
            static const LanguageDefinition &GLSL();
            static const LanguageDefinition &C();
            static const LanguageDefinition &SQL();
            static const LanguageDefinition &AngelScript();
            static const LanguageDefinition &Lua();
        };

        TextEditor();
        ~TextEditor();

        class UndoRecord {
        public:
            friend class TextEditor;
            UndoRecord() = default;
            ~UndoRecord() {}
            UndoRecord( const std::string &added,
                        Range addedRange,
                        const std::string &removed,
                        Range removedRange,
                        EditorState &before,
                        EditorState &after);

            void undo(TextEditor *editor);
            void redo(TextEditor *editor);
        private:
            std::string m_added;
            Range m_addedRange;
            std::string m_removed;
            Range m_removedRange;
            EditorState m_before;
            EditorState m_after;
        };

        class UndoAction {
        public:
            UndoAction() = default;
            ~UndoAction() {}
            explicit UndoAction(const UndoRecords &records) : m_records(records) {}
            void undo(TextEditor *editor);
            void redo(TextEditor *editor);
        private:
            UndoRecords m_records;
        };



        struct MatchedBracket {
            bool m_active = false;
            bool m_changed = false;
            Coordinates m_nearCursor;
            Coordinates m_matched;
            static const std::string s_separators;
            static const std::string s_operators;

            MatchedBracket(const MatchedBracket &other) : m_active(other.m_active), m_changed(other.m_changed),
                                                          m_nearCursor(other.m_nearCursor),
                                                          m_matched(other.m_matched) {}

            MatchedBracket() : m_nearCursor(0, 0), m_matched(0, 0) {}
            MatchedBracket(bool active, bool changed, const Coordinates &nearCursor, const Coordinates &matched)
                    : m_active(active), m_changed(changed), m_nearCursor(nearCursor), m_matched(matched) {}

            bool checkPosition(TextEditor *editor, const Coordinates &from);
            bool isNearABracket(TextEditor *editor, const Coordinates &from);
            i32 detectDirection(TextEditor *editor, const Coordinates &from);
            void findMatchingBracket(TextEditor *editor);
            [[nodiscard]] bool isActive() const { return m_active; }
            [[nodiscard]] bool hasChanged() const { return m_changed; }
        };


    public:
// Rendering
        ImVec2 underwaves(ImVec2 pos, u32 nChars, ImColor color = ImGui::GetStyleColorVec4(ImGuiCol_Text), const ImVec2 &size_arg = ImVec2(0, 0));
        void setTabSize(i32 value);
        float getPageSize() const;
        bool isEndOfLine(const Coordinates &coordinates) const;
        bool isEndOfFile(const Coordinates &coordinates) const;
        bool isEndOfLine() const;
        bool isStartOfLine() const;
        void setTopLine();
        void render(const char *title, const ImVec2 &size = ImVec2(), bool border = false);

        void setShowCursor(bool value) { m_showCursor = value; }
        void setShowLineNumbers(bool value) { m_showLineNumbers = value; }
        void setShowWhitespaces(bool value) { m_showWhitespaces = value; }
        bool isShowingWhitespaces() const { return m_showWhitespaces; }
        i32 getTabSize() const { return m_tabSize; }
        ImVec2 &getCharAdvance() { return m_charAdvance; }
        void clearGotoBoxes() { m_errorGotoBoxes.clear(); }
        void clearCursorBoxes() { m_cursorBoxes.clear(); }
        void addClickableText(std::string text) { m_clickableText.emplace_back(std::move(text)); }
        void setErrorMarkers(const ErrorMarkers &markers) { m_errorMarkers = markers; }
        Breakpoints &getBreakpoints() { return m_breakpoints; }
        void setBreakpoints(const Breakpoints &markers) { m_breakpoints = markers; }
        void setLongestLineLength(u64 line) { m_longestLineLength = line; }
        u64 getLongestLineLength() const { return m_longestLineLength; }
        void setTopMarginChanged(i32 newMargin);
        void setFocusAtCoords(const Coordinates &coords, bool scrollToCursor = false);
        void clearErrorMarkers();
        void clearActionables();
    private:
        void ensureCursorVisible();
        void resetCursorBlinkTime();
        void renderText(const char *title, const ImVec2 &lineNumbersStartPos, const ImVec2 &textEditorSize);
        void setFocus();
        void preRender();
        void drawSelection(float lineNo);
        void drawLineNumbers(ImVec2 position, float lineNo, const ImVec2 &contentSize, bool focused, ImDrawList *drawList);
        void renderCursor(float lineNo, ImDrawList *drawList);
        void renderGotoButtons(float lineNo);
        void drawText(Coordinates &lineStart, u64 i, u32 tokenLength, char color);
        void postRender(const char *title, ImVec2 position, float lineNo);
        ImVec2 calculateCharAdvance() const;
        float textDistanceToLineStart(const Coordinates &from);
// Highlighting
    public:
        void colorize();
        void setLanguageDefinition(const LanguageDefinition &aLanguageDef);
        static const Palette &getPalette();
        static void setPalette(const Palette &value);
        static const Palette &getDarkPalette();
        static const Palette &getLightPalette();
        static const Palette &getRetroBluePalette();
        bool isColorizerEnabled() const { return m_colorizerEnabled; }
        const LanguageDefinition &getLanguageDefinition() const { return m_languageDefinition; }
        void setNeedsUpdate(i32 line, bool needsUpdate);
        void setColorizedLine(i64 line, const std::string &tokens);
    private:
        void colorizeRange();
        void colorizeInternal();
//Editing
    public:
        void deleteWordLeft();
        void deleteWordRight();
        void backspace();
        bool canUndo();
        bool canRedo() const;
        void undo();
        void redo();
        void copy();
        void cut();
        void paste();
        void doPaste(const char *clipText);
        void deleteChar();
        void insertText(const std::string &value);
        void insertText(const char *value);
        void appendLine(const std::string &value);
        void setOverwrite(bool value) { m_overwrite = value; }
        bool isOverwrite() const { return m_overwrite; }
        void setText(const std::string &text, bool undo = false);
        std::string getText();
        std::vector<std::string> getTextLines() const;
        std::string getSelectedText();
        std::string getLineText(i32 line);
        void setTextChanged(bool value) { m_textChanged = value; }
        bool isTextChanged() { return m_textChanged; }
        void setReadOnly(bool value) { m_readOnly = value; }
        void setHandleMouseInputs(bool value) { m_handleMouseInputs = value; }
        bool isHandleMouseInputsEnabled() const { return m_handleMouseInputs; }
        void setHandleKeyboardInputs(bool value) { m_handleKeyboardInputs = value; }
        bool isHandleKeyboardInputsEnabled() const { return m_handleKeyboardInputs; }
    private:
        std::string getText(const Range &from);
        void deleteRange(const Range &rangeToDelete);
        i32 insertTextAt(Coordinates &where, const std::string &value);
        void removeLine(i32 start, i32 end);
        void removeLine(i32 index);
        Line &insertLine(i32 index);
        void insertLine(i32 index, const std::string &text);
        void enterCharacter(ImWchar character, bool shift);
        void deleteSelection();
// Navigating
    public:
        void jumpToLine(i32 line = -1);
        void jumpToCoords(const Coordinates &coords);
        void moveUp(i32 amount = 1, bool select = false);
        void moveDown(i32 amount = 1, bool select = false);
        void moveLeft(i32 amount = 1, bool select = false, bool wordMode = false);
        void moveRight(i32 amount = 1, bool select = false, bool wordMode = false);
        void moveTop(bool select = false);
        void moveBottom(bool select = false);
        void moveHome(bool select = false);
        void moveEnd(bool select = false);
        void moveToMatchedBracket(bool select = false);
        void setScrollY();
        void setScroll(ImVec2 scroll);
        ImVec2 getScroll() const { return m_scroll; }
        Coordinates getCursorPosition()  { return setCoordinates(m_state.m_cursorPosition); }
        void setCursorPosition(const Coordinates &position, bool scrollToCursor = true);
        void setCursorPosition();
    private:
        Coordinates setCoordinates(const Coordinates &value);
        Coordinates setCoordinates(i32 line, i32 column);
        Range setCoordinates(const Range &value);
        void advance(Coordinates &coordinates) const;
        Coordinates findWordStart(const Coordinates &from);
        Coordinates findWordEnd(const Coordinates &from);
        Coordinates findPreviousWord(const Coordinates &from);
        Coordinates findNextWord(const Coordinates &from);
        u32 skipSpaces(const Coordinates &from);
//Support
    public:
        void setSelection(const Range &selection);
        Range getSelection() const;
        void selectWordUnderCursor();
        void selectAll();
        bool hasSelection() const;
        void refreshSearchResults();
        i32 getTotalLines() const { return (i32) m_lines.size(); }
        FindReplaceHandler *getFindReplaceHandler() { return &m_findReplaceHandler; }
        void setSourceCodeEditor(TextEditor *editor) { m_sourceCodeEditor = editor; }
        void clearBreakpointsChanged() { m_breakPointsChanged = false; }
        bool isBreakpointsChanged() { return m_breakPointsChanged; }
        void setImGuiChildIgnored(bool value) { m_ignoreImGuiChild = value; }
        bool isImGuiChildIgnored() const { return m_ignoreImGuiChild; }
        bool raiseContextMenu() { return m_raiseContextMenu; }
        void clearRaiseContextMenu() { m_raiseContextMenu = false; }
        TextEditor *getSourceCodeEditor();
        bool isEmpty() const;
        void addUndo(UndoRecords &value);
    private:
        TextEditor::PaletteIndex getColorIndexFromFlags(Line::Flags flags);
        void handleKeyboardInputs();
        void handleMouseInputs();
// utf8
    public:
        static i32 imTextCharToUtf8(char *buffer, i32 buf_size, u32 c);
        static void imTextCharToUtf8(std::string &buffer, u32 c);
        static i32 utf8CharLength(uint8_t c);
        static i32 stringCharacterCount(const std::string &str);
        static TextEditor::Coordinates stringIndexToCoordinates(i32 strIndex, const std::string &input);
        i32 lineMaxColumn(i32 lineIndex);
    private:

        Coordinates screenPosToCoordinates(const ImVec2 &position);
        Coordinates lineCoordsToIndexCoords(const Coordinates &coordinates) const;
        i32 lineCoordinatesToIndex(const Coordinates &coordinates) const;
        Coordinates getCharacterCoordinates(i32 line, i32 index);
        i32 lineIndexColumn(i32 lineIndex, i32 stringIndex);
        u64 getLineByteCount(i32 line) const;

    public:
        FindReplaceHandler m_findReplaceHandler;
    private:
        float m_lineSpacing = 1.0F;
        Lines m_lines;
        EditorState m_state;
        UndoBuffer m_undoBuffer;
        i32 m_undoIndex = 0;
        bool m_scrollToBottom = false;
        float m_topMargin = 0.0F;
        float m_newTopMargin = 0.0F;
        float m_oldTopMargin = 0.0F;
        bool m_topMarginChanged = false;

        i32 m_tabSize = 4;
        bool m_overwrite = false;
        bool m_readOnly = false;
        bool m_withinRender = false;
        bool m_scrollToCursor = false;
        bool m_scrollToTop = false;
        bool m_textChanged = false;
        bool m_colorizerEnabled = true;
        float m_lineNumberFieldWidth = 0.0F;
        u64 m_longestLineLength = 0;
        float m_leftMargin = 10.0;
        float m_topLine = 0.0F;
        bool m_setTopLine = false;
        bool m_breakPointsChanged = false;
        bool m_handleKeyboardInputs = true;
        bool m_handleMouseInputs = true;
        bool m_ignoreImGuiChild = false;
        bool m_showWhitespaces = true;

        MatchedBracket m_matchedBracket;
        Palette m_palette = {};
        LanguageDefinition m_languageDefinition;
        RegexList m_regexList;
        bool m_updateFlags = true;
        Breakpoints m_breakpoints;
        ErrorMarkers m_errorMarkers;
        ErrorHoverBoxes m_errorHoverBoxes;
        ErrorGotoBoxes m_errorGotoBoxes;
        CursorBoxes m_cursorBoxes;
        ImVec2 m_charAdvance;
        Range m_interactiveSelection;
        u64 m_startTime = 0;
        std::vector<std::string> m_defines;
        TextEditor *m_sourceCodeEditor = nullptr;
        float m_shiftedScrollY = 0;
        ImVec2 m_scroll=ImVec2(0, 0);
        float m_scrollYIncrement = 0.0F;
        bool m_setScroll = false;
        bool m_setScrollY = false;
        float m_numberOfLinesDisplayed = 0;
        float m_lastClick = -1.0F;
        bool m_showCursor = true;
        bool m_showLineNumbers = true;
        bool m_raiseContextMenu = false;
        Coordinates m_focusAtCoords;
        bool m_updateFocus = false;

        std::vector<std::string> m_clickableText;

        constexpr static char inComment = 7;
        inline static const Line m_emptyLine = Line();
        inline static const Coordinates Invalid = Coordinates(0x80000000, 0x80000000);
        static const i32 s_cursorBlinkInterval;
        static const i32 s_cursorBlinkOnTime;
        static ImVec2 s_cursorScreenPosition;
    };

    bool tokenizeCStyleString(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end);
    bool tokenizeCStyleCharacterLiteral(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end);
    bool tokenizeCStyleIdentifier(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end);
    bool tokenizeCStyleNumber(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end);
    bool tokenizeCStyleOperator(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end);
    bool tokenizeCStyleSeparator(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end);

}