#pragma once

#include <string>
#include <utility>
#include <vector>
#include <array>
#include <memory>
#include <functional>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <atomic>
#include <regex>
#include <chrono>
#include <iostream>
#include <set>
#include "imgui.h"
#include "imgui_internal.h"
#include <hex/helpers/utils.hpp>
#include <hex/helpers/types.hpp>
#include <pl/core/token.hpp>
#include <pl/helpers/safe_iterator.hpp>
#include <pl/core/location.hpp>

namespace hex::ui {
    using strConstIter = std::string::const_iterator;

    class TextEditor {
    public:
        class Interval;
        class Lines;

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
                Coordinates sanitize(Lines &lines);
                bool isValid(Lines &lines);
                bool operator==(const Coordinates &o) const;
                bool operator!=(const Coordinates &o) const;
                bool operator<(const Coordinates &o) const;
                bool operator>(const Coordinates &o) const;
                bool operator<=(const Coordinates &o) const;
                bool operator>=(const Coordinates &o) const;
                Coordinates operator+(const Coordinates &o) const;
                Coordinates operator-(const Coordinates &o) const;
                [[nodiscard]] i32 getLine() const { return m_line; }
                [[nodiscard]] i32 getColumn() const { return m_column; }

            private:
                i32 m_line, m_column;
            };

            Range() = default;
            explicit Range(std::pair<Coordinates,Coordinates> coords) : m_start(coords.first), m_end(coords.second) {
                if (m_start > m_end) { std::swap(m_start, m_end); }}
            Range(Coordinates start, Coordinates end) : m_start(start), m_end(end) {
                if (m_start > m_end) { std::swap(m_start, m_end); }}
            explicit Range(const Interval &lines) : m_start(Coordinates(lines.m_start, 0)), m_end(Coordinates(lines.m_end, 0)) {
                if (m_start > m_end) { std::swap(m_start, m_end); }}


            Coordinates getSelectedLines();
            Coordinates getSelectedColumns();
            Coordinates getStart() { return m_start; }
            Coordinates getEnd() { return m_end; }
            bool isSingleLine();
            enum class EndsInclusive : u8 { None = 0, Start = 2, End = 1, Both = 3 };
            [[nodiscard]] bool contains(const Coordinates &coordinates, EndsInclusive endsInclusive = EndsInclusive::Both) const;
            [[nodiscard]] bool contains(const Range &range, EndsInclusive endsInclusive = EndsInclusive::Both) const;
            [[nodiscard]] bool containsLine(i32 value, EndsInclusive endsInclusive = EndsInclusive::Both) const;
            [[nodiscard]] bool containsColumn(i32 value, EndsInclusive endsInclusive = EndsInclusive::Both) const;
            [[nodiscard]] bool overlaps(const Range &o, EndsInclusive endsInclusive = EndsInclusive::Both) const;
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

        class Interval {
        public:
            friend class TextEditor;
            friend class Range;
            Interval() : m_start(0), m_end(0) {}
            Interval(i32 start, i32 end) : m_start(start), m_end(end) {
                if (m_start > m_end) std::swap(m_start, m_end);}
            Interval(const Interval &other) = default;
            explicit Interval(ImVec2 vec) : m_start((i32)vec.x), m_end((i32)vec.y) {
                if (m_start > m_end) std::swap(m_start, m_end);}

            Interval &operator=(const Interval &interval);
            Interval &operator=(Interval &&interval) noexcept;
            bool operator<(const Interval &other) const { return other.m_end > m_end; }
            bool operator>(const Interval &other) const { return m_end > other.m_end; }
            bool operator==(const Interval &other) const { return m_start == other.m_start && m_end == other.m_end; }
            bool operator!=(const Interval &other) const { return m_start != other.m_start || m_end != other.m_end; }
            bool operator<=(const Interval &other) const { return other.m_end >= m_end; }
            bool operator>=(const Interval &other) const { return m_end >= other.m_end; }
            [[nodiscard]] bool contains_or_equals(const Interval &other) const { return other.m_start >= m_start && other.m_end <= m_end; }
            [[nodiscard]] bool contains(const Interval &other) const { return (other.m_start >= m_start && other.m_end < m_end) || (other.m_start > m_start && other.m_end <= m_end); }
            [[nodiscard]] bool contains(i32 value, bool inclusive = true) const;
            [[nodiscard]] bool contiguous(const Interval &other) const;
            explicit operator ImVec2() const { return {(float)m_start, (float)m_end}; }
        private:
            i32 m_start;
            i32 m_end;
        };

        class EditorState {
        public:
            friend class TextEditor;
            bool operator==(const EditorState &o) const;
            EditorState() = default;
            EditorState(Range selection, Coordinates cursorPosition) : m_selection(selection), m_cursorPosition(cursorPosition) {}
        private:
            Range m_selection;
            Coordinates m_cursorPosition;
        };

        class UndoRecord;

        using Matches       = std::vector<EditorState>;
        using UndoRecords   = std::vector<UndoRecord>;

        class FindReplaceHandler {
        public:
            FindReplaceHandler();
            Matches &getMatches() { return m_matches; }
            bool findNext(Lines *lines, u64 &byteIndex);
            bool findNext(TextEditor *editor, u64 &byteIndex) { return findNext(&editor->m_lines, byteIndex); };
            u32 findMatch(Lines *lines, i32 index);
            u32 findMatch(TextEditor *editor, i32 index) { return findMatch(&editor->m_lines, index); };
            bool replace(Lines *lines, bool right);
            bool replace(TextEditor *editor, bool right) { return replace(&editor->m_lines, right); };
            bool replaceAll(Lines *lines);
            bool replaceAll(TextEditor *editor) { return replaceAll(&editor->m_lines); };
            std::string &getFindWord() { return m_findWord; }
            void setFindWord(Lines *lines, const std::string &findWord);
            void setFindWord(TextEditor *editor, const std::string &findWord) { setFindWord(&editor->m_lines, findWord); };
            std::string &getReplaceWord() { return m_replaceWord; }
            void setReplaceWord(const std::string &replaceWord) { m_replaceWord = replaceWord; }
            void selectFound(Lines *lines, i32 found);
            void selectFound(TextEditor *editor, i32 found) { selectFound(&editor->m_lines, found); };
            void findAllMatches(Lines *lines, std::string findWord);
            void findAllMatches(TextEditor *editor, std::string findWord) { findAllMatches(&editor->m_lines, findWord); };
            u32 findPosition(Lines *lines, Coordinates pos, bool isNext);
            u32 findPosition(TextEditor *editor, Coordinates pos, bool isNext) { return findPosition(&editor->m_lines, pos, isNext); };
            [[nodiscard]] bool getMatchCase() const { return m_matchCase; }
            void setMatchCase(Lines *lines, bool matchCase);
            void setMatchCase(TextEditor *editor, bool matchCase) { setMatchCase(&editor->m_lines, matchCase); };
            [[nodiscard]] bool getWholeWord() const { return m_wholeWord; }
            void setWholeWord(Lines *lines, bool wholeWord);
            void setWholeWord(TextEditor *editor, bool wholeWord) { setWholeWord(&editor->m_lines, wholeWord); };
            [[nodiscard]] bool getFindRegEx() const { return m_findRegEx; }
            void setFindRegEx(Lines *lines, bool findRegEx);
            void setFindRegEx(TextEditor *editor, bool findRegEx) { setFindRegEx(&editor->m_lines, findRegEx); };
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

        enum class PaletteIndex : u8 {
            Default, Identifier, Directive, Operator, Separator, BuiltInType, Keyword, NumericLiteral, StringLiteral, CharLiteral, Cursor, Background, LineNumber, Selection, Breakpoint, ErrorMarker, PreprocessorDeactivated,
            CurrentLineFill, CurrentLineFillInactive, CurrentLineEdge, ErrorText, WarningText, DebugText, DefaultText, Attribute, PatternVariable, LocalVariable, CalculatedPointer, TemplateArgument, Function, View,
            FunctionVariable, FunctionParameter, UserDefinedType, PlacedVariable, GlobalVariable, NameSpace, TypeDef, UnkIdentifier, DocComment, DocBlockComment, BlockComment, GlobalDocComment, Comment, PreprocIdentifier, Max
        };

        using Token             = pl::core::Token;
        using Tokens            = std::vector<Token>;
        using SafeTokenIterator = pl::hlp::SafeIterator<Tokens::const_iterator>;
        using Location          = pl::core::Location;
        using RegexList         = std::vector<std::pair<std::regex, PaletteIndex>>;
        using Keywords          = std::unordered_set<std::string>;
        using ErrorMarkers      = std::map<Coordinates, std::pair<i32, std::string>>;
        using Breakpoints       = std::unordered_set<u32>;
        using Palette           = std::array<ImU32, (u64) PaletteIndex::Max>;
        using Glyph             = u8;
        using CodeFoldBlocks    = std::map<Coordinates,Coordinates>;
        using GlobalBlocks      = std::set<Interval>;

        struct Identifier { Coordinates m_location; std::string m_declaration;};
        using Identifiers   = std::unordered_map<std::string, Identifier>;

        class ActionableBox {
        public:
            ActionableBox() : m_box(ImRect(ImVec2(0, 0), ImVec2(0, 0))) {};
            explicit ActionableBox(const ImRect &box) : m_box(box) {}

            [[nodiscard]] ImRect &getBox() const { return const_cast<ImRect &>(m_box);}
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


        class CodeFold : public ActionableBox {
        public:
            CodeFold() = default;
            explicit CodeFold(Lines *lines,  Range keys, const ImRect &startBox, const ImRect &endBox);

            bool trigger() override;
            void callback() override;
            bool startHovered();
            bool endHovered();
            bool isDetected();
            [[nodiscard]] bool isOpen() const;
            void open();
            void close();
            void moveFold(float lineCount, float lineHeight);
        private:
            Lines *lines = nullptr;
            Range key;
            CursorChangeBox codeFoldStartCursorBox;
            ActionableBox codeFoldEndActionBox;
            CursorChangeBox codeFoldEndCursorBox;
        };

        class CodeFoldTooltip : public ActionableBox {
        public:
            CodeFoldTooltip(Lines *lines,const  Range &key, const ImRect &box) : ActionableBox(box), m_lines(lines),  m_key(key) {}

            bool trigger() override;
            void callback() override;
        private:
            Lines *m_lines;
            Range m_key;
            const std::string popupLineNumbers = "##popupLineNumbers";
            const std::string popupText = "##popupText";
        };

        using ErrorGotoBoxes        = std::map<Coordinates, ErrorGotoBox>;
        using CursorBoxes           = std::map<Coordinates, CursorChangeBox>;
        using ErrorHoverBoxes       = std::map<Coordinates, ErrorHoverBox>;
        using Keys                  = std::vector<Range>;
        using CodeFoldKeys          = std::set<Range>;
        using CodeFoldDelimiters    = std::map<Range,std::pair<char,char>>;
        using Segments              = std::vector<Coordinates>;
        using CodeFoldKeyMap        = std::map<Coordinates,Coordinates>;
        using CodeFoldKeyLineMap    = std::multimap<i32,Coordinates>;
        using CodeFoldState         = std::map<Range,bool>;
        using Indices               = std::vector<i32>;
        using LineIndexToScreen     = std::map<i32,ImVec2>;
        using IndexMap              = std::map<i32,i32>;
        using RowCodeFoldTooltips   = std::map<i32,std::vector<CodeFoldTooltip>>;

        enum class TrimMode : u8 { TrimNone = 0, TrimEnd = 1, TrimStart = 2, TrimBoth = 3 };

        // A line of text in the pattern editor consists of three strings; the character encoding, the color encoding and the flags.
        // The char encoding is utf-8, the color encoding are indices to the color palette and the flags are used to override the colors
        // depending on priorities; e.g. comments, strings, etc.

        class LineIterator {
        public:
            friend class hex::ui::TextEditor;
            friend class hex::ui::TextEditor::Lines;
            LineIterator(const LineIterator &other) = default;
            LineIterator() = default;

            char operator*();
            LineIterator &operator++();
            LineIterator &operator=(const LineIterator &other);
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
                explicit Flags(char value) : m_value(value) {}
                explicit Flags(FlagBits bits) : m_bits(bits) {}
                FlagBits m_bits;
                char m_value;
            };

            enum class LinePart : u8 { Chars, Utf8, Colors, Flags };

            Line() : m_lineMaxColumn(-1) {}
            explicit Line(const char *line) : Line(std::string(line)) {}
            explicit Line(const std::string &line) : m_chars(line), m_colors(std::string(line.size(), 0x00)), m_flags(std::string(line.size(), 0x00)), m_lineMaxColumn(maxColumn()) {}
            Line(const Line &line) : m_chars(std::string(line.m_chars)), m_colors(std::string(line.m_colors)), m_flags(std::string(line.m_flags)), m_colorized(line.m_colorized), m_lineMaxColumn(line.m_lineMaxColumn) {}
            Line(Line &&line) noexcept : m_chars(std::move(line.m_chars)), m_colors(std::move(line.m_colors)), m_flags(std::move(line.m_flags)), m_colorized(line.m_colorized), m_lineMaxColumn(line.m_lineMaxColumn) {}
            Line(std::string chars, std::string colors, std::string flags) : m_chars(std::move(chars)), m_colors(std::move(colors)), m_flags(std::move(flags)), m_lineMaxColumn(maxColumn()) {}

            bool operator==(const Line &o) const;
            bool operator!=(const Line &o) const;
            [[nodiscard]] i32 indexColumn(i32 stringIndex) const;
            [[nodiscard]] i32 maxColumn();
            [[nodiscard]] i32 maxColumn() const;
            [[nodiscard]] i32 columnIndex(i32 column) const;
            [[nodiscard]] i32 textSize() const;
            [[nodiscard]] i32 textSize(u32 index) const;
            [[nodiscard]] ImVec2 intervalToScreen(Interval stringIndices) const;
            i32 lineTextSize(TrimMode trimMode = TrimMode::TrimNone);
            [[nodiscard]] i32 stringTextSize(const std::string &str) const;
            i32 textSizeIndex(float textSize, i32 position);
            Line trim(TrimMode trimMode = TrimMode::TrimNone);
            void print(i32 lineIndex, i32 maxLineIndex, std::optional<ImVec2> pos = std::nullopt);
            u32 skipSpaces(i32 index);
            [[nodiscard]] LineIterator begin() const;
            [[nodiscard]] LineIterator end() const;
            LineIterator begin();
            LineIterator end();
            Line &operator=(const Line &line);
            Line &operator=(Line &&line) noexcept;
            [[nodiscard]] u64 size() const;
            [[nodiscard]] char front(LinePart part = LinePart::Chars) const;
            [[nodiscard]] std::string frontUtf8(LinePart part = LinePart::Chars) const;
            void push_back(char c);
            [[nodiscard]] bool empty() const;
            [[nodiscard]] std::string substr(u64 start, u64 length = (u64) -1, LinePart part = LinePart::Chars) const;
            Line subLine(u64 start, u64 length = (u64) -1);
            char operator[](u64 index) const;
            [[nodiscard]] char at(u64 index) const;
            // C++ can't overload functions based on return type, so use any type other
            // than u64 to avoid ambiguity.
            std::string operator[](i64 column) const;
            [[nodiscard]] std::string at(i64 column) const;
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

        class FoldedLine {
        public:
            friend class Lines;
            friend class TextEditor;
            enum  class FoldType : u8 {
                NoDelimiters = 0,
                AddsFirstLine = 1,           // does not imply having open delimiter (ex. import).
                HasOpenDelimiter = 2,       // implies first line is added.
                FirstLineNeedsDelimiter = 4,   // implies having open delimiter. Needed when open delimiter is not on first line..
                AddsLastLine = 8,
                HasCloseDelimiter = 16,
                Invalid = 32
            };

            FoldedLine() = default;
            explicit FoldedLine(Lines *lines);

            void insertKey(const Range &key);
            void removeKey(const Range &key);
            void loadSegments();
            bool firstLineNeedsDelimiter();
            bool addsLastLineToFold();
            bool addsFullFirstLineToFold();
            Range findDelimiterCoordinates(Range key);
        private:
            Line m_foldedLine;
            Lines *m_lines{};
            Range m_full;
            i32 m_row{};
            FoldType m_type = FoldType::Invalid;
            std::string m_brackets;
            Keys m_keys;
            Keys m_keysToRemove;
            Indices m_ellipsisIndices;
            Segments m_foldedSegments;
            Segments m_unfoldedSegments;
            Coordinates m_cursorPosition;
            Range m_selection;
            bool m_built = false;
        };

        class MatchedDelimiter {
        public:
            friend class Lines;
            friend class TextEditor;
            MatchedDelimiter() : m_nearCursor(0, 0), m_matched(0, 0) {}
            MatchedDelimiter(bool active, bool changed, const Coordinates &nearCursor, const Coordinates &matched) : m_active(active), m_changed(changed), m_nearCursor(nearCursor), m_matched(matched) {}

            bool checkPosition(Lines *lines, Coordinates &from);
            bool setNearCursor(Lines *lines, const Coordinates &from);
            bool coordinatesNearDelimiter(Lines *lines, Coordinates &from);
            i32 detectDirection(Lines *lines, const Coordinates &from);
            void findMatchingDelimiter(Lines *lines, bool folded = true);
            Coordinates findMatchingDelimiter(Lines *lines, Coordinates &from, bool folded = true);
            [[nodiscard]] bool isActive() const { return m_active; }
            [[nodiscard]] bool hasChanged() const { return m_changed; }
        private:
            bool m_active = false;
            bool m_changed = false;
            Coordinates m_nearCursor{};
            Coordinates m_matched{};
        };

        class FoldSegment {
        public:
            friend class TextEditor;
            FoldSegment(Coordinates foldEnd, const Interval &segment) : m_foldEnd(foldEnd), m_segment(segment) {}
            bool operator==(const FoldSegment &o) const {
                return m_foldEnd == o.m_foldEnd && m_segment == o.m_segment;
            }
        private:
            Coordinates m_foldEnd;
            Interval m_segment;
        };

        struct LanguageDefinition {
            using TokenRegexString = std::pair<std::string, PaletteIndex>;
            using TokenRegexStrings = std::vector<TokenRegexString>;
            using TokenizeCallback = bool (*)(strConstIter, strConstIter, strConstIter &, strConstIter &, PaletteIndex &);

            std::string m_name;
            Keywords m_keywords;
            Identifiers m_identifiers;
            Identifiers m_preprocIdentifiers;
            std::string m_singleLineComment, m_commentEnd, m_commentStart, m_globalDocComment, m_docComment, m_blockDocComment;
            char m_preprocChar = '#';
            bool m_autoIndentation = true;
            TokenizeCallback m_tokenize = {};
            TokenRegexStrings m_tokenRegexStrings;
            bool m_caseSensitive = true;

            LanguageDefinition() : m_keywords({}), m_identifiers({}), m_preprocIdentifiers({}), m_tokenRegexStrings({}) {}

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
            ~UndoRecord() = default;
            UndoRecord( std::string added,
                        Range addedRange,
                        std::string removed,
                        Range removedRange,
                        EditorState before,
                        EditorState after);

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
            ~UndoAction() = default;
            explicit UndoAction(UndoRecords records) : m_records(std::move(records)) {}
            void undo(TextEditor *editor);
            void redo(TextEditor *editor);
        private:
            UndoRecords m_records;
        };

        class HiddenLine {
        public:
            friend class Lines;
            HiddenLine() = default;
            HiddenLine(i32 lineIndex, std::string lineContent) : m_lineIndex(lineIndex), m_line(std::move(lineContent)) {}
        private:
            i32 m_lineIndex{};
            std::string m_line;
        };

        enum class FoldSymbol : u8 { Line, Up, Down, Square };

        using CodeFolds             = std::map<Range, CodeFold>;
        using FoldSegments          = std::vector<FoldSegment>;
        using RowToFoldSegments     = std::map<i32, FoldSegments>;
        using UndoBuffer            = std::vector<UndoAction>;
        using FoldedLines           = std::map<i32, FoldedLine>;
        using UnfoldedLines         = std::vector<Line>;
        using HiddenLines           = std::vector<HiddenLine>;
        using FoldSymbols           = std::map<i32, FoldSymbol>;
        using StringVector          = std::vector<std::string>;
        using RangeFromCoordinates  = std::pair<Coordinates, Coordinates>;

        bool areEqual(const std::pair<Range,CodeFold> &a, const std::pair<Range,CodeFold> &b);

        class Lines {
        public:
            friend class TextEditor;
            Lines() : m_unfoldedLines({}), m_foldedLines({}), m_hiddenLines({}), m_defines({}) {}

            Line &at(i32 index);
            Line &operator[](i32 index);
            i32 size() const;
            void colorizeRange();
            void colorizeInternal();
            bool isEmpty();
            void moveToMatchedDelimiter(bool select = false);
            bool isTrueMatchingDelimiter();
            void clearErrorMarkers();
            void clearActionables();
            bool isEndOfLine(const Coordinates &coordinates);
            bool isEndOfFile(const Coordinates &coordinates);
            bool isEndOfLine();
            bool isStartOfLine() const;
            bool lineNeedsDelimiter(i32 lineIndex);
            FindReplaceHandler *getFindReplaceHandler() { return &m_findReplaceHandler; }
            void clearGotoBoxes() { m_errorGotoBoxes.clear(); }
            void clearCursorBoxes() { m_cursorBoxes.clear(); }
            void clearCodeFolds();
            void addClickableText(std::string text) { m_clickableText.push_back(text); }
            Breakpoints &getBreakpoints() { return m_breakpoints; }
            void saveCodeFoldStates();
            void applyCodeFoldStates();
            float lineIndexToRow(i32 lineNumber);
            float rowToLineIndex(i32 row);
            void getRowSegments();
            void initializeCodeFolds();
            bool updateCodeFolds();
            ImRect getBoxForRow(u32 lineNumber);
            void setFirstRow();
            i32 lineMaxColumn(i32 lineNumber);
            Interval indexScreenPosition(i32 lineIndex, Interval stringIndices);
            bool isMultiLineRow(i32 row);
            void enableCodeFolds(bool enable) { m_codeFoldsDisabled = !enable; };
            void moveRight(i32 amount, bool select, bool wordMode);
            void moveLeft(i32 amount, bool select, bool wordMode);
            void moveDown(i32 amount, bool select);
            void moveUp(i32 amount, bool select);
            void moveHome(bool select);
            void moveEnd(bool select);
            Keys removeEmbeddedFolds();
            bool isLastLine(i32 lineIndex);
            bool isLastLine();
            Coordinates find(const std::string &text, const Coordinates &start);
            Coordinates rfind(const std::string &text, const Coordinates &start);
            void setRowToLineIndexMap();
            Coordinates lineCoordinates(const Coordinates &coordinates);
            Coordinates lineCoordinates(i32 lineIndex, i32 column);
            Range lineCoordinates(const Range &value);
            friend bool Range::Coordinates::isValid(Lines &lines);
            friend TextEditor::Coordinates Range::Coordinates::sanitize(Lines &lines);
            void appendLine(const std::string &value);
            void removeHiddenLinesFromPattern();
            void addHiddenLinesToPattern();
            void setSelection(const Range &selection);
            Range getSelection() const;
            ImVec2 getLineStartScreenPos(float leftMargin, float lineNumber);
            ImVec2 &getCharAdvance() { return m_charAdvance; }
            Keys getDeactivatedBlocks();
            std::string getSelectedText();
            void deleteSelection();
            void setTextChanged(bool value) { m_textChanged = value; }
            bool isTextChanged() { return m_textChanged; }
            void setLanguageDefinition(const LanguageDefinition &aLanguageDef);
            const LanguageDefinition &getLanguageDefinition() const { return m_languageDefinition; }
            TextEditor::PaletteIndex getColorIndexFromFlags(Line::Flags flags);
            void insertLine(i32 index, const std::string &text);
            Coordinates lineIndexCoords(i32 lineNumber, i32 stringIndex);
            void colorize();
            i32 insertTextAt(Coordinates &where, const std::string &value);
            float getMaxDisplayedRow();
            float getGlobalRowMax();
            ImVec2 foldedCoordsToScreen(Coordinates coordinates);
            i32 lineCoordsIndex(const Coordinates &coordinates);
            u32 skipSpaces(const Coordinates &from);
            void closeCodeFold(const Range &key, bool userTriggered = false);
            void openCodeFold(const Range &key);
            void removeKeys();
            ImVec2 indexCoordsToScreen(Coordinates indexCoords);
            void setImGuiChildIgnored(bool value) { m_ignoreImGuiChild = value; }
            bool isImGuiChildIgnored() const { return m_ignoreImGuiChild; }
            ImVec2 lineIndexToScreen(i32 lineIndex, Interval stringIndices);
            void printCodeFold(const Range &key);
            void resetCursorBlinkTime();
            void setUnfoldIfNeeded(bool unfoldIfNeeded) {m_unfoldIfNeeded = unfoldIfNeeded;}
            std::string getRange(const Range &rangeToGet);
            void setCursorPosition(const Coordinates &position, bool unfoldIfNeeded = true, bool scrollToCursor = true);
            void setFocusAtCoords(const Coordinates &coords, bool ensureVisible = false);
            void ensureCursorVisible();
            Segments unfoldedEllipsisCoordinates(Range delimiterCoordinates);
            Coordinates unfoldedToFoldedCoords(const Coordinates &coords);
            Coordinates foldedToUnfoldedCoords(const Coordinates &coords);
            void setScrollY();
            Coordinates findPreviousWord(const Coordinates &from);
            Coordinates findNextWord(const Coordinates &from);
            Line &insertLine(i32 index);
            void deleteRange(const Range &rangeToDelete);
            void clearBreakpointsChanged() { m_breakPointsChanged = false; }
            bool isBreakpointsChanged() { return m_breakPointsChanged; }
            Coordinates stringIndexCoords(i32 strIndex, const std::string &input);
            void refreshSearchResults();
            void setReadOnly(bool value) { m_readOnly = value; }
            void removeLines(i32 start, i32 end);
            void removeLine(i32 index);
            float textDistanceToLineStart(const Coordinates &from);
            std::string getText();
            void setCursorPosition();
            void ensureSelectionNotFolded();
            bool hasSelection();
            i32 insertTextAtCursor(const std::string &value);
            void addUndo(UndoRecords value);
            void insertText(const std::string &value);
            void insertText(const char *value);
            Interval findBlockInRange(Interval interval);
            RangeFromCoordinates getDelimiterLineNumbers(i32 start, i32 end, const std::string &delimiters);
            void nonDelimitedFolds();
            std::pair<i32,char> findMatchingDelimiter(i32 from);
            CodeFoldBlocks foldPointsFromSource();
            Coordinates findCommentEndCoord(i32 tokenId);
            void skipAttribute();
            bool isTokenIdValid(i32 tokenId);
            bool isLocationValid(Location location);
            pl::core::Location getLocation(i32 tokenId);
            i32 getTokenId(pl::core::Location location);
            i32 getTokenId(SafeTokenIterator tokenIterator);
            i32 getTokenId();
            void loadFirstTokenIdOfLine();
            i32 nextLine(i32 line);
            void setAllCodeFolds();
            void setCodeFoldState(CodeFoldState states);
            CodeFoldState getCodeFoldState() const;
            void advanceToNextLine(i32 &lineIndex, i32 &currentTokenId, Location &location);
            void incrementTokenId(i32 &lineIndex, i32 &currentTokenId, Location &location);
            void moveToStringIndex(i32 stringIndex, i32 &currentTokenId, Location &location);
            void resetToTokenId(i32 &lineIndex, i32 &currentTokenId, Location &location);
            i32 findNextDelimiter(bool openOnly);

            constexpr static u32 Normal = 0;
            constexpr static u32 Not    = 1;
            template<typename T> T *getValue(i32 index);
            void next(i32 count = 1);
            bool begin();
            void partBegin();
            void reset();
            void partReset();
            bool resetIfFailed(bool value);
            template<auto S = Normal> bool sequenceImpl();
            template<auto S = Normal> bool matchOne(const Token &token);
            template<auto S = Normal> bool sequenceImpl(const auto &... args);
            template<auto S = Normal> bool sequence(const Token &token, const auto &... args);
            bool isValid();
            bool peek(const Token &token, i32 index = 0);

            friend bool Coordinates::operator==(const Coordinates &o) const;
            friend bool Coordinates::operator!=(const Coordinates &o) const;
            friend bool Coordinates::operator<(const Coordinates &o) const;
            friend bool Coordinates::operator>(const Coordinates &o) const;
            friend bool Coordinates::operator<=(const Coordinates &o) const;
            friend bool Coordinates::operator>=(const Coordinates &o) const;
            friend Coordinates Coordinates::operator+(const Coordinates &o) const;
            friend Coordinates Coordinates::operator-(const Coordinates &o) const;

        private:
            UnfoldedLines m_unfoldedLines;
            FoldedLines m_foldedLines;
            HiddenLines m_hiddenLines;
            FoldSymbols m_rowToFoldSymbol;
            MatchedDelimiter m_matchedDelimiter{};
            bool m_colorizerEnabled = true;
            StringVector m_defines;
            FindReplaceHandler m_findReplaceHandler;
            RowToFoldSegments m_rowToFoldSegments;
            EditorState m_state;
            UndoBuffer m_undoBuffer;
            Indices m_leadingLineSpaces;
            i32 m_undoIndex = 0;
            bool m_updateFlags = true;
            Breakpoints m_breakpoints;
            ErrorMarkers m_errorMarkers;
            ErrorHoverBoxes m_errorHoverBoxes;
            ErrorGotoBoxes m_errorGotoBoxes;
            CursorBoxes m_cursorBoxes;
            CodeFoldKeys m_codeFoldKeys;
            CodeFolds m_codeFolds;
            CodeFoldKeyMap m_codeFoldKeyMap;
            CodeFoldKeyMap m_codeFoldValueMap;
            CodeFoldKeyLineMap m_codeFoldKeyLineMap;
            CodeFoldKeyLineMap m_codeFoldValueLineMap;
            CodeFoldDelimiters m_codeFoldDelimiters;
            Range m_codeFoldHighlighted;
            CodeFoldState m_codeFoldState;
            bool m_codeFoldsDisabled = false;
            IndexMap m_rowToLineIndex;
            IndexMap m_lineIndexToRow;
            ImVec2 m_cursorScreenPosition;
            ImVec2 m_lineNumbersStartPos;
            LineIndexToScreen m_lineIndexToScreen;
            IndexMap m_multiLinesToRow;
            RowCodeFoldTooltips m_rowCodeFoldTooltips;
            Range m_interactiveSelection;
            StringVector m_clickableText;
            float m_topRow = 0.0F;
            bool m_setTopRow = false;
            bool m_restoreSavedFolds = true;
            ImVec2 m_charAdvance;
            float m_leftMargin = 0.0F;
            float m_topMargin = 0.0F;
            float m_lineNumberFieldWidth = 0.0F;
            bool m_textChanged = false;
            LanguageDefinition m_languageDefinition;
            RegexList m_regexList;
            float m_numberOfLinesDisplayed = 0;
            bool m_withinRender = false;
            bool m_initializedCodeFolds = false;
            bool m_ignoreImGuiChild = false;
            std::string m_title;
            bool m_unfoldIfNeeded = false;
            bool m_scrollToCursor = false;
            Coordinates m_focusAtCoords;
            bool m_updateFocus = false;
            float m_oldTopMargin = 0.0F;
            float m_scrollYIncrement = 0.0F;
            bool m_setScrollY = false;
            bool m_breakPointsChanged = false;
            bool m_readOnly = false;
            u64 m_startTime = 0;
            bool m_codeFoldsChanged = false;
            bool m_saveCodeFoldStateRequested = false;
            bool m_useSavedFoldStatesRequested = false;
            Tokens m_tokens;
            SafeTokenIterator m_curr;
            SafeTokenIterator m_startToken, m_originalPosition, m_partOriginalPosition;
            bool m_interrupt = false;
            Indices m_firstTokenIdOfLine;
            CodeFoldBlocks m_foldPoints;
            GlobalBlocks m_globalBlocks;
            i32 m_cachedGlobalRowMax{};
            bool m_globalRowMaxChanged = true;
        };

    private:
// Rendering
        ImVec2 underWavesAt(ImVec2 pos, i32 nChars, ImColor color = ImGui::GetStyleColorVec4(ImGuiCol_Text), const ImVec2 &size_arg = ImVec2(0, 0));
        void renderText(const ImVec2 &textEditorSize);
    public:
        TextEditor::Coordinates nextCoordinate(TextEditor::Coordinates coordinate);
        bool testfoldMaps(TextEditor::Range toTest);
        void setTabSize(i32 value);
        float getPageSize() const;
        void render(const char *title, const ImVec2 &size = ImVec2(), bool border = false);
        float getTopLineNumber();
        float getMaxLineNumber();
        void setShowCursor(bool value) { m_showCursor = value; }
        void setShowLineNumbers(bool value) { m_showLineNumbers = value; }
        void setShowWhitespaces(bool value) { m_showWhitespaces = value; }
        bool isShowingWhitespaces() const { return m_showWhitespaces; }
        i32 getTabSize() const { return m_tabSize; }
        ImVec2 &getCharAdvance() { return m_lines.getCharAdvance(); }
        void addClickableText(const std::string &text) {m_lines.addClickableText(text); }
        void setErrorMarkers(const ErrorMarkers &markers) { m_lines.m_errorMarkers = markers; }
        Breakpoints &getBreakpoints() { return m_lines.getBreakpoints(); }
        void setBreakpoints(const Breakpoints &markers) { m_lines.m_breakpoints = markers; }
        void setLongestLineLength(u64 line) { m_longestLineLength = line; }
        u64 getLongestLineLength() const { return m_longestLineLength; }
        void setTopMarginChanged(i32 newMargin);
        ImVec2 coordsToScreen(Coordinates coordinates);
        bool isBreakpointsChanged() { return m_lines.isBreakpointsChanged(); }
        void clearBreakpointsChanged() { m_lines.clearBreakpointsChanged(); }
        float screenPosToRow(const ImVec2 &position) const;
        float rowToLineIndex(i32 row);
        float lineIndexToRow(i32 lineNumber);
        void clearErrorMarkers();
        void clearActionables() { m_lines.clearActionables();}
        void saveCodeFoldStates();
        void applyCodeFoldStates();
        void removeHiddenLinesFromPattern() { m_lines.removeHiddenLinesFromPattern(); };
        void addHiddenLinesToPattern() { m_lines.addHiddenLinesToPattern(); };

// Highlighting
    private:
        void preRender();
        void drawSelection(float row, ImDrawList *drawList);
        void renderBottomHorizontal(ImVec2 lineStartScreenPos, ImDrawList *drawList, float boxSize, float verticalMargin, i32 color);
        void renderTopHorizontal(ImVec2 lineStartScreenPos, ImDrawList *drawList, float boxSize, float verticalMargin, i32 color);
        void renderPointingDown(ImVec2 lineStartScreenPos, ImDrawList *drawList, float boxSize, float verticalMargin, i32 color);
        void renderPointingUp(ImVec2 lineStartScreenPos, ImDrawList *drawList, float boxSize, float verticalMargin, i32 color);
        void renderVerticals(ImVec2 lineStartScreenPos, ImDrawList *drawList, float boxSize, float verticalMargin, i32 color);
        void renderSquare(ImVec2 lineStartScreenPos, ImDrawList *drawList, float boxSize, float verticalMargin, i32 color);
        void renderMinus(ImVec2 lineStartScreenPos, ImDrawList *drawList, float boxSize, float verticalMargin, i32 color);
        void renderPlus(ImVec2 lineStartScreenPos, ImDrawList *drawList, float boxSize, float verticalMargin, i32 color);
        void renderCodeFolds(i32 row, ImDrawList *drawList, i32 color, FoldSymbol state);
        void drawLineNumbers(float lineNumber);
        void drawBreakpoints(float lineIndex, const ImVec2 &contentSize, ImDrawList *drawList, std::string title);
        void drawCodeFolds(float lineIndex, ImDrawList *drawList);
        void drawCursor(float lineIndex, const ImVec2 &contentSize, bool focused, ImDrawList *drawList);
        void drawButtons(float lineNumber);
        void drawText(Coordinates &lineStart, u32 tokenLength, char color);
        i64 drawColoredText(i32 lineIndex, const ImVec2 &textEditorSize);
        void postRender(float lineNumber, std::string textWindowName);
        ImVec2 calculateCharAdvance() const;
        void openCodeFoldAt(Coordinates line);
    public:
        static const Palette &getPalette();
        static void setPalette(const Palette &value);
        static const Palette &getDarkPalette();
        static const Palette &getLightPalette();
        static const Palette &getRetroBluePalette();
        void setNeedsUpdate(i32 line, bool needsUpdate);
        void setColorizedLine(i64 line, const std::string &tokens);

//Editing
    private:
        void enterCharacter(ImWchar character, bool shift);
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
        void setReadOnly(bool value) { m_lines.setReadOnly(value); };
        void appendLine(const std::string &value);
        void setOverwrite(bool value) { m_overwrite = value; }
        bool isOverwrite() const { return m_overwrite; }
        void setText(const std::string &text, bool undo = false);
        void setImGuiChildIgnored(bool value) { m_lines.setImGuiChildIgnored(value); }
        StringVector getTextLines() const;
        void setLanguageDefinition(const LanguageDefinition &aLanguageDef) { m_lines.setLanguageDefinition(aLanguageDef); }
        std::string getLineText(i32 line);
        void setTextChanged(bool value) { m_lines.setTextChanged(value); }
        std::string getText() { return m_lines.getText(); }
        void addUndo(UndoRecords value) { m_lines.addUndo(value); }
        bool isTextChanged() { return m_lines.isTextChanged(); }
        void setHandleMouseInputs(bool value) { m_handleMouseInputs = value; }
        bool isHandleMouseInputsEnabled() const { return m_handleKeyboardInputs; }
        void setHandleKeyboardInputs(bool value) { m_handleKeyboardInputs = value; }
        bool isHandleKeyboardInputsEnabled() const { return m_handleKeyboardInputs; }
        Lines &getLines() { return m_lines; }
        const Lines &getLines() const { return m_lines; }
// Navigating
    private:
        Coordinates lineCoordinates(const Coordinates &value);
        Coordinates lineCoordinates(i32 lineIndex, i32 column);
        Range lineCoordinates(const Range &value);
        void advance(Coordinates &coordinates);
        Coordinates findWordStart(const Coordinates &from);
        Coordinates findWordEnd(const Coordinates &from);


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
        void moveToMatchedDelimiter(bool select = false);
        void setCursorPosition(const Coordinates &position, bool unfoldIfNeeded = true, bool scrollToCursor = true) {
            m_lines.setCursorPosition(position, unfoldIfNeeded, scrollToCursor);
        };
        void setScroll(ImVec2 scroll);
        ImVec2 getScroll() const {return m_scroll;}
        Coordinates getCursorPosition() { return m_lines.lineCoordinates(m_lines.m_state.m_cursorPosition); }

//Support
    private:
        void handleKeyboardInputs();
        void handleMouseInputs();
    public:
        void setSelection(const Range &selection);
        Range getSelection() const;
        void selectWordUnderCursor();
        void selectAll();
        bool hasSelection() { return m_lines.hasSelection(); }
        std::string getSelectedText() { return m_lines.getSelectedText(); }
        u32 getfirstNonWhite(u32 lineIndex);
        i32 getTotalLines() const;
        FindReplaceHandler *getFindReplaceHandler() { return m_lines.getFindReplaceHandler(); }
        void setSourceCodeEditor(TextEditor *editor) { m_sourceCodeEditor = editor; }
        bool raiseContextMenu() { return m_raiseContextMenu; }
        void clearRaiseContextMenu() { m_raiseContextMenu = false; }
        TextEditor *getSourceCodeEditor();
        void codeFoldExpand(i32 level=1, bool recursive=false, bool all=false);
        void codeFoldCollapse(i32 level=1, bool recursive=false, bool all=false);
        i32 getCodeFoldLevel(i32 line) const;
        void resetFoldedSelections();
        void computeLPSArray(const std::string &pattern, Indices & lps);
        Indices KMPSearch(const std::string& text, const std::string& pattern);
        bool isEmpty();

// utf8
    private:
        Coordinates screenPosCoordinates(const ImVec2 &position);
        i32 lineIndexColumn(i32 lineNumber, i32 stringIndex);
    public:
        static i32 imTextCharToUtf8(char *buffer, i32 buf_size, u32 c);
        static void imTextCharToUtf8(std::string &buffer, u32 c);
        static i32 utf8CharLength(uint8_t c);
        static i32 stringCharacterCount(const std::string &str);
        Coordinates lineCoordsToIndexCoords(const Coordinates &coordinates);
    private:
        float m_lineSpacing = 1.0F;
        Lines m_lines;
        float m_newTopMargin = 0.0F;
        bool m_topMarginChanged = false;
        i32 m_tabSize = 4;
        bool m_overwrite = false;
        u64 m_longestDrawnLineLength = 0;
        float m_topLineNumber = 0.0F;
        bool m_showWhitespaces = true;
        u64 m_longestLineLength = 0;
        bool m_handleKeyboardInputs = true;
        bool m_handleMouseInputs = true;
        bool m_drawMatchedBracket = false;

        TextEditor *m_sourceCodeEditor = nullptr;
        float m_shiftedScrollY = 0;
        bool m_setScroll = false;
        ImVec2 m_scroll;
        float m_scrollOffset = 0;
        float m_maxScroll =0;
        bool m_scrollFromLines = false;
        bool m_newMouseWheel = false;
        float m_lastClick = -1.0F;
        bool m_showCursor = true;
        bool m_showLineNumbers = true;
        bool m_raiseContextMenu = false;


        constexpr static char inComment = 7;
        inline static Palette m_palette = {};
        inline static Line Ellipsis = Line({'.','.','.'},{(i32)TextEditor::PaletteIndex::Operator,(i32)TextEditor::PaletteIndex::Operator,(i32)TextEditor::PaletteIndex::Operator},{0,0,0});
        inline static const Line m_emptyLine = Line();
        inline static const std::string s_delimiters = "()[]{}<>";
        inline static const std::string s_separators = "()[]{}";
        inline static const std::string s_operators = "<>";
        inline static const Coordinates Invalid = Coordinates(0x80000000, 0x80000000);
        inline static const Interval NotValid = Interval(0x80000000, 0x80000000);
        inline static const Range NoCodeFoldSelected = Range(Invalid, Invalid);
        inline static const i32 s_cursorBlinkInterval = 1200;
        inline static const i32 s_cursorBlinkOnTime = 800;
    };

    bool tokenizeCStyleString(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end);
    bool tokenizeCStyleCharacterLiteral(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end);
    bool tokenizeCStyleIdentifier(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end);
    bool tokenizeCStyleNumber(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end);
    bool tokenizeCStyleOperator(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end);
    bool tokenizeCStyleSeparator(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end);

}