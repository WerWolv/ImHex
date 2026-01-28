#include <ui/text_editor.hpp>
#include <hex/helpers/logger.hpp>
#include <wolv/utils/string.hpp>
#include <pl/api.hpp>
#include <pl/core/preprocessor.hpp>

#include <algorithm>

namespace hex::ui {
    using Interval           = TextEditor::Interval;
    using Coordinates        = TextEditor::Range::Coordinates;
    using Range              = TextEditor::Range;
    using Line               = TextEditor::Line;
    using Lines              = TextEditor::Lines;
    using LineIterator       = TextEditor::LineIterator;
    using Range              = TextEditor::Range;
    using FindReplaceHandler = TextEditor::FindReplaceHandler;
    using CodeFold           = TextEditor::CodeFold;
    using EditorState        = TextEditor::EditorState;


    bool Interval::contains(i32 value, bool inclusive) const {
        if (inclusive)
            return value >= m_start && value <= m_end;
        return value > m_start && value < m_end;
    }

    bool Interval::contiguous(const Interval &other) const {
        auto gap = m_start - other.m_end;
        if (gap == 0 || gap == 1)
            return true;
        gap = other.m_start - m_end;
        return gap == 0 || gap == 1;
    }

    bool Coordinates::operator==(const Coordinates &o) const {
        return m_line == o.m_line && m_column == o.m_column;
    }

    bool Coordinates::operator!=(const Coordinates &o) const {
        return m_line != o.m_line || m_column != o.m_column;
    }

    bool Coordinates::operator<(const Coordinates &o) const {
        if (m_line != o.m_line)
            return m_line < o.m_line;
        return m_column < o.m_column;
    }

    bool Coordinates::operator>(const Coordinates &o) const {
        if (m_line != o.m_line)
            return m_line > o.m_line;
        return m_column > o.m_column;
    }

    bool Coordinates::operator<=(const Coordinates &o) const {
        return !(*this > o);
    }

    bool Coordinates::operator>=(const Coordinates &o) const {
        return !(*this < o);
    }

    Coordinates Coordinates::operator+(const Coordinates &o) const {
        return {m_line + o.m_line, m_column + o.m_column};
    }

    Coordinates Coordinates::operator-(const Coordinates &o) const {
        return {m_line - o.m_line, m_column - o.m_column};
    }

    bool Range::operator==(const Range &o) const {
        return m_start == o.m_start && m_end == o.m_end;
    }
    bool Range::operator!=(const Range &o) const {
        return m_start != o.m_start || m_end != o.m_end;
    }

    Coordinates Range::getSelectedLines() {
        return {m_start.m_line, m_end.m_line};
    }

    Coordinates Range::getSelectedColumns() {
        if (isSingleLine())
            return {m_start.m_column, m_end.m_column - m_start.m_column};
        return {m_start.m_column, m_end.m_column};
    }

    bool Range::isSingleLine() {
        return m_start.m_line == m_end.m_line;
    }

    bool Range::contains(const Range &range, EndsInclusive endsInclusive) const {
        return contains(range.m_start, endsInclusive) && contains(range.m_end, endsInclusive);
    }

    bool Range::overlaps(const Range &o, EndsInclusive endsInclusive) const {
        return contains(o.m_start, endsInclusive) || contains(o.m_end, endsInclusive) ||
               o.contains(m_start, endsInclusive) || o.contains(m_end, endsInclusive);
    }

    // 0 = exclude both ends, 1 = include end, exclude start, 2 = include start, exclude end, 3 = include both ends
    bool Range::contains(const Coordinates &coordinates, EndsInclusive endsInclusive) const {
        bool result = true;
        if ((u8)endsInclusive & 2)
            result &= m_start <= coordinates;
        else
            result &= m_start < coordinates;

        if (!result)
            return false;
        if ((u8)endsInclusive & 1)
            result &= coordinates <= m_end;
        else
            result &= coordinates < m_end;

        return result;
    }

    bool Range::containsLine(i32 value, EndsInclusive endsInclusive) const {
        bool result = true;
        if ((u8)endsInclusive & 2)
            result &= m_start.m_line <= value;
        else
            result &= m_start.m_line < value;

        if (!result)
            return false;
        if ((u8)endsInclusive & 1)
            result &= value <= m_end.m_line;
        else
            result &= value < m_end.m_line;

        return result;
    }

    bool Range::containsColumn(i32 value, EndsInclusive endsInclusive) const {
        bool result = true;
        if ((u8)endsInclusive & 2)
            result &= m_start.m_column <= value;
        else
            result &= m_start.m_column < value;

        if (!result)
            return false;
        if ((u8)endsInclusive & 1)
            result &= value <= m_end.m_column;
        else
            result &= value < m_end.m_column;

        return result;
    }

    bool Line::operator==(const Line &line) const {
        return m_chars == line.m_chars && m_colors == line.m_colors && m_flags == line.m_flags &&
               m_colorized == line.m_colorized && m_lineMaxColumn == line.m_lineMaxColumn;
    }

    bool Line::operator!=(const Line &line) const {
        return m_chars != line.m_chars || m_colors != line.m_colors || m_flags != line.m_flags ||
               m_colorized != line.m_colorized || m_lineMaxColumn != line.m_lineMaxColumn;
    }

    char LineIterator::operator*() {
        return *m_charsIter;
    }

    LineIterator &LineIterator::operator++() {
        ++m_charsIter;
        ++m_colorsIter;
        ++m_flagsIter;
        return *this;
    }

    LineIterator &LineIterator::operator=(const LineIterator &other) {
        m_charsIter = other.m_charsIter;
        m_colorsIter = other.m_colorsIter;
        m_flagsIter = other.m_flagsIter;
        return *this;
    }

    bool LineIterator::operator!=(const LineIterator &other) const {
        return m_charsIter != other.m_charsIter || m_colorsIter != other.m_colorsIter ||
               m_flagsIter != other.m_flagsIter;
    }

    bool LineIterator::operator==(const LineIterator &other) const {
        return m_charsIter == other.m_charsIter && m_colorsIter == other.m_colorsIter &&
               m_flagsIter == other.m_flagsIter;
    }

    LineIterator LineIterator::operator+(i32 n) {
        LineIterator iter = *this;
        iter.m_charsIter += n;
        iter.m_colorsIter += n;
        iter.m_flagsIter += n;
        return iter;
    }

    i32 LineIterator::operator-(LineIterator l) {
        return m_charsIter - l.m_charsIter;
    }

    LineIterator Line::begin() const {
        LineIterator iter;
        iter.m_charsIter = m_chars.begin();
        iter.m_colorsIter = m_colors.begin();
        iter.m_flagsIter = m_flags.begin();
        return iter;
    }

    LineIterator Line::end() const {
        LineIterator iter;
        iter.m_charsIter = m_chars.end();
        iter.m_colorsIter = m_colors.end();
        iter.m_flagsIter = m_flags.end();
        return iter;
    }

    LineIterator Line::begin() {
        LineIterator iter;
        iter.m_charsIter = m_chars.begin();
        iter.m_colorsIter = m_colors.begin();
        iter.m_flagsIter = m_flags.begin();
        return iter;
    }

    LineIterator Line::end() {
        LineIterator iter;
        iter.m_charsIter = m_chars.end();
        iter.m_colorsIter = m_colors.end();
        iter.m_flagsIter = m_flags.end();
        return iter;
    }

    Line &Line::operator=(const Line &line) = default;

    Line &Line::operator=(Line &&line) noexcept {
        m_chars = std::move(line.m_chars);
        m_colors = std::move(line.m_colors);
        m_flags = std::move(line.m_flags);
        m_colorized = line.m_colorized;
        m_lineMaxColumn = line.m_lineMaxColumn;
        return *this;
    }

    u64 Line::size() const {
        return m_chars.size();
    }

    char Line::front(LinePart part) const {
        if (part == LinePart::Chars && !m_chars.empty())
            return m_chars.front();
        if (part == LinePart::Colors && !m_colors.empty())
            return m_colors.front();
        if (part == LinePart::Flags && !m_flags.empty())
            return m_flags.front();
        return 0x00;
    }

    std::string Line::frontUtf8(LinePart part) const {
        if (part == LinePart::Chars && !m_chars.empty())
            return m_chars.substr(0, TextEditor::utf8CharLength(m_chars[0]));
        if (part == LinePart::Colors && !m_colors.empty())
            return m_colors.substr(0, TextEditor::utf8CharLength(m_chars[0]));
        if (part == LinePart::Flags && !m_flags.empty())
            return m_flags.substr(0, TextEditor::utf8CharLength(m_chars[0]));
        return "";
    }

    void Line::push_back(char c) {
        m_chars.push_back(c);
        m_colors.push_back(0x00);
        m_flags.push_back(0x00);
        m_colorized = false;
        m_lineMaxColumn = -1;
    }

    bool Line::empty() const {
        return m_chars.empty();
    }

    std::string Line::substr(u64 start, u64 length, LinePart part) const {

        if (part != LinePart::Utf8) {
            if (start >= m_chars.size() || m_colors.size() != m_chars.size() || m_flags.size() != m_chars.size())
                return "";
            if (length == (u64) -1 || start + length >= m_chars.size())
                length = m_chars.size() - start;
            if (length == 0)
                return "";

            if (part == LinePart::Chars)
                return m_chars.substr(start, length);
            if (part == LinePart::Colors)
                return m_colors.substr(start, length);
            if (part == LinePart::Flags)
                return m_flags.substr(start, length);
        } else {
            if (start >= (u64) maxColumn())
                return "";
            if (length == (u64) -1 || start + length >= (u64) maxColumn())
                length = maxColumn() - start;
            if (length == 0)
                return "";

            u64 utf8Start = 0;
            for (u64 utf8Index = 0; utf8Index < start; ++utf8Index) {
                utf8Start += TextEditor::utf8CharLength(m_chars[utf8Start]);
            }
            u64 utf8Length = 0;
            for (u64 utf8Index = 0; utf8Index < length; ++utf8Index) {
                utf8Length += TextEditor::utf8CharLength(m_chars[utf8Start + utf8Length]);
            }
            return m_chars.substr(utf8Start, utf8Length);
        }
        return "";
    }

    Line Line::subLine(u64 start, u64 length) {
        if (start >= m_chars.size())
            return const_cast<Line &>(m_emptyLine);
        if (m_colors.size() != m_chars.size())
            m_colors.resize(m_chars.size(), 0x00);
        if (m_flags.size() != m_chars.size())
            m_flags.resize(m_chars.size(), 0x00);
        if (length == (u64) -1 || start + length >= m_chars.size())
            length = m_chars.size() - start;
        if (length == 0)
            return const_cast<Line &>(m_emptyLine);

        std::string chars = m_chars.substr(start, length);
        std::string colors = m_colors.substr(start, length);
        std::string flags = m_flags.substr(start, length);
        Line result(chars, colors, flags);
        result.m_colorized = m_colorized;
        result.m_lineMaxColumn = result.maxColumn();
        return result;
    }

    char Line::at(u64 index) const {
        auto charsSize = (i64) m_chars.size();
        i64 signedIndex = (i64) index;
        if (signedIndex < 0)
            signedIndex = charsSize + signedIndex;
        if (signedIndex >= (i64)m_chars.size())
            throw std::out_of_range("Line::at: index out of range");
        return m_chars[index];
    }


    char Line::operator[](u64 index) const {
        auto charsSize = (i64) m_chars.size();
        if (charsSize == 0)
            return '\0';
        i64 signedIndex = std::clamp((i64) index,0 - charsSize,charsSize - 1);
        if (signedIndex < 0)
            return m_chars[charsSize + signedIndex];
        signedIndex = std::clamp(signedIndex, 0LL, charsSize - 1);
        return m_chars[signedIndex];
    }

    std::string Line::at(i64 index) const {
        i64 utf8Length = TextEditor::stringCharacterCount(m_chars);
        if (utf8Length == 0 || index >= utf8Length || index < -utf8Length)
            throw std::out_of_range("Line::at: index out of range");
        if (index < 0)
            index = utf8Length + index;
        if (index < 0 || index >= utf8Length)
            throw std::out_of_range("Line::at: index out of range");
        i64 utf8Start = 0;
        for (i64 utf8Index = 0; utf8Index < index; ++utf8Index) {
            if (utf8Start >= (i64) m_chars.size())
                throw std::out_of_range("Line::at: index out of range");
            utf8Start += TextEditor::utf8CharLength(m_chars[utf8Start]);
        }
        i64 utf8CharLen = TextEditor::utf8CharLength(m_chars[utf8Start]);
        if (utf8Start + utf8CharLen > (i64) m_chars.size())
            throw std::out_of_range("Line::at: incomplete UTF-8 character at index");
        return m_chars.substr(utf8Start, utf8CharLen);
    }

    // C++ can't overload functions based on return type, so use any type other
    // than u64 to avoid ambiguity.
    std::string Line::operator[](i64 index) const {
        i64 utf8Length = TextEditor::stringCharacterCount(m_chars);
        if (utf8Length == 0)
            return "";
        index = std::clamp(index, -utf8Length, utf8Length - 1);
        if (index < 0)
            index = utf8Length + index;
        index = std::clamp(index, 0LL, utf8Length - 1);
        i64 utf8Start = 0;
        for (i64 utf8Index = 0; utf8Index < index; ++utf8Index) {
            utf8Start += TextEditor::utf8CharLength(m_chars[utf8Start]);
        }
        i64 utf8CharLen = TextEditor::utf8CharLength(m_chars[utf8Start]);
        if (utf8Start + utf8CharLen > (i64) m_chars.size())
            utf8CharLen = m_chars.size() - utf8Start;
        return m_chars.substr(utf8Start, utf8CharLen);
    }

    void Line::setNeedsUpdate(bool needsUpdate) {
        m_colorized = m_colorized && !needsUpdate;
    }

    void Line::append(const char *text) {
        append(std::string(text));
    }

    void Line::append(const char text) {
        append(std::string(1, text));
    }

    void Line::append(const std::string &text) {
        Line line(text);
        append(line);
    }

    void Line::append(const Line &line) {
        append(line.begin(), line.end());
    }

    void Line::append(LineIterator begin, LineIterator end) {
        if (begin.m_charsIter < end.m_charsIter) {
            m_chars.append(begin.m_charsIter, end.m_charsIter);
            m_lineMaxColumn = -1;
        }
        if (begin.m_colorsIter < end.m_colorsIter)
            m_colors.append(begin.m_colorsIter, end.m_colorsIter);
        if (begin.m_flagsIter < end.m_flagsIter)
            m_flags.append(begin.m_flagsIter, end.m_flagsIter);
        m_colorized = false;
    }

    void Line::insert(LineIterator iter, const std::string &text) {
        insert(iter, text.begin(), text.end());
    }

    void Line::insert(LineIterator iter, const char text) {
        insert(iter, std::string(1, text));
    }

    void Line::insert(LineIterator iter, strConstIter beginString, strConstIter endString) {
        Line line(std::string(beginString, endString));
        insert(iter, line);
    }

    void Line::insert(LineIterator iter, const Line &line) {
        insert(iter, line.begin(), line.end());
    }

    void Line::insert(LineIterator iter, LineIterator beginLine, LineIterator endLine) {
        if (iter == end())
            append(beginLine, endLine);
        else {
            m_chars.insert(iter.m_charsIter, beginLine.m_charsIter, endLine.m_charsIter);
            m_colors.insert(iter.m_colorsIter, beginLine.m_colorsIter, endLine.m_colorsIter);
            m_flags.insert(iter.m_flagsIter, beginLine.m_flagsIter, endLine.m_flagsIter);
            m_colorized = false;
            m_lineMaxColumn = -1;
        }
    }

    void Line::erase(LineIterator begin) {
        m_chars.erase(begin.m_charsIter);
        m_colors.erase(begin.m_colorsIter);
        m_flags.erase(begin.m_flagsIter);
        m_colorized = false;
        m_lineMaxColumn = -1;
    }

    void Line::erase(LineIterator begin, u64 count) {
        if (count == (u64) -1)
            count = m_chars.size() - (begin.m_charsIter - m_chars.begin());
        else
            count = std::min(count, (u64) (m_chars.size() - (begin.m_charsIter - m_chars.begin())));
        m_chars.erase(begin.m_charsIter, begin.m_charsIter + count);
        m_colors.erase(begin.m_colorsIter, begin.m_colorsIter + count);
        m_flags.erase(begin.m_flagsIter, begin.m_flagsIter + count);
        m_colorized = false;
        m_lineMaxColumn = -1;
    }

    void Line::erase(u64 start, i64 length) {
        if (m_chars.empty() || start >= (u64) maxColumn())
            return;
        if (length < 0 || start >= (u64) maxColumn() - length)
            length = maxColumn() - start;
        u64 utf8Start = 0;
        for (u64 utf8Index = 0; utf8Index < start; ++utf8Index) {
            utf8Start += TextEditor::utf8CharLength(m_chars[utf8Start]);
        }
        u64 utf8Length = 0;
        for (i64 utf8Index = 0; utf8Index < length; ++utf8Index) {
            utf8Length += TextEditor::utf8CharLength(m_chars[utf8Start + utf8Length]);
        }
        utf8Length = std::min(utf8Length, (u64) (m_chars.size() - utf8Start));
        erase(begin() + utf8Start, utf8Length);
    }

    void Line::clear() {
        m_chars.clear();
        m_colors.clear();
        m_flags.clear();
        m_colorized = false;
        m_lineMaxColumn = -1;
    }

    void Line::setLine(const std::string &text) {
        m_chars = text;
        m_colors = std::string(text.size(), 0x00);
        m_flags = std::string(text.size(), 0x00);
        m_colorized = false;
        m_lineMaxColumn = -1;
    }

    void Line::setLine(const Line &text) {
        m_chars = text.m_chars;
        m_colors = text.m_colors;
        m_flags = text.m_flags;
        m_colorized = text.m_colorized;
        m_lineMaxColumn = text.m_lineMaxColumn;
    }

    bool Line::needsUpdate() const {
        return !m_colorized;
    }

    Interval &Interval::operator=(const Interval &interval) = default;

    Interval &Interval::operator=(Interval &&interval) noexcept {
        m_start = interval.m_start;
        m_end = interval.m_end;
        return *this;
    }

    bool TextEditor::ActionableBox::trigger() {
        auto mousePos = ImGui::GetMousePos();
        return !(mousePos.x <= m_box.Min.x || mousePos.x >= m_box.Max.x ||
            mousePos.y < m_box.Min.y || mousePos.y > m_box.Max.y);
    }

    void TextEditor::ActionableBox::shiftBoxVertically(float lineCount, float lineHeight) {
        m_box.Min.y += lineCount * lineHeight;
        m_box.Max.y += lineCount * lineHeight;
    }

    void TextEditor::ErrorHoverBox::callback()  {
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

    bool CodeFold::isDetected() {
        return lines->m_codeFoldHighlighted == key;
    }

    TextEditor *TextEditor::getSourceCodeEditor() {
        if (m_sourceCodeEditor != nullptr)
            return m_sourceCodeEditor;
        return this;
    }

    bool TextEditor::isEmpty()  {
        return m_lines.isEmpty();
    }

    bool Lines::isEmpty()  {
        auto size = m_unfoldedLines.size();
        if (size > 1)
            return false;
        if (size == 0)
            return true;
        return m_unfoldedLines.at(0).empty();
    }

    void TextEditor::resetFoldedSelections() {
        for (auto &[row,foldedLine] : m_lines.m_foldedLines) {
            foldedLine.m_selection = Range(Invalid, Invalid);
        }
    }

    bool TextEditor::EditorState::operator==(const EditorState &o) const {
        return m_selection == o.m_selection && m_cursorPosition == o.m_cursorPosition;
    }

    void TextEditor::Lines::setSelection(const Range &selection) {
        m_state.m_selection = lineCoordinates(const_cast<Range &>(selection));
    }

    void TextEditor::setSelection(const Range &selection) {
        m_lines.setSelection(selection);
    }

    Range TextEditor::Lines::getSelection() const {
        return m_state.m_selection;
    }

    Range TextEditor::getSelection() const {
        return m_lines.getSelection();
    }

    void TextEditor::selectWordUnderCursor() {
        auto wordStart = findWordStart(getCursorPosition());
        setSelection(Range(wordStart, findWordEnd(wordStart)));
    }

    void TextEditor::selectAll() {
        setSelection(Range(m_lines.lineCoordinates(0, 0), m_lines.lineCoordinates(-1, -1)));
    }

    bool TextEditor::Lines::hasSelection()  {
        return !isEmpty() && m_state.m_selection.m_end > m_state.m_selection.m_start;
    }

    void TextEditor::Lines::addUndo(std::vector<UndoRecord> value) {
        if (m_readOnly)
            return;

        m_undoBuffer.resize(m_undoIndex + 1);
        m_undoBuffer.back() = UndoAction(value);
        m_undoIndex++;
    }

    TextEditor::PaletteIndex TextEditor::Lines::getColorIndexFromFlags(Line::Flags flags) {
        auto commentBits = flags.m_value & inComment;
        if (commentBits == (i32) Line::Comments::Global)
            return PaletteIndex::GlobalDocComment;
        if (commentBits == (i32) Line::Comments::BlockDoc)
            return PaletteIndex::DocBlockComment;
        if (commentBits == (i32) Line::Comments::Doc)
            return PaletteIndex::DocComment;
        if (commentBits == (i32) Line::Comments::Block)
            return PaletteIndex::BlockComment;
        if (commentBits == (i32) Line::Comments::Line)
            return PaletteIndex::Comment;
        if (flags.m_bits.deactivated)
            return PaletteIndex::PreprocessorDeactivated;
        if (flags.m_bits.preprocessor)
            return PaletteIndex::Directive;
        return PaletteIndex::Default;
    }

    void TextEditor::handleKeyboardInputs() {
        ImGuiIO &io = ImGui::GetIO();

        // command => Ctrl
        // control => Super
        // option  => Alt
        auto ctrl = io.KeyCtrl;
        auto alt = io.KeyAlt;
        auto shift = io.KeyShift;

        if (ImGui::IsWindowFocused()) {
            if (ImGui::IsWindowHovered())
                ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);

            io.WantCaptureKeyboard = true;
            io.WantTextInput = true;

            if (!m_lines.m_readOnly && !ctrl && !shift && !alt && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)))
                enterCharacter('\n', false);
            else if (!m_lines.m_readOnly && !ctrl && !alt && ImGui::IsKeyPressed(ImGuiKey_Tab))
                enterCharacter('\t', shift);

            if (!m_lines.m_readOnly && !io.InputQueueCharacters.empty()) {
                for (i32 i = 0; i < io.InputQueueCharacters.Size; i++) {
                    auto c = io.InputQueueCharacters[i];
                    if (c != 0 && (c == '\n' || c >= 32)) {
                        enterCharacter(c, shift);
                    }
                }
                io.InputQueueCharacters.resize(0);
            }
        }
    }

    void TextEditor::handleMouseInputs() {
        ImGuiIO &io = ImGui::GetIO();
        auto shift = io.KeyShift;
        auto ctrl = io.ConfigMacOSXBehaviors ? io.KeyAlt : io.KeyCtrl;
        auto alt = io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;

        if (ImGui::IsWindowHovered()) {
            if (!alt) {
                auto click = ImGui::IsMouseClicked(0);
                auto doubleClick = ImGui::IsMouseDoubleClicked(0);
                auto rightClick = ImGui::IsMouseClicked(1);
                auto t = ImGui::GetTime();
                auto tripleClick = click && !doubleClick && (m_lastClick != -1.0f && (t - m_lastClick) < io.MouseDoubleClickTime);
                bool resetBlinking = false;
                /*
                Left mouse button triple click
                */

                if (tripleClick) {
                    auto coordinates = screenPosCoordinates(ImGui::GetMousePos());
                    if (coordinates == Invalid)
                        return;
                    if (!ctrl) {
                        m_lines.m_state.m_cursorPosition = coordinates;
                        auto lineIndex = rowToLineIndex(lineIndexToRow(coordinates.m_line));
                        auto line = m_lines[lineIndex];
                        m_lines.m_state.m_selection.m_start = m_lines.foldedToUnfoldedCoords(Coordinates(lineIndex, 0));
                        m_lines.m_state.m_selection.m_end = m_lines.foldedToUnfoldedCoords(Coordinates(lineIndex, line.maxColumn()));
                    }

                    m_lastClick = -1.0f;
                    resetBlinking = true;
                }

                    /*
                    Left mouse button double click
                    */

                else if (doubleClick) {
                    auto coordinates = screenPosCoordinates(ImGui::GetMousePos());
                    if (coordinates == Invalid)
                        return;
                    if (!ctrl) {
                        m_lines.m_state.m_cursorPosition = coordinates;
                        m_lines.m_state.m_selection.m_start = findWordStart(m_lines.m_state.m_cursorPosition);
                        m_lines.m_state.m_selection.m_end = findWordEnd(m_lines.m_state.m_cursorPosition);
                    }

                    m_lastClick = (float) ImGui::GetTime();
                    resetBlinking = true;
                }

                    /*
                    Left mouse button click
                    */
                else if (click) {
                    auto coordinates = screenPosCoordinates(ImGui::GetMousePos());
                    if (coordinates == Invalid)
                        return;
                    if (ctrl) {
                        m_lines.m_state.m_cursorPosition = m_lines.m_interactiveSelection.m_start = m_lines.m_interactiveSelection.m_end = coordinates;
                        selectWordUnderCursor();
                    } else if (shift) {
                        m_lines.m_state.m_cursorPosition = m_lines.m_interactiveSelection.m_end = coordinates;
                        setSelection(m_lines.m_interactiveSelection);
                    } else {
                        m_lines.m_state.m_cursorPosition = m_lines.m_interactiveSelection.m_end  = m_lines.m_interactiveSelection.m_start = coordinates;
                        setSelection(m_lines.m_interactiveSelection);
                    }
                    m_lines.resetCursorBlinkTime();

                    m_lines.ensureCursorVisible();
                    m_lastClick = (float) ImGui::GetTime();
                } else if (rightClick) {
                    auto coordinates = screenPosCoordinates(ImGui::GetMousePos());
                    if (coordinates == Invalid)
                        return;
                    auto cursorPosition = coordinates;

                    if (!m_lines.hasSelection() || m_lines.m_state.m_selection.m_start > cursorPosition || cursorPosition > m_lines.m_state.m_selection.m_end) {
                        m_lines.m_state.m_cursorPosition = m_lines.m_interactiveSelection.m_start = m_lines.m_interactiveSelection.m_end = cursorPosition;
                        setSelection(m_lines.m_interactiveSelection);
                    }
                    m_lines.resetCursorBlinkTime();
                    m_raiseContextMenu = true;
                    ImGui::SetWindowFocus();
                }
                    // Mouse left button dragging (=> update selection)
                else if (ImGui::IsMouseDragging(0) && ImGui::IsMouseDown(0)) {
                    auto coordinates = screenPosCoordinates(ImGui::GetMousePos());
                    if (coordinates == Invalid)
                        return;
                    io.WantCaptureMouse = true;
                    m_lines.m_state.m_cursorPosition = m_lines.m_interactiveSelection.m_end = coordinates;
                    setSelection(m_lines.m_interactiveSelection);
                    m_lines.ensureCursorVisible();
                    resetBlinking = true;
                }
                if (resetBlinking)
                    m_lines.resetCursorBlinkTime();
            }
        }
    }

    bool Lines::isTrueMatchingDelimiter() {
        auto nearCoords = m_matchedDelimiter.m_nearCursor;
        auto matchedCoords = m_matchedDelimiter.m_matched;
        Line::Flags nearFlag(m_unfoldedLines[nearCoords.m_line].m_flags[nearCoords.m_column]);
        Line::Flags matchedFlag(m_unfoldedLines[matchedCoords.m_line].m_flags[matchedCoords.m_column]);
        return nearFlag.m_bits.matchedDelimiter && matchedFlag.m_bits.matchedDelimiter;
    }

    // the index here is array index so zero based
    void FindReplaceHandler::selectFound(Lines *lines, i32 found) {
        if (found < 0 || found >= (i64) m_matches.size())
            return;
        lines->setSelection(m_matches[found].m_selection);
        lines->setUnfoldIfNeeded(true);
        lines->setCursorPosition();
        lines->ensureSelectionNotFolded();
        lines->ensureCursorVisible();
    }

// The returned index is shown in the form
//  'index of count' so 1 based
    u32 FindReplaceHandler::findMatch(Lines *lines, i32 index) {

        if (lines->m_textChanged || m_optionsChanged) {
            std::string findWord = getFindWord();
            if (findWord.empty())
                return 0;
            resetMatches();
            findAllMatches(lines, findWord);
        }
        Coordinates targetPos = lines->m_state.m_cursorPosition;
        i32 count = m_matches.size();

        if (count == 0) {
            lines->setCursorPosition(targetPos, true);
            return 0;
        }

        if (lines->hasSelection()) {
            i32 matchIndex = 0;
            for (i32 i = 0; i < count; i++) {
                if (m_matches[i].m_selection == lines->m_state.m_selection) {
                    matchIndex = i;
                    break;
                }
            }
            if (matchIndex >= 0 && matchIndex < count) {
                while (matchIndex + index < 0)
                    index += count;
                auto rem = (matchIndex + index) % count;
                selectFound(lines, rem);
                return rem + 1;
            }
        }
        targetPos = index > 0 ? lines->m_state.m_selection.m_end : (index < 0 ? lines->m_state.m_selection.m_start : lines->m_state.m_selection.m_start + lines->lineCoordinates( 0, 1));


        if (index >= 0) {
            for (i32 i = 0; i < count; i++) {
                if (targetPos <= m_matches[i].m_selection.m_start) {
                    selectFound(lines, i);
                    return i + 1;
                }
            }
            selectFound(lines, 0);
            return 1;
        } else {
            for (i32 i = count - 1; i >= 0; i--) {
                if (targetPos >= m_matches[i].m_selection.m_end) {
                    selectFound(lines, i);
                    return i + 1;
                }
            }
            selectFound(lines, count - 1);
            return count;
        }
    }

// returns 1 based index
    u32 FindReplaceHandler::findPosition(Lines *lines, Coordinates pos, bool isNext) {
        if (lines->m_textChanged || m_optionsChanged) {
            std::string findWord = getFindWord();
            if (findWord.empty())
                return 0;
            resetMatches();
            findAllMatches(lines, findWord);
        }

        i32 count = m_matches.size();
        if (count == 0)
            return 0;
        if (isNext) {
            for (i32 i = 0; i < count; i++) {
                auto interval = Range(m_matches[i == 0 ? count - 1 : i - 1].m_selection.m_end,m_matches[i].m_selection.m_end);
                if (interval.contains(pos))
                    return i + 1;
            }
        } else {
            for (i32 i = 0; i < count; i++) {
                auto interval = Range(m_matches[i == 0 ? count - 1 : i - 1].m_selection.m_start,m_matches[i].m_selection.m_start);
                if (interval.contains(pos, Range::EndsInclusive::Start))
                    return i == 0 ? count : i;
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
        if (s.empty())
            return {};
        out.push_back('#');
        out.push_back('\\');
        out.push_back('b');
        for (auto ch: s) {
            if (strchr(metacharacters, ch))
                out.push_back('\\');
            out.push_back(ch);
        }
        out.push_back('\\');
        out.push_back('b');
        return out;
    }

    void FindReplaceHandler::setFindWord(Lines *lines, const std::string &findWord) {
        if (findWord != m_findWord) {
            findAllMatches(lines, findWord);
            m_findWord = findWord;
        }
    }

    void FindReplaceHandler::setMatchCase(Lines *lines, bool matchCase) {
        if (matchCase != m_matchCase) {
            m_matchCase = matchCase;
            m_optionsChanged = true;
            findAllMatches(lines, m_findWord);
        }
    }

    void FindReplaceHandler::setWholeWord(Lines *lines, bool wholeWord) {
        if (wholeWord != m_wholeWord) {
            m_wholeWord = wholeWord;
            m_optionsChanged = true;
            findAllMatches(lines, m_findWord);
        }
    }


    void FindReplaceHandler::setFindRegEx(Lines *lines, bool findRegEx) {
        if (findRegEx != m_findRegEx) {
            m_findRegEx = findRegEx;
            m_optionsChanged = true;
            findAllMatches(lines, m_findWord);
        }
    }

    void FindReplaceHandler::resetMatches() {
        m_matches.clear();
        m_findWord = "";
    }
/*
    void TextEditor::computeLPSArray(const std::string &pattern, std::vector<i32> & lps) {
        i32 length = 0; // length of the previous longest prefix suffix
        i32 i = 1;
        lps[0] = 0; // lps[0] is always 0
        i32 patternLength = pattern.length();

        while (i < patternLength) {
            if (pattern[i] == pattern[length]) {
                length++;
                lps[i] = length;
                i++;
            } else {
                if (length != 0)
                    length = lps[length - 1];
                else {
                    lps[i] = 0;
                    i++;
                }
            }
        }
    }

    std::vector<i32> TextEditor::KMPSearch(const std::string& text, const std::string& pattern) {
        i32 textLength = text.length();
        i32 patternLength = pattern.length();
        std::vector<i32> result;
        std::vector<i32> lps(patternLength);
        computeLPSArray(pattern, lps);

        i32 textIndex = 0;
        i32 patternIndex = 0;

        while (textIndex < textLength) {
            if (pattern[patternIndex] == text[textIndex]) {
                textIndex++;
                patternIndex++;
            }

            if (patternIndex == patternLength) {
                result.push_back(textIndex - patternIndex);
                patternIndex = lps[patternIndex - 1];
            } else if (textIndex < textLength && pattern[patternIndex] != text[textIndex]) {
                if (patternIndex != 0)
                    patternIndex = lps[patternIndex - 1];
                else
                    textIndex++;
            }
        }
        return result;
    }*/

// Performs actual search to fill mMatches
    bool FindReplaceHandler::findNext(Lines *lines, u64 &byteIndex) {

        u64 matchBytes = m_findWord.size();

        std::string wordLower = m_findWord;
        if (!getMatchCase())
            std::transform(wordLower.begin(), wordLower.end(), wordLower.begin(), ::tolower);

        std::string textSrc = lines->getText();
        if (!getMatchCase())
            std::transform(textSrc.begin(), textSrc.end(), textSrc.begin(), ::tolower);

        u64 textLoc;
        // TODO: use regexp find iterator in all cases
        //  to find all matches for FindAllMatches.
        //  That should make things faster (no need
        //  to call FindNext many times) and remove
        //  clunky match case code
        if (getWholeWord() || getFindRegEx()) {
            std::regex regularExpression;
            if (getFindRegEx()) {
                try {
                    regularExpression.assign(wordLower);
                } catch (const std::regex_error &e) {
                    hex::log::error("Error in regular expression: {}", e.what());
                    return false;
                }
            } else {
                try {
                    regularExpression.assign(make_wholeWord(wordLower));
                } catch (const std::regex_error &e) {
                    hex::log::error("Error in regular expression: {}", e.what());
                    return false;
                }
            }

            u64 pos = 0;
            std::sregex_iterator iter = std::sregex_iterator(textSrc.begin(), textSrc.end(), regularExpression);
            std::sregex_iterator end;
            if (!iter->ready())
                return false;
            u64 firstLoc = iter->position();

            if (firstLoc > byteIndex) {
                pos = firstLoc;
            } else {

                while (iter != end) {
                    iter++;
                    if (((pos = iter->position()) > byteIndex))
                        break;
                }
            }

            if (iter == end)
                return false;

            textLoc = pos;
        } else {
            // non regex search
            textLoc = textSrc.find(wordLower, byteIndex);
        }
        if (textLoc == std::string::npos)
            return false;
        EditorState state;
        state.m_selection = Range(lines->stringIndexCoords(textLoc, textSrc), lines->stringIndexCoords(textLoc + matchBytes, textSrc));
        state.m_cursorPosition = state.m_selection.m_end;
        if (!m_matches.empty() && state == m_matches.back())
            return false;
        m_matches.push_back(state);
        byteIndex = textLoc + 1;
        return true;
    }

    void FindReplaceHandler::findAllMatches(Lines *lines, std::string findWord) {

        if (findWord.empty()) {
            lines->ensureCursorVisible();
            m_findWord = "";
            m_matches.clear();
            return;
        }

        if (findWord == m_findWord && !lines->m_textChanged && !m_optionsChanged)
            return;

        if (m_optionsChanged)
            m_optionsChanged = false;

        u64 byteIndex = 0;
        m_matches.clear();
        m_findWord = findWord;
        auto startingPos = lines->m_state.m_cursorPosition;
        auto saveState = lines->m_state;
        Coordinates begin = lines->lineCoordinates(0, 0);
        lines->m_state.m_cursorPosition = begin;

        if (!findNext(lines, byteIndex)) {
            lines->m_state = saveState;
            lines->ensureCursorVisible();
            return;
        }
        EditorState state = m_matches.back();

        while (state.m_cursorPosition < startingPos) {
            if (!findNext(lines, byteIndex)) {
                lines->m_state = saveState;
                lines->ensureCursorVisible();
                return;
            }
            state = m_matches.back();
        }

        while (findNext(lines, byteIndex));


        lines->m_state = saveState;
        lines->ensureCursorVisible();
   }


    bool FindReplaceHandler::replace(Lines *lines, bool right) {
        if (m_matches.empty() || m_findWord == m_replaceWord || m_findWord.empty())
            return false;


        auto &state = lines->m_state;
        if (state.m_selection.contains(state.m_cursorPosition)) {
            state.m_cursorPosition = state.m_selection.m_start;
            if (lines->isStartOfLine()) {
                state.m_cursorPosition.m_line--;
                state.m_cursorPosition.m_column = lines->lineMaxColumn(state.m_cursorPosition.m_line);
            } else
                state.m_cursorPosition.m_column--;
        }
        i32 index = right ? 0 : -1;
        auto matchIndex = findMatch(lines, index);
        if (matchIndex != 0) {
            UndoRecord u;
            u.m_before = state;
            u.m_removed = lines->getSelectedText();
            u.m_removedRange = state.m_selection;
            lines->deleteSelection();
            if (getFindRegEx()) {
                std::string replacedText = std::regex_replace(lines->getText(), std::regex(m_findWord), m_replaceWord,std::regex_constants::format_first_only |std::regex_constants::format_no_copy);
                u.m_added = replacedText;
            } else
                u.m_added = m_replaceWord;

            u.m_addedRange.m_start = lines->lineCoordinates(lines->m_state.m_cursorPosition);
            lines->insertText(u.m_added);

            u.m_addedRange.m_end = lines->lineCoordinates(lines->m_state.m_cursorPosition);

            lines->ensureCursorVisible();
            ImGui::SetKeyboardFocusHere(0);

            u.m_after = state;
            m_undoBuffer.emplace_back(std::move(u));
            lines->m_textChanged = true;

            return true;
        }
        lines->m_state = state;
        return false;
    }

    bool FindReplaceHandler::replaceAll(Lines *lines) {
        u32 count = m_matches.size();
        m_undoBuffer.clear();
        for (u32 i = 0; i < count; i++)
            replace(lines, true);

        lines->addUndo(m_undoBuffer);
        return true;
    }

    ImRect TextEditor::Lines::getBoxForRow(u32 row) {
        auto boxSize = m_charAdvance.x + (((u32) m_charAdvance.x % 2) ? 2.0f : 1.0f);
        auto lineStartScreenPos = getLineStartScreenPos(0,row);
        ImVec2 lineNumberStartScreenPos = ImVec2(m_lineNumbersStartPos.x + m_lineNumberFieldWidth, lineStartScreenPos.y);
        ImRect result;
        result.Min = ImVec2(lineNumberStartScreenPos.x - (boxSize - 1) / 2, lineNumberStartScreenPos.y);
        result.Max = result.Min + ImVec2(boxSize - 1, m_charAdvance.y);
        return result;
    }

    void CodeFold::callback() {
        if (isOpen())
            close();
        else
            open();
    }

    bool CodeFold::startHovered() {
        return codeFoldStartCursorBox.trigger();
    }

    bool CodeFold::endHovered() {
        if (isOpen())
            return codeFoldEndCursorBox.trigger();
        return false;
    }

    bool CodeFold::isOpen() const {
        return lines->m_codeFoldState[key];
    }

    void CodeFold::open() {

        lines->openCodeFold(key);
    }

    void CodeFold::close() {
        lines->closeCodeFold(key, true);
    }

    void CodeFold::moveFold(float lineCount, float lineHeight) {
        codeFoldStartCursorBox.shiftBoxVertically(lineCount, lineHeight);
        codeFoldEndActionBox.shiftBoxVertically(lineCount, lineHeight);
        codeFoldEndCursorBox.shiftBoxVertically(lineCount, lineHeight);
        shiftBoxVertically(lineCount, lineHeight);
    }

    Line &Lines::at(i32 index)  {
        if (index < -size() || index >= size())
            throw std::out_of_range("Line index out of range");
        if (index < 0)
            index += size();
        auto row = lineIndexToRow(index);
        if (row < 0 || row > getGlobalRowMax())
            throw std::out_of_range("Line index out of range");
        if (!m_foldedLines.contains(row) || m_foldedLines.at(row).m_full.m_start.m_line != index)
            return m_unfoldedLines.at(index);
        return m_foldedLines.at(row).m_foldedLine;
    }

     Line &Lines::operator[](i32 index)  {
        if (size() != 0)
            index = std::clamp(index,-size(), size()-1);
        else
            index = 0;
        if (index < 0)
            index += size();
         i32 row = lineIndexToRow(index);
         i32 globalRowMax = getGlobalRowMax();
         if (globalRowMax > 0)
             row = std::clamp(row,0, globalRowMax);
         else
             row = 0;
         if (!m_foldedLines.contains(row) || m_foldedLines[row].m_full.m_start.m_line != index)
             return m_unfoldedLines.at(index);
         return m_foldedLines[row].m_foldedLine;
    }

    i32 Lines::size() const {
        return (i32)m_unfoldedLines.size();
    }

    void clearCodeFolds();

    u32 TextEditor::getfirstNonWhite(u32 lineIndex) {
        if (lineIndex >= (u64)m_lines.size())
            return 0;
        auto line = m_lines[lineIndex];
        u64 i = 0;
        while (i < line.size() && line[i] == ' ')
            i++;
        return i + 1;
    }

      i32 TextEditor::getTotalLines() const {
        return m_lines.size();
    }

    void TextEditor::Lines::setAllCodeFolds() {
        CodeFoldBlocks intervals = foldPointsFromSource();
        m_codeFoldKeys.clear();
        m_codeFolds.clear();
        m_codeFoldKeyMap.clear();
        m_codeFoldValueMap.clear();
        m_codeFoldKeyLineMap.clear();
        m_codeFoldValueLineMap.clear();
        m_codeFoldDelimiters.clear();
        for (auto interval : intervals) {
            Range foldInterval(interval);
            if (foldInterval.m_start.m_line > size() || foldInterval.m_end.m_line > size())
                return;
            std::pair<char,char> foldDelimiters = {};
            foldDelimiters.second = m_unfoldedLines[foldInterval.m_end.m_line].m_chars[foldInterval.m_end.m_column];
            if (foldDelimiters.second == '/')
                foldDelimiters.first = foldDelimiters.second;
            else
                foldDelimiters.first = foldDelimiters.second - 2 + (foldDelimiters.second == ')');
            m_codeFoldKeys.insert(foldInterval);
            m_codeFoldDelimiters[foldInterval] = foldDelimiters;
            if (!m_codeFoldKeyMap.contains(foldInterval.m_start))
                m_codeFoldKeyMap[foldInterval.m_start] = foldInterval.m_end;
            if (!m_codeFoldValueMap.contains(foldInterval.m_end))
                m_codeFoldValueMap[foldInterval.m_end] = foldInterval.m_start;
            m_codeFoldKeyLineMap.insert(std::make_pair(foldInterval.m_start.m_line,foldInterval.m_start));
            m_codeFoldValueLineMap.insert(std::make_pair(foldInterval.m_end.m_line,foldInterval.m_end));
        }

        std::vector<Range> deactivated = getDeactivatedBlocks();
        for (auto interval : deactivated) {
            m_codeFoldKeys.insert(interval);
            if (!m_codeFoldKeyMap.contains(interval.m_start))
                m_codeFoldKeyMap[interval.m_start] = interval.m_end;
            if (!m_codeFoldValueMap.contains(interval.m_end))
                m_codeFoldValueMap[interval.m_end] = interval.m_start;
            m_codeFoldKeyLineMap.insert(std::make_pair(interval.m_start.m_line,interval.m_start));
            m_codeFoldValueLineMap.insert(std::make_pair(interval.m_end.m_line,interval.m_end));
        }
    }

    void TextEditor::Lines::removeHiddenLinesFromPattern() {
        i32 lineIndex = 0;
        const auto totalLines = (i32)m_unfoldedLines.size();
        while (lineIndex < totalLines && m_unfoldedLines[lineIndex].m_chars.starts_with("//+-"))
            lineIndex++;
        if (lineIndex > 0) {
            m_hiddenLines.clear();
            setSelection(Range(lineCoordinates(0, 0), lineCoordinates(lineIndex, 0)));
            for (i32 i = 0; i < lineIndex; i++) {
                HiddenLine hiddenLine(i, m_unfoldedLines[i].m_chars);
                m_hiddenLines.emplace_back(std::move(hiddenLine));
                m_useSavedFoldStatesRequested = true;
            }
            deleteSelection();
        }
    }

    void TextEditor::Lines::addHiddenLinesToPattern() {
        if (m_hiddenLines.empty())
            return;
        for (const auto &hiddenLine : m_hiddenLines) {
            i32 lineIndex;
            if (hiddenLine.m_lineIndex < 0)
                lineIndex = size() + hiddenLine.m_lineIndex;
            else
                lineIndex = hiddenLine.m_lineIndex;
            if (m_unfoldedLines[lineIndex].m_chars.starts_with("//+-"))
                m_unfoldedLines.erase(m_unfoldedLines.begin() + lineIndex);
            insertLine(lineIndex, hiddenLine.m_line);
        }
    }

    i32 TextEditor::getCodeFoldLevel(i32 line) const {
        return std::count_if(m_lines.m_codeFoldKeys.begin(), m_lines.m_codeFoldKeys.end(), [line](const Range &key) {
            return key.m_start.m_line <= line && key.m_end.m_line >= line;
        });
    }

    void TextEditor::codeFoldCollapse(i32 level, bool recursive, bool all) {

        for (const auto& [key, codeFold]: m_lines.m_codeFolds) {
            if (key.containsLine(m_lines.m_state.m_cursorPosition.m_line) || all) {
                if (getCodeFoldLevel(key.m_start.m_line) >= level || level == 0) {
                    if (m_lines.m_codeFoldState.contains(key) && m_lines.m_codeFoldState[key]) {
                        m_lines.m_codeFoldState[key] = false;
                        m_lines.m_saveCodeFoldStateRequested = true;
                    }
                    if (recursive) {
                        for (const auto &k: m_lines.m_codeFoldKeys) {
                            if (key.contains(k) && m_lines.m_codeFoldState.contains(k) && m_lines.m_codeFoldState[k]) {
                                m_lines.m_codeFoldState[k] = false;
                                m_lines.m_saveCodeFoldStateRequested = true;
                            }
                        }
                    }
                    if (!all) {
                        break;
                    }
                }
            }
        }
    }

    void TextEditor::codeFoldExpand(i32 level, bool recursive, bool all) {
        for (const auto& [key, codeFold]: m_lines.m_codeFolds) {
            if (key.containsLine(m_lines.m_state.m_cursorPosition.m_line) || all) {
                if (getCodeFoldLevel(key.m_start.m_line) >= level || level == 0) {
                    if (m_lines.m_codeFoldState.contains(key) && !m_lines.m_codeFoldState[key]) {
                        m_lines.m_codeFoldState[key] = true;
                        m_lines.m_saveCodeFoldStateRequested = true;
                    }
                    if (recursive) {
                        for (const auto &k: m_lines.m_codeFoldKeys) {
                            if (key.contains(k) && m_lines.m_codeFoldState.contains(k) && !m_lines.m_codeFoldState[k]) {
                                m_lines.m_codeFoldState[k] = true;
                                m_lines.m_saveCodeFoldStateRequested = true;
                            }
                        }
                    }
                    if (!all) {
                        break;
                    }
                }
            }
        }
    }
    bool TextEditor::Lines::isTokenIdValid(i32 tokenId) {
        return tokenId >= 0 && tokenId < (i32) m_tokens.size();
    }

    bool TextEditor::Lines::isLocationValid(Location location) {
        const pl::api::Source *source;
        try {
            source = location.source;
        } catch  (const std::out_of_range &e) {
            log::error("TextHighlighter::IsLocationValid: Out of range error: {}", e.what());
            return false;
        }
        if (source == nullptr)
            return false;
        return location > m_tokens.front().location && location < m_tokens.back().location;
    }

    void TextEditor::Lines::loadFirstTokenIdOfLine() {
        const u32 tokenCount = m_tokens.size();
        m_firstTokenIdOfLine.clear();
        u32 tokenId = 0;
        u32 lineIndex = m_tokens.at(tokenId).location.line - 1;
        const u32 count = size();
        m_firstTokenIdOfLine.resize(count, -1);
        m_firstTokenIdOfLine.at(lineIndex) = 0;
        tokenId++;
        constexpr auto commentType = pl::core::Token::Type::Comment;
        constexpr auto docCommentType = pl::core::Token::Type::DocComment;
        u32 currentLine = lineIndex;
        while (currentLine < count) {
            while (tokenId < tokenCount && (currentLine >= lineIndex)) {
                lineIndex = m_tokens.at(tokenId).location.line - 1;
                tokenId++;
            }
            if (tokenId > tokenCount - 1)
                break;
            if (tokenId > 0)
                tokenId--;
            auto type = m_tokens.at(tokenId).type;
            if ((type == docCommentType || type == commentType)) {
                auto *comment = std::get_if<pl::core::Token::Comment>(&(m_tokens.at(tokenId).value));
                auto *docComment = std::get_if<pl::core::Token::DocComment>(&(m_tokens.at(tokenId).value));
                if ((comment != nullptr && !comment->singleLine) || (docComment != nullptr && !docComment->singleLine)) {
                    auto commentTokenId = tokenId;
                    auto commentStartLine = m_tokens.at(tokenId).location.line - 1;
                    std::string value = m_tokens.at(tokenId).getFormattedValue();
                    auto commentEndLine = commentStartLine + std::count(value.begin(), value.end(), '\n');
                    for (u32 i = commentStartLine; i <= commentEndLine; i++) {
                        m_firstTokenIdOfLine.at(i) = commentTokenId;
                    }
                    lineIndex = commentEndLine;
                } else
                    m_firstTokenIdOfLine.at(lineIndex) = tokenId;
            } else
                m_firstTokenIdOfLine.at(lineIndex) = tokenId;
            tokenId++;
            currentLine = lineIndex;
        }

        if (tokenCount > 0 && (u32) m_firstTokenIdOfLine.back() != tokenCount - 1)
            m_firstTokenIdOfLine.push_back(tokenCount - 1);

    }

    pl::core::Location TextEditor::Lines::getLocation(i32 tokenId) {

        if (tokenId >= (i32) m_tokens.size())
            return Location::Empty();
        return m_tokens[tokenId].location;
    }

    i32 TextEditor::Lines::getTokenId(hex::ui::TextEditor::SafeTokenIterator tokenIterator) {
            auto start = m_tokens.data();
            auto m_start = &tokenIterator.front();
            auto m_end = &tokenIterator.back();
            if (m_start < start || start > m_end) {
                throw std::out_of_range("iterator out of range");
            }
            return m_start - start;
    }

    i32 TextEditor::Lines::getTokenId() {
        auto start = m_tokens.data();
        auto m_start = &m_curr.front();
        auto m_end = &m_curr.back();
        if (m_start < start || start > m_end) {
            throw std::out_of_range("iterator out of range");
        }
        return m_start - start;
    }

// Get the token index for a given location.
    i32 TextEditor::Lines::getTokenId(pl::core::Location location) {
        if (location == m_tokens.at(0).location)
            return 0;
        if (location == m_tokens.back().location)
            return (i32) m_tokens.size() - 1;

        if (!isLocationValid(location))
            return -1;
        i32 line1 = location.line - 1;
        i32 line2 = nextLine(line1);
        auto tokenCount = m_tokens.size();
        i32 lineCount = m_firstTokenIdOfLine.size();
        if (line1 < 0 || line1 >= lineCount || tokenCount == 0)
            return -1;
        i32 tokenStart = m_firstTokenIdOfLine[line1];
        i32 tokenEnd = tokenCount - 1;
        if (line2 < lineCount && line2 >= 0)
            tokenEnd = m_firstTokenIdOfLine[line2] - 1;

        if (tokenStart == -1 || tokenEnd == -1 || tokenStart >= (i32) tokenCount)
            return -1;

        for (i32 i = tokenStart; i <= tokenEnd; i++) {
            auto location2 = m_tokens[i].location;
            if (location2.column <= location.column && location2.column + location2.length >= location.column)
                return i + 1;
        }
        return -1;
    }

    i32 TextEditor::Lines::nextLine(i32 line) {
        if (line < 0 || line >= size())
            return -1;
        auto currentTokenId = m_firstTokenIdOfLine[line];
        i32 i = 1;
        while (line + i < size() &&
               (m_firstTokenIdOfLine[line + i] == currentTokenId || m_firstTokenIdOfLine[line + i] == (i32) 0xFFFFFFFF))
            i++;
        return i + line;
    }
}