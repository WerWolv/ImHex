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
#include <iostream>
#include "imgui.h"
#include "imgui_internal.h"
#include <hex/helpers/utils.hpp>

namespace hex::ui {
    using strConstIter = std::string::const_iterator;

    class TextEditor {

    public:
        // indices of the arrays that contain the lines (vector) and the columns (a string) of the
        // text editor. Negative values indicate the distance to the last element of the array.
        // When comparing coordinates ensure they have the same sign because coordinates don't have
        // information about the size of the array. Currently positive coordinates are always bigger
        // than negatives even if that gives a wrong result.
        struct Coordinates {
            i32 m_line, m_column;

            Coordinates() : m_line(0), m_column(0) {}
            Coordinates(i32 line, i32 column) : m_line(line), m_column(column) {}
            bool operator==(const Coordinates &o) const;
            bool operator!=(const Coordinates &o) const;
            bool operator<(const Coordinates &o) const;
            bool operator>(const Coordinates &o) const;
            bool operator<=(const Coordinates &o) const;
            bool operator>=(const Coordinates &o) const;
            Coordinates operator+(const Coordinates &o) const;
            Coordinates operator-(const Coordinates &o) const;
        };

        inline static const Coordinates Invalid = Coordinates(0x80000000, 0x80000000);

        struct Selection {
            Coordinates m_start;
            Coordinates m_end;

            Selection() = default;

            Selection(Coordinates start, Coordinates end) : m_start(start), m_end(end) {
                if (m_start > m_end) {
                    std::swap(m_start, m_end);
                }
            }

            Coordinates getSelectedLines();
            Coordinates getSelectedColumns();
            bool isSingleLine();
            bool contains(Coordinates coordinates, int8_t endsInclusive=1);
        };

        struct EditorState {
            Selection m_selection;
            Coordinates m_cursorPosition;
        };

        class FindReplaceHandler {
        public:
            FindReplaceHandler();
            typedef std::vector<EditorState> Matches;
            Matches &getMatches() { return m_matches; }
            bool findNext(TextEditor *editor);
            u32 findMatch(TextEditor *editor, bool isNex);
            bool replace(TextEditor *editor, bool right);
            bool replaceAll(TextEditor *editor);
            std::string &getFindWord() { return m_findWord; }

            void setFindWord(TextEditor *editor, const std::string &findWord) {
                if (findWord != m_findWord) {
                    findAllMatches(editor, findWord);
                    m_findWord = findWord;
                }
            }

            std::string &getReplaceWord() { return m_replaceWord; }
            void setReplaceWord(const std::string &replaceWord) { m_replaceWord = replaceWord; }
            void selectFound(TextEditor *editor, i32 found);
            void findAllMatches(TextEditor *editor, std::string findWord);
            u32 findPosition(TextEditor *editor, Coordinates pos, bool isNext);
            bool getMatchCase() const { return m_matchCase; }

            void setMatchCase(TextEditor *editor, bool matchCase) {
                if (matchCase != m_matchCase) {
                    m_matchCase = matchCase;
                    m_optionsChanged = true;
                    findAllMatches(editor, m_findWord);
                }
            }

            bool getWholeWord() const { return m_wholeWord; }
            void setWholeWord(TextEditor *editor, bool wholeWord) {
                if (wholeWord != m_wholeWord) {
                    m_wholeWord = wholeWord;
                    m_optionsChanged = true;
                    findAllMatches(editor, m_findWord);
                }
            }

            bool getFindRegEx() const { return m_findRegEx; }
            void setFindRegEx(TextEditor *editor, bool findRegEx) {
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


        typedef std::vector<std::pair<std::regex, PaletteIndex>> RegexList;

        struct Identifier {
            Coordinates m_location;
            std::string m_declaration;
        };

        using String = std::string;
        using Identifiers = std::unordered_map<std::string, Identifier>;
        using Keywords = std::unordered_set<std::string>;
        using ErrorMarkers = std::map<Coordinates, std::pair<u32, std::string>>;
        using Breakpoints = std::unordered_set<u32>;
        using Palette = std::array<ImU32, (u64) PaletteIndex::Max>;
        using Glyph = uint8_t;

        class ActionableBox {

            ImRect m_box;
        public:
            ActionableBox() = default;
            explicit ActionableBox(const ImRect &box) : m_box(box) {}

            virtual bool trigger() {
                return ImGui::IsMouseHoveringRect(m_box.Min, m_box.Max);
            }

            virtual void callback() {}
        };

        class CursorChangeBox : public ActionableBox {
        public:
            CursorChangeBox() = default;
            explicit CursorChangeBox(const ImRect &box) : ActionableBox(box) {}

            void callback() override {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }
        };

        class ErrorGotoBox : public ActionableBox {
            Coordinates m_pos;
        public:
            ErrorGotoBox() = default;

            ErrorGotoBox(const ImRect &box, const Coordinates &pos, TextEditor *editor) : ActionableBox(box), m_pos(pos), m_editor(editor) {}

            bool trigger() override {
                return ActionableBox::trigger() && ImGui::IsMouseClicked(0);
            }

            void callback() override {
                m_editor->jumpToCoords(m_pos);
            }

        private:
            TextEditor *m_editor;
        };

        using ErrorGotoBoxes = std::map<Coordinates, ErrorGotoBox>;
        using CursorBoxes = std::map<Coordinates, CursorChangeBox>;

        class ErrorHoverBox : public ActionableBox {
            Coordinates m_pos;
            std::string m_errorText;
        public:
            ErrorHoverBox() = default;
            ErrorHoverBox(const ImRect &box, const Coordinates &pos, const char *errorText) : ActionableBox(box), m_pos(pos), m_errorText(errorText) {}

            void callback() override {
                ImGui::BeginTooltip();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
                ImGui::Text("Error at line %d:", m_pos.m_line);
                ImGui::PopStyleColor();
                ImGui::Separator();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.2f, 1.0f));
                ImGui::TextUnformatted(m_errorText.c_str());
                ImGui::PopStyleColor();
                ImGui::EndTooltip();
            }
        };

        using ErrorHoverBoxes = std::map<Coordinates, ErrorHoverBox>;

        // A line of text in the pattern editor consists of three strings; the character encoding, the color encoding and the flags.
        // The char encoding is utf-8, the color encoding are indices to the color palette and the flags are used to override the colors
        // depending on priorities; e.g. comments, strings, etc.

        class Line {
        public:
            struct FlagBits {
                bool comment: 1;
                bool blockComment: 1;
                bool docComment: 1;
                bool blockDocComment: 1;
                bool globalDocComment: 1;
                bool deactivated: 1;
                bool preprocessor: 1;
                bool matchedBracket: 1;
            };

            union Flags {
                Flags(char value) : m_value(value) {}
                Flags(FlagBits bits) : m_bits(bits) {}
                FlagBits m_bits;
                char m_value;
            };

            constexpr static char inComment = 31;

            class LineIterator {
            public:
                strConstIter m_charsIter;
                strConstIter m_colorsIter;
                strConstIter m_flagsIter;

                LineIterator(const LineIterator &other) : m_charsIter(other.m_charsIter), m_colorsIter(other.m_colorsIter), m_flagsIter(other.m_flagsIter) {}

                LineIterator() = default;

                char operator*();
                LineIterator operator++();
                LineIterator operator=(const LineIterator &other);
                bool operator!=(const LineIterator &other) const;
                bool operator==(const LineIterator &other) const;
                LineIterator operator+(i32 n);
                i32 operator-(LineIterator l);
            };

            enum class LinePart {
                Chars,
                Utf8,
                Colors,
                Flags
            };

            LineIterator begin() const;
            LineIterator end() const;

            std::string m_chars;
            std::string m_colors;
            std::string m_flags;
            bool m_colorized = false;

            Line() : m_chars(), m_colors(), m_flags(), m_colorized(false) {}
            explicit Line(const char *line) { Line(std::string(line)); }
            explicit Line(const std::string &line) : m_chars(line), m_colors(std::string(line.size(), 0x00)), m_flags(std::string(line.size(), 0x00)), m_colorized(false) {}
            Line(const Line &line) : m_chars(line.m_chars), m_colors(line.m_colors), m_flags(line.m_flags), m_colorized(line.m_colorized) {}

            i32 getCharacterColumn(i32 index) const;
            LineIterator begin();
            LineIterator end();
            Line &operator=(const Line &line);
            Line &operator=(Line &&line) noexcept;
            u64 size() const;
            char front(LinePart part = LinePart::Chars) const;
            std::string frontUtf8(LinePart part = LinePart::Chars) const;
            void push_back(char c);
            bool empty() const;
            std::string substr(u64 start, u64 length = (u64) -1, LinePart part = LinePart::Chars) const;
            char operator[](u64 index) const;
            // C++ can't overload functions based on return type, so use any type other
            // than u64 to avoid ambiguity.
            std::string operator[](i64 column) const;
            void setNeedsUpdate(bool needsUpdate);
            void append(const char *text);
            void append(const char text);
            void append(const std::string &text);
            void append(const Line &line);
            void append(LineIterator begin, LineIterator end);
            void insert(LineIterator iter, const std::string &text);
            void insert(LineIterator iter, const char text);
            void insert(LineIterator iter, strConstIter beginString, strConstIter endString);
            void insert(LineIterator iter, const Line &line);
            void insert(LineIterator iter, LineIterator beginLine, LineIterator endLine);
            void erase(LineIterator begin);
            void erase(LineIterator begin, u64 count);
            void erase(u64 start, u64 length = (u64) -1);
            void clear();
            void setLine(const std::string &text);
            void setLine(const Line &text);
            bool needsUpdate() const;
        };

        using Lines = std::vector<Line>;

        struct LanguageDefinition {
            typedef std::pair<std::string, PaletteIndex> TokenRegexString;
            typedef std::vector<TokenRegexString> TokenRegexStrings;

            typedef bool(*TokenizeCallback)(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin,
                                            strConstIter &out_end, PaletteIndex &paletteIndex);

            std::string m_name;
            Keywords m_keywords;
            Identifiers m_identifiers;
            Identifiers m_preprocIdentifiers;
            std::string m_singleLineComment, m_commentEnd, m_commentStart, m_globalDocComment, m_docComment, m_blockDocComment;
            char m_preprocChar;
            bool m_autoIndentation;
            TokenizeCallback m_tokenize;
            TokenRegexStrings m_tokenRegexStrings;
            bool m_caseSensitive;

            LanguageDefinition() : m_name(""), m_keywords({}), m_identifiers({}), m_preprocIdentifiers({}),
                                   m_singleLineComment(""), m_commentEnd(""),
                                   m_commentStart(""), m_globalDocComment(""), m_docComment(""), m_blockDocComment(""),
                                   m_preprocChar('#'), m_autoIndentation(true), m_tokenize(nullptr),
                                   m_tokenRegexStrings({}), m_caseSensitive(true) {}

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

    private:
        class UndoRecord {
        public:
            UndoRecord() {}

            ~UndoRecord() {}

            UndoRecord( const std::string &added,
                        const TextEditor::Selection addedSelection,
                        const std::string &removed,
                        const TextEditor::Selection removedSelection,
                        TextEditor::EditorState &before,
                        TextEditor::EditorState &after);

            void undo(TextEditor *editor);
            void redo(TextEditor *editor);

            std::string m_added;
            Selection m_addedSelection;
            std::string m_removed;
            Selection m_removedSelection;
            EditorState m_before;
            EditorState m_after;
        };

        typedef std::vector<UndoRecord> UndoBuffer;

        struct MatchedBracket {
            bool m_active = false;
            bool m_changed = false;
            Coordinates m_nearCursor = {};
            Coordinates m_matched = {};
            static const std::string s_separators;
            static const std::string s_operators;

            MatchedBracket(const MatchedBracket &other) : m_active(other.m_active), m_changed(other.m_changed),
                                                          m_nearCursor(other.m_nearCursor),
                                                          m_matched(other.m_matched) {}

            MatchedBracket() : m_active(false), m_changed(false), m_nearCursor(0, 0), m_matched(0, 0) {}
            MatchedBracket(bool active, bool changed, const Coordinates &nearCursor, const Coordinates &matched)
                    : m_active(active), m_changed(changed), m_nearCursor(nearCursor), m_matched(matched) {}

            bool checkPosition(TextEditor *editor, const Coordinates &from);
            bool isNearABracket(TextEditor *editor, const Coordinates &from);
            i32 detectDirection(TextEditor *editor, const Coordinates &from);
            void findMatchingBracket(TextEditor *editor);
            bool isActive() const { return m_active; }
            bool hasChanged() const { return m_changed; }
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

        inline void setShowCursor(bool value) { m_showCursor = value; }
        inline void setShowLineNumbers(bool value) { m_showLineNumbers = value; }
        inline void setShowWhitespaces(bool value) { m_showWhitespaces = value; }
        inline bool isShowingWhitespaces() const { return m_showWhitespaces; }
        inline i32 getTabSize() const { return m_tabSize; }
        ImVec2 &getCharAdvance() { return m_charAdvance; }
        void clearGotoBoxes() { m_errorGotoBoxes.clear(); }
        void clearCursorBoxes() { m_cursorBoxes.clear(); }
        void addClickableText(std::string text) { m_clickableText.push_back(text); }
        void setErrorMarkers(const ErrorMarkers &markers) { m_errorMarkers = markers; }
        Breakpoints &getBreakpoints() { return m_breakpoints; }
        void setBreakpoints(const Breakpoints &markers) { m_breakpoints = markers; }
        void setLongestLineLength(u64 line) { m_longestLineLength = line; }
        u64 getLongestLineLength() const { return m_longestLineLength; }
        void setTopMarginChanged(i32 newMargin);
        void setFocusAtCoords(const Coordinates &coords);
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
        float textDistanceToLineStart(const Coordinates &from) const;
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
        void setNeedsUpdate(u32 line, bool needsUpdate);
        void setColorizedLine(u64 line, const std::string &tokens);
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
        void undo(i32 steps = 1);
        void redo(i32 steps = 1);
        void copy();
        void cut();
        void paste();
        void deleteChar();
        void insertText(const std::string &value);
        void insertText(const char *value);
        void appendLine(const std::string &value);
        void setOverwrite(bool value) { m_overwrite = value; }
        bool isOverwrite() const { return m_overwrite; }
        void setText(const std::string &text, bool undo = false);
        std::string getText() const;
        std::vector<std::string> getTextLines() const;
        std::string getSelectedText() const;
        std::string getLineText(i32 line) const;
        inline void setTextChanged(bool value) { m_textChanged = value; }
        inline bool isTextChanged() { return m_textChanged; }
        inline void setReadOnly(bool value) { m_readOnly = value; }
        inline void setHandleMouseInputs(bool value) { m_handleMouseInputs = value; }
        inline bool isHandleMouseInputsEnabled() const { return m_handleKeyboardInputs; }
        inline void setHandleKeyboardInputs(bool value) { m_handleKeyboardInputs = value; }
        inline bool isHandleKeyboardInputsEnabled() const { return m_handleKeyboardInputs; }
    private:
        std::string getText(const Selection &selection) const;
        void deleteRange(const Selection &selection);
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
        Coordinates getCursorPosition() const { return setCoordinates(m_state.m_cursorPosition); }
        void setCursorPosition(const Coordinates &position);
        void setCursorPosition();
    private:
        Coordinates setCoordinates(const Coordinates &value) const;
        Coordinates setCoordinates(i32 line, i32 column) const;
        Selection setCoordinates(const Selection &value) const;
        void advance(Coordinates &coordinates) const;
        Coordinates findWordStart(const Coordinates &from) const;
        Coordinates findWordEnd(const Coordinates &from) const;
        Coordinates findPreviousWord(const Coordinates &from) const;
        Coordinates findNextWord(const Coordinates &from) const;
        u32 skipSpaces(const Coordinates &from);
//Support
    public:
        void setSelection(const Selection &selection);
        Selection getSelection() const;
        void selectWordUnderCursor();
        void selectAll();
        bool hasSelection() const;
        void refreshSearchResults();
        i32 getTotalLines() const { return (i32) m_lines.size(); }
        FindReplaceHandler *getFindReplaceHandler() { return &m_findReplaceHandler; }
        void setSourceCodeEditor(TextEditor *editor) { m_sourceCodeEditor = editor; }
        inline void clearBreakpointsChanged() { m_breakPointsChanged = false; }
        inline bool isBreakpointsChanged() { return m_breakPointsChanged; }
        inline void setImGuiChildIgnored(bool value) { m_ignoreImGuiChild = value; }
        inline bool isImGuiChildIgnored() const { return m_ignoreImGuiChild; }
        bool raiseContextMenu() { return m_raiseContextMenu; }
        void clearRaiseContextMenu() { m_raiseContextMenu = false; }
        TextEditor *GetSourceCodeEditor();
        bool isEmpty() const;
    private:
        void addUndo(UndoRecord &value);
        TextEditor::PaletteIndex getColorIndexFromFlags(Line::Flags flags);
        void handleKeyboardInputs();
        void handleMouseInputs();
// utf8
    public:
        static i32 imTextCharToUtf8(char *buffer, i32 buf_size, u32 c);
        static void imTextCharToUtf8(std::string &buffer, u32 c);
        static i32 utf8CharLength(uint8_t c);
        static i32 getStringCharacterCount(const std::string &str);
        static TextEditor::Coordinates stringIndexToCoordinates(i32 strIndex, const std::string &input);
    private:

        Coordinates screenPosToCoordinates(const ImVec2 &position) const;
        Coordinates lineCoordsToIndexCoords(const Coordinates &coordinates) const;
        i32 lineCoordinatesToIndex(const Coordinates &coordinates) const;
        Coordinates getCharacterCoordinates(i32 line, i32 index) const;
        i32 getLineCharacterCount(i32 line) const;
        u64 getLineByteCount(i32 line) const;
        i32 getLineMaxColumn(i32 line) const;

    public:
        FindReplaceHandler m_findReplaceHandler;
    private:
        float m_lineSpacing = 1.0F;
        Lines m_lines;
        EditorState m_state = {};
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

        MatchedBracket m_matchedBracket = {};
        Palette m_palette = {};
        LanguageDefinition m_languageDefinition = {};
        RegexList m_regexList;
        bool m_updateFlags = true;
        Breakpoints m_breakpoints = {};
        ErrorMarkers m_errorMarkers = {};
        ErrorHoverBoxes m_errorHoverBoxes = {};
        ErrorGotoBoxes m_errorGotoBoxes = {};
        CursorBoxes m_cursorBoxes = {};
        ImVec2 m_charAdvance = {};
        Selection m_interactiveSelection = {};
        u64 m_startTime = 0;
        std::vector<std::string> m_defines;
        TextEditor *m_sourceCodeEditor = nullptr;
        float m_shiftedScrollY = 0;
        float m_scrollYIncrement = 0.0F;
        bool m_setScrollY = false;
        float m_numberOfLinesDisplayed = 0;
        float m_lastClick = -1.0F;
        bool m_showCursor = true;
        bool m_showLineNumbers = true;
        bool m_raiseContextMenu = false;
        Coordinates m_focusAtCoords = {};
        bool m_updateFocus = false;

        std::vector<std::string> m_clickableText;

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