#include <algorithm>
#include <ui/text_editor.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/logger.hpp>

namespace hex::ui {
    extern TextEditor::Palette s_paletteBase;

    template<class InputIt1, class InputIt2, class BinaryPredicate>
    bool equals(InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2, BinaryPredicate p) {
        for (; first1 != last1 && first2 != last2; ++first1, ++first2) {
            if (!p(*first1, *first2))
                return false;
        }
        return first1 == last1 && first2 == last2;
    }

    void TextEditor::setNeedsUpdate(i32 line, bool needsUpdate) {
        if (line < (i32) m_lines.size())
            m_lines[line].setNeedsUpdate(needsUpdate);
    }

    void TextEditor::setColorizedLine(i64 line, const std::string &tokens) {
        if (line < (i64)m_lines.size()) {
            auto &lineTokens = m_lines[line].m_colors;
            if (lineTokens.size() != tokens.size()) {
                lineTokens.resize(tokens.size());
                std::ranges::fill(lineTokens, 0x00);
            }
            bool needsUpdate = false;
            for (u64 i = 0; i < tokens.size(); ++i) {
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

    void TextEditor::colorize() {
        m_updateFlags = true;
    }

    void TextEditor::colorizeRange() {

        if (isEmpty())
            return;

        std::smatch results;
        std::string id;
        if (m_languageDefinition.m_tokenize == nullptr) {
            m_languageDefinition.m_tokenize = [](strConstIter, strConstIter, strConstIter &, strConstIter &, PaletteIndex &) { return false; };
            log::warn("Syntax highlighting tokenize callback is nullptr");
            return;
        }
        i32 linesSize = m_lines.size();
        for (i32 i = 0; i < linesSize; ++i) {
            auto &line = m_lines[i];
            auto size = line.size();

            if (line.m_colors.size() != size) {
                line.m_colors.resize(size, 0);
                line.m_colorized = false;
            }

            if (line.m_colorized || line.empty())
                continue;

            auto last = line.end();

            auto first = line.begin();
            for (auto current = first; (current - first) < (i64)size;) {
                strConstIter token_begin;
                strConstIter token_end;
                PaletteIndex token_color = PaletteIndex::Default;

                bool hasTokenizeResult = m_languageDefinition.m_tokenize(current.m_charsIter, last.m_charsIter, token_begin, token_end, token_color);

                if (!hasTokenizeResult)
                    current = current + 1;
                else {
                    auto token_offset = token_begin - first.m_charsIter;
                    current = first + token_offset;
                    u64 token_length = 0;
                    Line::Flags flags(0);
                    flags.m_value = line.m_flags[token_offset];
                    if (flags.m_value == 0) {
                        token_length = token_end - token_begin;
                        if (token_color == PaletteIndex::Identifier) {
                            id.assign(token_begin, token_end);

                            // todo : almost all language definitions use lower case to specify keywords, so shouldn't this use ::tolower ?
                            if (!m_languageDefinition.m_caseSensitive)
                                std::ranges::transform(id, id.begin(), ::toupper);
                            else if (m_languageDefinition.m_keywords.contains(id))
                                token_color = PaletteIndex::Keyword;
                            else if (m_languageDefinition.m_identifiers.contains(id))
                                token_color = PaletteIndex::BuiltInType;
                            else if (id == "$")
                                token_color = PaletteIndex::GlobalVariable;
                        }
                    } else {
                        if ((token_color == PaletteIndex::Identifier || flags.m_bits.preprocessor) && !flags.m_bits.deactivated && !(flags.m_value & inComment)) {
                            id.assign(token_begin, token_end);
                            if (m_languageDefinition.m_preprocIdentifiers.contains(id)) {
                                token_color = PaletteIndex::Directive;
                                token_begin -= 1;
                                token_length = token_end - token_begin;
                                token_offset -= 1;
                            } else if (flags.m_bits.preprocessor) {
                                token_color = PaletteIndex::PreprocIdentifier;
                                token_length = token_end - token_begin;
                            }
                        }
                        if (flags.m_bits.matchedDelimiter) {
                            token_color = PaletteIndex::WarningText;
                            token_length = token_end - token_begin;
                        } else if (flags.m_bits.preprocessor && !flags.m_bits.deactivated) {
                            token_length = token_end - token_begin;
                        } else if ((token_color != PaletteIndex::Directive && token_color != PaletteIndex::PreprocIdentifier) || flags.m_bits.deactivated) {
                            if (flags.m_bits.deactivated && flags.m_bits.preprocessor) {
                                token_color = PaletteIndex::PreprocessorDeactivated;
                                token_begin -= 1;
                                token_offset -= 1;
                            } else if (id.assign(token_begin, token_end);flags.m_value & inComment && m_languageDefinition.m_preprocIdentifiers.contains(id)) {
                                token_color = getColorIndexFromFlags(flags);
                                token_begin -= 1;
                                token_offset -= 1;
                            }

                            auto flag = line.m_flags[token_offset];
                            token_length = line.m_flags.find_first_not_of(flag, token_offset + 1);
                            if (token_length == std::string::npos)
                                token_length = line.size() - token_offset;
                            else
                                token_length -= token_offset;

                            token_end = token_begin + token_length;
                            if (!flags.m_bits.preprocessor || flags.m_bits.deactivated)
                                token_color = getColorIndexFromFlags(flags);
                        }
                    }

                    if (token_color != PaletteIndex::Identifier || *current.m_colorsIter == static_cast<char>(PaletteIndex::Identifier)) {
                        if (token_offset + token_length >= line.m_colors.size()) {
                            auto colors = line.m_colors;
                            line.m_colors.resize(token_offset + token_length, static_cast<char>(PaletteIndex::Default));
                            std::ranges::copy(colors, line.m_colors.begin());
                        }
                        try {
                            line.m_colors.replace(token_offset, token_length, token_length, static_cast<char>(token_color));
                        } catch (const std::exception &e) {
                            fmt::print(stderr, "Error replacing color: {}\n", e.what());
                            return;
                        }
                    }
                    current = current + token_length;
                }
            }
            line.m_colorized = true;
        }
    }

    void TextEditor::colorizeInternal() {
        if (isEmpty() || !m_colorizerEnabled)
            return;

        if (m_updateFlags) {
            auto endLine = m_lines.size();
            auto commentStartLine = endLine;
            auto commentStartIndex = 0;
            auto withinGlobalDocComment = false;
            auto withinBlockDocComment = false;
            auto withinString = false;
            auto withinBlockComment = false;
            auto withinNotDef = false;
            auto currentLine = endLine;
            auto commentLength = 0;
            auto matchedBracket = false;

            std::vector<bool> ifDefs;
            ifDefs.push_back(true);
            m_defines.emplace_back("__IMHEX__");
            for (currentLine = 0; currentLine < endLine; currentLine++) {
                auto &line = m_lines[currentLine];
                auto lineLength = line.size();

                if (line.m_flags.size() != lineLength) {
                    line.m_flags.resize(lineLength, 0);
                    line.m_colorized = false;
                }


                auto withinComment = false;
                auto withinDocComment = false;
                auto withinPreproc = false;
                auto firstChar = true;   // there is no other non-whitespace characters in the line before

                auto setGlyphFlags = [&](i32 index) {
                    Line::Flags flags(0);
                    if (withinComment)
                        flags.m_value = (i32) Line::Comments::Line;
                    else if (withinDocComment)
                        flags.m_value = (i32) Line::Comments::Doc;
                    else if (withinBlockComment)
                        flags.m_value = (i32) Line::Comments::Block;
                    else if (withinGlobalDocComment)
                        flags.m_value = (i32) Line::Comments::Global;
                    else if (withinBlockDocComment)
                        flags.m_value = (i32) Line::Comments::BlockDoc;
                    flags.m_bits.deactivated = withinNotDef;
                    flags.m_bits.matchedDelimiter = matchedBracket;
                    if (m_lines[currentLine].m_flags[index] != flags.m_value) {
                        m_lines[currentLine].m_colorized = false;
                        m_lines[currentLine].m_flags[index] = flags.m_value;
                    }
                };

                u64 currentIndex = 0;
                if (line.empty())
                    continue;
                while (currentIndex < lineLength) {

                    char c = line[currentIndex];

                    matchedBracket = false;
                    if (MatchedBracket::s_separators.contains(c) && m_matchedBracket.isActive()) {
                        if (m_matchedBracket.m_nearCursor == getCharacterCoordinates(currentLine, currentIndex) || m_matchedBracket.m_matched == getCharacterCoordinates(currentLine, currentIndex))
                            matchedBracket = true;
                    } else if (MatchedBracket::s_operators.contains(c) && m_matchedBracket.isActive()) {
                        Coordinates current = setCoordinates(currentLine,currentIndex);
                        auto udt = static_cast<char>(PaletteIndex::UserDefinedType);
                        Coordinates cursor = Invalid;
                        //if (m_matchedBracket.m_nearCursor == setCoordinates(currentLine, currentIndex) || m_matchedBracket.m_matched == setCoordinates(currentLine, currentIndex)) {
                        if ((c == '<' && m_matchedBracket.m_nearCursor == current) ||  (c == '>' && m_matchedBracket.m_matched == current))
                            cursor = m_matchedBracket.m_nearCursor;
                        else if ((c == '>' && m_matchedBracket.m_nearCursor == current) ||  (c == '<' && m_matchedBracket.m_matched == current))
                            cursor = m_matchedBracket.m_matched;

                        if (cursor != Invalid) {
                            if (cursor.m_column == 0 && cursor.m_line > 0) {
                                cursor.m_line--;
                                cursor.m_column = m_lines[cursor.m_line].m_colors.size() - 1;
                            } else if (cursor.m_column > 0) {
                                cursor.m_column--;
                            }
                            while (std::isblank(m_lines[cursor.m_line].m_colors[cursor.m_column]) && (cursor.m_line != 0 || cursor.m_column != 0)) {
                                if (cursor.m_column == 0 && cursor.m_line > 0) {
                                    cursor.m_line--;
                                    cursor.m_column = m_lines[cursor.m_line].m_colors.size() - 1;
                                } else
                                    cursor.m_column--;
                            }
                            if (m_lines[cursor.m_line].m_colors[cursor.m_column] == udt && (cursor.m_line != 0 || cursor.m_column != 0))
                                matchedBracket = true;
                        }
                    }


                    if (c != m_languageDefinition.m_preprocChar && !isspace(c))
                        firstChar = false;

                    bool isComment = (commentStartLine < currentLine || (commentStartLine == currentLine && commentStartIndex <= (i64) currentIndex));

                    if (withinString) {
                        setGlyphFlags(currentIndex);
                        if (c == '\\') {
                            currentIndex++;
                            setGlyphFlags(currentIndex);
                        } else if (c == '\"')
                            withinString = false;
                    } else {
                        if (firstChar && c == m_languageDefinition.m_preprocChar && !isComment && !withinComment && !withinDocComment && !withinString) {
                            withinPreproc = true;
                            std::string directive;
                            auto start = currentIndex + 1;
                            while (start < line.size() && !isspace(line[start])) {
                                directive += line[start];
                                start++;
                            }

                            while (start < line.size() && isspace(line[start]))
                                start++;

                            if (directive == "endif" && !ifDefs.empty()) {
                                ifDefs.pop_back();
                                withinNotDef = !ifDefs.back();
                            } else {
                                std::string identifier;
                                while (start < line.size() && !isspace(line[start])) {
                                    identifier += line[start];
                                    start++;
                                }
                                auto definesBegin = m_defines.begin();
                                auto definesEnd = m_defines.end();
                                if (directive == "define") {
                                    if (!identifier.empty() && !withinNotDef && std::find(definesBegin, definesEnd, identifier) == definesEnd)
                                        m_defines.push_back(identifier);
                                } else if (directive == "undef") {
                                    if (!identifier.empty() && !withinNotDef)
                                        m_defines.erase(std::remove(definesBegin, definesEnd, identifier), definesEnd);
                                } else if (directive == "ifdef") {
                                    if (!withinNotDef) {
                                        bool isConditionMet = std::find(definesBegin, definesEnd, identifier) != definesEnd;
                                        ifDefs.push_back(isConditionMet);
                                    } else
                                        ifDefs.push_back(false);
                                } else if (directive == "ifndef") {
                                    if (!withinNotDef) {
                                        bool isConditionMet = std::find(definesBegin, definesEnd, identifier) == definesEnd;
                                        ifDefs.push_back(isConditionMet);
                                    } else
                                        ifDefs.push_back(false);
                                }
                            }
                        }

                        if (c == '\"' && !withinPreproc && !isComment && !withinComment && !withinDocComment) {
                            withinString = true;
                            setGlyphFlags(currentIndex);
                        } else {
                            auto pred = [](const char &a, const char &b) { return a == b; };

                            auto compareForth = [&](const std::string &a, const std::string &b) {
                                return !a.empty() && (currentIndex + a.size() <= b.size()) && equals(a.begin(), a.end(), b.begin() + currentIndex, b.begin() + (currentIndex + a.size()), pred);
                            };

                            auto compareBack = [&](const std::string &a, const std::string &b) {
                                return !a.empty() && currentIndex + 1 >= a.size() && equals(a.begin(), a.end(), b.begin() + (currentIndex + 1 - a.size()), b.begin() + (currentIndex + 1), pred);
                            };

                            if (!isComment && !withinComment && !withinDocComment && !withinPreproc && !withinString) {
                                if (compareForth(m_languageDefinition.m_docComment, line.m_chars)) {
                                    withinDocComment = !isComment;
                                    commentLength = 3;
                                } else if (compareForth(m_languageDefinition.m_singleLineComment, line.m_chars)) {
                                    withinComment = !isComment;
                                    commentLength = 2;
                                } else {
                                    bool isGlobalDocComment = compareForth(m_languageDefinition.m_globalDocComment, line.m_chars);
                                    bool isBlockDocComment = compareForth(m_languageDefinition.m_blockDocComment, line.m_chars);
                                    bool isBlockComment = compareForth(m_languageDefinition.m_commentStart, line.m_chars);
                                    if (isGlobalDocComment || isBlockDocComment || isBlockComment) {
                                        commentStartLine = currentLine;
                                        commentStartIndex = currentIndex;
                                        if (currentIndex < line.size() - 4 && isBlockComment &&
                                            line.m_chars[currentIndex + 2] == '*' &&
                                            line.m_chars[currentIndex + 3] == '/') {
                                            withinBlockComment = true;
                                            commentLength = 2;
                                        } else if (isGlobalDocComment) {
                                            withinGlobalDocComment = true;
                                            commentLength = 3;
                                        } else if (isBlockDocComment) {
                                            withinBlockDocComment = true;
                                            commentLength = 3;
                                        } else {
                                            withinBlockComment = true;
                                            commentLength = 2;
                                        }
                                    }
                                }
                                isComment = (commentStartLine < currentLine || (commentStartLine == currentLine && commentStartIndex <= (i64) currentIndex));
                            }
                            setGlyphFlags(currentIndex);

                            if (compareBack(m_languageDefinition.m_commentEnd, line.m_chars) && ((commentStartLine != currentLine) || (commentStartIndex + commentLength < (i64) currentIndex))) {
                                withinBlockComment = false;
                                withinBlockDocComment = false;
                                withinGlobalDocComment = false;
                                commentStartLine = endLine;
                                commentStartIndex = 0;
                                commentLength = 0;
                            }
                        }
                    }
                    if (currentIndex < line.size()) {
                        Line::Flags flags(0);
                        flags.m_value = m_lines[currentLine].m_flags[currentIndex];
                        flags.m_bits.preprocessor = withinPreproc;
                        m_lines[currentLine].m_flags[currentIndex] = flags.m_value;
                    }
                    auto utf8CharLen = utf8CharLength(c);
                    if (utf8CharLen > 1) {
                        Line::Flags flags(0);
                        flags.m_value = m_lines[currentLine].m_flags[currentIndex];
                        for (i32 j = 1; j < utf8CharLen; j++) {
                            currentIndex++;
                            m_lines[currentLine].m_flags[currentIndex] = flags.m_value;
                        }
                    }
                    currentIndex++;
                }
                withinNotDef = !ifDefs.back();
            }
            m_defines.clear();
        }
        colorizeRange();
    }

    void TextEditor::setLanguageDefinition(const LanguageDefinition &languageDef) {
        m_languageDefinition = languageDef;
        m_regexList.clear();

        for (auto &r: m_languageDefinition.m_tokenRegexStrings)
            m_regexList.emplace_back(std::regex(r.first, std::regex_constants::optimize), r.second);

        colorize();
    }

    const TextEditor::Palette &TextEditor::getPalette() { return s_paletteBase; }

    void TextEditor::setPalette(const Palette &value) {
        s_paletteBase = value;
    }

    const TextEditor::Palette &TextEditor::getDarkPalette() {
        const static Palette p = {
                {
                        0xff7f7f7f, // Default
                        0xffd69c56, // Keyword
                        0xff00ff00, // Number
                        0xff7070e0, // String
                        0xff70a0e0, // Char literal
                        0xffffffff, // Punctuation
                        0xff408080, // Preprocessor
                        0xffaaaaaa, // Identifier
                        0xff9bc64d, // Known identifier
                        0xffc040a0, // Preproc identifier
                        0xff708020, // Global Doc Comment
                        0xff586820, // Doc Comment
                        0xff206020, // Comment (single line)
                        0xff406020, // Comment (multi line)
                        0xff004545, // Preprocessor deactivated
                        0xff101010, // Background
                        0xffe0e0e0, // Cursor
                        0x80a06020, // Selection
                        0x800020ff, // ErrorMarker
                        0x40f08000, // Breakpoint
                        0xff707000, // Line number
                        0x40000000, // Current line fill
                        0x40808080, // Current line fill (inactive)
                        0x40a0a0a0, // Current line edge
                }
        };
        return p;
    }

    const TextEditor::Palette &TextEditor::getLightPalette() {
        const static Palette p = {
                {
                        0xff7f7f7f, // None
                        0xffff0c06, // Keyword
                        0xff008000, // Number
                        0xff2020a0, // String
                        0xff304070, // Char literal
                        0xff000000, // Punctuation
                        0xff406060, // Preprocessor
                        0xff404040, // Identifier
                        0xff606010, // Known identifier
                        0xffc040a0, // Preproc identifier
                        0xff707820, // Global Doc Comment
                        0xff586020, // Doc Comment
                        0xff205020, // Comment (single line)
                        0xff405020, // Comment (multi line)
                        0xffa7cccc, // Preprocessor deactivated
                        0xffffffff, // Background
                        0xff000000, // Cursor
                        0x80600000, // Selection
                        0xa00010ff, // ErrorMarker
                        0x80f08000, // Breakpoint
                        0xff505000, // Line number
                        0x40000000, // Current line fill
                        0x40808080, // Current line fill (inactive)
                        0x40000000, // Current line edge
                }
        };
        return p;
    }

    const TextEditor::Palette &TextEditor::getRetroBluePalette() {
        const static Palette p = {
                {
                        0xff00ffff, // None
                        0xffffff00, // Keyword
                        0xff00ff00, // Number
                        0xff808000, // String
                        0xff808000, // Char literal
                        0xffffffff, // Punctuation
                        0xff008000, // Preprocessor
                        0xff00ffff, // Identifier
                        0xffffffff, // Known identifier
                        0xffff00ff, // Preproc identifier
                        0xff101010, // Global Doc Comment
                        0xff202020, // Doc Comment
                        0xff808080, // Comment (single line)
                        0xff404040, // Comment (multi line)
                        0xff004000, // Preprocessor deactivated
                        0xff800000, // Background
                        0xff0080ff, // Cursor
                        0x80ffff00, // Selection
                        0xa00000ff, // ErrorMarker
                        0x80ff8000, // Breakpoint
                        0xff808000, // Line number
                        0x40000000, // Current line fill
                        0x40808080, // Current line fill (inactive)
                        0x40000000, // Current line edge
                }
        };
        return p;
    }

    const TextEditor::LanguageDefinition &TextEditor::LanguageDefinition::CPlusPlus() {
        static bool inited = false;
        static LanguageDefinition langDef;
        if (!inited) {
            static const char *const cppKeywords[] = {
                    "alignas", "alignof", "and", "and_eq", "asm", "atomic_cancel", "atomic_commit", "atomic_noexcept",
                    "auto", "bitand", "bitor", "bool", "break", "case", "catch", "char", "char16_t", "char32_t",
                    "class", "compl", "concept", "const", "constexpr", "const_cast", "continue", "decltype", "default",
                    "delete", "do", "double", "dynamic_cast", "else", "enum", "explicit", "export", "extern", "false",
                    "float", "for", "friend", "goto", "if", "import", "inline", "int", "long", "module", "mutable",
                    "namespace", "new", "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private",
                    "protected", "public", "register", "reinterpret_cast", "requires", "return", "short", "signed",
                    "sizeof", "static", "static_assert", "static_cast", "struct", "switch", "synchronized", "template",
                    "this", "thread_local", "throw", "true", "try", "typedef", "typeid", "typename", "union",
                    "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq"
            };
            for (auto &k: cppKeywords)
                langDef.m_keywords.insert(k);

            static const char *const identifiers[] = {
                    "abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh",
                    "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha",
                    "isdigit", "isgraph", "ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp",
                    "modf", "pow", "printf", "sprintf", "snprintf", "putchar", "putenv", "puts", "rand", "remove",
                    "rename", "sinh", "sqrt", "srand", "strcat", "strcmp", "strerror", "time", "tolower", "toupper",
                    "std", "string", "vector", "map", "unordered_map", "set", "unordered_set", "min", "max"
            };
            for (auto &k: identifiers) {
                Identifier id;
                id.m_declaration = "Built-in function";
                langDef.m_identifiers.insert(std::make_pair(std::string(k), id));
            }

            langDef.m_tokenize = [](strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end, PaletteIndex &paletteIndex) -> bool {
                paletteIndex = PaletteIndex::Max;

                while (in_begin < in_end && isascii(*in_begin) && isblank(*in_begin))
                    in_begin++;

                if (in_begin == in_end) {
                    out_begin = in_end;
                    out_end = in_end;
                    paletteIndex = PaletteIndex::Default;
                } else if (tokenizeCStyleString(in_begin, in_end, out_begin, out_end))
                    paletteIndex = PaletteIndex::StringLiteral;
                else if (tokenizeCStyleCharacterLiteral(in_begin, in_end, out_begin, out_end))
                    paletteIndex = PaletteIndex::CharLiteral;
                else if (tokenizeCStyleIdentifier(in_begin, in_end, out_begin, out_end))
                    paletteIndex = PaletteIndex::Identifier;
                else if (tokenizeCStyleNumber(in_begin, in_end, out_begin, out_end))
                    paletteIndex = PaletteIndex::NumericLiteral;
                else if (tokenizeCStyleSeparator(in_begin, in_end, out_begin, out_end))
                    paletteIndex = PaletteIndex::Separator;
                else if (tokenizeCStyleOperator(in_begin, in_end, out_begin, out_end))
                    paletteIndex = PaletteIndex::Operator;

                return paletteIndex != PaletteIndex::Max;
            };

            langDef.m_commentStart = "/*";
            langDef.m_commentEnd = "*/";
            langDef.m_singleLineComment = "//";

            langDef.m_caseSensitive = true;
            langDef.m_autoIndentation = true;

            langDef.m_name = "C++";

            inited = true;
        }
        return langDef;
    }

    const TextEditor::LanguageDefinition &TextEditor::LanguageDefinition::HLSL() {
        static bool inited = false;
        static LanguageDefinition langDef;
        if (!inited) {
            static const char *const keywords[] = {
                    "AppendStructuredBuffer", "asm", "asm_fragment", "BlendState", "bool", "break", "Buffer",
                    "ByteAddressBuffer", "case", "cbuffer", "centroid", "class", "column_major", "compile",
                    "compile_fragment", "CompileShader", "const", "continue", "ComputeShader", "ConsumeStructuredBuffer",
                    "default", "DepthStencilState", "DepthStencilView", "discard", "do", "double", "DomainShader",
                    "dword", "else", "export", "extern", "false", "float", "for", "fxgroup", "GeometryShader",
                    "groupshared", "half", "Hullshader", "if", "in", "inline", "inout", "InputPatch", "int",
                    "interface", "line", "lineadj", "linear", "LineStream", "matrix", "min16float", "min10float",
                    "min16int", "min12int", "min16uint", "namespace", "nointerpolation", "noperspective", "NULL",
                    "out", "OutputPatch", "packoffset", "pass", "pixelfragment", "PixelShader", "point", "PointStream",
                    "precise", "RasterizerState", "RenderTargetView", "return", "register", "row_major", "RWBuffer",
                    "RWByteAddressBuffer", "RWStructuredBuffer", "RWTexture1D", "RWTexture1DArray", "RWTexture2D",
                    "RWTexture2DArray", "RWTexture3D", "sample", "sampler", "SamplerState", "SamplerComparisonState",
                    "shared", "snorm", "stateblock", "stateblock_state", "static", "string", "struct", "switch",
                    "StructuredBuffer", "tbuffer", "technique", "technique10", "technique11", "texture", "Texture1D",
                    "Texture1DArray", "Texture2D", "Texture2DArray", "Texture2DMS", "Texture2DMSArray", "Texture3D",
                    "TextureCube", "TextureCubeArray", "true", "typedef", "triangle", "triangleadj", "TriangleStream",
                    "uint", "uniform", "unorm", "unsigned", "vector", "vertexfragment", "VertexShader", "void",
                    "volatile", "while", "bool1", "bool2", "bool3", "bool4", "double1", "double2", "double3",
                    "double4", "float1", "float2", "float3", "float4", "int1", "int2", "int3", "int4", "in", "out",
                    "inout", "uint1", "uint2", "uint3", "uint4", "dword1", "dword2", "dword3", "dword4", "half1",
                    "half2", "half3", "half4", "float1x1", "float2x1", "float3x1", "float4x1", "float1x2", "float2x2",
                    "float3x2", "float4x2", "float1x3", "float2x3", "float3x3", "float4x3", "float1x4", "float2x4",
                    "float3x4", "float4x4", "half1x1", "half2x1", "half3x1", "half4x1", "half1x2", "half2x2", "half3x2",
                    "half4x2", "half1x3", "half2x3", "half3x3", "half4x3", "half1x4", "half2x4", "half3x4", "half4x4",
            };
            for (auto &k: keywords)
                langDef.m_keywords.insert(k);

            static const char *const identifiers[] = {
                    "abort", "abs", "acos", "all", "AllMemoryBarrier", "AllMemoryBarrierWithGroupSync", "any",
                    "asdouble", "asfloat", "asin", "asint", "asint", "asuint", "asuint", "atan", "atan2", "ceil",
                    "CheckAccessFullyMapped", "clamp", "clip", "cos", "cosh", "countbits", "cross", "D3DCOLORtoUBYTE4",
                    "ddx", "ddx_coarse", "ddx_fine", "ddy", "ddy_coarse", "ddy_fine", "degrees", "determinant",
                    "DeviceMemoryBarrier", "DeviceMemoryBarrierWithGroupSync", "distance", "dot", "dst", "errorf",
                    "EvaluateAttributeAtCentroid", "EvaluateAttributeAtSample", "EvaluateAttributeSnapped", "exp",
                    "exp2", "f16tof32", "f32tof16", "faceforward", "firstbithigh", "firstbitlow", "floor", "fma",
                    "fmod", "frac", "frexp", "fwidth", "GetRenderTargetSampleCount", "GetRenderTargetSamplePosition",
                    "GroupMemoryBarrier", "GroupMemoryBarrierWithGroupSync", "InterlockedAdd", "InterlockedAnd",
                    "InterlockedCompareExchange", "InterlockedCompareStore", "InterlockedExchange", "InterlockedMax",
                    "InterlockedMin", "InterlockedOr", "InterlockedXor", "isfinite", "isinf", "isnan", "ldexp",
                    "length", "lerp", "lit", "log", "log10", "log2", "mad", "max", "min", "modf", "msad4", "mul",
                    "noise", "normalize", "pow", "printf", "Process2DQuadTessFactorsAvg", "Process2DQuadTessFactorsMax",
                    "Process2DQuadTessFactorsMin", "ProcessIsolineTessFactors", "ProcessQuadTessFactorsAvg",
                    "ProcessQuadTessFactorsMax", "ProcessQuadTessFactorsMin", "ProcessTriTessFactorsAvg",
                    "ProcessTriTessFactorsMax", "ProcessTriTessFactorsMin", "radians", "rcp", "reflect", "refract",
                    "reversebits", "round", "rsqrt", "saturate", "sign", "sin", "sincos", "sinh", "smoothstep", "sqrt",
                    "step", "tan", "tanh", "tex1D", "tex1D", "tex1Dbias", "tex1Dgrad", "tex1Dlod", "tex1Dproj", "tex2D",
                    "tex2D", "tex2Dbias", "tex2Dgrad", "tex2Dlod", "tex2Dproj", "tex3D", "tex3D", "tex3Dbias",
                    "tex3Dgrad", "tex3Dlod", "tex3Dproj", "texCUBE", "texCUBE", "texCUBEbias", "texCUBEgrad",
                    "texCUBElod", "texCUBEproj", "transpose", "trunc"
            };
            for (auto &k: identifiers) {
                Identifier id;
                id.m_declaration = "Built-in function";
                langDef.m_identifiers.insert(std::make_pair(std::string(k), id));
            }

            langDef.m_tokenRegexStrings.emplace_back("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Directive);
            langDef.m_tokenRegexStrings.emplace_back(R"(L?\"(\\.|[^\"])*\")", PaletteIndex::StringLiteral);
            langDef.m_tokenRegexStrings.emplace_back(R"(\'\\?[^\']\')", PaletteIndex::CharLiteral);
            langDef.m_tokenRegexStrings.emplace_back("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::NumericLiteral);
            langDef.m_tokenRegexStrings.emplace_back("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral);
            langDef.m_tokenRegexStrings.emplace_back("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral);
            langDef.m_tokenRegexStrings.emplace_back("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::NumericLiteral);
            langDef.m_tokenRegexStrings.emplace_back("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier);
            langDef.m_tokenRegexStrings.emplace_back(R"([\!\%\^\&\*\-\+\=\~\|\<\>\?\/])", PaletteIndex::Operator);
            langDef.m_tokenRegexStrings.emplace_back(R"([\[\]\{\}\(\)\;\,\.])", PaletteIndex::Separator);
            langDef.m_commentStart = "/*";
            langDef.m_commentEnd = "*/";
            langDef.m_singleLineComment = "//";

            langDef.m_caseSensitive = true;
            langDef.m_autoIndentation = true;

            langDef.m_name = "HLSL";

            inited = true;
        }
        return langDef;
    }

    const TextEditor::LanguageDefinition &TextEditor::LanguageDefinition::GLSL() {
        static bool inited = false;
        static LanguageDefinition langDef;
        if (!inited) {
            static const char *const keywords[] = {
                    "auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum",
                    "extern", "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return",
                    "short", "signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void",
                    "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic",
                    "_Imaginary", "_Noreturn", "_Static_assert", "_Thread_local"
            };
            for (auto &k: keywords)
                langDef.m_keywords.insert(k);

            static const char *const identifiers[] = {
                    "abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh",
                    "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha",
                    "isdigit", "isgraph", "ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp",
                    "modf", "pow", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand",
                    "strcat", "strcmp", "strerror", "time", "tolower", "toupper"
            };
            for (auto &k: identifiers) {
                Identifier id;
                id.m_declaration = "Built-in function";
                langDef.m_identifiers.insert(std::make_pair(std::string(k), id));
            }

            langDef.m_tokenRegexStrings.emplace_back("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Directive);
            langDef.m_tokenRegexStrings.emplace_back(R"(L?\"(\\.|[^\"])*\")", PaletteIndex::StringLiteral);
            langDef.m_tokenRegexStrings.emplace_back(R"(\'\\?[^\']\')", PaletteIndex::CharLiteral);
            langDef.m_tokenRegexStrings.emplace_back("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?",PaletteIndex::NumericLiteral);
            langDef.m_tokenRegexStrings.emplace_back("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral);
            langDef.m_tokenRegexStrings.emplace_back("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral);
            langDef.m_tokenRegexStrings.emplace_back("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::NumericLiteral);
            langDef.m_tokenRegexStrings.emplace_back("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier);
            langDef.m_tokenRegexStrings.emplace_back(R"([\!\%\^\&\*\-\+\=\~\|\<\>\?\/])", PaletteIndex::Operator);
            langDef.m_tokenRegexStrings.emplace_back(R"([\[\]\{\}\(\)\;\,\.])", PaletteIndex::Separator);
            langDef.m_commentStart = "/*";
            langDef.m_commentEnd = "*/";
            langDef.m_singleLineComment = "//";

            langDef.m_caseSensitive = true;
            langDef.m_autoIndentation = true;

            langDef.m_name = "GLSL";

            inited = true;
        }
        return langDef;
    }

    const TextEditor::LanguageDefinition &TextEditor::LanguageDefinition::C() {
        static bool inited = false;
        static LanguageDefinition langDef;
        if (!inited) {
            static const char *const keywords[] = {
                    "auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else", "enum",
                    "extern", "float", "for", "goto", "if", "inline", "int", "long", "register", "restrict", "return",
                    "short", "signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void",
                    "volatile", "while", "_Alignas", "_Alignof", "_Atomic", "_Bool", "_Complex", "_Generic",
                    "_Imaginary", "_Noreturn", "_Static_assert", "_Thread_local"
            };
            for (auto &k: keywords)
                langDef.m_keywords.insert(k);

            static const char *const identifiers[] = {
                    "abort", "abs", "acos", "asin", "atan", "atexit", "atof", "atoi", "atol", "ceil", "clock", "cosh",
                    "ctime", "div", "exit", "fabs", "floor", "fmod", "getchar", "getenv", "isalnum", "isalpha",
                    "isdigit", "isgraph", "ispunct", "isspace", "isupper", "kbhit", "log10", "log2", "log", "memcmp",
                    "modf", "pow", "putchar", "putenv", "puts", "rand", "remove", "rename", "sinh", "sqrt", "srand",
                    "strcat", "strcmp", "strerror", "time", "tolower", "toupper"
            };
            for (auto &k: identifiers) {
                Identifier id;
                id.m_declaration = "Built-in function";
                langDef.m_identifiers.insert(std::make_pair(std::string(k), id));
            }

            langDef.m_tokenize = [](strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end, PaletteIndex &paletteIndex) -> bool {
                paletteIndex = PaletteIndex::Max;

                while (in_begin < in_end && isascii(*in_begin) && isblank(*in_begin))
                    in_begin++;

                if (in_begin == in_end) {
                    out_begin = in_end;
                    out_end = in_end;
                    paletteIndex = PaletteIndex::Default;
                } else if (tokenizeCStyleString(in_begin, in_end, out_begin, out_end))
                    paletteIndex = PaletteIndex::StringLiteral;
                else if (tokenizeCStyleCharacterLiteral(in_begin, in_end, out_begin, out_end))
                    paletteIndex = PaletteIndex::CharLiteral;
                else if (tokenizeCStyleIdentifier(in_begin, in_end, out_begin, out_end))
                    paletteIndex = PaletteIndex::Identifier;
                else if (tokenizeCStyleNumber(in_begin, in_end, out_begin, out_end))
                    paletteIndex = PaletteIndex::NumericLiteral;
                else if (tokenizeCStyleOperator(in_begin, in_end, out_begin, out_end))
                    paletteIndex = PaletteIndex::Operator;
                else if (tokenizeCStyleSeparator(in_begin, in_end, out_begin, out_end))
                    paletteIndex = PaletteIndex::Separator;

                return paletteIndex != PaletteIndex::Max;
            };

            langDef.m_commentStart = "/*";
            langDef.m_commentEnd = "*/";
            langDef.m_singleLineComment = "//";

            langDef.m_caseSensitive = true;
            langDef.m_autoIndentation = true;

            langDef.m_name = "C";

            inited = true;
        }
        return langDef;
    }

    const TextEditor::LanguageDefinition &TextEditor::LanguageDefinition::SQL() {
        static bool inited = false;
        static LanguageDefinition langDef;
        if (!inited) {
            static const char *const keywords[] = {
                    "ADD", "EXCEPT", "PERCENT", "ALL", "EXEC", "PLAN", "ALTER", "EXECUTE", "PRECISION", "AND", "EXISTS",
                    "PRIMARY", "ANY", "EXIT", "PRINT", "AS", "FETCH", "PROC", "ASC", "FILE", "PROCEDURE",
                    "AUTHORIZATION", "FILLFACTOR", "PUBLIC", "BACKUP", "FOR", "RAISERROR", "BEGIN", "FOREIGN", "READ",
                    "BETWEEN", "FREETEXT", "READTEXT", "BREAK", "FREETEXTTABLE", "RECONFIGURE", "BROWSE", "FROM",
                    "REFERENCES", "BULK", "FULL", "REPLICATION", "BY", "FUNCTION", "RESTORE", "CASCADE", "GOTO",
                    "RESTRICT", "CASE", "GRANT", "RETURN", "CHECK", "GROUP", "REVOKE", "CHECKPOINT", "HAVING", "RIGHT",
                    "CLOSE", "HOLDLOCK", "ROLLBACK", "CLUSTERED", "IDENTITY", "ROWCOUNT", "COALESCE", "IDENTITY_INSERT",
                    "ROWGUIDCOL", "COLLATE", "IDENTITYCOL", "RULE", "COLUMN", "IF", "SAVE", "COMMIT", "IN", "SCHEMA",
                    "COMPUTE", "INDEX", "SELECT", "CONSTRAINT", "INNER", "SESSION_USER", "CONTAINS", "INSERT", "SET",
                    "CONTAINSTABLE", "INTERSECT", "SETUSER", "CONTINUE", "INTO", "SHUTDOWN", "CONVERT", "IS", "SOME",
                    "CREATE", "JOIN", "STATISTICS", "CROSS", "KEY", "SYSTEM_USER", "CURRENT", "KILL", "TABLE",
                    "CURRENT_DATE", "LEFT", "TEXTSIZE", "CURRENT_TIME", "LIKE", "THEN", "CURRENT_TIMESTAMP", "LINENO",
                    "TO", "CURRENT_USER", "LOAD", "TOP", "CURSOR", "NATIONAL", "TRAN", "DATABASE", "NOCHECK",
                    "TRANSACTION", "DBCC", "NONCLUSTERED", "TRIGGER", "DEALLOCATE", "NOT", "TRUNCATE", "DECLARE",
                    "NULL", "TSEQUAL", "DEFAULT", "NULLIF", "UNION", "DELETE", "OF", "UNIQUE", "DENY", "OFF", "UPDATE",
                    "DESC", "OFFSETS", "UPDATETEXT", "DISK", "ON", "USE", "DISTINCT", "OPEN", "USER", "DISTRIBUTED",
                    "OPENDATASOURCE", "VALUES", "DOUBLE", "OPENQUERY", "VARYING", "DROP", "OPENROWSET", "VIEW", "DUMMY",
                    "OPENXML", "WAITFOR", "DUMP", "OPTION", "WHEN", "ELSE", "OR", "WHERE", "END", "ORDER", "WHILE",
                    "ERRLVL", "OUTER", "WITH", "ESCAPE", "OVER", "WRITETEXT"
            };

            for (auto &k: keywords)
                langDef.m_keywords.insert(k);

            static const char *const identifiers[] = {
                    "ABS", "ACOS", "ADD_MONTHS", "ASCII", "ASCIISTR", "ASIN", "ATAN", "ATAN2", "AVG", "BFILENAME",
                    "BIN_TO_NUM", "BITAND", "CARDINALITY", "CASE", "CAST", "CEIL", "CHARTOROWID", "CHR", "COALESCE",
                    "COMPOSE", "CONCAT", "CONVERT", "CORR", "COS", "COSH", "COUNT", "COVAR_POP", "COVAR_SAMP",
                    "CUME_DIST", "CURRENT_DATE", "CURRENT_TIMESTAMP", "DBTIMEZONE", "DECODE", "DECOMPOSE", "DENSE_RANK",
                    "DUMP", "EMPTY_BLOB", "EMPTY_CLOB", "EXP", "EXTRACT", "FIRST_VALUE", "FLOOR", "FROM_TZ", "GREATEST",
                    "GROUP_ID", "HEXTORAW", "INITCAP", "INSTR", "INSTR2", "INSTR4", "INSTRB", "INSTRC", "LAG",
                    "LAST_DAY", "LAST_VALUE", "LEAD", "LEAST", "LENGTH", "LENGTH2", "LENGTH4", "LENGTHB", "LENGTHC",
                    "LISTAGG", "LN", "LNNVL", "LOCALTIMESTAMP", "LOG", "LOWER", "LPAD", "LTRIM", "MAX", "MEDIAN", "MIN",
                    "MOD", "MONTHS_BETWEEN", "NANVL", "NCHR", "NEW_TIME", "NEXT_DAY", "NTH_VALUE", "NULLIF",
                    "NUMTODSINTERVAL", "NUMTOYMINTERVAL", "NVL", "NVL2", "POWER", "RANK", "RAWTOHEX", "REGEXP_COUNT",
                    "REGEXP_INSTR", "REGEXP_REPLACE", "REGEXP_SUBSTR", "REMAINDER", "REPLACE", "ROUND", "ROWNUM",
                    "RPAD", "RTRIM", "SESSIONTIMEZONE", "SIGN", "SIN", "SINH", "SOUNDEX", "SQRT", "STDDEV", "SUBSTR",
                    "SUM", "SYS_CONTEXT", "SYSDATE", "SYSTIMESTAMP", "TAN", "TANH", "TO_CHAR", "TO_CLOB", "TO_DATE",
                    "TO_DSINTERVAL", "TO_LOB", "TO_MULTI_BYTE", "TO_NCLOB", "TO_NUMBER", "TO_SINGLE_BYTE",
                    "TO_TIMESTAMP", "TO_TIMESTAMP_TZ", "TO_YMINTERVAL", "TRANSLATE", "TRIM", "TRUNC", "TZ_OFFSET",
                    "UID", "UPPER", "USER", "USERENV", "VAR_POP", "VAR_SAMP", "VARIANCE", "VSIZE "
            };
            for (auto &k: identifiers) {
                Identifier id;
                id.m_declaration = "Built-in function";
                langDef.m_identifiers.insert(std::make_pair(std::string(k), id));
            }

            langDef.m_tokenRegexStrings.emplace_back("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Directive);
            langDef.m_tokenRegexStrings.emplace_back(R"(L?\"(\\.|[^\"])*\")", PaletteIndex::StringLiteral);
            langDef.m_tokenRegexStrings.emplace_back(R"(\'\\?[^\']\')", PaletteIndex::CharLiteral);
            langDef.m_tokenRegexStrings.emplace_back("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?",PaletteIndex::NumericLiteral);
            langDef.m_tokenRegexStrings.emplace_back("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral);
            langDef.m_tokenRegexStrings.emplace_back("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral);
            langDef.m_tokenRegexStrings.emplace_back("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::NumericLiteral);
            langDef.m_tokenRegexStrings.emplace_back("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier);
            langDef.m_tokenRegexStrings.emplace_back(R"([\!\%\^\&\*\-\+\=\~\|\<\>\?\/])", PaletteIndex::Operator);
            langDef.m_tokenRegexStrings.emplace_back(R"([\[\]\{\}\(\)\;\,\.])", PaletteIndex::Separator);

            langDef.m_commentStart = "/*";
            langDef.m_commentEnd = "*/";
            langDef.m_singleLineComment = "//";

            langDef.m_caseSensitive = false;
            langDef.m_autoIndentation = false;

            langDef.m_name = "SQL";

            inited = true;
        }
        return langDef;
    }

    const TextEditor::LanguageDefinition &TextEditor::LanguageDefinition::AngelScript() {
        static bool inited = false;
        static LanguageDefinition langDef;
        if (!inited) {
            static const char *const keywords[] = {
                    "and", "abstract", "auto", "bool", "break", "case", "cast", "class", "const", "continue", "default",
                    "do", "double", "else", "enum", "false", "final", "float", "for", "from", "funcdef", "function",
                    "get", "if", "import", "in", "inout", "int", "interface", "int8", "int16", "int32", "int64", "is",
                    "mixin", "namespace", "not", "null", "or", "out", "override", "private", "protected", "return",
                    "set", "shared", "super", "switch", "this ", "true", "typedef", "uint", "uint8", "uint16", "uint32",
                    "uint64", "void", "while", "xor"
            };

            for (auto &k: keywords)
                langDef.m_keywords.insert(k);

            static const char *const identifiers[] = {
                    "cos", "sin", "tab", "acos", "asin", "atan", "atan2", "cosh", "sinh", "tanh", "log", "log10", "pow",
                    "sqrt", "abs", "ceil", "floor", "fraction", "closeTo", "fpFromIEEE", "fpToIEEE", "complex",
                    "opEquals", "opAddAssign", "opSubAssign", "opMulAssign", "opDivAssign", "opAdd", "opSub", "opMul",
                    "opDiv"
            };
            for (auto &k: identifiers) {
                Identifier id;
                id.m_declaration = "Built-in function";
                langDef.m_identifiers.insert(std::make_pair(std::string(k), id));
            }

            langDef.m_tokenRegexStrings.emplace_back("[ \\t]*#[ \\t]*[a-zA-Z_]+", PaletteIndex::Directive);
            langDef.m_tokenRegexStrings.emplace_back(R"(L?\"(\\.|[^\"])*\")", PaletteIndex::StringLiteral);
            langDef.m_tokenRegexStrings.emplace_back(R"(\'\\?[^\']\')", PaletteIndex::CharLiteral);
            langDef.m_tokenRegexStrings.emplace_back("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?",PaletteIndex::NumericLiteral);
            langDef.m_tokenRegexStrings.emplace_back("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral);
            langDef.m_tokenRegexStrings.emplace_back("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral);
            langDef.m_tokenRegexStrings.emplace_back("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::NumericLiteral);
            langDef.m_tokenRegexStrings.emplace_back("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier);
            langDef.m_tokenRegexStrings.emplace_back(R"([\!\%\^\&\*\-\+\=\~\|\<\>\?\/])", PaletteIndex::Operator);
            langDef.m_tokenRegexStrings.emplace_back(R"([\[\]\{\}\(\)\;\,\.])", PaletteIndex::Separator);

            langDef.m_commentStart = "/*";
            langDef.m_commentEnd = "*/";
            langDef.m_singleLineComment = "//";

            langDef.m_caseSensitive = true;
            langDef.m_autoIndentation = true;

            langDef.m_name = "AngelScript";

            inited = true;
        }
        return langDef;
    }

    const TextEditor::LanguageDefinition &TextEditor::LanguageDefinition::Lua() {
        static bool inited = false;
        static LanguageDefinition langDef;
        if (!inited) {
            static const char *const keywords[] = {
                    "and", "break", "do", "", "else", "elseif", "end", "false", "for", "function", "if", "in", "",
                    "local", "nil", "not", "or", "repeat", "return", "then", "true", "until", "while"
            };

            for (auto &k: keywords)
                langDef.m_keywords.insert(k);

            static const char *const identifiers[] = {
                    "assert", "collectgarbage", "dofile", "error", "getmetatable", "ipairs", "loadfile", "load",
                    "loadstring", "next", "pairs", "pcall", "print", "rawequal", "rawlen", "rawget", "rawset", "select",
                    "setmetatable", "tonumber", "tostring", "type", "xpcall", "_G", "_VERSION", "arshift", "band",
                    "bnot", "bor", "bxor", "btest", "extract", "lrotate", "lshift", "replace", "rrotate", "rshift",
                    "create", "resume", "running", "status", "wrap", "yield", "isyieldable", "debug", "getuservalue",
                    "gethook", "getinfo", "getlocal", "getregistry", "getmetatable", "getupvalue", "upvaluejoin",
                    "upvalueid", "setuservalue", "sethook", "setlocal", "setmetatable", "setupvalue", "traceback",
                    "close", "flush", "input", "lines", "open", "output", "popen", "read", "tmpfile", "type", "write",
                    "close", "flush", "lines", "read", "seek", "setvbuf", "write", "__gc", "__tostring", "abs", "acos",
                    "asin", "atan", "ceil", "cos", "deg", "exp", "tointeger", "floor", "fmod", "ult", "log", "max",
                    "min", "modf", "rad", "random", "randomseed", "sin", "sqrt", "string", "tan", "type", "atan2",
                    "cosh", "sinh", "tanh", "pow", "frexp", "ldexp", "log10", "pi", "huge", "maxinteger", "mininteger",
                    "loadlib", "searchpath", "seeall", "preload", "cpath", "path", "searchers", "loaded", "module",
                    "require", "clock", "date", "difftime", "execute", "exit", "getenv", "remove", "rename",
                    "setlocale", "time", "tmpname", "byte", "char", "dump", "find", "format", "gmatch", "gsub", "len",
                    "lower", "match", "rep", "reverse", "sub", "upper", "pack", "packsize", "unpack", "concat", "maxn",
                    "insert", "pack", "unpack", "remove", "move", "sort", "offset", "codepoint", "char", "len", "codes",
                    "charpattern", "coroutine", "table", "io", "os", "string", "utf8", "bit32", "math", "debug",
                    "package"
            };
            for (auto &k: identifiers) {
                Identifier id;
                id.m_declaration = "Built-in function";
                langDef.m_identifiers.insert(std::make_pair(std::string(k), id));
            }

            langDef.m_tokenRegexStrings.emplace_back(R"([ \t]*#[ \t]*[a-zA-Z_]+)", PaletteIndex::Directive);
            langDef.m_tokenRegexStrings.emplace_back(R"(L?\"(\\.|[^\"])*\")", PaletteIndex::StringLiteral);
            langDef.m_tokenRegexStrings.emplace_back(R"(\'\\?[^\']\')", PaletteIndex::CharLiteral);
            langDef.m_tokenRegexStrings.emplace_back("[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?", PaletteIndex::NumericLiteral);
            langDef.m_tokenRegexStrings.emplace_back("[+-]?[0-9]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral);
            langDef.m_tokenRegexStrings.emplace_back("0[0-7]+[Uu]?[lL]?[lL]?", PaletteIndex::NumericLiteral);
            langDef.m_tokenRegexStrings.emplace_back("0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?", PaletteIndex::NumericLiteral);
            langDef.m_tokenRegexStrings.emplace_back("[a-zA-Z_][a-zA-Z0-9_]*", PaletteIndex::Identifier);
            langDef.m_tokenRegexStrings.emplace_back(R"([\!\%\^\&\*\-\+\=\~\|\<\>\?\/])", PaletteIndex::Operator);
            langDef.m_tokenRegexStrings.emplace_back(R"([\[\]\{\}\(\)\;\,\.])", PaletteIndex::Separator);

            langDef.m_commentStart = "--[[";
            langDef.m_commentEnd = "]]";
            langDef.m_singleLineComment = "--";

            langDef.m_caseSensitive = true;
            langDef.m_autoIndentation = false;

            langDef.m_name = "Lua";

            inited = true;
        }
        return langDef;
    }

    bool tokenizeCStyleString(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end) {
        auto p = in_begin;

        if (*p == '"') {
            p++;

            while (p < in_end) {
                // handle end of string
                if (*p == '"') {
                    out_begin = in_begin;
                    out_end = p + 1;
                    return true;
                }

                // handle escape character for "
                if (*p == '\\' && p + 1 < in_end) {
                    if (p[1] == '\\' || p[1] == '"') {
                        p++;
                    }
                }

                p++;
            }
        }

        return false;
    }

    bool tokenizeCStyleCharacterLiteral(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end) {
        auto p = in_begin;

        if (*p == '\'') {
            p++;

            // handle escape characters
            if (p < in_end && *p == '\\')
                p++;

            if (p < in_end)
                p++;

            // handle end of character literal
            if (p < in_end && *p == '\'') {
                out_begin = in_begin;
                out_end = p + 1;
                return true;
            }
        }

        return false;
    }

    bool tokenizeCStyleIdentifier(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end) {
        auto p = in_begin;

        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_' || *p == '$') {
            p++;

            while ((p < in_end) && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_'))
                p++;

            out_begin = in_begin;
            out_end = p;
            return true;
        }

        return false;
    }

    bool tokenizeCStyleNumber(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end) {
        auto p = in_begin;

        const bool startsWithNumber = *p >= '0' && *p <= '9';

        if (!startsWithNumber)
            return false;

        p++;

        bool hasNumber = startsWithNumber;

        while (p < in_end && (*p >= '0' && *p <= '9')) {
            hasNumber = true;

            p++;
        }

        if (!hasNumber)
            return false;

        bool isFloat = false;
        bool isHex = false;
        bool isBinary = false;

        if (p < in_end) {
            if (*p == '.') {
                isFloat = true;

                p++;

                while (p < in_end && (*p >= '0' && *p <= '9'))
                    p++;
            } else if (*p == 'x' || *p == 'X') {
                // hex formatted integer of the type 0xef80

                isHex = true;

                p++;

                while (p < in_end && ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F') || *p == '.' || *p == 'p' || *p == 'P'))
                    p++;
            } else if (*p == 'b' || *p == 'B') {
                // binary formatted integer of the type 0b01011101

                isBinary = true;

                p++;

                while (p < in_end && (*p >= '0' && *p <= '1'))
                    p++;
            }
        }

        if (!isHex && !isBinary) {
            // floating point exponent
            if (p < in_end && (*p == 'e' || *p == 'E')) {
                isFloat = true;

                p++;

                if (p < in_end && (*p == '+' || *p == '-'))
                    p++;

                bool hasDigits = false;

                while (p < in_end && (*p >= '0' && *p <= '9')) {
                    hasDigits = true;

                    p++;
                }

                if (!hasDigits)
                    return false;
            }

            // single and double precision floating point type
            if (p < in_end && (*p == 'f' || *p == 'F' || *p == 'd' || *p == 'D'))
                p++;
        }

        if (!isFloat) {
            // integer size type
            while (p < in_end && (*p == 'u' || *p == 'U' || *p == 'l' || *p == 'L'))
                p++;
        }

        out_begin = in_begin;
        out_end = p;
        return true;
    }

    bool tokenizeCStyleOperator(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end) {
        (void) in_end;

        switch (*in_begin) {
            case '!':
            case '%':
            case '^':
            case '&':
            case '*':
            case '-':
            case '+':
            case '=':
            case '~':
            case '|':
            case '<':
            case '>':
            case '?':
            case ':':
            case '/':
            case '@':
                out_begin = in_begin;
                out_end = in_begin + 1;
                return true;
            default:
                return false;
        }
    }

    bool tokenizeCStyleSeparator(strConstIter in_begin, strConstIter in_end, strConstIter &out_begin, strConstIter &out_end) {
        (void) in_end;

        switch (*in_begin) {
            case '[':
            case ']':
            case '{':
            case '}':
            case '(':
            case ')':
            case ';':
            case ',':
            case '.':
                out_begin = in_begin;
                out_end = in_begin + 1;
                return true;
            default:
                return false;
        }
    }
}
