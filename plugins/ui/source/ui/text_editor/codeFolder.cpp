#include <ui/text_editor.hpp>
#include <pl/core/tokens.hpp>
#include <pl/core/token.hpp>
#include <wolv/utils/string.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/api/content_registry/pattern_language.hpp>
#include <pl/core/preprocessor.hpp>
#include <pl/api.hpp>

namespace hex::ui {
    using namespace pl::core;
    using Interval              = TextEditor::Interval;
    using Token                 = pl::core::Token;
    using Coordinates           = TextEditor::Coordinates;
    using RangeFromCoordinates  = TextEditor::RangeFromCoordinates;
    using CodeFoldState         = TextEditor::CodeFoldState;

    void TextEditor::Lines::skipAttribute() {

        if (sequence(tkn::Separator::LeftBracket, tkn::Separator::LeftBracket)) {
            while (!sequence(tkn::Separator::RightBracket, tkn::Separator::RightBracket))
                next();
        }
    }

    Interval TextEditor::Lines::findBlockInRange(Interval interval) {
        Interval result = NotValid;
        auto tokenStart = SafeTokenIterator(m_tokens.begin(), m_tokens.end());

        bool foundKey = false;
        bool foundComment = false;
        m_curr = tokenStart + interval.m_start;
        while (interval.m_end >= getTokenId()) {
            if (peek(tkn::Separator::EndOfProgram))
                return NotValid;
            if (result.m_start = getTokenId(); result.m_start < 0)
                return NotValid;

            while (true) {
                if (const auto *docComment = const_cast<Token::DocComment *>(getValue<Token::DocComment>(0)); docComment != nullptr) {
                    if (foundKey)
                        break;
                    if (docComment->singleLine) {
                        foundComment = true;
                        next();
                    } else {
                        if (foundComment)
                            break;
                        return {result.m_start, result.m_start};
                    }
                } else if (const auto *comment = const_cast<Token::Comment *>(getValue<Token::Comment>(0)); comment != nullptr) {
                    if (foundKey)
                        break;
                    if (comment->singleLine) {
                        foundComment = true;
                        next();
                    } else {
                        if (foundComment)
                            break;
                        return {result.m_start, result.m_start};
                    }
                } else if (const auto *keyword = const_cast<Token::Keyword *>(getValue<Token::Keyword>(0));keyword != nullptr && *keyword == Token::Keyword::Import) {
                    if (foundComment)
                        break;
                    foundKey = true;
                    while (!peek(tkn::Separator::Semicolon) && !peek(tkn::Separator::EndOfProgram))
                        next();
                    next();
                } else if (const auto *directive = const_cast<Token::Directive *>(getValue<Token::Directive>(0));directive != nullptr && *directive == Token::Directive::Include) {
                    if (foundComment)
                        break;
                    foundKey = true;
                    u32 line = m_curr->location.line;
                    while (m_curr->location.line == line && !peek(tkn::Separator::EndOfProgram))
                        next();
                } else
                    break;
            }
            if (foundKey || foundComment) {
                auto currentId = getTokenId();
                if (peek(tkn::Separator::EndOfProgram) || (currentId > 0 && currentId < (i32) m_tokens.size())) {
                    next(-1);
                    if (result.m_end = getTokenId(); result.m_end < 0)
                        return NotValid;

                    return result;
                } else
                    return NotValid;
            }
            next();
        }
        return NotValid;
    }

    Coordinates TextEditor::Lines::findCommentEndCoord(i32 tokenId) {
        Coordinates result = Invalid;
        auto save = m_curr;
        m_curr = SafeTokenIterator(m_tokens.begin(), m_tokens.end()) + tokenId;
        if (peek(tkn::Literal::Comment)) {
            auto comment = getValue<Token::Comment>(0);
            if (comment != nullptr) {
                auto location = m_curr->location;
                if (comment->singleLine)
                    return {(i32) (location.line - 1), (i32) (location.column + location.length - 2)};

                std::string commentText = comment->comment;
                auto vectorString = wolv::util::splitString(commentText, "\n");
                size_t lineCount = vectorString.size();
                auto endColumn = (lineCount == 1) ? location.column + location.length - 1 : vectorString.back().length() + 1;
                return {(i32)(location.line + lineCount - 2), (i32)endColumn};
            }

        } else if (peek(tkn::Literal::DocComment)) {
            auto docComment = getValue<Token::DocComment>(0);
            if (docComment != nullptr) {
                auto location = m_curr->location;
                if (docComment->singleLine)
                    return {(i32)(location.line - 1), (i32)(location.column + location.length - 2)};

                std::string commentText = docComment->comment;
                auto vectorString = wolv::util::splitString(commentText, "\n");
                size_t lineCount = vectorString.size();
                auto endColumn = (lineCount == 1) ? location.column + location.length - 1 : vectorString.back().length() + 1;
                return {(i32)(location.line + lineCount - 2), (i32)endColumn};
            }
        }
        m_curr = save;
        return result;
    }

//comments imports and includes
    void TextEditor::Lines::nonDelimitedFolds() {
        auto size = m_tokens.size();
        if (size > 0) {
            Interval block = {0,static_cast<i32>(size-1)};
            while (true) {
                auto interval = findBlockInRange(block);

                if (interval == NotValid)
                    break;

                Coordinates endCoord, startCoord = Coordinates(m_tokens[interval.m_start].location);

                if (interval.m_end == interval.m_start)
                    endCoord = findCommentEndCoord(interval.m_start);
                else
                    endCoord = Coordinates(m_tokens[interval.m_end].location);

                if (startCoord.getLine() != endCoord.getLine())
                    m_foldPoints[startCoord] = endCoord;

                if (interval.m_end >= block.m_end)
                    break;
                block.m_start = interval.m_end + 1;
            }
        }
    }

    RangeFromCoordinates TextEditor::Lines::getDelimiterLineNumbers(i32 start, i32 end, const std::string &delimiters) {
        RangeFromCoordinates result = {Invalid, Invalid};
        Coordinates first = Invalid;
        auto tokenStart = SafeTokenIterator(m_tokens.begin(), m_tokens.end());
        m_curr = tokenStart + start;
        Token::Separator openSeparator = Token::Separator::EndOfProgram;
        Token::Separator closeSeparator = Token::Separator::EndOfProgram;
        Token::Operator openOperator, closeOperator;
        if (delimiters == "{}") {
            openSeparator = Token::Separator::LeftBrace;
            closeSeparator = Token::Separator::RightBrace;
        } else if (delimiters == "[]") {
            openSeparator = Token::Separator::LeftBracket;
            closeSeparator = Token::Separator::RightBracket;
        } else if (delimiters == "()") {
            openSeparator = Token::Separator::LeftParenthesis;
            closeSeparator = Token::Separator::RightParenthesis;
        } else if (delimiters == "<>") {
            openOperator = Token::Operator::BoolLessThan;
            closeOperator = Token::Operator::BoolGreaterThan;
        }

        Token::Separator *separator;
        if (separator = const_cast<Token::Separator *>(getValue<Token::Separator>(0)); separator == nullptr || *separator != openSeparator) {
            if (const auto *opener = const_cast<Token::Operator *>(getValue<Token::Operator>(0)); opener == nullptr || *opener != openOperator)
                return result;
        }
        if (!m_curr->location.source->mainSource)
            return result;
        if (start > 0) {
            Location location1 = m_curr->location;
            Location location2;
            auto save = m_curr;
            while (peek(tkn::Literal::Comment, -1) || peek(tkn::Literal::DocComment, -1)) {
                if (getTokenId() == 0)
                    break;
                next(-1);
            }
            next(-1);
            if (separator != nullptr && *separator == Token::Separator::LeftParenthesis) {
                if (const auto *separator2 = const_cast<Token::Separator *>(getValue<Token::Separator>(0)); separator2 != nullptr && (*separator2 == Token::Separator::Semicolon || *separator2 == Token::Separator::LeftBrace || *separator2 == Token::Separator::RightBrace)) {
                    m_curr = save;
                    location2 = m_curr->location;
                } else {
                    location2 = m_curr->location;
                    m_curr = save;
                }
            } else {
                location2 = m_curr->location;
                m_curr = save;
            }
            if (location1.line != location2.line) {
                Coordinates coord(location2);
                first = coord + Coordinates(0, location2.length);
            } else
                first = Coordinates(location1);
        } else
            first = Coordinates(m_curr->location);
        m_curr = tokenStart + end;
        if (separator = const_cast<Token::Separator *>(getValue<Token::Separator>(0)); separator == nullptr || *separator != closeSeparator) {
            if (const auto *closer = const_cast<Token::Operator *>(getValue<Token::Operator>(0)); closer == nullptr || *closer != closeOperator) {
                if (const auto *separator2 = const_cast<Token::Separator *>(getValue<Token::Separator>(1)); separator2 != nullptr && *separator2 == closeSeparator) {
                    next();
                } else if (const auto *closer2 = const_cast<Token::Operator *>(getValue<Token::Operator>(1)); closer2 != nullptr && *closer2 == closeOperator) {
                    next();
                } else
                    return result;
            }
        }
        if (!m_curr->location.source->mainSource)
            return result;

        result.first = first;
        result.second = Coordinates(m_curr->location);
        return result;
    }

    void TextEditor::Lines::advanceToNextLine(i32 &lineIndex, i32 &currentTokenId, Location &location) {
        i32 tempLineIndex;
        if (tempLineIndex = nextLine(lineIndex); lineIndex == tempLineIndex) {
            lineIndex++;
            currentTokenId = -1;
            location = Location::Empty();
            return;
        }
        lineIndex = tempLineIndex;
        currentTokenId = m_firstTokenIdOfLine[lineIndex];
        m_curr = m_startToken + currentTokenId;
        location = m_curr->location;
    }

    void TextEditor::Lines::incrementTokenId(i32 &lineIndex, i32 &currentTokenId, Location &location) {
        currentTokenId++;
        m_curr = m_startToken + currentTokenId;
        location = m_curr->location;
        lineIndex = location.line - 1;
    }

    void TextEditor::Lines::moveToStringIndex(i32 stringIndex, i32 &currentTokenId, Location &location) {
        auto curr = m_curr;
        i32 tempTokenId;
        auto &line = operator[](location.line - 1);
        while (stringIndex > line.columnIndex(m_curr->location.column) + (i32) m_curr->location.length && m_curr->location.line == location.line)
            next();
        if (peek(tkn::Separator::EndOfProgram))
            return;

        if (tempTokenId = getTokenId(); tempTokenId < 0) {
            m_curr = curr;
            location = curr->location;
        } else {
            location = m_curr->location;
            currentTokenId = tempTokenId;
        }

    }

    void TextEditor::Lines::resetToTokenId(i32 &lineIndex, i32 &currentTokenId, Location &location) {
        m_curr = m_startToken + currentTokenId;
        location = m_curr->location;
        lineIndex = location.line - 1;
    }

    i32 TextEditor::Lines::findNextDelimiter(bool openOnly) {
        while (!peek(tkn::Separator::EndOfProgram)) {

            if (peek(tkn::Separator::LeftBrace) || peek(tkn::Separator::LeftBracket) || peek(tkn::Separator::LeftParenthesis) || peek(tkn::Operator::BoolLessThan))
                return getTokenId();
            if (!openOnly) {
                if (peek(tkn::Separator::RightBrace) || peek(tkn::Separator::RightBracket) || peek(tkn::Separator::RightParenthesis) || peek(tkn::Operator::BoolGreaterThan))
                    return getTokenId();
            }
            next();
        }
        return getTokenId();
    }

    TextEditor::CodeFoldBlocks TextEditor::Lines::foldPointsFromSource() {
        auto code = getText();
        if (code.empty())
            return m_foldPoints;
        std::unique_ptr<pl::PatternLanguage> runtime = std::make_unique<pl::PatternLanguage>();
        ContentRegistry::PatternLanguage::configureRuntime(*runtime, nullptr);
        std::ignore = runtime->preprocessString(code, pl::api::Source::DefaultSource);
        m_tokens = runtime->getInternals().preprocessor->getResult();
        const u32 tokenCount = m_tokens.size();
        if (tokenCount == 0)
            return m_foldPoints;
        loadFirstTokenIdOfLine();
        if (m_firstTokenIdOfLine.empty())
            return m_foldPoints;
        m_foldPoints.clear();
        nonDelimitedFolds();
        std::string openDelimiters = "{[(<";
        size_t topLine = 0;
        m_startToken = SafeTokenIterator(m_tokens.begin(), m_tokens.end());
        m_curr = m_startToken;
        auto location = m_curr->location;
        i32 lineIndex = topLine;
        i32 currentTokenId = 0;
        while (currentTokenId < (i32) tokenCount) {

            if (currentTokenId = findNextDelimiter(true); !peek(tkn::Separator::EndOfProgram)) {
                if (currentTokenId < 0) {
                    return m_foldPoints;
                }
                auto line = operator[](m_curr->location.line - 1);
                size_t stringIndex = line.columnIndex(m_curr->location.column);
                std::string openDelimiter = std::string(1, line[stringIndex - 1]);
                location = m_curr->location;

                if (auto idx = openDelimiters.find(openDelimiter); idx != std::string::npos) {
                    if (idx == 3) {
                        if (currentTokenId == 0) {
                            return m_foldPoints;
                        }
                        next(-1);
                        auto column = m_curr[0].location.column - 1;
                        if (line.m_colors[column] != (char) PaletteIndex::UserDefinedType) {
                            next(2);
                            if (peek(tkn::Separator::EndOfProgram, -1)) {
                                return m_foldPoints;
                            }

                            if (currentTokenId = getTokenId(); currentTokenId < 0) {
                                return m_foldPoints;
                            }
                            resetToTokenId(lineIndex, currentTokenId, location);
                            continue;
                        }
                    }
                    auto start = currentTokenId;
                    auto end = findMatchingDelimiter(currentTokenId);
                    if (end.first < 0) {
                        return m_foldPoints;
                    }
                    std::string value = openDelimiter + end.second;
                    auto lineBased = getDelimiterLineNumbers(start, end.first, value);

                    if (lineBased.first.getLine() != lineBased.second.getLine())
                        m_foldPoints[lineBased.first] = lineBased.second;

                    if (currentTokenId = getTokenId(m_startToken + end.first); currentTokenId < 0 || currentTokenId >= (i32) m_tokens.size()) {
                        return m_foldPoints;
                    }
                    incrementTokenId(lineIndex, currentTokenId, location);
                } else {
                    return m_foldPoints;
                }
            } else {
                return m_foldPoints;
            }
        }
        return m_foldPoints;
    }


    std::pair<i32, char> TextEditor::Lines::findMatchingDelimiter(i32 from) {
        std::string blockDelimiters = "{}[]()<>";
        std::pair<i32, char> result = std::make_pair(-1, '\0');
        auto tokenStart = SafeTokenIterator(m_tokens.begin(), m_tokens.end());
        const i32 tokenCount = m_tokens.size();
        if (from >= tokenCount)
            return result;
        m_curr = tokenStart + from;
        Location location = m_curr->location;
        auto line = operator[](location.line - 1);
        std::string openDelimiter;
        if (auto columnIndex = line.m_chars.find_first_of(blockDelimiters, location.column - 1); columnIndex != std::string::npos)
            openDelimiter = line[columnIndex];
        else
            return result;

        i32 currentTokenId = from + 1;
        std::string closeDelimiter;
        if (size_t idx = blockDelimiters.find(openDelimiter); idx != std::string::npos && currentTokenId < (i32) m_tokens.size())
            closeDelimiter = blockDelimiters[idx + 1];
        else
            return result;
        m_curr = tokenStart + currentTokenId;
        location = m_curr->location;
        i32 lineIndex = location.line - 1;
        while (currentTokenId < tokenCount) {

            if (currentTokenId = findNextDelimiter(false); !peek(tkn::Separator::EndOfProgram)) {
                if (currentTokenId < 0) {
                    return result;
                }
                line = operator[](m_curr->location.line - 1);
                std::string currentChar = std::string(1, line[(u64)(m_curr->location.column - 1)]);
                location = m_curr->location;

                if (auto idx = blockDelimiters.find(currentChar); idx != std::string::npos) {
                    if (currentChar == closeDelimiter) {
                        return std::make_pair(currentTokenId, closeDelimiter[0]);
                    } else {
                        if (idx == 6 || idx == 7) {
                            next(-1);
                            if (const auto *identifier = const_cast<Token::Identifier *>(getValue<Token::Identifier>(0));
                                    ((idx == 6) && (identifier == nullptr || identifier->getType() != Token::Identifier::IdentifierType::UDT)) || (idx == 7)) {
                                next(2);
                                if (peek(tkn::Separator::EndOfProgram, -1)) {
                                    return result;
                                }

                                if (currentTokenId = getTokenId(); currentTokenId < 0)
                                    return result;

                                resetToTokenId(lineIndex, currentTokenId, location);
                                continue;
                            }
                        }
                        if (idx % 2 == 0) {
                            auto start = currentTokenId;
                            auto end = findMatchingDelimiter(currentTokenId);
                            if (end.first < 0)
                                return result;
                            std::string value = currentChar + end.second;
                            auto lineBased = getDelimiterLineNumbers(start, end.first, value);
                            if (lineBased.first.getLine() != lineBased.second.getLine())
                                m_foldPoints[lineBased.first] = lineBased.second;

                            if (currentTokenId = getTokenId(tokenStart + end.first); currentTokenId < 0 || currentTokenId >= (i32) m_tokens.size())
                                return result;

                            incrementTokenId(lineIndex, currentTokenId, location);
                        } else
                            return result;
                    }
                } else {
                    return result;
                }
            } else {
                return result;
            }
        }
        return result;
    }

    void TextEditor::saveCodeFoldStates() {
        m_lines.saveCodeFoldStates();
    }

    void TextEditor::Lines::saveCodeFoldStates() {
        i32 codeFoldIndex = 0;
        Indices closedFoldIncrements;
        for (auto key: m_codeFoldKeys) {
            if (m_codeFoldState.contains(key) && !m_codeFoldState[key]) {
                closedFoldIncrements.push_back(codeFoldIndex);
                codeFoldIndex = 1;
            } else
                codeFoldIndex++;
        }
        if (!m_hiddenLines.empty())
            m_hiddenLines.clear();
        if (closedFoldIncrements.empty())
            return;
        std::string result = "//+-#:";
        for (u32 i = 0; i < closedFoldIncrements.size(); ++i) {
            result += std::to_string(closedFoldIncrements[i]);
            if (i < closedFoldIncrements.size() - 1)
                result += ",";
        }
        auto lineIndex = 0;
        m_hiddenLines.emplace_back(lineIndex, result);
    }

    void TextEditor::applyCodeFoldStates() {
        m_lines.applyCodeFoldStates();
    }

    void TextEditor::Lines::setCodeFoldState(CodeFoldState state) {
        m_codeFoldState = state;
    }

    CodeFoldState TextEditor::Lines::getCodeFoldState() const {
        return m_codeFoldState;
    }

    void TextEditor::Lines::applyCodeFoldStates() {

        std::string commentLine;
        for (const auto& line: m_hiddenLines) {
            if (line.m_line.starts_with("//+-#:")) {
                commentLine = line.m_line;
                break;
            }
        }
        if (commentLine.size() < 6 || !commentLine.starts_with("//+-#:")) {
            for (auto key: m_codeFoldKeys)
                m_codeFoldState[key] = true;
            return;
        }
        auto states = commentLine.substr(6);
        i32 count;
        StringVector stringVector;
        if (states.empty())
            count = 0;
        else {
            stringVector = wolv::util::splitString(states, ",", true);
            count = stringVector.size();
        }
        if (count == 1 && stringVector[0].empty())
            return;
        Indices closedFoldIncrements(count);
        i32 value = 0;
        for (i32 i = 0; i < count; ++i) {
            auto stateStr = stringVector[i];
            std::from_chars(stateStr.data(), stateStr.data() + stateStr.size(), value);
            closedFoldIncrements[i] = value;
        }
        m_codeFoldState.clear();
        auto codeFoldKeyIter = m_codeFoldKeys.begin();
        i32 closeFold = 0;
        for (auto closedFoldIncrement: closedFoldIncrements) {
            closeFold += closedFoldIncrement;
            if (closeFold < 0 || closeFold >= (i32) m_codeFoldKeys.size())
                continue;
            std::advance(codeFoldKeyIter, closedFoldIncrement);
            m_codeFoldState[*codeFoldKeyIter] = false;
        }
    }

    void TextEditor::Lines::closeCodeFold(const Range &key, bool userTriggered) {
        float topRow;

        if (userTriggered) {
            topRow = m_topRow;
        }
        LineIndexToScreen lineIndexToScreen;
        bool needsDelimiter = lineNeedsDelimiter(key.m_start.m_line);
        auto lineIter = m_lineIndexToScreen.begin();

        while (lineIter != m_lineIndexToScreen.end() && lineIter->first <= key.m_start.m_line) {
            lineIndexToScreen[lineIter->first] = lineIter->second;
            lineIter++;
        }
        auto startingXScreenCoordinate = foldedCoordsToScreen(lineCoordinates(0, 0)).x;
        float currentYScreenCoordinate = 0;

        if (needsDelimiter) {
            auto screenCoordinates = lineIter->second;
            auto &line = m_unfoldedLines[key.m_start.m_line];
            screenCoordinates.y = m_lineIndexToScreen[key.m_start.m_line].y;
            TrimMode trimMode = TrimMode::TrimBoth;
            auto row = lineIndexToRow(key.m_start.m_line);

            if (m_foldedLines.contains(row) && m_foldedLines[row].m_full.m_start.m_line == key.m_start.m_line)
                trimMode = TrimMode::TrimEnd;
            screenCoordinates.x = m_lineIndexToScreen[key.m_start.m_line].x + line.trim(trimMode).lineTextSize();
            lineIndexToScreen[lineIter->first] = screenCoordinates;
            lineIter++;
        }
        while (lineIter != m_lineIndexToScreen.end()) {

            if (lineIter->first >= key.m_end.m_line) {
                auto screenCoordinates = lineIter->second;
                if (lineIter->first == key.m_end.m_line && m_codeFoldDelimiters.contains(key) && !s_delimiters.contains(m_codeFoldDelimiters[key].first)) {
                    lineIndexToScreen[lineIter->first] = ImVec2(-1, -1);
                    lineIter++;
                    continue;
                } else if (lineIter->first == key.m_end.m_line) {
                    auto &line = m_unfoldedLines[key.m_start.m_line];
                    screenCoordinates.y = m_lineIndexToScreen[key.m_start.m_line].y;
                    currentYScreenCoordinate = screenCoordinates.y;
                    TrimMode trim = TrimMode::TrimBoth;
                    auto row = lineIndexToRow(key.m_start.m_line);

                    if (m_foldedLines.contains(row) && m_foldedLines[row].m_full.m_start.m_line == key.m_start.m_line)
                        trim = TrimMode::TrimEnd;

                    screenCoordinates.x = m_lineIndexToScreen[key.m_start.m_line].x + line.trim(trim).lineTextSize() + Ellipsis.lineTextSize();

                    if (needsDelimiter) {
                        Line bracketLine("{");
                        screenCoordinates.x += bracketLine.lineTextSize();
                    }
                } else if (screenCoordinates != ImVec2(-1, -1)) {
                    screenCoordinates.y = currentYScreenCoordinate;
                    if (screenCoordinates.x == startingXScreenCoordinate)
                        screenCoordinates.y += m_charAdvance.y;
                    currentYScreenCoordinate = screenCoordinates.y;
                }
                lineIndexToScreen[lineIter->first] = screenCoordinates;
            } else {
                lineIndexToScreen[lineIter->first] = ImVec2(-1, -1);
            }
            lineIter++;
        }
        m_lineIndexToScreen = std::move(lineIndexToScreen);

        if (m_codeFoldState.contains(key) && m_codeFoldState[key])
            m_codeFoldState[key] = false;
        bool found = false;
        FoldedLine currentFoldedLine;
        for (auto [row, foldedLine]: m_foldedLines) {

            if (!foldedLine.m_keys.empty() && (key.m_start.m_line == foldedLine.m_keys.back().m_end.m_line || key.m_end.m_line == foldedLine.m_keys.front().m_start.m_line)) {
                foldedLine.insertKey(key);
                foldedLine.m_row = row;
                m_foldedLines.erase(row);
                m_foldedLines[row] = foldedLine;
                found = true;
                currentFoldedLine = m_foldedLines[row];
                break;
            } else if (key.contains(foldedLine.m_full)) {
                FoldedLine newFoldedLine(this);
                newFoldedLine.insertKey(key);
                m_foldedLines.erase(row);
                m_foldedLines[newFoldedLine.m_row] = newFoldedLine;
                found = true;
                currentFoldedLine = m_foldedLines[newFoldedLine.m_row];
            }
        }

        if (!found) {
            FoldedLine newFoldedLine(this);
            newFoldedLine.insertKey(key);

            if (m_foldedLines.contains(newFoldedLine.m_row)) {
                const auto &foldedLine = m_foldedLines[newFoldedLine.m_row];

                if (foldedLine.m_built) {
                    newFoldedLine.m_foldedLine = foldedLine.m_foldedLine;
                    newFoldedLine.m_ellipsisIndices = foldedLine.m_ellipsisIndices;
                    newFoldedLine.m_cursorPosition = foldedLine.m_cursorPosition;
                    newFoldedLine.m_built = true;
                }
            }
            m_foldedLines[newFoldedLine.m_row] = newFoldedLine;
            currentFoldedLine = m_foldedLines[newFoldedLine.m_row];
        }
        FoldedLines updatedFoldedLines;
        for (auto &[row, foldedLine] : m_foldedLines) {
            if (row > currentFoldedLine.m_row) {
                foldedLine.m_row -= (key.m_end.m_line - key.m_start.m_line);
                updatedFoldedLines[foldedLine.m_row] = foldedLine;
            } else {
                updatedFoldedLines[row] = foldedLine;
            }
        }
        m_foldedLines = std::move(updatedFoldedLines);
        if (userTriggered) {
            auto topLine = lineCoordinates( rowToLineIndex(topRow), 0);
            if (key.contains(topLine))
                m_topRow = lineIndexToRow(key.m_start.m_line);
            else
                m_topRow = topRow;
            m_setTopRow = true;
            m_saveCodeFoldStateRequested = true;
            m_globalRowMaxChanged = true;
        }
        m_foldedLines[currentFoldedLine.m_row].loadSegments();
    }

    void TextEditor::Lines::openCodeFold(const Range &key) {
        for (const auto& foldedLine : m_foldedLines) {
            for (auto foldKey : foldedLine.second.m_keys) {
                if (foldKey.contains(key) && foldKey != key) {
                    m_codeFoldState[key] = true;
                    return;
                }
            }
        }

        LineIndexToScreen indicesToScreen;
        auto lineIter = m_lineIndexToScreen.begin();
        while (lineIter != m_lineIndexToScreen.end() && lineIter->first < key.m_start.m_line) {
            indicesToScreen[lineIter->first] = lineIter->second;
            lineIter++;
        }

        while (lineIter != m_lineIndexToScreen.end()) {
            if (lineIter->first + 1  > key.m_end.m_line) {
                auto sc = lineIter->second;
                if (sc != ImVec2(-1, -1))
                    sc.y += m_charAdvance.y * (key.m_end.m_line - key.m_start.m_line);
                indicesToScreen[lineIter->first] = sc;
            } else {
                auto sc = ImVec2(m_cursorScreenPosition.x + m_leftMargin, m_lineIndexToScreen[key.m_start.m_line-1].y + m_charAdvance.y*(lineIter->first + 1 - key.m_start.m_line));
                indicesToScreen[lineIter->first] = sc;
            }
            lineIter++;
        }
        m_lineIndexToScreen = std::move(indicesToScreen);
        if (m_codeFoldState.contains(key) && !m_codeFoldState[key])
            m_codeFoldState[key]=true;
        i32 erasedRow = -1;
        for (auto &[row, foldedLine] : m_foldedLines) {
            if (std::any_of(foldedLine.m_keys.begin(), foldedLine.m_keys.end(), [key](const Range& currentKey) { return currentKey == key; })) {
                foldedLine.removeKey(key);
                if (foldedLine.m_keys.empty()) {
                    m_foldedLines.erase(row);
                    erasedRow = row;
                }
                break;
            }
        }
        if (erasedRow != -1) {
            FoldedLines updatedFoldedLines;
            for (auto &[row, foldedLine] : m_foldedLines) {
                if (row > erasedRow) {
                    foldedLine.m_row += (key.m_end.m_line - key.m_start.m_line);
                    updatedFoldedLines[foldedLine.m_row] = foldedLine;
                } else {
                    updatedFoldedLines[row] = foldedLine;
                }
            }
            m_foldedLines = std::move(updatedFoldedLines);
        }
        m_saveCodeFoldStateRequested = true;
        m_globalRowMaxChanged = true;
    }

    void TextEditor::openCodeFoldAt(Coordinates line) {
        for (auto fold : m_lines.m_codeFoldKeys) {
            if (fold.contains(line) && m_lines.m_codeFoldState.contains(fold) && !m_lines.m_codeFoldState[fold]) {
                m_lines.openCodeFold(fold);
                if (m_lines.m_lineIndexToScreen[line.m_line] != ImVec2(-1.0f, -1.0f))
                    return;
            }
        }
    }

    template<typename T>
    T *TextEditor::Lines::getValue(const i32 index) {
        return const_cast<T*>(std::get_if<T>(&m_curr[index].value));
    }

    void TextEditor::Lines::next(i32 count) {
        if (m_interrupt) {
            m_interrupt = false;
            throw std::out_of_range("Highlights were deliberately interrupted");
        }
        if (count == 0)
            return;
        i32 id = getTokenId();

        if (count > 0)
            m_curr += std::min(count,static_cast<i32>(m_tokens.size() - id));
        else
            m_curr += -std::min(-count,id);

    }
    constexpr static u32 Normal = 0;
    constexpr static u32 Not    = 1;

    bool TextEditor::Lines::begin() {
        m_originalPosition = m_curr;

        return true;
    }

    void TextEditor::Lines::partBegin() {
        m_partOriginalPosition = m_curr;
    }

    void TextEditor::Lines::reset() {
        m_curr = m_originalPosition;
    }

    void TextEditor::Lines::partReset() {
        m_curr = m_partOriginalPosition;
    }

    bool TextEditor::Lines::resetIfFailed(const bool value) {
        if (!value) reset();

        return value;
    }

    template<auto S>
    bool TextEditor::Lines::sequenceImpl() {
        if constexpr (S == Normal)
            return true;
        else if constexpr (S == Not)
            return false;
        else
            std::unreachable();
    }

    template<auto S>
    bool TextEditor::Lines::matchOne(const Token &token) {
        if constexpr (S == Normal) {
            if (!peek(token)) {
                partReset();
                return false;
            }

            next();
            return true;
        } else if constexpr (S == Not) {
            if (!peek(token))
                return true;

            next();
            partReset();
            return false;
        } else
            std::unreachable();
    }

    template<auto S>
    bool TextEditor::Lines::sequenceImpl(const auto &... args) {
        return (matchOne<S>(args) && ...);
    }


    template<auto S>
    bool TextEditor::Lines::sequence(const Token &token, const auto &... args) {
        partBegin();
        return sequenceImpl<S>(token, args...);
    }


    bool TextEditor::Lines::isValid() {
        Token token;
        try {
            token = m_curr[0];
        }
        catch (const std::out_of_range &e) {
            auto t = e.what();
            if (t == nullptr)
                return false;
            return false;
        }
        return isLocationValid(token.location);
    }

    bool TextEditor::Lines::peek(const Token &token, const i32 index) {
        if (!isValid())
            return false;
        i32 id = getTokenId();
        if (id+index < 0 || id+index >= (i32)m_tokens.size())
            return false;
        return m_curr[index].type == token.type && m_curr[index] == token.value;
    }
}