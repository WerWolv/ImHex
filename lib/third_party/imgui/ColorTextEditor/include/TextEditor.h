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
using strConstIter = std::string::const_iterator;
int32_t utf8CharLength(uint8_t c);
int32_t getStringCharacterCount(const std::string& str);

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

	// indices of the arrays that contain the lines (vector) and the columns (a string) of the
    // text editor. Negative values indicate the distance to the last element of the array.
    // When comparing coordinates ensure they have the same sign because coordinates don't have
    // information about the size of the array. Currently positive coordinates are always bigger
    // than negatives even if that gives a wrong result.
	struct Coordinates
	{
		int32_t m_line, m_column;
		Coordinates() : m_line(0), m_column(0) {}
		Coordinates(int32_t line, int32_t column) : m_line(line), m_column(column) {}

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

        Coordinates operator +(const Coordinates & o) const {
            return Coordinates(m_line + o.m_line, m_column + o.m_column);
        }
        Coordinates operator -(const Coordinates & o) const {
            return Coordinates(m_line - o.m_line, m_column - o.m_column);
        }
	};
    inline static const Coordinates Invalid = Coordinates(0x80000000,0x80000000);

	struct Identifier
	{
		Coordinates m_location;
		std::string m_declaration;
	};

    using String = std::string;
	using Identifiers = std::unordered_map<std::string, Identifier>;
	using Keywords = std::unordered_set<std::string> ;
    using ErrorMarkers = std::map<Coordinates, std::pair<uint32_t ,std::string>>;
    using Breakpoints = std::unordered_set<uint32_t>;
    using Palette = std::array<ImU32, (uint64_t )PaletteIndex::Max>;
    using Glyph = uint8_t ;

    class ActionableBox {

        ImRect m_box;
    public:
        ActionableBox()=default;
        explicit ActionableBox(const ImRect &box) : m_box(box) {}
        virtual bool trigger() {
            return ImGui::IsMouseHoveringRect(m_box.Min, m_box.Max);
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
        Coordinates m_pos;
    public:
        ErrorGotoBox()=default;
        ErrorGotoBox(const ImRect &box, const Coordinates &pos, TextEditor *editor) : ActionableBox(box), m_pos(pos), m_editor(editor) {

        }

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
        ErrorHoverBox()=default;
        ErrorHoverBox(const ImRect &box, const Coordinates &pos,const char *errorText) : ActionableBox(box), m_pos(pos), m_errorText(errorText) {

        }

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
		    bool comment : 1;
		    bool blockComment : 1;
		    bool docComment : 1;
            bool blockDocComment : 1;
            bool globalDocComment : 1;
            bool deactivated : 1;
            bool preprocessor : 1;
            bool matchedBracket : 1;
        };
        union Flags {
            Flags(char value) : m_value(value) {}
            Flags(FlagBits bits) : m_bits(bits) {}
            FlagBits m_bits;
            char m_value;
        };
        constexpr static char inComment = 31;

        int32_t getCharacterColumn(int32_t index) const;

        class LineIterator {
        public:
            strConstIter m_charsIter;
            strConstIter m_colorsIter;
            strConstIter m_flagsIter;

            LineIterator(const LineIterator &other) : m_charsIter(other.m_charsIter), m_colorsIter(other.m_colorsIter), m_flagsIter(other.m_flagsIter) {}

            LineIterator() = default;

            char operator*() {
                return *m_charsIter;
            }

            LineIterator operator++() {
                LineIterator iter = *this;
                ++iter.m_charsIter;
                ++iter.m_colorsIter;
                ++iter.m_flagsIter;
                return iter;
            }

            LineIterator operator=(const LineIterator &other) {
                m_charsIter = other.m_charsIter;
                m_colorsIter = other.m_colorsIter;
                m_flagsIter = other.m_flagsIter;
                return *this;
            }

            bool operator!=(const LineIterator &other) const {
                return m_charsIter != other.m_charsIter || m_colorsIter != other.m_colorsIter || m_flagsIter != other.m_flagsIter;
            }

            bool operator==(const LineIterator &other) const {
                return m_charsIter == other.m_charsIter && m_colorsIter == other.m_colorsIter && m_flagsIter == other.m_flagsIter;
            }

            LineIterator operator+(int32_t n) {
                LineIterator iter = *this;
                iter.m_charsIter += n;
                iter.m_colorsIter += n;
                iter.m_flagsIter += n;
                return iter;
            }

            int32_t operator-(LineIterator l) {
                return m_charsIter - l.m_charsIter;
            }
        };

        LineIterator begin() const {
            LineIterator iter;
            iter.m_charsIter = m_chars.begin();
            iter.m_colorsIter = m_colors.begin();
            iter.m_flagsIter = m_flags.begin();
            return iter;
        }

        LineIterator end() const {
            LineIterator iter;
            iter.m_charsIter = m_chars.end();
            iter.m_colorsIter = m_colors.end();
            iter.m_flagsIter = m_flags.end();
            return iter;
        }

        std::string m_chars;
        std::string m_colors;
        std::string m_flags;
        bool m_colorized = false;
        Line() : m_chars(), m_colors(), m_flags(), m_colorized(false) {}

        explicit Line(const char *line) {
            Line(std::string(line));
        }

        explicit Line(const std::string &line) : m_chars(line), m_colors(std::string(line.size(), 0x00)), m_flags(std::string(line.size(), 0x00)), m_colorized(false) {}
        Line(const Line &line) : m_chars(line.m_chars), m_colors(line.m_colors), m_flags(line.m_flags), m_colorized(line.m_colorized) {}

        LineIterator begin() {
            LineIterator iter;
            iter.m_charsIter = m_chars.begin();
            iter.m_colorsIter = m_colors.begin();
            iter.m_flagsIter = m_flags.begin();
            return iter;
        }

        LineIterator end() {
            LineIterator iter;
            iter.m_charsIter = m_chars.end();
            iter.m_colorsIter = m_colors.end();
            iter.m_flagsIter = m_flags.end();
            return iter;
        }

        Line &operator=(const Line &line) {
            m_chars = line.m_chars;
            m_colors = line.m_colors;
            m_flags = line.m_flags;
            m_colorized = line.m_colorized;
            return *this;
        }

        Line &operator=(Line &&line) noexcept {
            m_chars = std::move(line.m_chars);
            m_colors = std::move(line.m_colors);
            m_flags = std::move(line.m_flags);
            m_colorized = line.m_colorized;
            return *this;
        }

        uint64_t size() const {
            return m_chars.size();
        }
        enum class LinePart {
            Chars,
            Utf8,
            Colors,
            Flags
        };

        char front(LinePart part = LinePart::Chars) const {
            if (part == LinePart::Chars && !m_chars.empty())
                return m_chars.front();
            if (part == LinePart::Colors && !m_colors.empty())
                return m_colors.front();
            if (part == LinePart::Flags && !m_flags.empty())
                return m_flags.front();
            return 0x00;
        }

        std::string frontUtf8(LinePart part = LinePart::Chars) const {
            if (part == LinePart::Chars && !m_chars.empty())
                return m_chars.substr(0, utf8CharLength(m_chars[0]));
            if (part == LinePart::Colors && !m_colors.empty())
                return m_colors.substr(0, utf8CharLength(m_chars[0]));
            if (part == LinePart::Flags && !m_flags.empty())
                return m_flags.substr(0, utf8CharLength(m_chars[0]));
            return "";
        }

        void push_back(char c) {
            m_chars.push_back(c);
            m_colors.push_back(0x00);
            m_flags.push_back(0x00);
            m_colorized = false;
        }

        bool empty() const {
            return m_chars.empty();
        }

        std::string substr(uint64_t start, uint64_t length = (uint64_t)-1, LinePart part = LinePart::Chars ) const {
            if (start >= m_chars.size() || m_colors.size() != m_chars.size() || m_flags.size() != m_chars.size())
                return "";
            if (length == (uint64_t)-1 || start + length >= m_chars.size())
                length = m_chars.size() - start;
            if (length == 0)
                return "";

            if (part == LinePart::Chars)
                return m_chars.substr(start, length);
            if (part == LinePart::Colors)
                return m_colors.substr(start, length);
            if (part == LinePart::Flags)
                return m_flags.substr(start, length);
            if (part == LinePart::Utf8) {
                uint64_t utf8Start= 0;
                for (uint64_t utf8Index = 0; utf8Index < start; ++utf8Index) {
                    utf8Start += utf8CharLength(m_chars[utf8Start]);
                }
                uint64_t utf8Length = 0;
                for (uint64_t utf8Index = 0; utf8Index < length; ++utf8Index) {
                    utf8Length += utf8CharLength(m_chars[utf8Start + utf8Length]);
                }
                return m_chars.substr(utf8Start, utf8Length);
            }
            return "";
        }

        char operator[](uint64_t index) const {
            index = std::clamp(index, (uint64_t)0, (uint64_t)(m_chars.size() - 1));
            return m_chars[index];
        }
        // C++ can't overload functions based on return type, so use any type other
        // than uint64_t to avoid ambiguity.
       template<class T>
       std::string operator[](T column) const {
           uint64_t utf8Length = getStringCharacterCount(m_chars);
           uint64_t index = static_cast<uint64_t>(column);
           index = std::clamp(index,(uint64_t)0,utf8Length-1);
           uint64_t utf8Start = 0;
           for (uint64_t utf8Index = 0; utf8Index < index; ++utf8Index) {
               utf8Start += utf8CharLength(m_chars[utf8Start]);
           }
           uint64_t utf8CharLen = utf8CharLength(m_chars[utf8Start]);
           if (utf8Start + utf8CharLen > m_chars.size())
                utf8CharLen = m_chars.size() - utf8Start;
           return m_chars.substr(utf8Start, utf8CharLen);
       }

        void setNeedsUpdate(bool needsUpdate) {
            m_colorized = m_colorized && !needsUpdate;
        }

        void append(const char *text) {
            append(std::string(text));
        }

        void append(const char text) {
            append(std::string(1, text));
        }

        void append(const std::string &text) {
            Line line(text);
            append(line);
        }

        void append(const Line &line) {
            append(line.begin(), line.end());
        }

        void append(LineIterator begin, LineIterator end) {
            if (begin.m_charsIter < end.m_charsIter)
                m_chars.append(begin.m_charsIter, end.m_charsIter);
            if (begin.m_colorsIter < end.m_colorsIter)
                m_colors.append(begin.m_colorsIter, end.m_colorsIter);
            if (begin.m_flagsIter < end.m_flagsIter)
                m_flags.append(begin.m_flagsIter, end.m_flagsIter);
            m_colorized = false;
        }

        void insert(LineIterator iter, const std::string &text) {
            insert(iter, text.begin(), text.end());
        }

        void insert(LineIterator iter, const char text) {
            insert(iter,std::string(1, text));
        }

        void insert(LineIterator iter, strConstIter beginString, strConstIter endString) {
            Line line(std::string(beginString, endString));
            insert(iter, line);
        }

        void insert(LineIterator iter,const Line &line) {
            insert(iter, line.begin(), line.end());
        }

        void insert(LineIterator iter,LineIterator beginLine, LineIterator endLine) {
            if (iter == end())
                append(beginLine, endLine);
            else {
                m_chars.insert(iter.m_charsIter, beginLine.m_charsIter, endLine.m_charsIter);
                m_colors.insert(iter.m_colorsIter, beginLine.m_colorsIter, endLine.m_colorsIter);
                m_flags.insert(iter.m_flagsIter, beginLine.m_flagsIter, endLine.m_flagsIter);
                m_colorized = false;
            }
        }

        void erase(LineIterator begin) {
            m_chars.erase(begin.m_charsIter);
            m_colors.erase(begin.m_colorsIter);
            m_flags.erase(begin.m_flagsIter);
            m_colorized = false;
        }

        void erase(LineIterator begin, uint64_t count) {
            if (count == (uint64_t) -1)
                count = m_chars.size() - (begin.m_charsIter - m_chars.begin());
            m_chars.erase(begin.m_charsIter, begin.m_charsIter + count);
            m_colors.erase(begin.m_colorsIter, begin.m_colorsIter + count);
            m_flags.erase(begin.m_flagsIter, begin.m_flagsIter + count);
            m_colorized = false;
        }

        void erase(uint64_t start, uint64_t length = (uint64_t)-1) {
            if (length == (uint64_t)-1 || start + length >= m_chars.size())
                length = m_chars.size() - start;
            uint64_t utf8Start= 0;
            for (uint64_t utf8Index = 0; utf8Index < start; ++utf8Index) {
                utf8Start += utf8CharLength(m_chars[utf8Start]);
            }
            uint64_t utf8Length = 0;
            for (uint64_t utf8Index = 0; utf8Index < length; ++utf8Index) {
                utf8Length += utf8CharLength(m_chars[utf8Start + utf8Length]);
            }
            erase(begin() + utf8Start, utf8Length);
        }

        void clear() {
            m_chars.clear();
            m_colors.clear();
            m_flags.clear();
            m_colorized = false;
        }

        void setLine(const std::string &text) {
            m_chars = text;
            m_colors = std::string(text.size(), 0x00);
            m_flags = std::string(text.size(), 0x00);
            m_colorized = false;
        }

        void setLine(const Line &text) {
            m_chars = text.m_chars;
            m_colors = text.m_colors;
            m_flags = text.m_flags;
            m_colorized = text.m_colorized;
        }


        bool needsUpdate() const {
            return !m_colorized;
        }

    };

    using Lines = std::vector<Line>;

	struct LanguageDefinition
	{
		typedef std::pair<std::string, PaletteIndex> TokenRegexString;
		typedef std::vector<TokenRegexString> TokenRegexStrings;
		typedef bool(*TokenizeCallback)(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end, PaletteIndex &paletteIndex);

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

		LanguageDefinition() : m_name(""), m_keywords({}), m_identifiers({}), m_preprocIdentifiers({}), m_singleLineComment(""), m_commentEnd(""),
                               m_commentStart(""), m_globalDocComment(""), m_docComment(""), m_blockDocComment(""), m_preprocChar('#'), m_autoIndentation(true), m_tokenize(nullptr), m_tokenRegexStrings({}), m_caseSensitive(true) {}

		static const LanguageDefinition& CPlusPlus();
		static const LanguageDefinition& HLSL();
		static const LanguageDefinition& GLSL();
		static const LanguageDefinition& C();
		static const LanguageDefinition& SQL();
		static const LanguageDefinition& AngelScript();
		static const LanguageDefinition& Lua();
	};
    void clearErrorMarkers() {
        m_errorMarkers.clear();
        m_errorHoverBoxes.clear();
    }
    void clearGotoBoxes() { m_errorGotoBoxes.clear(); }
    void clearCursorBoxes() { m_cursorBoxes.clear(); }

    void clearActionables() {
        clearErrorMarkers();
        clearGotoBoxes();
        clearCursorBoxes();
    }

    struct Selection {
        Coordinates m_start;
        Coordinates m_end;
        Selection() = default;
        Selection(Coordinates start, Coordinates end) : m_start(start), m_end(end) {
            if (m_start > m_end) {
                std::swap(m_start, m_end);
            }
        }
        Coordinates getSelectedLines() {
            return Coordinates(m_start.m_line,m_end.m_line);
        }
        Coordinates getSelectedColumns() {
            if (isSingleLine())
                return Coordinates(m_start.m_column, m_end.m_column - m_start.m_column);
            return Coordinates(m_start.m_column, m_end.m_column);
        }
        bool isSingleLine() {
            return m_start.m_line == m_end.m_line;
        }
    };

	TextEditor();
	~TextEditor();

	void setLanguageDefinition(const LanguageDefinition& aLanguageDef);
	const LanguageDefinition& getLanguageDefinition() const { return m_languageDefinition; }

	static const Palette& getPalette();
	static void setPalette(const Palette& value);

	void setErrorMarkers(const ErrorMarkers& markers) { m_errorMarkers = markers; }
    Breakpoints &getBreakpoints() { return m_breakpoints; }
	void setBreakpoints(const Breakpoints& markers) { m_breakpoints = markers; }
    ImVec2 underwaves(ImVec2 pos, uint32_t nChars, ImColor color= ImGui::GetStyleColorVec4(ImGuiCol_Text), const ImVec2 &size_arg= ImVec2(0, 0));

	void render(const char* title, const ImVec2& size = ImVec2(), bool border = false);
	void setText(const std::string& text, bool undo = false);
    void jumpToLine(int32_t line=-1);
    void jumpToCoords(const Coordinates &coords);
    void setLongestLineLength(uint64_t line) { m_longestLineLength = line; }
    uint64_t getLongestLineLength() const { return m_longestLineLength; }
	std::string getText() const;
    bool isEmpty() const {
        if (m_lines.empty())
            return true;
        if (m_lines.size() == 1) {
            if (m_lines[0].empty())
                return true;
            if (m_lines[0].size() == 1 && m_lines[0].front() == '\n')
                return true;
        }
        return false;
    }

    void setTopLine();
    void setNeedsUpdate (uint32_t line, bool needsUpdate) {
        if (line < m_lines.size())
            m_lines[line].setNeedsUpdate(needsUpdate);
    }

    void setColorizedLine(uint64_t line, const std::string &tokens) {
        if (line < m_lines.size()) {
            auto &lineTokens = m_lines[line].m_colors;
            if (lineTokens.size() != tokens.size()) {
                lineTokens.resize(tokens.size());
                std::fill(lineTokens.begin(), lineTokens.end(), 0x00);
            }
            bool needsUpdate = false;
            for (uint64_t i = 0; i < tokens.size(); ++i) {
                if (tokens[i] != 0x00) {
                    if (tokens[i] != lineTokens[i]) {
                        lineTokens[i] = tokens[i];
                        needsUpdate = true;
                    }
                }
            }
            setNeedsUpdate(line, needsUpdate);
        }
    }

    void setScrollY();
	std::vector<std::string> getTextLines() const;

	std::string getSelectedText() const;

    std::string getLineText(int32_t line)const;
    void setSourceCodeEditor(TextEditor *editor) { m_sourceCodeEditor = editor; }
    TextEditor *GetSourceCodeEditor() {
        if(m_sourceCodeEditor != nullptr)
            return m_sourceCodeEditor;
        return this;
    }

    class FindReplaceHandler;

public:
    void addClickableText(std::string text) {
        m_clickableText.push_back(text);
    }
    FindReplaceHandler *getFindReplaceHandler() { return &m_findReplaceHandler; }
	int32_t getTotalLines() const { return (int32_t)m_lines.size(); }
	bool isOverwrite() const { return m_overwrite; }
    void setTopMarginChanged(int32_t newMargin) {
        m_newTopMargin = newMargin;
        m_topMarginChanged = true;
    }
    void setFocusAtCoords(const Coordinates &coords) {
        m_focusAtCoords = coords;
        m_updateFocus = true;
    }
    void setOverwrite(bool value) { m_overwrite = value; }

    std::string replaceStrings(std::string string, const std::string &search, const std::string &replace);
    static std::vector<std::string> splitString(const std::string &string, const std::string &delimiter, bool removeEmpty);
    std::string replaceTabsWithSpaces(const std::string& string, uint32_t tabSize);
    std::string preprocessText(const std::string &code);

	void setReadOnly(bool value);
    bool isEndOfLine(const Coordinates &coordinates) const;
    bool isEndOfFile(const Coordinates &coordinates) const;
	bool isReadOnly() const { return m_readOnly; }
	bool isTextChanged() const { return m_textChanged; }
    void setTextChanged(bool value) { m_textChanged = value; }
    bool isBreakpointsChanged() const { return m_breakPointsChanged; }
    void clearBreakpointsChanged() { m_breakPointsChanged = false; }

    void setShowCursor(bool value) { m_showCursor = value; }
    void setShowLineNumbers(bool value) { m_showLineNumbers = value; }

	bool isColorizerEnabled() const { return m_colorizerEnabled; }
    void colorize();

	Coordinates getCursorPosition() const { return setCoordinates(m_state.m_cursorPosition); }
	void setCursorPosition(const Coordinates& position);

    bool raiseContextMenu() { return m_raiseContextMenu; }
    void clearRaiseContextMenu() { m_raiseContextMenu = false; }

	inline void setHandleMouseInputs    (bool value){ m_handleMouseInputs    = value;}
	inline bool isHandleMouseInputsEnabled() const { return m_handleKeyboardInputs; }

	inline void setHandleKeyboardInputs (bool value){ m_handleKeyboardInputs = value;}
	inline bool isHandleKeyboardInputsEnabled() const { return m_handleKeyboardInputs; }

	inline void setImGuiChildIgnored    (bool value){ m_ignoreImGuiChild     = value;}
	inline bool isImGuiChildIgnored() const { return m_ignoreImGuiChild; }

	inline void setShowWhitespaces(bool value) { m_showWhitespaces = value; }
	inline bool isShowingWhitespaces() const { return m_showWhitespaces; }

	void setTabSize(int32_t value);
	inline int32_t getTabSize() const { return m_tabSize; }

	void insertText(const std::string& value);
	void insertText(const char* value);
    void appendLine(const std::string &value);

	void moveUp(int32_t amount = 1, bool select = false);
	void moveDown(int32_t amount = 1, bool select = false);
	void moveLeft(int32_t amount = 1, bool select = false, bool wordMode = false);
	void moveRight(int32_t amount = 1, bool select = false, bool wordMode = false);
	void moveTop(bool select = false);
	void moveBottom(bool select = false);
	void moveHome(bool select = false);
	void moveEnd(bool select = false);
    void moveToMatchedBracket(bool select = false);

	void setSelection(const Selection& selection);
    Selection getSelection() const;
	void selectWordUnderCursor();
	void selectAll();
	bool hasSelection() const;

	void copy();
	void cut();
	void paste();
	void deleteChar();
    float getPageSize() const;

	ImVec2 &getCharAdvance() { return m_charAdvance; }

	bool canUndo();
	bool canRedo() const;
	void undo(int32_t steps = 1);
	void redo(int32_t steps = 1);

    void deleteWordLeft();
    void deleteWordRight();
    void backspace();

	static const Palette& getDarkPalette();
	static const Palette& getLightPalette();
	static const Palette& getRetroBluePalette();

private:
	typedef std::vector<std::pair<std::regex, PaletteIndex>> RegexList;

	struct EditorState
	{
        Selection m_selection;
		Coordinates m_cursorPosition;
	};


public:
    class FindReplaceHandler {
    public:
        FindReplaceHandler();
        typedef std::vector<EditorState> Matches;
        Matches &getMatches() { return m_matches; }
        bool findNext(TextEditor *editor);
        uint32_t findMatch(TextEditor *editor, bool isNex);
        bool replace(TextEditor *editor, bool right);
        bool replaceAll(TextEditor *editor);
        std::string &getFindWord()  { return m_findWord; }
        void setFindWord(TextEditor *editor, const std::string &findWord) {
            if (findWord != m_findWord) {
                findAllMatches(editor, findWord);
                m_findWord = findWord;
            }
        }
        std::string &getReplaceWord()  { return m_replaceWord; }
        void setReplaceWord(const std::string &replaceWord) { m_replaceWord = replaceWord; }
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

    private:
        std::string m_findWord;
        std::string m_replaceWord;
        bool m_matchCase;
        bool m_wholeWord;
        bool m_findRegEx;
        bool m_optionsChanged;
        Matches m_matches;
    };
    FindReplaceHandler m_findReplaceHandler;
private:
	class UndoRecord
	{
	public:
		UndoRecord() {}
		~UndoRecord() {}

		UndoRecord(
			const std::string& added,
			const TextEditor::Selection addedSelection,


			const std::string& removed,
			const TextEditor::Selection removedSelection,

			TextEditor::EditorState& before,
			TextEditor::EditorState& after);

		void undo(TextEditor* editor);
		void redo(TextEditor* editor);

		std::string m_added;
		Selection m_addedSelection;

		std::string m_removed;
		Selection m_removedSelection;

		EditorState m_before;
		EditorState m_after;
	};

	typedef std::vector<UndoRecord> UndoBuffer;

    struct MatchedBracket {
        bool m_active=false;
        bool m_changed=false;
        Coordinates m_nearCursor = {};
        Coordinates m_matched = {};
        static const std::string s_separators;
        static const std::string s_operators;
        MatchedBracket(const MatchedBracket &other) : m_active(other.m_active), m_changed(other.m_changed), m_nearCursor(other.m_nearCursor), m_matched(other.m_matched) {}
        MatchedBracket() : m_active(false), m_changed(false), m_nearCursor(0, 0), m_matched(0, 0) {}
        MatchedBracket(bool active, bool changed, const Coordinates &nearCursor, const Coordinates &matched) : m_active(active), m_changed(changed), m_nearCursor(nearCursor), m_matched(matched) {}
        bool checkPosition(TextEditor *editor, const Coordinates &from);
        bool isNearABracket(TextEditor *editor, const Coordinates &from);
        int32_t detectDirection(TextEditor *editor, const Coordinates &from);

        void findMatchingBracket(TextEditor *editor);
        bool isActive() const { return m_active; }
        bool hasChanged() const { return m_changed; }
    };

	void colorizeRange();
	void colorizeInternal();
	float textDistanceToLineStart(const Coordinates& from) const;
	void ensureCursorVisible();
	std::string getText(const Selection& selection) const;
	Coordinates setCoordinates(const Coordinates& value) const;
    Coordinates setCoordinates( int32_t line, int32_t column ) const;
    Selection setCoordinates(const Selection &value) const;
	void advance(Coordinates& coordinates) const;
	void deleteRange(const Selection& selection);
	int32_t insertTextAt(Coordinates &where, const std::string &value);
	void addUndo(UndoRecord& value);
  	Coordinates screenPosToCoordinates(const ImVec2& position) const;
	Coordinates findWordStart(const Coordinates& from) const;
	Coordinates findWordEnd(const Coordinates& from) const;
    Coordinates findPreviousWord(const Coordinates& from) const;
	Coordinates findNextWord(const Coordinates& from) const;
    int32_t lineCoordinateToIndex(const Coordinates& coordinates) const;
	Coordinates getCharacterCoordinates(int32_t line, int32_t index) const;
	int32_t getLineCharacterCount(int32_t line) const;
    uint64_t getLineByteCount(int32_t line) const;
	int32_t getLineMaxColumn(int32_t line) const;
	void removeLine(int32_t start, int32_t end);
	void removeLine(int32_t index);
	Line& insertLine(int32_t index);
    void insertLine(int32_t index, const std::string &text);
    void enterCharacter(ImWchar character, bool shift);
	void deleteSelection();
    TextEditor::PaletteIndex getColorIndexFromFlags(Line::Flags flags);
    void resetCursorBlinkTime();
    uint32_t skipSpaces(const Coordinates &from);
	void handleKeyboardInputs();
	void handleMouseInputs();
	void renderText(const char *title, const ImVec2 &lineNumbersStartPos, const ImVec2 &textEditorSize);
    void setFocus();
	float m_lineSpacing = 1.0F;
	Lines m_lines;
	EditorState m_state = {};
	UndoBuffer m_undoBuffer;
	int32_t m_undoIndex = 0;
    bool m_scrollToBottom = false;
    float m_topMargin = 0.0F;
    float m_newTopMargin = 0.0F;
    float m_oldTopMargin = 0.0F;
    bool m_topMarginChanged = false;

	int32_t m_tabSize = 4;
	bool m_overwrite = false;
	bool m_readOnly = false;
	bool m_withinRender = false;
	bool m_scrollToCursor = false;
	bool m_scrollToTop = false;
	bool m_textChanged = false;
	bool m_colorizerEnabled = true;
    float m_lineNumberFieldWidth = 0.0F;
    uint64_t m_longestLineLength = 0;
	float  m_leftMargin = 10.0;
    float m_topLine = 0.0F;
    bool m_setTopLine = false;
    bool m_breakPointsChanged = false;
	bool m_handleKeyboardInputs = true;
	bool m_handleMouseInputs = true;
	bool m_ignoreImGuiChild = false;
	bool m_showWhitespaces = true;

    MatchedBracket m_matchedBracket={};
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
	std::string m_lineBuffer;
	uint64_t m_startTime = 0;
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

    std::vector<std::string>  m_clickableText;

    static const int32_t s_cursorBlinkInterval;
    static const int32_t s_cursorBlinkOnTime;
};

bool tokenizeCStyleString(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end);
bool tokenizeCStyleCharacterLiteral(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end);
bool tokenizeCStyleIdentifier(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end);
bool tokenizeCStyleNumber(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end);
bool tokenizeCStyleOperator(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end);
bool tokenizeCStyleSeparator(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end);