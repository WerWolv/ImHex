#include "imgui.h"
#include <ui/text_editor.hpp>
#include <algorithm>


namespace hex::ui {
    TextEditor::Palette s_paletteBase = TextEditor::getDarkPalette();

    inline void TextUnformattedColoredAt(const ImVec2 &pos, const ImU32 &color, const char *text) {
        ImGui::SetCursorScreenPos(pos);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(text);
        ImGui::PopStyleColor();
    }

    void TextEditor::setTopMarginChanged(i32 newMargin) {
        m_newTopMargin = newMargin;
        m_topMarginChanged = true;
    }

    void TextEditor::setFocusAtCoords(const Coordinates &coords, bool ensureVisible) {
        m_focusAtCoords = coords;
        m_updateFocus = true;
        m_ensureCursorVisible = ensureVisible;
    }

    void TextEditor::clearErrorMarkers() {
        m_errorMarkers.clear();
        m_errorHoverBoxes.clear();
    }

    void TextEditor::clearActionables() {
        clearErrorMarkers();
        clearGotoBoxes();
        clearCursorBoxes();
    }

    ImVec2 TextEditor::underwaves(ImVec2 pos, u32 nChars, ImColor color, const ImVec2 &size_arg) {
        ImGui::GetStyle().AntiAliasedLines = false;
        ImGuiWindow *window = ImGui::GetCurrentWindow();
        window->DC.CursorPos = pos;
        const ImVec2 label_size = ImGui::CalcTextSize("W", nullptr, true);
        ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x, label_size.y);
        float lineWidth = size.x / 3.0f + 0.5f;
        float halfLineW = lineWidth / 2.0f;

        for (u32 i = 0; i < nChars; i++) {
            pos = window->DC.CursorPos;
            float lineY = pos.y + size.y;

            ImVec2 pos1_1 = ImVec2(pos.x + 0 * lineWidth, lineY + halfLineW);
            ImVec2 pos1_2 = ImVec2(pos.x + 1 * lineWidth, lineY - halfLineW);
            ImVec2 pos2_1 = ImVec2(pos.x + 2 * lineWidth, lineY + halfLineW);
            ImVec2 pos2_2 = ImVec2(pos.x + 3 * lineWidth, lineY - halfLineW);

            ImGui::GetWindowDrawList()->AddLine(pos1_1, pos1_2, ImU32(color), 0.4f);
            ImGui::GetWindowDrawList()->AddLine(pos1_2, pos2_1, ImU32(color), 0.4f);
            ImGui::GetWindowDrawList()->AddLine(pos2_1, pos2_2, ImU32(color), 0.4f);

            window->DC.CursorPos = ImVec2(pos.x + size.x, pos.y);
        }
        auto ret = window->DC.CursorPos;
        ret.y += size.y;
        return ret;
    }

    void TextEditor::setTabSize(i32 value) {
        m_tabSize = std::max(0, std::min(32, value));
    }

    float TextEditor::getPageSize() const {
        auto height = ImGui::GetCurrentWindow()->InnerClipRect.GetHeight();
        return height / m_charAdvance.y;
    }

    bool TextEditor::isEndOfLine() const {
        return isEndOfLine(m_state.m_cursorPosition);
    }

    bool TextEditor::isStartOfLine() const {
        return m_state.m_cursorPosition.m_column == 0;
    }

    bool TextEditor::isEndOfLine(const Coordinates &coordinates) const {
        if (coordinates.m_line < (i32) m_lines.size())
            return coordinates.m_column >= getStringCharacterCount(m_lines[coordinates.m_line].m_chars);
        return true;
    }

    bool TextEditor::isEndOfFile(const Coordinates &coordinates) const {
        if (coordinates.m_line < (i32) m_lines.size())
            return coordinates.m_line >= (i32) m_lines.size() - 1 && isEndOfLine(coordinates);
        return true;
    }

    void TextEditor::setTopLine() {
        if (!m_withinRender) {
            m_setTopLine = true;
            return;
        } else {
            m_setTopLine = false;
            ImGui::SetScrollY(m_topLine * m_charAdvance.y);
            ensureCursorVisible();
        }
    }

    void TextEditor::render(const char *title, const ImVec2 &size, bool border) {
        m_withinRender = true;

        if (m_lines.capacity() < 2 * m_lines.size())
            m_lines.reserve(2 * m_lines.size());

        auto scrollBg = ImGui::GetStyleColorVec4(ImGuiCol_ScrollbarBg);
        scrollBg.w = 0.0f;
        auto scrollBarSize = ImGui::GetStyle().ScrollbarSize;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(m_palette[(i32) PaletteIndex::Background]));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImGui::ColorConvertFloat4ToU32(scrollBg));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, scrollBarSize);

        auto position = ImGui::GetCursorScreenPos();
        if (m_showLineNumbers) {
            std::string lineNumber = " " + std::to_string(m_lines.size()) + " ";
            m_lineNumberFieldWidth = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, lineNumber.c_str(), nullptr, nullptr).x + m_leftMargin;
            ImGui::SetNextWindowPos(position);
            ImGui::SetCursorScreenPos(position);
            auto lineNoSize = ImVec2(m_lineNumberFieldWidth, size.y);
            if (!m_ignoreImGuiChild) {
                ImGui::BeginChild("##lineNumbers", lineNoSize, false, ImGuiWindowFlags_NoScrollbar);
                ImGui::EndChild();
            }
        } else {
            m_lineNumberFieldWidth = 0;
        }

        ImVec2 textEditorSize = size;
        textEditorSize.x -= m_lineNumberFieldWidth;

        bool scroll_x = m_longestLineLength * m_charAdvance.x >= textEditorSize.x;

        bool scroll_y = m_lines.size() > 1;
        if (!border)
            textEditorSize.x -= scrollBarSize;
        ImGui::SetCursorScreenPos(ImVec2(position.x + m_lineNumberFieldWidth, position.y));
        ImGuiChildFlags childFlags = border ? ImGuiChildFlags_Borders : ImGuiChildFlags_None;
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove;
        if (!m_ignoreImGuiChild)
            ImGui::BeginChild(title, textEditorSize, childFlags, windowFlags);
        auto window = ImGui::GetCurrentWindow();
        window->ScrollbarSizes = ImVec2(scrollBarSize * scroll_x, scrollBarSize * scroll_y);
        ImGui::GetCurrentWindowRead()->ScrollbarSizes = ImVec2(scrollBarSize * scroll_y, scrollBarSize * scroll_x);
        if (scroll_y) {
            ImGui::GetCurrentWindow()->ScrollbarY = true;
            ImGui::Scrollbar(ImGuiAxis_Y);
            ImGui::GetCurrentWindow()->ScrollbarY = false;
        }
        if (scroll_x) {
            ImGui::GetCurrentWindow()->ScrollbarX = true;
            ImGui::Scrollbar(ImGuiAxis_X);
            ImGui::GetCurrentWindow()->ScrollbarX = false;
        }

        if (m_handleKeyboardInputs) {
            handleKeyboardInputs();
        }

        if (m_handleMouseInputs)
            handleMouseInputs();


        colorizeInternal();
        renderText(title, position, textEditorSize);

        if (!m_ignoreImGuiChild)
            ImGui::EndChild();

        ImGui::PopStyleVar(3);
        ImGui::PopStyleColor(2);

        m_withinRender = false;
        ImGui::SetCursorScreenPos(ImVec2(position.x, position.y + size.y - 1));
        ImGui::Dummy({});
    }

    void TextEditor::ensureCursorVisible() {
        if (!m_withinRender) {
            m_scrollToCursor = true;
            return;
        }

        auto scrollBarSize = ImGui::GetStyle().ScrollbarSize;
        float scrollX = ImGui::GetScrollX();
        float scrollY = ImGui::GetScrollY();

        auto windowPadding = ImGui::GetStyle().FramePadding * 2.0f;

        auto height = ImGui::GetWindowHeight() - m_topMargin - scrollBarSize - m_charAdvance.y;
        auto width = ImGui::GetWindowWidth() - windowPadding.x - scrollBarSize;

        auto top = (i32) rint((m_topMargin > scrollY ? m_topMargin - scrollY : scrollY) / m_charAdvance.y);
        auto bottom = top + (i32) rint(height / m_charAdvance.y);

        auto left = (i32) rint(scrollX / m_charAdvance.x);
        auto right = left + (i32) rint(width / m_charAdvance.x);

        auto pos = setCoordinates(m_state.m_cursorPosition);
        pos.m_column = (i32) rint(textDistanceToLineStart(pos) / m_charAdvance.x);

        bool mScrollToCursorX = true;
        bool mScrollToCursorY = true;

        if (pos.m_line >= top && pos.m_line <= bottom)
            mScrollToCursorY = false;
        if ((pos.m_column >= left) && (pos.m_column <= right))
            mScrollToCursorX = false;
        if (!mScrollToCursorX && !mScrollToCursorY && m_oldTopMargin == m_topMargin) {
            m_scrollToCursor = false;
            return;
        }

        if (mScrollToCursorY) {
            if (pos.m_line < top)
                ImGui::SetScrollY(std::max(0.0f, pos.m_line * m_charAdvance.y));
            if (pos.m_line > bottom)
                ImGui::SetScrollY(std::max(0.0f, pos.m_line * m_charAdvance.y - height));
        }
        if (mScrollToCursorX) {
            if (pos.m_column < left)
                ImGui::SetScrollX(std::max(0.0f, pos.m_column * m_charAdvance.x));
            if (pos.m_column > right)
                ImGui::SetScrollX(std::max(0.0f, pos.m_column * m_charAdvance.x - width));
        }
        m_oldTopMargin = m_topMargin;
    }

    void TextEditor::resetCursorBlinkTime() {
        m_startTime = ImGui::GetTime() * 1000 - s_cursorBlinkOnTime;
    }

    void TextEditor::renderText(const char *title, const ImVec2 &lineNumbersStartPos, const ImVec2 &textEditorSize) {

        preRender();
        auto drawList = ImGui::GetWindowDrawList();
        s_cursorScreenPosition = ImGui::GetCursorScreenPos();
        ImVec2 position = lineNumbersStartPos;
        if (m_setScrollY)
            setScrollY();
        auto scrollY = ImGui::GetScrollY();
        if (m_setTopLine)
            setTopLine();
        else
            m_topLine = std::max<float>(0.0F, (scrollY - m_topMargin) / m_charAdvance.y);
        auto lineNo = m_topLine;

        auto lineMax = std::clamp(lineNo + m_numberOfLinesDisplayed, 0.0F, m_lines.size() - 1.0F);

        if (!m_lines.empty()) {
            bool focused = ImGui::IsWindowFocused();

            while (lineNo <= lineMax) {

                drawSelection(lineNo);

                if (!m_ignoreImGuiChild)
                    ImGui::EndChild();

                if (m_showLineNumbers)
                    drawLineNumbers(position, lineNo, textEditorSize, focused, drawList);

                if (!m_ignoreImGuiChild)
                    ImGui::BeginChild(title);

                if (m_state.m_cursorPosition.m_line == lineNo && m_showCursor && focused)
                    renderCursor(lineNo, drawList);

                renderGotoButtons(lineNo);

                // Render colorized text

                auto &line = m_lines[lineNo];
                if (line.empty()) {
                    ImGui::Dummy(m_charAdvance);
                    lineNo = std::floor(lineNo + 1.0F);
                    if (m_updateFocus)
                        setFocus();
                    continue;
                }
                auto colors = m_lines[lineNo].m_colors;
                u64 colorsSize = std::min((u64)std::floor(textEditorSize.x / m_charAdvance.x), colors.size());
                u64 i = ImGui::GetScrollX() / m_charAdvance.x;
                u64 maxI = i + colorsSize;
                while (i < maxI) {
                    char color = std::clamp(colors[i], (char) PaletteIndex::Default, (char) ((u8) PaletteIndex::Max - 1));
                    auto index = colors.find_first_not_of(color, i);
                    index -= i;

                    u32 tokenLength = std::clamp((u64) index,(u64) 1, maxI - i);
                    if (m_updateFocus)
                        setFocus();
                    auto lineStart = setCoordinates(lineNo, i);

                    drawText(lineStart, i, tokenLength, color);

                    i += (tokenLength + skipSpaces(lineStart));
                }

                lineNo = std::floor(lineNo + 1.0F);
            }
        }

        if (m_scrollToCursor)
            ensureCursorVisible();

        postRender(title, position, lineNo);

    }

    void TextEditor::setFocus() {
        m_state.m_cursorPosition = m_focusAtCoords;
        resetCursorBlinkTime();
        if (m_ensureCursorVisible)
            ensureCursorVisible();

        if (!this->m_readOnly)
            ImGui::SetKeyboardFocusHere(0);
        m_updateFocus = false;
    }

    void TextEditor::preRender() {
        m_charAdvance = calculateCharAdvance();

        /* Update palette with the current alpha from style */
        for (i32 i = 0; i < (i32) PaletteIndex::Max; ++i) {
            auto color = ImGui::ColorConvertU32ToFloat4(s_paletteBase[i]);
            color.w *= ImGui::GetStyle().Alpha;
            m_palette[i] = ImGui::ColorConvertFloat4ToU32(color);
        }

        m_numberOfLinesDisplayed = getPageSize();

        if (m_scrollToTop) {
            m_scrollToTop = false;
            ImGui::SetScrollY(0.f);
        }

        if (m_scrollToBottom && ImGui::GetScrollMaxY() >= ImGui::GetScrollY()) {
            m_scrollToBottom = false;
            ImGui::SetScrollY(ImGui::GetScrollMaxY());
        }

    }

    void TextEditor::drawSelection(float lineNo) {
        ImVec2 lineStartScreenPos = s_cursorScreenPosition + ImVec2(m_leftMargin, m_topMargin + std::floor(lineNo) * m_charAdvance.y);
        Selection lineCoords = Selection(setCoordinates(lineNo, 0), setCoordinates(lineNo, -1));
        auto drawList = ImGui::GetWindowDrawList();

        if (m_state.m_selection.m_start <= lineCoords.m_end && m_state.m_selection.m_end > lineCoords.m_start) {
            float selectionStart = textDistanceToLineStart(std::max(m_state.m_selection.m_start, lineCoords.m_start));
            float selectionEnd = textDistanceToLineStart(std::min(m_state.m_selection.m_end, lineCoords.m_end)) + m_charAdvance.x * (m_state.m_selection.m_end.m_line > lineNo);

            if (selectionStart < selectionEnd) {
                ImVec2 rectStart(lineStartScreenPos.x + selectionStart, lineStartScreenPos.y);
                ImVec2 rectEnd(lineStartScreenPos.x + selectionEnd, lineStartScreenPos.y + m_charAdvance.y);
                drawList->AddRectFilled(rectStart, rectEnd, m_palette[(i32) PaletteIndex::Selection]);
            }
        }
    }

    void TextEditor::drawLineNumbers(ImVec2 position, float lineNo, const ImVec2 &contentSize, bool focused, ImDrawList *drawList) {
        ImVec2 lineStartScreenPos = s_cursorScreenPosition + ImVec2(m_leftMargin, m_topMargin + std::floor(lineNo) * m_charAdvance.y);
        ImVec2 lineNoStartScreenPos = ImVec2(position.x, m_topMargin + s_cursorScreenPosition.y + std::floor(lineNo) * m_charAdvance.y);
        auto start = ImVec2(lineNoStartScreenPos.x + m_lineNumberFieldWidth - m_charAdvance.x / 2, lineStartScreenPos.y);
        i32 totalDigitCount = std::floor(std::log10(m_lines.size())) + 1;
        ImGui::SetCursorScreenPos(position);
        if (!m_ignoreImGuiChild)
            ImGui::BeginChild("##lineNumbers");

        i32 padding = totalDigitCount - std::floor(std::log10(lineNo + 1)) - 1;
        std::string space = std::string(padding, ' ');
        std::string lineNoStr = space + std::to_string((i32) (lineNo + 1));
        ImGui::SetCursorScreenPos(ImVec2(position.x, lineStartScreenPos.y));
        if (ImGui::InvisibleButton(lineNoStr.c_str(), ImVec2(m_lineNumberFieldWidth, m_charAdvance.y))) {
            if (m_breakpoints.contains(lineNo + 1))
                m_breakpoints.erase(lineNo + 1);
            else
                m_breakpoints.insert(lineNo + 1);
            m_breakPointsChanged = true;
            setFocusAtCoords(m_state.m_cursorPosition, false);
        }
        auto color = m_palette[(i32) PaletteIndex::LineNumber];

        if (m_state.m_cursorPosition.m_line == lineNo && m_showCursor) {
            color = m_palette[(i32) PaletteIndex::Cursor];
            // Highlight the current line (where the cursor is)
            if (!hasSelection()) {
                auto end = ImVec2(lineNoStartScreenPos.x + contentSize.x + m_lineNumberFieldWidth, lineStartScreenPos.y + m_charAdvance.y);
                drawList->AddRectFilled(ImVec2(position.x, lineStartScreenPos.y), end, m_palette[(i32) (focused ? PaletteIndex::CurrentLineFill : PaletteIndex::CurrentLineFillInactive)]);
                drawList->AddRect(ImVec2(position.x, lineStartScreenPos.y), end, m_palette[(i32) PaletteIndex::CurrentLineEdge], 1.0f);
            }
        }
        // Draw breakpoints
        if (m_breakpoints.count(lineNo + 1) != 0) {
            auto end = ImVec2(lineNoStartScreenPos.x + contentSize.x + m_lineNumberFieldWidth, lineStartScreenPos.y + m_charAdvance.y);
            drawList->AddRectFilled(ImVec2(position.x, lineStartScreenPos.y), end, m_palette[(i32) PaletteIndex::Breakpoint]);

            drawList->AddCircleFilled(start + ImVec2(0, m_charAdvance.y) / 2, m_charAdvance.y / 3, m_palette[(i32) PaletteIndex::Breakpoint]);
            drawList->AddCircle(start + ImVec2(0, m_charAdvance.y) / 2, m_charAdvance.y / 3, m_palette[(i32) PaletteIndex::Default]);
            drawList->AddText(ImVec2(lineNoStartScreenPos.x + m_leftMargin, lineStartScreenPos.y), color, lineNoStr.c_str());
        }
        TextUnformattedColoredAt(ImVec2(m_leftMargin + lineNoStartScreenPos.x, lineStartScreenPos.y), color, lineNoStr.c_str());

        if (!m_ignoreImGuiChild)
            ImGui::EndChild();
    }

    void TextEditor::renderCursor(float lineNo, ImDrawList *drawList) {
        ImVec2 lineStartScreenPos = s_cursorScreenPosition + ImVec2(m_leftMargin, m_topMargin + std::floor(lineNo) * m_charAdvance.y);
        auto timeEnd = ImGui::GetTime() * 1000;
        auto elapsed = timeEnd - m_startTime;
        if (elapsed > s_cursorBlinkOnTime) {
            float width = 1.0f;
            u64 charIndex = lineCoordinatesToIndex(m_state.m_cursorPosition);
            float cx = textDistanceToLineStart(m_state.m_cursorPosition);
            auto &line = m_lines[std::floor(lineNo)];
            if (m_overwrite && charIndex < line.size()) {
                char c = line[charIndex];
                std::string s(1, c);
                width = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, s.c_str()).x;
            }
            ImVec2 rectStart(lineStartScreenPos.x + cx, lineStartScreenPos.y);
            ImVec2 rectEnd(lineStartScreenPos.x + cx + width, lineStartScreenPos.y + m_charAdvance.y);
            drawList->AddRectFilled(rectStart, rectEnd, m_palette[(i32) PaletteIndex::Cursor]);
            if (elapsed > s_cursorBlinkInterval)
                m_startTime = timeEnd;
            if (m_matchedBracket.isNearABracket(this, m_state.m_cursorPosition))
                m_matchedBracket.findMatchingBracket(this);
        }
    }

    void TextEditor::renderGotoButtons(float lineNo) {
        ImVec2 lineStartScreenPos = s_cursorScreenPosition + ImVec2(m_leftMargin, m_topMargin + std::floor(lineNo) * m_charAdvance.y);
        auto lineText = getLineText(lineNo);
        Coordinates gotoKey = setCoordinates(lineNo + 1, 0);
        if (gotoKey != Invalid) {
            std::string errorLineColumn;
            bool found = false;
            for (auto text: m_clickableText) {
                if (lineText.find(text) == 0) {
                    errorLineColumn = lineText.substr(text.size());
                    if (!errorLineColumn.empty()) {
                        found = true;
                        break;
                    }
                }
            }
            if (found) {
                i32 currLine = 0, currColumn = 0;
                if (auto idx = errorLineColumn.find(":"); idx != std::string::npos) {
                    auto errorLine = errorLineColumn.substr(0, idx);
                    if (!errorLine.empty())
                        currLine = std::stoi(errorLine) - 1;
                    auto errorColumn = errorLineColumn.substr(idx + 1);
                    if (!errorColumn.empty())
                        currColumn = std::stoi(errorColumn) - 1;
                }
                TextEditor::Coordinates errorPos = GetSourceCodeEditor()->setCoordinates(currLine, currColumn);
                if (errorPos != Invalid) {
                    ImVec2 errorStart = ImVec2(lineStartScreenPos.x, lineStartScreenPos.y);
                    auto lineEnd = setCoordinates(lineNo, -1);
                    if (lineEnd != Invalid) {
                        ImVec2 errorEnd = ImVec2(lineStartScreenPos.x + textDistanceToLineStart(lineEnd), lineStartScreenPos.y + m_charAdvance.y);
                        ErrorGotoBox box = ErrorGotoBox(ImRect({errorStart, errorEnd}), errorPos, GetSourceCodeEditor());
                        m_errorGotoBoxes[gotoKey] = box;
                        CursorChangeBox cursorBox = CursorChangeBox(ImRect({errorStart, errorEnd}));
                        m_cursorBoxes[gotoKey] = cursorBox;
                    }
                }
            }
            if (m_cursorBoxes.find(gotoKey) != m_cursorBoxes.end()) {
                auto box = m_cursorBoxes[gotoKey];
                if (box.trigger()) box.callback();
            }

            if (m_errorGotoBoxes.find(gotoKey) != m_errorGotoBoxes.end()) {
                auto box = m_errorGotoBoxes[gotoKey];
                if (box.trigger()) box.callback();
            }
        }
    }

    void TextEditor::drawText(Coordinates &lineStart, u64 i, u32 tokenLength, char color) {
        auto &line = m_lines[lineStart.m_line];
        ImVec2 lineStartScreenPos = s_cursorScreenPosition + ImVec2(m_leftMargin, m_topMargin + std::floor(lineStart.m_line) * m_charAdvance.y);
        auto textStart = textDistanceToLineStart(lineStart);
        auto begin = lineStartScreenPos + ImVec2(textStart, 0);

        TextUnformattedColoredAt(begin, m_palette[(i32) color], line.substr(i, tokenLength).c_str());

        ErrorMarkers::iterator errorIt;
        auto key = lineStart + Coordinates(1, 1);
        if (errorIt = m_errorMarkers.find(key); errorIt != m_errorMarkers.end()) {
            auto errorMessage = errorIt->second.second;
            auto errorLength = errorIt->second.first;
            if (errorLength == 0)
                errorLength = line.size() - i - 1;

            auto end = underwaves(begin, errorLength, m_palette[(i32) PaletteIndex::ErrorMarker]);
            ErrorHoverBox box = ErrorHoverBox(ImRect({begin, end}), key, errorMessage.c_str());
            m_errorHoverBoxes[key] = box;
        }
        if (m_errorHoverBoxes.find(key) != m_errorHoverBoxes.end()) {
            auto box = m_errorHoverBoxes[key];
            if (box.trigger()) box.callback();
        }
        lineStart = lineStart + Coordinates(0, tokenLength);
    }

    void TextEditor::postRender(const char *title, ImVec2 position, float lineNo) {
        ImVec2 lineStartScreenPos = ImVec2(s_cursorScreenPosition.x + m_leftMargin, m_topMargin + s_cursorScreenPosition.y + std::floor(lineNo) * m_charAdvance.y);
        float globalLineMax = m_lines.size();
        auto lineMax = std::clamp(lineNo + m_numberOfLinesDisplayed, 0.0F, globalLineMax - 1.0F);
        if (!m_ignoreImGuiChild)
            ImGui::EndChild();

        if (m_showLineNumbers && !m_ignoreImGuiChild) {
            ImGui::BeginChild("##lineNumbers");
            ImGui::SetCursorScreenPos(ImVec2(position.x, lineStartScreenPos.y));
            ImGui::Dummy(ImVec2(m_lineNumberFieldWidth, (globalLineMax - lineMax - 1) * m_charAdvance.y + ImGui::GetCurrentWindow()->InnerClipRect.GetHeight() - m_charAdvance.y));
            ImGui::EndChild();
        }
        if (!m_ignoreImGuiChild)
            ImGui::BeginChild(title);

        ImGui::SetCursorScreenPos(lineStartScreenPos);
        if (m_showLineNumbers)
            ImGui::Dummy(ImVec2(m_longestLineLength * m_charAdvance.x + m_charAdvance.x, (globalLineMax - lineMax - 2.0F) * m_charAdvance.y + ImGui::GetCurrentWindow()->InnerClipRect.GetHeight()));
        else
            ImGui::Dummy(ImVec2(m_longestLineLength * m_charAdvance.x + m_charAdvance.x, (globalLineMax - lineMax - 3.0F) * m_charAdvance.y + ImGui::GetCurrentWindow()->InnerClipRect.GetHeight() - 1.0f));


        if (m_topMarginChanged) {
            m_topMarginChanged = false;
            auto window = ImGui::GetCurrentWindow();
            auto maxScroll = window->ScrollMax.y;
            if (maxScroll > 0) {
                float pixelCount;
                if (m_newTopMargin > m_topMargin) {
                    pixelCount = m_newTopMargin - m_topMargin;
                } else if (m_newTopMargin > 0) {
                    pixelCount = m_topMargin - m_newTopMargin;
                } else {
                    pixelCount = m_topMargin;
                }
                auto oldScrollY = ImGui::GetScrollY();

                if (m_newTopMargin > m_topMargin)
                    m_shiftedScrollY = oldScrollY + pixelCount;
                else
                    m_shiftedScrollY = oldScrollY - pixelCount;
                ImGui::SetScrollY(m_shiftedScrollY);
                m_topMargin = m_newTopMargin;
            }
        }
    }

    ImVec2 TextEditor::calculateCharAdvance() const {
        /* Compute mCharAdvance regarding scaled font size (Ctrl + mouse wheel)*/
        const float fontSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, "#", nullptr, nullptr).x;
        return ImVec2(fontSize, ImGui::GetTextLineHeightWithSpacing() * m_lineSpacing);
    }

    float TextEditor::textDistanceToLineStart(const Coordinates &aFrom) const {
        auto &line = m_lines[aFrom.m_line];
        i32 colIndex = lineCoordinatesToIndex(aFrom);
        auto substr = line.m_chars.substr(0, colIndex);
        return ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, substr.c_str(), nullptr, nullptr).x;
    }
}