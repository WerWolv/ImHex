#include <ui/text_editor.hpp>
#include <hex/helpers/logger.hpp>
#include <algorithm>

namespace hex::ui {
    using Coordinates        = TextEditor::Coordinates;
    using Line               = TextEditor::Line;
    using LineIterator       = TextEditor::LineIterator;
    using Range              = TextEditor::Range;
    using FindReplaceHandler = TextEditor::FindReplaceHandler;
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
        return Coordinates(m_line + o.m_line, m_column + o.m_column);
    }

    Coordinates Coordinates::operator-(const Coordinates &o) const {
        return Coordinates(m_line - o.m_line, m_column - o.m_column);
    }

    bool Range::operator==(const Range &o) const {
        return m_start == o.m_start && m_end == o.m_end;
    }
    bool Range::operator!=(const Range &o) const {
        return m_start != o.m_start || m_end != o.m_end;
    }

    Coordinates Range::getSelectedLines() {
        return Coordinates(m_start.m_line, m_end.m_line);
    }

    Coordinates Range::getSelectedColumns() {
        if (isSingleLine())
            return Coordinates(m_start.m_column, m_end.m_column - m_start.m_column);
        return Coordinates(m_start.m_column, m_end.m_column);
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

    LineIterator LineIterator::operator++() {
        LineIterator iter = *this;
        ++iter.m_charsIter;
        ++iter.m_colorsIter;
        ++iter.m_flagsIter;
        return iter;
    }

    LineIterator LineIterator::operator=(const LineIterator &other) {
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

    Line &Line::operator=(const Line &line) {
        m_chars = line.m_chars;
        m_colors = line.m_colors;
        m_flags = line.m_flags;
        m_colorized = line.m_colorized;
        m_lineMaxColumn = line.m_lineMaxColumn;
        return *this;
    }

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

    char Line::operator[](u64 index) const {
        i64 signedIndex = std::clamp((i64) index,0 - (i64) m_chars.size(), (i64) (m_chars.size() - 1));
        if (signedIndex < 0)
            return m_chars[m_chars.size() + signedIndex];
        return m_chars[signedIndex];
    }

    // C++ can't overload functions based on return type, so use any type other
    // than u64 to avoid ambiguity.
    std::string Line::operator[](i64 index) const {
        i64 utf8Length = TextEditor::stringCharacterCount(m_chars);
        index = std::clamp(index, (i64) -utf8Length, (i64) utf8Length - 1);
        if (index < 0)
            index = utf8Length + index;
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

    bool TextEditor::ActionableBox::trigger() {
        auto mousePos = ImGui::GetMousePos();
        if (mousePos.x <= m_box.Min.x || mousePos.x >= m_box.Max.x ||
            mousePos.y < m_box.Min.y || mousePos.y > m_box.Max.y)
            return false;
        return true;
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

    TextEditor *TextEditor::getSourceCodeEditor() {
        if (m_sourceCodeEditor != nullptr)
            return m_sourceCodeEditor;
        return this;
    }

    bool TextEditor::isEmpty() const {
        auto size = m_lines.size();
        if (size > 1)
            return false;
        if (size == 0)
            return true;
        return m_lines[0].empty();
    }

    bool TextEditor::EditorState::operator==(const EditorState &o) const {
        return m_selection == o.m_selection && m_cursorPosition == o.m_cursorPosition;
    }

    void TextEditor::setSelection(const Range &selection) {
        m_state.m_selection = setCoordinates(selection);
    }

    Range TextEditor::getSelection() const {
        return m_state.m_selection;
    }

    void TextEditor::selectWordUnderCursor() {
        auto wordStart = findWordStart(getCursorPosition());
        setSelection(Range(wordStart, findWordEnd(wordStart)));
    }

    void TextEditor::selectAll() {
        setSelection(Range(Coordinates(this, 0, 0), Coordinates(this, -1, -1)));
    }

    bool TextEditor::hasSelection() const {
        return !isEmpty() && m_state.m_selection.m_end > m_state.m_selection.m_start;
    }

    void TextEditor::addUndo(UndoRecords &value) {
        if (m_readOnly)
            return;

        m_undoBuffer.resize((u64) (m_undoIndex + 1));
        m_undoBuffer.back() = UndoAction(value);
        m_undoIndex++;
    }

    TextEditor::PaletteIndex TextEditor::getColorIndexFromFlags(Line::Flags flags) {
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

            if (!m_readOnly && !ctrl && !shift && !alt && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)))
                enterCharacter('\n', false);
            else if (!m_readOnly && !ctrl && !alt && ImGui::IsKeyPressed(ImGuiKey_Tab))
                enterCharacter('\t', shift);

            if (!m_readOnly && !io.InputQueueCharacters.empty()) {
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
                    if (!ctrl) {
                        m_state.m_cursorPosition = screenPosToCoordinates(ImGui::GetMousePos());
                        auto line = m_state.m_cursorPosition.m_line;
                        m_state.m_selection.m_start = setCoordinates(line, 0);
                        m_state.m_selection.m_end = setCoordinates(line, lineMaxColumn(line));
                    }

                    m_lastClick = -1.0f;
                    resetBlinking = true;
                }

                    /*
                    Left mouse button double click
                    */

                else if (doubleClick) {
                    if (!ctrl) {
                        m_state.m_cursorPosition = screenPosToCoordinates(ImGui::GetMousePos());
                        m_state.m_selection.m_start = findWordStart(m_state.m_cursorPosition);
                        m_state.m_selection.m_end = findWordEnd(m_state.m_cursorPosition);
                    }

                    m_lastClick = (float) ImGui::GetTime();
                    resetBlinking = true;
                }

                    /*
                    Left mouse button click
                    */
                else if (click) {
                    if (ctrl) {
                        m_state.m_cursorPosition = m_interactiveSelection.m_start = m_interactiveSelection.m_end = screenPosToCoordinates(ImGui::GetMousePos());
                        selectWordUnderCursor();
                    } else if (shift) {
                        m_interactiveSelection.m_end = screenPosToCoordinates(ImGui::GetMousePos());
                        m_state.m_cursorPosition = m_interactiveSelection.m_end;
                        setSelection(m_interactiveSelection);
                    } else {
                        m_state.m_cursorPosition = m_interactiveSelection.m_start = m_interactiveSelection.m_end = screenPosToCoordinates(ImGui::GetMousePos());
                        setSelection(m_interactiveSelection);
                    }
                    resetCursorBlinkTime();

                    ensureCursorVisible();
                    m_lastClick = (float) ImGui::GetTime();
                } else if (rightClick) {
                    auto cursorPosition = screenPosToCoordinates(ImGui::GetMousePos());

                    if (!hasSelection() || m_state.m_selection.m_start > cursorPosition || cursorPosition > m_state.m_selection.m_end) {
                        m_state.m_cursorPosition = m_interactiveSelection.m_start = m_interactiveSelection.m_end = cursorPosition;
                        setSelection(m_interactiveSelection);
                    }
                    resetCursorBlinkTime();
                    m_raiseContextMenu = true;
                    ImGui::SetWindowFocus();
                }
                    // Mouse left button dragging (=> update selection)
                else if (ImGui::IsMouseDragging(0) && ImGui::IsMouseDown(0)) {
                    io.WantCaptureMouse = true;
                    m_state.m_cursorPosition = m_interactiveSelection.m_end = screenPosToCoordinates(ImGui::GetMousePos());
                    setSelection(m_interactiveSelection);
                    ensureCursorVisible();
                    resetBlinking = true;
                }
                if (resetBlinking)
                    resetCursorBlinkTime();
            }
        }
    }

    // the index here is array index so zero based
    void FindReplaceHandler::selectFound(TextEditor *editor, i32 found) {
        if (found < 0 || found >= (i64) m_matches.size())
            return;
        editor->setSelection(m_matches[found].m_selection);
        editor->setCursorPosition();
    }

// The returned index is shown in the form
//  'index of count' so 1 based
    u32 FindReplaceHandler::findMatch(TextEditor *editor, i32 index) {

        if (editor->m_textChanged || m_optionsChanged) {
            std::string findWord = getFindWord();
            if (findWord.empty())
                return 0;
            resetMatches();
            findAllMatches(editor, findWord);
        }
        Coordinates targetPos = editor->m_state.m_cursorPosition;
        i32 count = m_matches.size();

        if (count == 0) {
            editor->setCursorPosition(targetPos, true);
            return 0;
        }

        if (editor->hasSelection()) {
            i32 matchIndex = 0;
            for (i32 i = 0; i < count; i++) {
                if (m_matches[i].m_selection == editor->m_state.m_selection) {
                    matchIndex = i;
                    break;
                }
            }
            if (matchIndex >= 0 && matchIndex < count) {
                while (matchIndex + index < 0)
                    index += count;
                auto rem = (matchIndex + index) % count;
                selectFound(editor, rem);
                return rem + 1;
            }
        }
        targetPos = index > 0 ? editor->m_state.m_selection.m_end : (index < 0 ? editor->m_state.m_selection.m_start :  editor->m_state.m_selection.m_start + Coordinates(0,1));


        if (index >= 0) {
            for (i32 i = 0; i < count; i++) {
                if (targetPos <= m_matches[i].m_selection.m_start) {
                    selectFound(editor, i);
                    return i + 1;
                }
            }
            selectFound(editor, 0);
            return 1;
        } else {
            for (i32 i = count - 1; i >= 0; i--) {
                if (targetPos >= m_matches[i].m_selection.m_end) {
                    selectFound(editor, i);
                    return i + 1;
                }
            }
            selectFound(editor, count - 1);
            return count;
        }
    }

// returns 1 based index
    u32 FindReplaceHandler::findPosition(TextEditor *editor, Coordinates pos, bool isNext) {
        if (editor->m_textChanged || m_optionsChanged) {
            std::string findWord = getFindWord();
            if (findWord.empty())
                return 0;
            resetMatches();
            findAllMatches(editor, findWord);
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
        if (s[0] == '#')
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

    void FindReplaceHandler::setFindWord(TextEditor *editor, const std::string &findWord) {
        if (findWord != m_findWord) {
            findAllMatches(editor, findWord);
            m_findWord = findWord;
        }
    }

    void FindReplaceHandler::setMatchCase(TextEditor *editor, bool matchCase) {
        if (matchCase != m_matchCase) {
            m_matchCase = matchCase;
            m_optionsChanged = true;
            findAllMatches(editor, m_findWord);
        }
    }

    void FindReplaceHandler::setWholeWord(TextEditor *editor, bool wholeWord) {
        if (wholeWord != m_wholeWord) {
            m_wholeWord = wholeWord;
            m_optionsChanged = true;
            findAllMatches(editor, m_findWord);
        }
    }


    void FindReplaceHandler::setFindRegEx(TextEditor *editor, bool findRegEx) {
        if (findRegEx != m_findRegEx) {
            m_findRegEx = findRegEx;
            m_optionsChanged = true;
            findAllMatches(editor, m_findWord);
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
    bool FindReplaceHandler::findNext(TextEditor *editor, u64 &byteIndex) {

        u64 matchBytes = m_findWord.size();

        std::string wordLower = m_findWord;
        if (!getMatchCase())
            std::transform(wordLower.begin(), wordLower.end(), wordLower.begin(), ::tolower);

        std::string textSrc = editor->getText();
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
        TextEditor::EditorState state;
        state.m_selection = Range(TextEditor::stringIndexToCoordinates(textLoc, textSrc), TextEditor::stringIndexToCoordinates(textLoc + matchBytes, textSrc));
        state.m_cursorPosition = state.m_selection.m_end;
        if (!m_matches.empty() && state == m_matches.back())
            return false;
        m_matches.push_back(state);
        byteIndex = textLoc + 1;
        return true;
    }

    void FindReplaceHandler::findAllMatches(TextEditor *editor, std::string findWord) {

        if (findWord.empty()) {
            editor->ensureCursorVisible();
            m_findWord = "";
            m_matches.clear();
            return;
        }

        if (findWord == m_findWord && !editor->m_textChanged && !m_optionsChanged)
            return;

        if (m_optionsChanged)
            m_optionsChanged = false;

        u64 byteIndex = 0;
        m_matches.clear();
        m_findWord = findWord;
        auto startingPos = editor->m_state.m_cursorPosition;
        auto saveState = editor->m_state;
        Coordinates begin = editor->setCoordinates(0, 0);
        editor->m_state.m_cursorPosition = begin;

        if (!findNext(editor, byteIndex)) {
            editor->m_state = saveState;
            editor->ensureCursorVisible();
            return;
        }
        TextEditor::EditorState state = m_matches.back();

        while (state.m_cursorPosition < startingPos) {
            if (!findNext(editor, byteIndex)) {
                editor->m_state = saveState;
                editor->ensureCursorVisible();
                return;
            }
            state = m_matches.back();
        }

        while (findNext(editor, byteIndex));

        editor->m_state = saveState;
        editor->ensureCursorVisible();
        return;
    }


    bool FindReplaceHandler::replace(TextEditor *editor, bool right) {
        if (m_matches.empty() || m_findWord == m_replaceWord || m_findWord.empty())
            return false;


        auto state = editor->m_state;
        if (editor->m_state.m_selection.contains(editor->m_state.m_cursorPosition)) {
            editor->m_state.m_cursorPosition = editor->m_state.m_selection.m_start;
            if (editor->isStartOfLine()) {
                editor->m_state.m_cursorPosition.m_line--;
                editor->m_state.m_cursorPosition.m_column = editor->lineMaxColumn(editor->m_state.m_cursorPosition.m_line);
            } else
                editor->m_state.m_cursorPosition.m_column--;
        }
        i32 index = right ? 0 : -1;
        auto matchIndex = findMatch(editor, index);
        if (matchIndex != 0) {
            UndoRecord u;
            u.m_before = editor->m_state;
            u.m_removed = editor->getSelectedText();
            u.m_removedRange = editor->m_state.m_selection;
            editor->deleteSelection();
            if (getFindRegEx()) {
                std::string replacedText = std::regex_replace(editor->getText(), std::regex(m_findWord), m_replaceWord, std::regex_constants::format_first_only | std::regex_constants::format_no_copy);
                u.m_added = replacedText;
            } else
                u.m_added = m_replaceWord;

            u.m_addedRange.m_start = editor->setCoordinates(editor->m_state.m_cursorPosition);
            editor->insertText(u.m_added);

            editor->setCursorPosition(editor->m_state.m_selection.m_end);

            u.m_addedRange.m_end = editor->setCoordinates(editor->m_state.m_cursorPosition);

            editor->ensureCursorVisible();
            ImGui::SetKeyboardFocusHere(0);

            u.m_after = editor->m_state;
            m_undoBuffer.push_back(u);
            editor->m_textChanged = true;

            return true;
        }
        editor->m_state = state;
        return false;
    }

    bool FindReplaceHandler::replaceAll(TextEditor *editor) {
        u32 count = m_matches.size();
        m_undoBuffer.clear();
        for (u32 i = 0; i < count; i++)
            replace(editor, true);

        editor->addUndo(m_undoBuffer);
        return true;
    }
}
