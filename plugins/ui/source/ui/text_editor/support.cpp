#include <ui/text_editor.hpp>
#include <hex/helpers/logger.hpp>
#include <algorithm>

namespace hex::ui {
    bool TextEditor::Coordinates::operator==(const Coordinates &o) const {
        return
                m_line == o.m_line &&
                m_column == o.m_column;
    }

    bool TextEditor::Coordinates::operator!=(const Coordinates &o) const {
        return
                m_line != o.m_line ||
                m_column != o.m_column;
    }

    bool TextEditor::Coordinates::operator<(const Coordinates &o) const {
        if (m_line != o.m_line)
            return m_line < o.m_line;
        return m_column < o.m_column;
    }

    bool TextEditor::Coordinates::operator>(const Coordinates &o) const {
        if (m_line != o.m_line)
            return m_line > o.m_line;
        return m_column > o.m_column;
    }

    bool TextEditor::Coordinates::operator<=(const Coordinates &o) const {
        if (m_line != o.m_line)
            return m_line < o.m_line;
        return m_column <= o.m_column;
    }

    bool TextEditor::Coordinates::operator>=(const Coordinates &o) const {
        if (m_line != o.m_line)
            return m_line > o.m_line;
        return m_column >= o.m_column;
    }

    TextEditor::Coordinates TextEditor::Coordinates::operator+(const Coordinates &o) const {
        return Coordinates(m_line + o.m_line, m_column + o.m_column);
    }

    TextEditor::Coordinates TextEditor::Coordinates::operator-(const Coordinates &o) const {
        return Coordinates(m_line - o.m_line, m_column - o.m_column);
    }

    TextEditor::Coordinates TextEditor::Selection::getSelectedLines() {
        return Coordinates(m_start.m_line, m_end.m_line);
    }

    TextEditor::Coordinates TextEditor::Selection::getSelectedColumns() {
        if (isSingleLine())
            return Coordinates(m_start.m_column, m_end.m_column - m_start.m_column);
        return Coordinates(m_start.m_column, m_end.m_column);
    }

    bool TextEditor::Selection::isSingleLine() {
        return m_start.m_line == m_end.m_line;
    }

    bool TextEditor::Selection::contains(Coordinates coordinates, int8_t endsInclusive) {
        bool result = true;
        if (endsInclusive & 2)
            result &= m_start <= coordinates;
        else
            result &= m_start < coordinates;

        if (!result)
            return false;
        if (endsInclusive & 1)
            result &= coordinates <= m_end;
        else
            result &= coordinates < m_end;

        return result;
    }

    char TextEditor::Line::LineIterator::operator*() {
        return *m_charsIter;
    }

    TextEditor::Line::LineIterator TextEditor::Line::LineIterator::operator++() {
        LineIterator iter = *this;
        ++iter.m_charsIter;
        ++iter.m_colorsIter;
        ++iter.m_flagsIter;
        return iter;
    }

    TextEditor::Line::LineIterator TextEditor::Line::LineIterator::operator=(const LineIterator &other) {
        m_charsIter = other.m_charsIter;
        m_colorsIter = other.m_colorsIter;
        m_flagsIter = other.m_flagsIter;
        return *this;
    }

    bool TextEditor::Line::LineIterator::operator!=(const LineIterator &other) const {
        return m_charsIter != other.m_charsIter || m_colorsIter != other.m_colorsIter ||
               m_flagsIter != other.m_flagsIter;
    }

    bool TextEditor::Line::LineIterator::operator==(const LineIterator &other) const {
        return m_charsIter == other.m_charsIter && m_colorsIter == other.m_colorsIter &&
               m_flagsIter == other.m_flagsIter;
    }

    TextEditor::Line::LineIterator TextEditor::Line::LineIterator::operator+(i32 n) {
        LineIterator iter = *this;
        iter.m_charsIter += n;
        iter.m_colorsIter += n;
        iter.m_flagsIter += n;
        return iter;
    }

    i32 TextEditor::Line::LineIterator::operator-(LineIterator l) {
        return m_charsIter - l.m_charsIter;
    }

    TextEditor::Line::LineIterator TextEditor::Line::begin() const {
        LineIterator iter;
        iter.m_charsIter = m_chars.begin();
        iter.m_colorsIter = m_colors.begin();
        iter.m_flagsIter = m_flags.begin();
        return iter;
    }

    TextEditor::Line::LineIterator TextEditor::Line::end() const {
        LineIterator iter;
        iter.m_charsIter = m_chars.end();
        iter.m_colorsIter = m_colors.end();
        iter.m_flagsIter = m_flags.end();
        return iter;
    }

    TextEditor::Line::LineIterator TextEditor::Line::begin() {
        LineIterator iter;
        iter.m_charsIter = m_chars.begin();
        iter.m_colorsIter = m_colors.begin();
        iter.m_flagsIter = m_flags.begin();
        return iter;
    }

    TextEditor::Line::LineIterator TextEditor::Line::end() {
        LineIterator iter;
        iter.m_charsIter = m_chars.end();
        iter.m_colorsIter = m_colors.end();
        iter.m_flagsIter = m_flags.end();
        return iter;
    }

    TextEditor::Line &TextEditor::Line::operator=(const Line &line) {
        m_chars = line.m_chars;
        m_colors = line.m_colors;
        m_flags = line.m_flags;
        m_colorized = line.m_colorized;
        m_lineMaxColumn = line.m_lineMaxColumn;
        return *this;
    }

    TextEditor::Line &TextEditor::Line::operator=(Line &&line) noexcept {
        m_chars = std::move(line.m_chars);
        m_colors = std::move(line.m_colors);
        m_flags = std::move(line.m_flags);
        m_colorized = line.m_colorized;
        m_lineMaxColumn = line.m_lineMaxColumn;
        return *this;
    }

    u64 TextEditor::Line::size() const {
        return m_chars.size();
    }

    char TextEditor::Line::front(LinePart part) const {
        if (part == LinePart::Chars && !m_chars.empty())
            return m_chars.front();
        if (part == LinePart::Colors && !m_colors.empty())
            return m_colors.front();
        if (part == LinePart::Flags && !m_flags.empty())
            return m_flags.front();
        return 0x00;
    }

    std::string TextEditor::Line::frontUtf8(LinePart part) const {
        if (part == LinePart::Chars && !m_chars.empty())
            return m_chars.substr(0, TextEditor::utf8CharLength(m_chars[0]));
        if (part == LinePart::Colors && !m_colors.empty())
            return m_colors.substr(0, TextEditor::utf8CharLength(m_chars[0]));
        if (part == LinePart::Flags && !m_flags.empty())
            return m_flags.substr(0, TextEditor::utf8CharLength(m_chars[0]));
        return "";
    }

    void TextEditor::Line::push_back(char c) {
        m_chars.push_back(c);
        m_colors.push_back(0x00);
        m_flags.push_back(0x00);
        m_colorized = false;
        m_lineMaxColumn = -1;
    }

    bool TextEditor::Line::empty() const {
        return m_chars.empty();
    }

    std::string TextEditor::Line::substr(u64 start, u64 length, LinePart part) const {
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
        if (part == LinePart::Utf8) {
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

    char TextEditor::Line::operator[](u64 index) const {
        index = std::clamp(index, (u64) 0, (u64) (m_chars.size() - 1));
        return m_chars[index];
    }

    // C++ can't overload functions based on return type, so use any type other
    // than u64 to avoid ambiguity.
    std::string TextEditor::Line::operator[](i64 column) const {
        u64 utf8Length = TextEditor::getStringCharacterCount(m_chars);
        u64 index = static_cast<u64>(column);
        index = std::clamp(index, (u64) 0, utf8Length - 1);
        u64 utf8Start = 0;
        for (u64 utf8Index = 0; utf8Index < index; ++utf8Index) {
            utf8Start += TextEditor::utf8CharLength(m_chars[utf8Start]);
        }
        u64 utf8CharLen = TextEditor::utf8CharLength(m_chars[utf8Start]);
        if (utf8Start + utf8CharLen > m_chars.size())
            utf8CharLen = m_chars.size() - utf8Start;
        return m_chars.substr(utf8Start, utf8CharLen);
    }

    void TextEditor::Line::setNeedsUpdate(bool needsUpdate) {
        m_colorized = m_colorized && !needsUpdate;
    }

    void TextEditor::Line::append(const char *text) {
        append(std::string(text));
    }

    void TextEditor::Line::append(const char text) {
        append(std::string(1, text));
    }

    void TextEditor::Line::append(const std::string &text) {
        Line line(text);
        append(line);
    }

    void TextEditor::Line::append(const Line &line) {
        append(line.begin(), line.end());
    }

    void TextEditor::Line::append(LineIterator begin, LineIterator end) {
        if (begin.m_charsIter < end.m_charsIter) {
            m_chars.append(begin.m_charsIter, end.m_charsIter);
            std::string charsAppended(begin.m_charsIter, end.m_charsIter);

            if (m_lineMaxColumn < 0)
                m_lineMaxColumn = this->getMaxCharColumn();

            m_lineMaxColumn += TextEditor::getStringCharacterCount(charsAppended);
        }
        if (begin.m_colorsIter < end.m_colorsIter)
            m_colors.append(begin.m_colorsIter, end.m_colorsIter);
        if (begin.m_flagsIter < end.m_flagsIter)
            m_flags.append(begin.m_flagsIter, end.m_flagsIter);
        m_colorized = false;
    }

    void TextEditor::Line::insert(LineIterator iter, const std::string &text) {
        insert(iter, text.begin(), text.end());
    }

    void TextEditor::Line::insert(LineIterator iter, const char text) {
        insert(iter, std::string(1, text));
    }

    void TextEditor::Line::insert(LineIterator iter, strConstIter beginString, strConstIter endString) {
        Line line(std::string(beginString, endString));
        insert(iter, line);
    }

    void TextEditor::Line::insert(LineIterator iter, const Line &line) {
        insert(iter, line.begin(), line.end());
    }

    void TextEditor::Line::insert(LineIterator iter, LineIterator beginLine, LineIterator endLine) {
        if (iter == end())
            append(beginLine, endLine);
        else {
            m_chars.insert(iter.m_charsIter, beginLine.m_charsIter, endLine.m_charsIter);
            m_colors.insert(iter.m_colorsIter, beginLine.m_colorsIter, endLine.m_colorsIter);
            m_flags.insert(iter.m_flagsIter, beginLine.m_flagsIter, endLine.m_flagsIter);
            m_colorized = false;
            std::string charsInserted(beginLine.m_charsIter, endLine.m_charsIter);

            if (m_lineMaxColumn < 0)
                m_lineMaxColumn = this->getMaxCharColumn();

            m_lineMaxColumn += TextEditor::getStringCharacterCount(charsInserted);
        }
    }

    void TextEditor::Line::erase(LineIterator begin) {
        m_chars.erase(begin.m_charsIter);
        m_colors.erase(begin.m_colorsIter);
        m_flags.erase(begin.m_flagsIter);
        m_colorized = false;
        std::string charsErased(begin.m_charsIter, end().m_charsIter);

        if (m_lineMaxColumn < 0)
            m_lineMaxColumn = this->getMaxCharColumn();

        m_lineMaxColumn -= TextEditor::getStringCharacterCount(charsErased);
    }

    void TextEditor::Line::erase(LineIterator begin, u64 count) {
        if (count == (u64) -1)
            count = m_chars.size() - (begin.m_charsIter - m_chars.begin());
        m_chars.erase(begin.m_charsIter, begin.m_charsIter + count);
        m_colors.erase(begin.m_colorsIter, begin.m_colorsIter + count);
        m_flags.erase(begin.m_flagsIter, begin.m_flagsIter + count);
        m_colorized = false;
        std::string charsErased(begin.m_charsIter, begin.m_charsIter + count);

        if (m_lineMaxColumn < 0)
            m_lineMaxColumn = this->getMaxCharColumn();

        m_lineMaxColumn -= TextEditor::getStringCharacterCount(charsErased);
    }

    void TextEditor::Line::erase(u64 start, u64 length) {
        if (length == (u64) -1 || start + length >= m_chars.size())
            length = m_chars.size() - start;
        u64 utf8Start = 0;
        for (u64 utf8Index = 0; utf8Index < start; ++utf8Index) {
            utf8Start += TextEditor::utf8CharLength(m_chars[utf8Start]);
        }
        u64 utf8Length = 0;
        for (u64 utf8Index = 0; utf8Index < length; ++utf8Index) {
            utf8Length += TextEditor::utf8CharLength(m_chars[utf8Start + utf8Length]);
        }
        utf8Length = std::min(utf8Length, (u64) (m_chars.size() - utf8Start));
        erase(begin() + utf8Start, utf8Length);
    }

    void TextEditor::Line::clear() {
        m_chars.clear();
        m_colors.clear();
        m_flags.clear();
        m_colorized = false;
        m_lineMaxColumn = 0;
    }

    void TextEditor::Line::setLine(const std::string &text) {
        m_chars = text;
        m_colors = std::string(text.size(), 0x00);
        m_flags = std::string(text.size(), 0x00);
        m_colorized = false;
        m_lineMaxColumn = -1;
    }

    void TextEditor::Line::setLine(const Line &text) {
        m_chars = text.m_chars;
        m_colors = text.m_colors;
        m_flags = text.m_flags;
        m_colorized = text.m_colorized;
        m_lineMaxColumn = text.m_lineMaxColumn;
    }

    bool TextEditor::Line::needsUpdate() const {
        return !m_colorized;
    }

    TextEditor *TextEditor::GetSourceCodeEditor() {
        if (m_sourceCodeEditor != nullptr)
            return m_sourceCodeEditor;
        return this;
    }

    bool TextEditor::isEmpty() const {
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

    void TextEditor::setSelection(const Selection &selection) {
        m_state.m_selection = setCoordinates(selection);
    }

    TextEditor::Selection TextEditor::getSelection() const {
        return m_state.m_selection;
    }

    void TextEditor::selectWordUnderCursor() {
        auto wordStart = findWordStart(getCursorPosition());
        setSelection(Selection(wordStart, findWordEnd(wordStart)));
    }

    void TextEditor::selectAll() {
        setSelection(Selection(setCoordinates(0, 0), setCoordinates(-1, -1)));
    }

    bool TextEditor::hasSelection() const {
        return !isEmpty() && m_state.m_selection.m_end > m_state.m_selection.m_start;
    }

    void TextEditor::addUndo(UndoRecord &value) {
        if (m_readOnly)
            return;

        m_undoBuffer.resize((u64) (m_undoIndex + 1));
        m_undoBuffer.back() = value;
        ++m_undoIndex;
    }

    TextEditor::PaletteIndex TextEditor::getColorIndexFromFlags(Line::Flags flags) {
        if (flags.m_bits.globalDocComment)
            return PaletteIndex::GlobalDocComment;
        if (flags.m_bits.blockDocComment)
            return PaletteIndex::DocBlockComment;
        if (flags.m_bits.docComment)
            return PaletteIndex::DocComment;
        if (flags.m_bits.blockComment)
            return PaletteIndex::BlockComment;
        if (flags.m_bits.comment)
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
                        m_state.m_selection.m_end = setCoordinates(line, getLineMaxColumn(line));
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
                    resetBlinking = true;
                }
                if (resetBlinking)
                    resetCursorBlinkTime();
            }
        }
    }

    // the index here is array index so zero based
    void TextEditor::FindReplaceHandler::selectFound(TextEditor *editor, i32 found) {
        if (found < 0 || found >= (i64) m_matches.size())
            return;
        editor->setSelection(m_matches[found].m_selection);
        editor->setCursorPosition();
    }

// The returned index is shown in the form
//  'index of count' so 1 based
    u32 TextEditor::FindReplaceHandler::findMatch(TextEditor *editor, bool isNext) {

        if (editor->m_textChanged || m_optionsChanged) {
            std::string findWord = getFindWord();
            if (findWord.empty())
                return 0;
            resetMatches();
            findAllMatches(editor, findWord);
        }
        Coordinates targetPos = editor->m_state.m_cursorPosition;
        if (editor->hasSelection())
            targetPos = isNext ? editor->m_state.m_selection.m_end : editor->m_state.m_selection.m_start;

        auto count = m_matches.size();

        if (count == 0) {
            editor->setCursorPosition(targetPos);
            return 0;
        }

        if (isNext) {
            for (u32 i = 0; i < count; i++) {
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
    u32 TextEditor::FindReplaceHandler::findPosition(TextEditor *editor, Coordinates pos, bool isNext) {
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
                auto interval = Selection(m_matches[i==0 ? count-1 : i - 1].m_selection.m_end,m_matches[i].m_selection.m_end);
                if (interval.contains(pos))
                    return i + 1;
            }
        } else {
            for (i32 i = 0; i < count; i++) {
                auto interval = Selection(m_matches[i == 0 ? count - 1 : i - 1].m_selection.m_start, m_matches[i].m_selection.m_start);
                if (interval.contains(pos, 2))
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

// Performs actual search to fill mMatches
    bool TextEditor::FindReplaceHandler::findNext(TextEditor *editor) {
        Coordinates curPos = m_matches.empty() ? editor->m_state.m_cursorPosition : editor->lineCoordsToIndexCoords(m_matches.back().m_cursorPosition);

        u64 matchLength = getStringCharacterCount(m_findWord);
        u64 matchBytes = m_findWord.size();
        u64 byteIndex = 0;

        for (i64 ln = 0; ln < curPos.m_line; ln++)
            byteIndex += editor->getLineByteCount(ln) + 1;
        byteIndex += curPos.m_column;

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
            u64 firstLength = iter->length();

            if (firstLoc > byteIndex) {
                pos = firstLoc;
                matchLength = firstLength;
            } else {

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
        state.m_selection = Selection(TextEditor::stringIndexToCoordinates(textLoc, textSrc), TextEditor::stringIndexToCoordinates(textLoc + matchBytes, textSrc));
        state.m_cursorPosition = state.m_selection.m_end;
        m_matches.push_back(state);
        return true;
    }

    void TextEditor::FindReplaceHandler::findAllMatches(TextEditor *editor, std::string findWord) {

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

        m_matches.clear();
        m_findWord = findWord;
        auto startingPos = editor->m_state.m_cursorPosition;
        auto saveState = editor->m_state;
        Coordinates begin = editor->setCoordinates(0, 0);
        editor->m_state.m_cursorPosition = begin;

        if (!findNext(editor)) {
            editor->m_state = saveState;
            editor->ensureCursorVisible();
            return;
        }
        TextEditor::EditorState state = m_matches.back();

        while (state.m_cursorPosition < startingPos) {
            if (!findNext(editor)) {
                editor->m_state = saveState;
                editor->ensureCursorVisible();
                return;
            }
            state = m_matches.back();
        }

        while (findNext(editor));

        editor->m_state = saveState;
        editor->ensureCursorVisible();
        return;
    }


    bool TextEditor::FindReplaceHandler::replace(TextEditor *editor, bool right) {
        if (m_matches.empty() || m_findWord == m_replaceWord || m_findWord.empty())
            return false;


        auto state = editor->m_state;
        if (editor->m_state.m_selection.contains(editor->m_state.m_cursorPosition)) {
            editor->m_state.m_cursorPosition = editor->m_state.m_selection.m_start;
            if (editor->isStartOfLine()) {
                editor->m_state.m_cursorPosition.m_line--;
                editor->m_state.m_cursorPosition.m_column = editor->getLineMaxColumn(editor->m_state.m_cursorPosition.m_line);
            } else
                editor->m_state.m_cursorPosition.m_column--;
        }
        auto matchIndex = findMatch(editor, right);
        if (matchIndex != 0) {
            UndoRecord u;
            u.m_before = editor->m_state;
            u.m_removed = editor->getSelectedText();
            u.m_removedSelection = editor->m_state.m_selection;
            editor->deleteSelection();
            if (getFindRegEx()) {
                std::string replacedText = std::regex_replace(editor->getText(), std::regex(m_findWord), m_replaceWord, std::regex_constants::format_first_only | std::regex_constants::format_no_copy);
                u.m_added = replacedText;
            } else
                u.m_added = m_replaceWord;

            u.m_addedSelection.m_start = editor->setCoordinates(editor->m_state.m_cursorPosition);
            editor->insertText(u.m_added);

            editor->setCursorPosition(editor->m_state.m_selection.m_end);

            u.m_addedSelection.m_end = editor->setCoordinates(editor->m_state.m_cursorPosition);

            editor->ensureCursorVisible();
            ImGui::SetKeyboardFocusHere(0);

            u.m_after = editor->m_state;
            editor->addUndo(u);
            editor->m_textChanged = true;

            return true;
        }
        editor->m_state = state;
        return false;
    }

    bool TextEditor::FindReplaceHandler::replaceAll(TextEditor *editor) {
        u32 count = m_matches.size();

        for (u32 i = 0; i < count; i++)
            replace(editor, true);

        return true;
    }
}
