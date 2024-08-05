#include <content/text_highlighting/pattern_language.hpp>
#include <pl/core/ast/ast_node_compound_statement.hpp>
#include <pl/core/ast/ast_node_type_decl.hpp>
#include <pl/core/ast/ast_node_variable_decl.hpp>
#include <pl/core/ast/ast_node_lvalue_assignment.hpp>
#include <pl/core/ast/ast_node_struct.hpp>
#include <pl/core/ast/ast_node_union.hpp>
#include <pl/core/ast/ast_node_bitfield.hpp>
#include <pl/core/ast/ast_node_enum.hpp>
#include <pl/core/tokens.hpp>
#include <hex/helpers/utils.hpp>
#include <wolv/utils/string.hpp>
#include <iostream>
#include <content/views/view_pattern_editor.hpp>
#include <pl/pattern_language.hpp>
#include <toasts/toast_notification.hpp>
#include <fmt/chrono.h>
#include <chrono>


namespace hex::plugin::builtin {
    TextHighlighter::TextHighlighter(ViewPatternEditor *viewPatternEditor, std::unique_ptr<pl::PatternLanguage> *patternLanguage )
    : m_viewPatternEditor(viewPatternEditor), patternLanguage(patternLanguage) {
        m_needsToUpdateColors = true;
    }

    i32 TextHighlighter::escapeCharCount(const std::string &str) {
        int count = 0;
        for (auto c: str) {
            if (c == '\"' || c == '\\' || c == '\'' || c == '\0' || c == '\t' ||
                c == '\n' || c == '\r' || c == '\a' || c == '\b' || c == '\f')
                count++;
        }
        return count;
    }

    using namespace pl::core;
    using Identifier = Token::Identifier;
    using Keyword = Token::Keyword;
    using Separator = Token::Separator;
    using Operator = Token::Operator;
    using Comment = Token::Comment;
    using DocComment = Token::DocComment;
    using Literal = Token::Literal;
    using ValueType = Token::ValueType;

    bool TextHighlighter::getIdentifierName(std::string &identifierName, Identifier *&identifier) {
        auto keyword = getValue<Keyword>(0);
        identifier = (Identifier *)getValue<Identifier>(0);

        if (identifier != nullptr) {
            identifierName = identifier->get();
            return true;
        } else if (keyword != nullptr) {
            identifier = nullptr;
            if (peek(tkn::Keyword::Parent)) {
                identifierName = "Parent";
                return true;
            }

            if (peek(tkn::Keyword::This)) {
                identifierName = "This";
                return true;
            }
        }
        identifier = nullptr;
        return false;
    }

// Returns a chain of identifiers like a.b.c or a::b::c
    bool TextHighlighter::getFullName(std::string &identifierName, std::vector<Identifier *> &identifiers, bool preserveCurr) {
        Identifier *identifier = nullptr;

        if (!peek(tkn::Literal::Identifier) || getTokenId(m_curr->location) < 1)
            return getIdentifierName(identifierName, identifier);

        forwardIdentifierName(identifierName, identifiers, preserveCurr);
        return true;
    }

    bool TextHighlighter::forwardIdentifierName(std::string &identifierName, std::vector<Identifier *> &identifiers, bool preserveCurr ) {
        auto curr = m_curr;
        Identifier *identifier = (Identifier *) getValue<Identifier>(0);
        std::string current;

        if (identifier != nullptr) {
            identifiers.push_back(identifier);
            identifierName += identifier->get();
        }  else if (getIdentifierName(current, identifier)) {
            identifiers.push_back(identifier);
            identifierName += current;
        } else {
            m_curr = curr;
            return false;
        }

        skipArray(200, true);

        while ((peek(tkn::Operator::ScopeResolution, 1) || peek(tkn::Separator::Dot, 1))) {
            next();

            if (peek(tkn::Operator::ScopeResolution))
                identifierName += "::";
            else if (peek(tkn::Separator::Dot))
                identifierName += ".";
            else {
                m_curr = curr;
                return false;
            }
            next();

            if (getIdentifierName(current, identifier)) {
                identifiers.push_back(identifier);
                identifierName += current;

               skipArray(200, true);

            } else {
                m_curr = curr;
                return false;
            }
        }
        if (preserveCurr)
            m_curr = curr;
        return true;
    }

// Adds namespace if it exists
    bool TextHighlighter::getQualifiedName(std::string &identifierName, std::vector<Identifier *> &identifiers, bool useDefinitions, bool preserveCurr) {

        std::string shortName;
        std::string qualifiedName;

        if (!getFullName(identifierName, identifiers, preserveCurr))
            return false;

        if (std::ranges::find(m_UDTs, identifierName) != m_UDTs.end())
            return true;
        std::vector<std::string> vectorString;
        if (identifierName.contains("::")) {
            vectorString = hex::splitString(identifierName, "::");
            if (vectorString.size() > 1) {
                shortName = vectorString.back();
                vectorString.pop_back();
                identifierName = hex::combineStrings(vectorString, "::");
            }
        }
        bool found = true;
        for (auto name : vectorString) {
            found = found || std::ranges::find(m_nameSpaces, name) != m_nameSpaces.end();
        }
        if (found) {
            if (!shortName.empty())
                identifierName = identifierName + "::" + shortName;
            return true;
        }

        if (useDefinitions) {
            if (m_functionDefinitions.contains(identifierName) || m_UDTDefinitions.contains(identifierName)) {
                if (!shortName.empty())
                    identifierName = identifierName + "::" + shortName;
                return true;
            }
            std::string nameSpace;
            for (auto [name, definition]: m_UDTDefinitions) {
                findNamespace(nameSpace, definition.tokenIndex);

                if (!nameSpace.empty() && !identifierName.contains(nameSpace)) {
                    qualifiedName = nameSpace + "::" + identifierName;

                    if (name == qualifiedName) {
                        identifierName = qualifiedName;
                        if (!shortName.empty())
                            identifierName = identifierName + "::" + shortName;
                        return true;
                    }
                }

                if (name == identifierName) {
                    identifierName = name;
                    if (!shortName.empty())
                        identifierName = identifierName + "::" + shortName;
                    return true;
                }
            }
        }

        if (identifierName.empty())
            return false;
        return true;
    }

// Finds the token range of a function, namespace or UDT
    bool TextHighlighter::getTokenRange(std::vector<Token> keywords,
                                                           TextHighlighter::UnorderedBlocks &tokenRange,
                                                           TextHighlighter::OrderedBlocks &tokenRangeInv,
                                                           bool fullName, VariableScopes *blocks) {

        bool addArgumentBlock = !fullName;
        std::stack<i32> tokenStack;
        if (getTokenId(m_curr->location) < 1)
            return false;
        std::string name;
        if (fullName) {
            std::vector<Identifier *> identifiers;
            if (!getFullName(name, identifiers))
                return false;
        } else {
            Identifier *identifier = nullptr;
            if (!getIdentifierName(name, identifier))
                return false;
            std::string nameSpace;
            findNamespace(nameSpace, getTokenId(m_curr->location));
            if (!nameSpace.empty())
                name = nameSpace + "::" + name;
        }

        i32 tokenCount = m_tokens->size();
        auto saveCurr = m_curr - 1;
        skipTemplate(200);
        next();
        if (sequence(tkn::Operator::Colon)) {
            while (peek(tkn::Literal::Identifier)) {
                auto identifier = (Identifier *) getValue<Identifier>(0);
                if (identifier == nullptr)
                    break;
                auto identifierName = identifier->get();
                if (std::ranges::find(m_inheritances[name], identifierName) == m_inheritances[name].end())
                    m_inheritances[name].push_back(identifierName);
                skipTemplate(200);
                next(2);
            }
        }

        m_curr = saveCurr;
        i32 index1 = getTokenId(m_curr->location);
        bool result = true;
        for (auto keyword: keywords)
            result = result && !peek(keyword);
        if (result)
            return false;
        u32 nestedLevel = 0;
        next();
        auto limit = m_startToken + tokenCount;
        while (m_curr != limit) {

            if (sequence(tkn::Separator::LeftBrace)) {
                auto tokenId = getTokenId(m_curr[-1].location);
                tokenStack.push(tokenId);
                nestedLevel++;
            } else if (sequence(tkn::Separator::RightBrace)) {
                nestedLevel--;

                if (tokenStack.empty())
                    return false;
                Interval range(tokenStack.top(), getTokenId(m_curr[-1].location));
                tokenStack.pop();

                if (nestedLevel == 0) {
                    range.end -= 1;
                    if (blocks != nullptr)
                        blocks->operator[](name).insert(range);
                    skipAttribute();
                    break;
                }
                if (blocks != nullptr)
                    blocks->operator[](name).insert(range);
            } else if (sequence(tkn::Separator::EndOfProgram))
                return false;
            else
                next();
        }
        i32 index2 = getTokenId(m_curr->location);

        if (index2 > index1 && index2 < tokenCount) {
            if (fullName) {
                tokenRangeInv[Interval(index1, index2)] = name;
            } else {
                tokenRange[name] = Interval(index1, index2);
            }
            if (blocks != nullptr) {
                if (addArgumentBlock) {
                    auto tokenIndex = blocks->operator[](name).begin()->start;
                    blocks->operator[](name).insert(Interval(index1, tokenIndex));
                }
                blocks->operator[](name).insert(Interval(index1, index2));
            }
            return true;
        }
        return false;
    }

// Searches through tokens and loads all the ranges of one kind. First namespaces are searched.
    void TextHighlighter::getAllTokenRanges(IdentifierType IdentifierTypeToSearch) {

        if (m_tokens->empty())
            return;

        std::string name;
        Identifier *identifier;
        IdentifierType identifierType;
        m_startToken = m_originalPosition = m_partOriginalPosition = TokenIter(m_tokens->begin(), m_tokens->end());

        for (m_curr = m_startToken; !peek(tkn::Separator::EndOfProgram); next()) {
            auto curr = m_curr;

            if (peek(tkn::Literal::Identifier)) {
                identifier = (Identifier *) getValue<Identifier>(0);
                identifierType = identifier->getType();
                name = identifier->get();

                if (identifierType == IdentifierTypeToSearch) {
                    switch (identifierType) {
                        case IdentifierType::Function:
                            getTokenRange({tkn::Keyword::Function}, m_functionTokenRange, m_namespaceTokenRange, false, &m_functionBlocks);
                            break;
                        case IdentifierType::NameSpace:
                            if (std::ranges::find(m_nameSpaces, name) == m_nameSpaces.end())
                                m_nameSpaces.push_back(name);
                            getTokenRange({tkn::Keyword::Namespace}, m_functionTokenRange, m_namespaceTokenRange, true, nullptr);
                            break;
                        case IdentifierType::UDT:
                            getTokenRange({tkn::Keyword::Struct, tkn::Keyword::Union, tkn::Keyword::Enum,
                                           tkn::Keyword::Bitfield}, m_UDTTokenRange, m_namespaceTokenRange, false, &m_UDTBlocks);
                            break;
                        case IdentifierType::Attribute:
                            linkAttribute();
                            break;
                        case IdentifierType::Typedef:
                        case IdentifierType::TemplateArgument:
                        case IdentifierType::GlobalVariable:
                        case IdentifierType::PlacedVariable:
                        case IdentifierType::PatternVariable:
                        case IdentifierType::LocalVariable:
                        case IdentifierType::CalculatedPointer:
                        case IdentifierType::View:
                        case IdentifierType::FunctionVariable:
                        case IdentifierType::FunctionParameter:
                        case IdentifierType::Macro:
                        case IdentifierType::Unknown:
                        case IdentifierType::FunctionUnknown:
                        case IdentifierType::MemberUnknown:
                        case IdentifierType::ScopeResolutionUnknown:
                            break;
                    }
                }
            }
            m_curr = curr;
        }
    }

    void TextHighlighter::skipDelimiters(i32 maxSkipCount, Token delimiter[2], i8 increment) {
        auto curr = m_curr;
        i32 skipCount = 0;
        i32 depth = 0;

        if (!isValid())
            return;
        i32 tokenId = getTokenId(m_curr->location);
        auto tokenCount = m_tokens->size();

        if (tokenId == -1 || tokenId >= (i32) tokenCount-1)
            return;
        i32 skipCountLimit =
                increment > 0 ? std::min(maxSkipCount, (i32) tokenCount - 1 - tokenId) : std::min(maxSkipCount, tokenId);
        next(increment);

        if (peek(delimiter[0])) {
            next(increment);
            while (skipCount < skipCountLimit) {

                if (peek(delimiter[1])) {

                    if (depth == 0)
                        return;
                    depth--;
                } else if (peek(delimiter[0]))
                    depth++;
                else if (peek(tkn::Separator::Semicolon)) {
                    if (increment < 0)
                        m_curr = curr;
                    return;
                } else if (peek(tkn::Literal::Identifier)) {

                    if (peek(tkn::Separator::Dot,1) && peek(tkn::Literal::Identifier,2) )

                        m_memberChains.push_back(getTokenId(m_curr->location));
                    else if (peek(tkn::Operator::ScopeResolution,1) && peek(tkn::Literal::Identifier,2))

                        m_scopeChains.push_back(getTokenId(m_curr->location));
                    else
                        m_taggedIdentifiers.push_back(getTokenId(m_curr->location));
                }
                next(increment);
                skipCount++;
            }
        }
        m_curr = curr;
        return;
    }

    void TextHighlighter::skipTemplate(i32 maxSkipCount, bool forward) {
        Token delimiters[2];

        if (forward) {
            delimiters[0] = Token(tkn::Operator::BoolLessThan);
            delimiters[1] = Token(tkn::Operator::BoolGreaterThan);
        } else {
            delimiters[0] = Token(tkn::Operator::BoolGreaterThan);
            delimiters[1] = Token(tkn::Operator::BoolLessThan);
        }
        skipDelimiters(maxSkipCount, delimiters, forward ? 1 : -1);
    }

    void TextHighlighter::skipArray(i32 maxSkipCount, bool forward) {
        Token delimiters[2];

        if (forward) {
            delimiters[0] = Token(tkn::Separator::LeftBracket);
            delimiters[1] = Token(tkn::Separator::RightBracket);
        } else {
            delimiters[0] = Token(tkn::Separator::RightBracket);
            delimiters[1] = Token(tkn::Separator::LeftBracket);
        }
        skipDelimiters(maxSkipCount, delimiters, forward ? 1 : -1);
    }

// Used to skip references,pointers,...
    void TextHighlighter::skipToken(Token token, i8 step) {

        if (peek(token, step))
            next(step);
    }

    void TextHighlighter::skipAttribute() {

        if (sequence(tkn::Separator::LeftBracket, tkn::Separator::LeftBracket)) {
            while (!sequence(tkn::Separator::RightBracket, tkn::Separator::RightBracket))
                next();
        }
    }

// Takes an identifier chain resolves the type of the end from the rest iteratively.
    bool TextHighlighter::resolveIdentifierType(Definition &result, std::string identifierName) {
        std::string separator;

        if (identifierName.contains("::"))
            separator = "::";
        else
            separator = ".";
        auto vectorString = hex::splitString(identifierName, separator);

        std::string nameSpace;
        u32 index = 0;
        std::string currentName = vectorString[index];
        index++;
        std::string variableParentType;

        Definition definition;

        if (vectorString.size() > 1) {

            if (findIdentifierDefinition(definition, currentName)) {
                variableParentType = definition.typeStr;
                auto tokenIndex = getTokenId(m_curr->location);
                setColor(tokenIndex, definition.idType);
                skipArray(200, true);
                next();
            } else
                return false;
        }
        while (index < vectorString.size()) {

            if ( separator == ".") {
                currentName = vectorString[index];
                next();

                if (findIdentifierDefinition(result, currentName, variableParentType)) {
                    variableParentType = result.typeStr;
                    auto tokenIndex = getTokenId(m_curr->location);
                    setColor(tokenIndex, result.idType);
                    skipArray(200, true);
                    next();
                } else
                    return false;

            } else if (separator == "::") {
                next();

                if (std::ranges::find(m_nameSpaces, currentName) != m_nameSpaces.end()) {
                    nameSpace += currentName + "::";

                    variableParentType = vectorString[index];
                    currentName = variableParentType;

                } else if (std::ranges::find(m_UDTs, currentName) != m_UDTs.end()) {
                    variableParentType = currentName;

                    if (!nameSpace.empty() && !variableParentType.contains(nameSpace))
                        variableParentType = nameSpace + variableParentType;

                    else if (findNamespace(nameSpace) && !variableParentType.contains(nameSpace))
                        variableParentType = nameSpace + "::" + variableParentType;

                    currentName = vectorString[index];

                    if (findIdentifierDefinition(result, currentName, variableParentType)) {
                        variableParentType = result.typeStr;
                        auto tokenIndex = getTokenId(m_curr->location);
                        setColor(tokenIndex, result.idType);
                        skipArray(200, true);
                        next();
                    } else
                        return false;
                }
            }
            index++;
        }

        return true;
    }

// If contex then find it otherwise check if it belongs in map
    bool TextHighlighter::findOrContains(std::string &context, UnorderedBlocks tokenRange, VariableMap variableMap) {

        if (context.empty())
            return findScope(context, tokenRange);
        else
            return variableMap.contains(context);
    }

    void TextHighlighter::setBlockInstancesColor(const std::string &name, const Definition &definition, const Interval &block) {

        if (definition.idType == IdentifierType::Unknown)
            return;
        for (auto instance: m_instances[name]) {

            if (block.contains(instance)) {
                if (auto identifier = std::get_if<Identifier>(&(*m_tokens)[instance].value);
                    identifier != nullptr && identifier->getType() == IdentifierType::Unknown)
                    setColor(instance, definition.idType);
            }
        }
    }

    bool TextHighlighter::findIdentifierDefinition( Definition &result, const std::string &optionalIdentifierName, std::string optionalName, bool setInstances) {
        auto curr = m_curr;
        bool isFunction = false;
        auto tokenId = getTokenId(m_curr->location);
        std::vector<Definition> definitions;
        std::string name = optionalName;
        result.idType = IdentifierType::Unknown;
        std::string identifierName = optionalIdentifierName;

        if (optionalIdentifierName == "") {
            std::vector<Identifier *> identifiers;
            getFullName(identifierName, identifiers);
        }
        Interval tokenRange;
        Scopes blocks;
        Scopes::iterator blocksIterBegin, blocksIterEnd;

        if (findOrContains(name, m_UDTTokenRange, m_UDTVariables) && m_UDTVariables[name].contains(identifierName)) {
            definitions = m_UDTVariables[name][identifierName];
            tokenRange = m_UDTTokenRange[name];
            blocksIterBegin = m_UDTBlocks[name].begin();
            blocksIterEnd = m_UDTBlocks[name].end();

        } else if (findOrContains(name, m_functionTokenRange, m_functionVariables) &&
                   m_functionVariables[name].contains(identifierName)) {
            isFunction = true;
            definitions = m_functionVariables[name][identifierName];
            tokenRange = m_functionTokenRange[name];
            blocksIterBegin = m_functionBlocks[name].begin();
            blocksIterEnd = m_functionBlocks[name].end();
            --blocksIterEnd;

        } else if (m_globalVariables.contains(identifierName)) {
            definitions = m_globalVariables[identifierName];
            tokenRange = Interval(0, m_tokens->size());
            blocks.insert(tokenRange);
            blocksIterBegin = blocks.begin();
            blocksIterEnd = blocks.end();
        }

        if (isFunction) {
            for (auto block = blocksIterBegin; block != blocksIterEnd; block++) {

                if (tokenId > block->start && tokenId < block->end) {
                    blocksIterBegin = block;
                    break;
                }
            }
            for (auto definition : definitions) {
                for (auto block = blocksIterBegin; block != blocksIterEnd; block++) {

                    if (definition.tokenIndex > block->start && definition.tokenIndex < block->end) {
                        result = definition;
                        m_curr = curr;

                        if (setInstances)
                            setBlockInstancesColor(identifierName, definition, *block);
                        return true;
                    }
                }
            }
            for (auto definition : definitions) {

                if (definition.tokenIndex > tokenRange.start && definition.tokenIndex < tokenRange.end) {
                    result = definition;
                    m_curr = curr;

                    if (setInstances)
                        setBlockInstancesColor(identifierName, definition, tokenRange);
                    return true;
                }
            }
        } else {
            for (auto block = blocksIterBegin; block != blocksIterEnd; block++) {

                if (tokenId > block->start && tokenId < block->end) {
                    blocksIterBegin = block;
                    break;
                }
            }
            for (auto block = blocksIterBegin; block != blocksIterEnd; block++) {
                for (auto definition: definitions) {

                    if (definition.tokenIndex > block->start && definition.tokenIndex < block->end) {
                        result = definition;
                        m_curr = curr;

                        if (setInstances)
                            setBlockInstancesColor(identifierName, definition, *block);
                        return true;
                    }
                }
            }
        }
        m_curr = curr;
        return false;
    }

    using Definition = TextHighlighter::Definition;

    bool TextHighlighter::colorOperatorDotChain() {
        std::vector<Identifier *> identifiers;
        std::string variableName;

        if (!getQualifiedName(variableName, identifiers, true))
            return false;

        auto vectorString = hex::splitString(variableName, ".");
        u32 index = 0;
        std::string currentName = vectorString[index];
        index++;
        std::string variableParentType;
        Definition definition;

        if (findIdentifierDefinition(definition, currentName)) {
            variableParentType = definition.typeStr;
            auto tokenIndex = getTokenId(m_curr->location);
            setColor(tokenIndex, definition.idType);
            skipArray(200, true);
            next();
        }

        while (index < vectorString.size()) {
            currentName = vectorString[index];
            next();
            Definition result;

            if (findIdentifierDefinition(result, currentName, variableParentType)) {
                variableParentType = result.typeStr;
                auto tokenIndex = getTokenId(m_curr->location);
                setColor(tokenIndex, result.idType);
                skipArray(200, true);
                next();
            } else
                return false;
            index++;
        }
        return true;
    }

    bool TextHighlighter::colorSeparatorScopeChain() {
        std::vector<Identifier *> identifiers;
        std::string variableName;

        if (!getQualifiedName(variableName, identifiers, true))
            return false;
        auto tokenCount = m_tokens->size();
        auto vectorString = hex::splitString(variableName, "::");
        auto vectorStringCount = vectorString.size();
        auto curr = m_curr;
        std::string nameSpace;

        for (u32 i = 0; i < vectorStringCount ; i++) {
            auto name = vectorString[i];
            auto identifier = identifiers[i];

            if (std::ranges::find(m_nameSpaces, name) != m_nameSpaces.end()) {
                setColor(-1, IdentifierType::NameSpace);
                nameSpace += name + "::";

            } else if (std::ranges::find(m_UDTs, nameSpace + name) != m_UDTs.end()) {
                setColor(-1, IdentifierType::UDT);
                next(2);
                setColor(-1,  IdentifierType::LocalVariable);
                return true;

            } else if (identifier->getType() == IdentifierType::Function) {
                setColor(-1, IdentifierType::Function);
                return true;
            }
            next(2);
        }
        m_curr = curr;

        if (std::ranges::find(m_nameSpaces, variableName) != m_nameSpaces.end()) {
            setColor(-1, IdentifierType::NameSpace);
            return true;
        }

        u32 currentLine = m_curr->location.line - 1;
        u32 startingLineTokenIndex = m_firstTokenIdOfLine[currentLine];

        if (startingLineTokenIndex == 0xFFFFFFFFu || startingLineTokenIndex > tokenCount)
            return false;

        if (auto *keyword = std::get_if<Keyword>(&(*m_tokens)[startingLineTokenIndex].value);
                keyword != nullptr && *keyword == Keyword::Import) {
            setColor(-1, IdentifierType::NameSpace);
            return true;
        }

        i32 index = getTokenId(m_curr->location);

        if (index < (i32) tokenCount - 1 && index > 2) {
            auto nextToken = m_curr[1];
            auto *separator = std::get_if<Token::Separator>(&nextToken.value);
            auto *operatortk = std::get_if<Token::Operator>(&nextToken.value);

            if ((separator != nullptr && *separator == Separator::Semicolon) ||
                (operatortk != nullptr && *operatortk == Operator::BoolLessThan)) {
                auto previousToken = m_curr[-1];
                auto prevprevToken = m_curr[-2];
                operatortk = std::get_if<Operator>(&previousToken.value);
                auto *identifier2 = std::get_if<Identifier>(&prevprevToken.value);

                if (operatortk != nullptr && identifier2 != nullptr && *operatortk == Operator::ScopeResolution) {

                    if (identifier2->getType() == IdentifierType::UDT) {
                        setColor(-1, IdentifierType::LocalVariable);
                        return true;

                    } else if (identifier2->getType() == IdentifierType::NameSpace) {
                        setColor(-1, IdentifierType::UDT);
                        return true;
                    }
                }
            }
        }
        return false;
    }

// finds the name of the token range that the given or the current token index is in.
    bool TextHighlighter::findScope(std::string &name, const UnorderedBlocks &map, i32 optionalTokenId) {
        auto tokenId = optionalTokenId ==-1 ? getTokenId(m_curr->location) : optionalTokenId;

        for (auto [scopeName, range]: map) {

            if (range.contains(tokenId)) {
                name = scopeName;
                return true;
            }
        }
        return false;
    }


// Finds the namespace of the given or the current token index.
    bool TextHighlighter::findNamespace(std::string &nameSpace, i32 optionalTokenId) {
        nameSpace = "";

        for (auto [interval, name]: m_namespaceTokenRange) {
            i32 tokenId = optionalTokenId == -1 ? getTokenId(m_curr->location) : optionalTokenId;

            if (tokenId > interval.start && tokenId < interval.end) {

                if (nameSpace == "")
                    nameSpace = name;
                else
                    nameSpace = name + "::" + nameSpace;
            }
        }

        if (nameSpace != "")
            return true;
        return false;
    }

//The context is the name of the function or UDT that the variable is in.
    std::string TextHighlighter::findIdentifierTypeStr(const std::string &identifierName, std::string context) {
        Definition result;
        findIdentifierDefinition(result, identifierName, context);
        return result.typeStr;
    }

// Creates a map from the attribute function to the type of the argument it takes.
    void TextHighlighter::linkAttribute() {
        auto curr = m_curr;
        bool qualifiedAttribute = false;
        using Types = std::map<std::string, pl::hlp::safe_shared_ptr<pl::core::ast::ASTNodeTypeDecl>>;
        auto parser = patternLanguage->get()->getInternals().parser.get();
        Types types = parser->getTypes();

        while (sequence(tkn::Literal::Identifier, tkn::Operator::ScopeResolution))
            qualifiedAttribute = true;

        if (qualifiedAttribute) {
            auto identifier = (Identifier *) getValue<Identifier>(0);

            if (identifier != nullptr)
                setColor(-1, IdentifierType::Attribute);
            m_curr = curr;
            identifier = (Identifier *) getValue<Identifier>(0);

            if (identifier != nullptr)
                setColor(-1, IdentifierType::NameSpace);
        } else
            m_curr = curr;

        std::string functionName;
        next();

        if (sequence(tkn::Separator::LeftParenthesis, tkn::Literal::String)) {
            functionName = getValue<Literal>(-1)->toString(false);

            if (!functionName.contains("::")) {
                std::string namespaceName;

                if (findNamespace(namespaceName))
                    functionName = namespaceName + "::" + functionName;
            } else {

                auto vectorString = hex::splitString(functionName, "::");
                vectorString.pop_back();
                for (auto nameSpace: vectorString) {

                    if (std::ranges::find(m_nameSpaces, nameSpace) == m_nameSpaces.end())
                        m_nameSpaces.push_back(nameSpace);
                }
            }
        } else
            return;

        u32 line = m_curr->location.line;
        i32 tokenIndex;

        while (!peek(tkn::Separator::Semicolon, -1)) {

            if (line = previousLine(line); line > m_firstTokenIdOfLine.size())
                return;

            if (tokenIndex = m_firstTokenIdOfLine[line]; !isTokenIdValid(tokenIndex))
                return;

            m_curr = m_startToken;
            next(tokenIndex);
            while (peek(tkn::Literal::Comment, -1) || peek(tkn::Literal::DocComment, -1))
                next(-1);
        }

        while (peek(tkn::Literal::Comment) || peek(tkn::Literal::DocComment))
            next();

        Identifier *identifier;
        std::string UDTName;
        while (sequence(tkn::Literal::Identifier, tkn::Operator::ScopeResolution)) {
            identifier = (Identifier *) getValue<Identifier>(-2);
            UDTName += identifier->get() + "::";
        }

        if (sequence(tkn::Literal::Identifier)) {
            identifier = (Identifier *) getValue<Identifier>(-1);
            UDTName += identifier->get();

            if (!UDTName.contains("::")) {
                std::string namespaceName;

                if (findNamespace(namespaceName))
                    UDTName = namespaceName + "::" + UDTName;
            }
            if (types.contains(UDTName))
                m_attributeFunctionArgumentType[functionName] = UDTName;

        } else if (sequence(tkn::ValueType::Any)) {
            auto valueType = getValue<ValueType>(-1);
            m_attributeFunctionArgumentType[functionName] = Token::getTypeName(*valueType);
        } else {
            if (findScope(UDTName, m_UDTTokenRange) && !UDTName.empty())
                m_attributeFunctionArgumentType[functionName] = UDTName;
        }
    }

// This function assumes that the first variable in the link that concatenates sequences including the Parent keyword started with Parent and was removed. Uses a function to find
// all the parents of a variable, If there are subsequent elements in the link that are Parent then for each parent it finds all the grandparents and puts them in a vector called
// parentTypes. It stops when a link that's not Parent is found amd only returns the last generation of parents.
    bool TextHighlighter::findAllParentTypes(std::vector<std::string> &parentTypes,
                                             std::vector<Identifier *> &identifiers, std::string &optionalFullName) {
        auto fullName = optionalFullName;

        if (optionalFullName.empty())
            forwardIdentifierName(fullName, identifiers);

        auto nameParts = hex::splitString(fullName, ".");
        std::vector<std::string> grandpaTypes;
        findParentTypes(parentTypes);

        if (parentTypes.empty())
            return false;

        auto currentName = nameParts[0];
        nameParts.erase(nameParts.begin());
        auto identifier = identifiers[0];
        identifiers.erase(identifiers.begin());

        while (currentName == "Parent" && !nameParts.empty()) {
            for (auto parentType: parentTypes)
                findParentTypes(grandpaTypes, parentType);

            currentName = nameParts[0];
            nameParts.erase(nameParts.begin());
            identifier = identifiers[0];
            identifiers.erase(identifiers.begin());
            parentTypes = grandpaTypes;
            grandpaTypes.clear();
        }

        nameParts.insert(nameParts.begin(), currentName);
        identifiers.insert(identifiers.begin(), identifier);
        optionalFullName = hex::combineStrings(nameParts, ".");
        return true;
    }

// Searches for parents through every custom type,i.e. for structs that have members
// of the same type as the one being searched and places them in a vector called parentTypes.
    bool TextHighlighter::findParentTypes(std::vector<std::string> &parentTypes, const std::string &optionalUDTName) {
        std::string UDTName;

        if (optionalUDTName.empty()) {
            if (!findScope(UDTName, m_UDTTokenRange))
                return false;
        } else
            UDTName = optionalUDTName;

        bool found = false;
        for (auto [name, variables]: m_UDTVariables) {
            for (auto [variableName, definitions]: variables) {
                for (auto definition: definitions) {

                    if (definition.typeStr == UDTName) {

                        if (std::ranges::find(parentTypes, name) == parentTypes.end()) {
                            parentTypes.push_back(name);
                            found = true;
                        }
                    }
                }
            }
        }
        return found;
    }

// this function searches all the parents recursively until it can match the variable name at the end of the chain
// and selects its type to colour the variable because the search only occurs pn type declarations which we know
// the types of. Once the end link is found then all the previous links are also assigned the types that were found
// for them during the search.
    bool TextHighlighter::tryParentType(const std::string &parentType, std::string &variableName,
                                                           std::optional<Definition> &result,
                                                           std::vector<Identifier *> &identifiers) {

        auto vectorString = hex::splitString(variableName, ".");
        auto count = vectorString.size();
        auto UDTName = parentType;
        auto currentName = vectorString[0];

        if (m_UDTVariables.contains(UDTName) && m_UDTVariables[UDTName].contains(currentName)) {
            auto definitions = m_UDTVariables[UDTName][currentName];
            for (auto definition: definitions) {
                UDTName = definition.typeStr;

                if (count == 1) {
                    setColor(-1, definition.idType);
                    result = definition;
                    return true;
                }

                vectorString.erase(vectorString.begin());
                variableName = hex::combineStrings(vectorString, ".");
                Identifier *identifier = identifiers[0];
                identifiers.erase(identifiers.begin());
                skipArray(200, true);
                next(2);

                if (tryParentType(UDTName, variableName, result, identifiers)) {
                    next(-1);
                    skipArray(200, false);
                    next(-1);
                    setColor(-1, definition.idType);
                    return true;
                }

                identifiers.insert(identifiers.begin(), identifier);
                variableName += "." + currentName;
                next(-1);
                skipArray(200, false);
                next(-1);
                return false;
            }
            return false;
        } else
            return false;
        return false;
    }

// Handles Parent keyword.
    std::optional<Definition> TextHighlighter::setChildrenTypes() {
        auto curr = m_curr;
        std::string fullName;
        std::vector<Identifier *> identifiers;
        std::vector<Definition> definitions;
        std::optional<Definition> result;

        forwardIdentifierName(fullName, identifiers);

        std::vector<std::string> parentTypes;
        auto vectorString = hex::splitString(fullName, ".");
        if (vectorString[0] == "Parent") {
            vectorString.erase(vectorString.begin());
            fullName = hex::combineStrings(vectorString, ".");
            identifiers.erase(identifiers.begin());
            if (!findAllParentTypes(parentTypes, identifiers, fullName)) {
                m_curr = curr;
                return std::nullopt;
            }
        } else {
            m_curr = curr;
            return std::nullopt;
        }

        for (auto parentType: parentTypes) {
            m_curr = curr;
            while (peek(tkn::Keyword::Parent))
                next(2);

            if (tryParentType(parentType, fullName, result, identifiers)) {

                if (result.has_value())
                    definitions.push_back(result.value());
            } else {
                m_curr = curr;
                return std::nullopt;
            }
        }
        // Todo: Are all definitions supposed to be the same? If not, which one should be used?
        // for now, use the first one.
        if (!definitions.empty())
            result = definitions[0];
        m_curr = curr;
        return result;
    }

    const TextHighlighter::TokenTypeColor TextHighlighter::m_tokenTypeColor = {
            {Token::Type::Keyword,   TextEditor::PaletteIndex::Keyword},
            {Token::Type::ValueType, TextEditor::PaletteIndex::BuiltInType},
            {Token::Type::Operator,  TextEditor::PaletteIndex::Operator},
            {Token::Type::Separator, TextEditor::PaletteIndex::Separator},
            {Token::Type::String,    TextEditor::PaletteIndex::StringLiteral},
            {Token::Type::Directive, TextEditor::PaletteIndex::Directive},
    };

    const TextHighlighter::IdentifierTypeColor TextHighlighter::m_identifierTypeColor = {
            {Identifier::IdentifierType::Macro,                  TextEditor::PaletteIndex::PreprocIdentifier},
            {Identifier::IdentifierType::UDT,                    TextEditor::PaletteIndex::UserDefinedType},
            {Identifier::IdentifierType::Function,               TextEditor::PaletteIndex::Function},
            {Identifier::IdentifierType::Attribute,              TextEditor::PaletteIndex::Attribute},
            {Identifier::IdentifierType::NameSpace,              TextEditor::PaletteIndex::NameSpace},
            {Identifier::IdentifierType::Typedef,                TextEditor::PaletteIndex::TypeDef},
            {Identifier::IdentifierType::PatternVariable,        TextEditor::PaletteIndex::PatternVariable},
            {Identifier::IdentifierType::LocalVariable,          TextEditor::PaletteIndex::LocalVariable},
            {Identifier::IdentifierType::CalculatedPointer,      TextEditor::PaletteIndex::CalculatedPointer},
            {Identifier::IdentifierType::TemplateArgument,       TextEditor::PaletteIndex::TemplateArgument},
            {Identifier::IdentifierType::PlacedVariable,         TextEditor::PaletteIndex::PlacedVariable},
            {Identifier::IdentifierType::View,                   TextEditor::PaletteIndex::View},
            {Identifier::IdentifierType::FunctionVariable,       TextEditor::PaletteIndex::FunctionVariable},
            {Identifier::IdentifierType::FunctionParameter,      TextEditor::PaletteIndex::FunctionParameter},
            {Identifier::IdentifierType::Unknown,                TextEditor::PaletteIndex::UnkIdentifier},
            {Identifier::IdentifierType::FunctionUnknown,        TextEditor::PaletteIndex::UnkIdentifier},
            {Identifier::IdentifierType::MemberUnknown,          TextEditor::PaletteIndex::UnkIdentifier},
            {Identifier::IdentifierType::ScopeResolutionUnknown, TextEditor::PaletteIndex::UnkIdentifier},
            {Identifier::IdentifierType::GlobalVariable,         TextEditor::PaletteIndex::GlobalVariable},
    };

// Second paletteIndex called from processLineTokens to process literals
    TextEditor::PaletteIndex TextHighlighter::getPaletteIndex(Literal *literal) {

        if (literal->isFloatingPoint() || literal->isSigned() || literal->isUnsigned())
            return TextEditor::PaletteIndex::NumericLiteral;

        else if (literal->isCharacter() || literal->isBoolean()) return TextEditor::PaletteIndex::CharLiteral;

        else if (literal->isString()) return TextEditor::PaletteIndex::StringLiteral;

        else return TextEditor::PaletteIndex::UnkIdentifier;
    }

// Render the compilation errors using squiggly lines
    void TextHighlighter::renderErrors() {
        const auto processMessage = [](const auto &message) {
            auto lines = hex::splitString(message, "\n");

            std::ranges::transform(lines, lines.begin(), [](auto line) {

                if (line.size() >= 128)
                    line = wolv::util::trim(line);

                return hex::limitStringLength(line, 128);
            });

            return hex::combineStrings(lines, "\n");
        };
        TextEditor::ErrorMarkers errorMarkers;

        if (!m_compileErrors.empty()) {
            for (const auto &error: m_compileErrors) {

                if (isLocationValid(error.getLocation())) {
                    auto key = TextEditor::Coordinates(error.getLocation().line, error.getLocation().column);

                    if (!errorMarkers.contains(key) || errorMarkers[key].first < error.getLocation().length)
                        errorMarkers[key] = std::make_pair(error.getLocation().length, processMessage(error.getMessage()));
                }
            }
        }
        m_viewPatternEditor->getTextEditor().SetErrorMarkers(errorMarkers);
    }

// creates a map from variable names to a vector of token indices
// od every instance of the variable name in the code.
    void TextHighlighter::setInitialColors() {

        m_startToken = m_originalPosition = m_partOriginalPosition = TokenIter(m_tokens->begin(), m_tokens->end());
        for (m_curr = m_startToken; !peek(tkn::Separator::EndOfProgram); next()) {

            if (peek(tkn::Literal::Identifier)) {

                if (auto identifier = (Identifier *) getValue<Identifier>(0); identifier != nullptr) {

                    if (auto identifierType = identifier->getType(); identifierType != IdentifierType::Unknown &&
                                                                     identifierType != IdentifierType::MemberUnknown &&
                                                                     identifierType != IdentifierType::FunctionUnknown &&
                                                                     identifierType != IdentifierType::ScopeResolutionUnknown) {

                        setColor(-1, identifierType);
                    }
                }
            } else
                setColor();
        }
    }

    void TextHighlighter::loadInstances() {
        std::map<std::string, std::vector<i32>> instances;
        for (m_curr = m_startToken; !peek(tkn::Separator::EndOfProgram); next()) {

            if (peek(tkn::Literal::Identifier)) {
                std::string name;

                if (auto identifier = (Identifier *) getValue<Identifier>(0); identifier != nullptr) {

                    if (auto identifierType = identifier->getType(); identifierType != IdentifierType::Unknown &&
                                                                     identifierType != IdentifierType::MemberUnknown &&
                                                                     identifierType != IdentifierType::FunctionUnknown &&
                                                                     identifierType != IdentifierType::ScopeResolutionUnknown) {
                        name = identifier->get();

                        if (identifierType == IdentifierType::Typedef)
                            m_typeDefs.push_back(name);
                    } else {
                        name = identifier->get();
                        auto curr = m_curr;
                        auto tokenIndex = getTokenId(m_curr->location);
                        skipArray(200,true);
                        next();
                        bool chainStarted = false;
                        while (sequence(tkn::Operator::ScopeResolution, tkn::Literal::Identifier)) {

                            if (identifier = (Identifier *) getValue<Identifier>(-1); identifier != nullptr)
                                name += "::"+identifier->get();

                            if (!chainStarted) {
                                chainStarted = true;
                                m_scopeChains.push_back(tokenIndex);
                            }
                            curr = m_curr;
                        }
                        while (sequence(tkn::Separator::Dot, tkn::Literal::Identifier)) {

                            if (identifier = (Identifier *) getValue<Identifier>(-1); identifier != nullptr)
                                name += "."+identifier->get();

                            if (!chainStarted) {
                                chainStarted = true;
                                m_memberChains.push_back(tokenIndex);
                            }
                            skipArray(200,true);
                            curr = m_curr;
                        }
                        m_curr = curr;
                    }
                }
                auto id = getTokenId(m_curr->location);

                if (instances.contains(name)) {
                    auto &nameInstances = instances[name];

                    if (std::ranges::find(nameInstances, id) == nameInstances.end())
                        nameInstances.push_back(id);
                } else
                    instances[name].push_back(id);
            }
        }
        m_instances = std::move(instances);
    }

// Get the location of a given token index
    pl::core::Location TextHighlighter::getLocation(i32 tokenId) {
        pl::core::Location location;

        if (tokenId >= (i32) m_tokens->size())
            return location;
        return (*m_tokens)[tokenId].location;
    }

// Get the token index for a given location.
    i32 TextHighlighter::getTokenId(pl::core::Location location) {

        if (!isLocationValid(location))
            return -1;
        auto line1 = location.line - 1;
        auto line2 = nextLine(line1);
        auto tokenCount = m_tokens->size();
        i32 tokenStart = m_firstTokenIdOfLine[line1];
        i32 tokenEnd = m_firstTokenIdOfLine[line2] - 1;

        if (tokenEnd >= (i32) tokenCount)
            tokenEnd = tokenCount - 1;

        if (tokenStart == -1 || tokenEnd == -1 || tokenStart >= (i32) tokenCount)
            return -1;

        for (i32 i = tokenStart; i <= tokenEnd; i++) {

            if ((*m_tokens)[i].location.column >= location.column)
                return i;
        }
        return -1;
    }

    TextEditor::Coordinates TextHighlighter::stringCoordinates() {
        Token *token = const_cast<Token *>(&m_curr[0]);
        TextEditor::Coordinates tokenCoords, result;
        tokenCoords.mLine = token->location.line - 1;
        tokenCoords.mColumn = token->location.column - 1;
        result.mLine = tokenCoords.mLine;
        result.mColumn = tokenCoords.mColumn + token->location.length - 1;

        u32 escapeCount = 0;
        Literal literal = std::get<Literal>(m_curr->value);
        std::string str = literal.toString();
        escapeCount = escapeCharCount(str);
        result.mColumn += escapeCount;
        return result;
    }

    template<typename T>
    TextEditor::Coordinates TextHighlighter::commentCoordinates(Token *token) {
        TextEditor::Coordinates tokenCoords, result;
        tokenCoords.mLine = token->location.line - 1;
        tokenCoords.mColumn = token->location.column - 1;
        result.mLine = tokenCoords.mLine;
        result.mColumn = tokenCoords.mColumn + token->location.length - 1;
        auto tokenValue = std::get_if<T>(&token->value);

        if (tokenValue != nullptr && !tokenValue->singleLine) {
            auto comment = tokenValue->comment;
            auto commentLines = hex::splitString(comment, "\n");
            auto commentLineCount = commentLines.size();

            if (commentLineCount > 1) {
                result.mLine += commentLines.size() - 1;
                result.mColumn = commentLines.back().size() + 1;
            }
        }
        return result;
    }


    void TextHighlighter::setColor(i32 tokenId, const IdentifierType &type) {
        Token *token;

        if (tokenId == -1)
            token = const_cast<Token *>(&m_curr[0]);
        else
            token = const_cast<Token *>(&(*m_tokens)[tokenId]);

        if (token->type == Token::Type::Integer) {
            auto literal = (Literal *) getValue<Literal>(0);

            if (literal != nullptr && !m_tokenColors.contains(token))
                m_tokenColors[token] = getPaletteIndex(literal);

        } else if (token->type == Token::Type::DocComment) {
            auto docComment = (DocComment *) getValue<DocComment>(0);

            if (docComment != nullptr && !m_tokenColors.contains(token)) {

                if (docComment->singleLine)
                    m_tokenColors[token] = TextEditor::PaletteIndex::DocComment;
                else if (docComment->global)
                    m_tokenColors[token] = TextEditor::PaletteIndex::DocGlobalComment;
                else
                    m_tokenColors[token] = TextEditor::PaletteIndex::DocBlockComment;
            }
        } else if (token->type == Token::Type::Comment) {
            auto comment = (Comment *) getValue<Comment>(0);

            if (comment != nullptr && !m_tokenColors.contains(token)) {

                if (comment->singleLine)
                    m_tokenColors[token] = TextEditor::PaletteIndex::Comment;
                else
                    m_tokenColors[token] = TextEditor::PaletteIndex::BlockComment;
            }
        } else if (token->type == Token::Type::Identifier && (!m_tokenColors.contains(token) || m_tokenColors.at(token) == TextEditor::PaletteIndex::UnkIdentifier))
            m_tokenColors[token]  = m_identifierTypeColor.at(type);
        else if (!m_tokenColors.contains(token))
            m_tokenColors[token] = m_tokenTypeColor.at(token->type);
    }

    void TextHighlighter::SetColorsSize( TextEditor::LinesOfColors &linesOfColors) {
        linesOfColors.resize(m_lines.size()-1);
        auto i = 0u;
        for (auto &line: m_lines) {

            if (!line.empty() && i < linesOfColors.size())
                linesOfColors[i].resize(line.size());
            i++;
        }
     }


    void TextHighlighter::colorRemainingTokens() {
        i32 excludedCount = m_excludedLocations.size();
        std::vector<Interval> excludedLines(excludedCount/2);

        if (excludedCount > 1) {
            i32 current = 0;
            while (current < excludedCount - 1) {
                while (!m_excludedLocations[current].isExcluded &&
                       m_excludedLocations[current + 1].isExcluded &&
                       current < excludedCount - 1)
                    current++;

                if (current >= excludedCount - 1)
                    break;
                excludedLines[current/2].start = m_excludedLocations[current].location.line ;
                excludedLines[current/2].end = m_excludedLocations[current + 1].location.line - 1;
                current += 2;
            }
        }
        excludedCount = excludedLines.size();
        auto temp = 0;
        i32 excludedLineIndex = 0;
        for (m_curr = m_startToken; !peek(tkn::Separator::EndOfProgram); next()) {
            i32 line = m_curr->location.line;

            if (excludedLineIndex >= 0 && excludedLineIndex < excludedCount && (line == excludedLines[excludedLineIndex].start)) {
                while (line <= excludedLines[excludedLineIndex].end) {
                    Token *token = const_cast<Token *>(&m_curr[0]);
                    m_tokenColors[token]= TextEditor::PaletteIndex::PreprocessorDeactivated;
                    next();
                    line = m_curr->location.line;
                }
                excludedLineIndex++;
            }
            auto tokenId = getTokenId(m_curr->location);

            if (!m_taggedIdentifiers.empty() && tokenId > m_taggedIdentifiers.back()) {
                next(m_taggedIdentifiers.back()-tokenId);
                m_taggedIdentifiers.pop_back();
            }
            Token *token = const_cast<Token *>(&m_curr[0]);

            if (peek(tkn::Literal::Identifier)) {
                auto curr = m_curr;

                if (getTokenId(m_curr->location) > 2) {
                    next(-3);

                    if (sequence(tkn::Keyword::Import, tkn::Literal::Identifier, tkn::Separator::Dot)) {
                        next(-2);

                        if (auto identifier = const_cast<Identifier *>(getValue<Token::Identifier>(0)); identifier != nullptr) {
                            auto identifierName = identifier->get();

                            if (identifierName == "std" || identifierName == "hex" || identifierName == "type") {
                                setColor(-1, IdentifierType::NameSpace);
                                next(2);
                                setColor(-1, IdentifierType::NameSpace);
                            } else
                                m_curr = curr;
                        } else
                            m_curr = curr;
                    } else
                        m_curr = curr;
                }
                skipArray(200,true);
                m_curr = curr;
                auto identifier = const_cast<Identifier *>( getValue<Token::Identifier>(0));

                if (identifier == nullptr)
                    continue;
                auto identifierType = identifier->getType();
                auto variableName = identifier->get();

                if (m_tokenColors.contains(token) && m_tokenColors.at(token) != TextEditor::PaletteIndex::UnkIdentifier)// && !dontSkip)
                    continue;
                temp++;
                Definition definition;

                if (findIdentifierDefinition(definition)) {
                    identifierType = definition.idType;
                    setColor(-1, identifierType);
                    continue;
                }  else if (std::ranges::find(m_UDTs, variableName) != m_UDTs.end()) {
                    if (std::ranges::find(m_typeDefs, variableName) != m_typeDefs.end())
                        setColor(-1, IdentifierType::Typedef);
                    else
                        setColor(-1, IdentifierType::UDT);
                    continue;
                } else if (peek(tkn::Keyword::Parent, -2)) {
                    auto save = m_curr;
                    next(-2);
                    while (peek(tkn::Keyword::Parent, -2))
                        next(-2);
                    auto optional = setChildrenTypes();
                    setColor(-1, optional->idType);
                    m_curr = save;
                    continue;
                } else {
                    setColor(-1, IdentifierType::Unknown);
                    continue;
                }
            }
        }
    }

    void TextHighlighter::setAllColors() {
        TextEditor::LinesOfColors linesOfColors;
        SetColorsSize(linesOfColors);
        TextEditor::Coordinates fileEnd;
        fileEnd.mLine = m_tokens->back().location.line - 1;
        fileEnd.mColumn = m_tokens->back().location.column - 1;

        for (m_curr = m_startToken; !peek(tkn::Separator::EndOfProgram); next()) {
            Token *token = const_cast<Token *>(&m_curr[0]);
            auto type = token->type;
            TextEditor::Coordinates initial, final;
            initial.mLine = token->location.line - 1;
            initial.mColumn = token->location.column - 1;
            final.mLine = initial.mLine;
            final.mColumn = initial.mColumn + token->location.length - 1;
            TextEditor::PaletteIndex paletteIndex = m_tokenColors[token];

            if (type == Token::Type::DocComment)
                final = commentCoordinates<DocComment>(token);
            else if (type == Token::Type::Comment)
                final = commentCoordinates<Comment>(token);
            else if (type == Token::Type::String) {
                final = stringCoordinates();
            }

            if (final.mColumn > (i32) linesOfColors[final.mLine].size())
                final.mColumn = linesOfColors[final.mLine].size() - 1;

            if (initial <= final && final < fileEnd) {
                for (auto lineNumber = initial.mLine; lineNumber <= final.mLine; lineNumber++) {
                    auto &line = linesOfColors[lineNumber];

                    if (line.empty())
                        continue;
                    i64 ending = (lineNumber == final.mLine) ? final.mColumn : line.size() - 1;
                    for (auto i = initial.mColumn; i <= ending; i++)
                        line[i] = paletteIndex;
                }
            }
        }
        m_viewPatternEditor->getTextEditor().getColorsStaging()->stage(linesOfColors);
    }

    void TextHighlighter::appendInheritances() {
        for (auto [name, inheritances]: m_inheritances) {
            for (auto inheritance: inheritances) {
                auto definitions = m_UDTVariables[inheritance];
                for (auto [variableName, variableDefinitions]: definitions) {
                    auto tokenRange = m_UDTTokenRange[name];
                    u32 tokenIndex;
                    for (auto token = tokenRange.start; token < tokenRange.end; token++) {

                       if (auto operatorTkn = std::get_if<Operator>(&m_tokens->at(token).value); operatorTkn != nullptr && *operatorTkn == Token::Operator::Colon)
                           tokenIndex = token + 1;
                    }
                    for (auto variableDefinition: variableDefinitions) {
                        variableDefinition.tokenIndex = tokenIndex;
                        m_UDTVariables[name][variableName].push_back(variableDefinition);
                    }
                }
            }
        }
    }

// Get the string of the argument type. This works on function arguments and non-type template arguments.
    std::string TextHighlighter::getArgumentTypeName(i32 rangeStart, Token delimiter2) {
        auto curr = m_curr;
        i32 parameterIndex = getArgumentNumber(rangeStart, getTokenId(m_curr->location));
        Token delimiter;
        std::string typeStr;

        if (parameterIndex > 0)
            delimiter = tkn::Separator::Comma;
        else
            delimiter = delimiter2;

        while (!peek(delimiter))
            next(-1);
        skipToken(tkn::Keyword::Reference);
        next();

        if (peek(tkn::ValueType::Any))
            typeStr = Token::getTypeName(*getValue<Token::ValueType>(0));
        else if (peek(tkn::Literal::Identifier))
            typeStr = getValue<Token::Identifier>(0)->get();

        m_curr = curr;
        return typeStr;
    }

    bool TextHighlighter::isTokenIdValid(i32 tokenId) {
        return tokenId >= 0 && tokenId < (i32) m_tokens->size();
    }

    bool TextHighlighter::isLocationValid(hex::plugin::builtin::TextHighlighter::Location location) {
        const pl::api::Source *source;
        try {
            source = location.source;
        }
        catch (const std::exception &e) {
            return false;
        }
        std::string code;
        try {
            code = source->source;
        }
        catch (const std::exception &e) {
            return false;
        }

        if (code != "<Source Code>")
            return false;
        i32 line = location.line - 1;
        i32 col = location.column - 1;
        i32 length = location.length;

        if ( line < 0 || line >= (i32) m_lines.size())
            return false;

        if ( col < 0 || col > (i32) m_lines[line].size())
            return false;

        if (length < 0 || length > (i32) m_lines[line].size()-col)
            return false;
        return true;
    }

// Find the string of the variable type. This works on function variables, views,
// local variables as well as on calculated pointers and pattern variables.
    std::string TextHighlighter::getVariableTypeName() {
        auto curr = m_curr;
        auto varTokenId = getTokenId(m_curr->location);

        if (!isTokenIdValid(varTokenId))
            return "";

        std::string typeStr;
        skipToken(tkn::Operator::Star, -1);

        while (peek(tkn::Separator::Comma, -1))
            next(-2);

        if (peek(tkn::ValueType::Any, -1))
            typeStr = Token::getTypeName(*getValue<Token::ValueType>(-1));
        else {
            skipTemplate(200, false);
            next(-1);

            if (peek(tkn::Literal::Identifier)) {
                typeStr = getValue<Token::Identifier>(0)->get();
                next(-1);
            }
            std::string nameSpace;
            while (peek(tkn::Operator::ScopeResolution)) {
                next(-1);
                nameSpace = getValue<Token::Identifier>(0)->get() + "::" + nameSpace;
                next(-1);
            }
            typeStr = nameSpace + typeStr;
            using Types = std::map<std::string, pl::hlp::safe_shared_ptr<pl::core::ast::ASTNodeTypeDecl>>;
            auto parser = patternLanguage->get()->getInternals().parser.get();
            Types types = parser->getTypes();

            if (types.contains(typeStr)) {
                m_curr = curr;
                return typeStr;
            }
            std::vector<std::string> candidates;
            for (auto name: m_UDTs) {
                auto vectorString = hex::splitString(name, "::");

                if (typeStr == vectorString.back())
                    candidates.push_back(name);
            }

            if (candidates.size() == 1) {
                m_curr = curr;
                return candidates[0];
            }
        }
        m_curr = curr;
        return typeStr;
    }

// Definitions of global variables and placed variables.
    void TextHighlighter::loadGlobalDefinitions( Scopes tokenRangeSet, std::vector<IdentifierType> identifierTypes, Variables &variables) {
        for (auto range: tokenRangeSet) {

            auto limit = m_startToken + range.end;
            for ( m_curr = m_startToken + range.start; m_curr != limit; next()) {

                if (peek(tkn::Literal::Identifier)) {
                    auto identifier = (Identifier *) getValue<Token::Identifier>(0);
                    auto identifierType = identifier->getType();
                    auto identifierName = identifier->get();

                    if (std::ranges::find(identifierTypes, identifierType) != identifierTypes.end()) {
                        std::string typeStr = getVariableTypeName();

                        if (typeStr.empty())
                            continue;
                        i32 tokenId = getTokenId(m_curr->location);
                        Definition definition(identifierType, typeStr, tokenId, m_curr->location);
                        variables[identifierName].push_back(definition);
                        continue;
                    }
                }
            }
        }
    }

// Definitions of variables and arguments in functions and user defined types.
    void TextHighlighter::loadVariableDefinitions(UnorderedBlocks tokenRangeMap, Token delimiter1, Token delimiter2,
                                                                     std::vector<IdentifierType> identifierTypes,
                                                                     bool isArgument, VariableMap &variableMap) {
        for (auto [name, range]: tokenRangeMap) {
            m_curr = m_startToken;
            auto limit = m_startToken;
            next(range.start);

            if (isArgument) {
                while (!peek(delimiter1)) {

                    if (peek(tkn::Separator::LeftBrace))
                        break;
                    next();
                }

                if (peek(tkn::Separator::LeftBrace))
                    continue;
                limit = m_curr;
                while (!peek(delimiter2)) {

                    if (peek(tkn::Separator::LeftBrace))
                        break;
                    next();
                }

                if (peek(tkn::Separator::LeftBrace))
                    continue;
                auto temp = m_curr;
                m_curr = limit;
                limit = temp;
            } else
                limit = limit + range.end;

            Keyword *keyword;
            for (keyword = std::get_if<Keyword>(&(*m_tokens)[range.start].value); m_curr != limit; next()) {

                if (peek(tkn::Literal::Identifier)) {
                    auto identifier = (Identifier *) getValue<Token::Identifier>(0);

                    if (identifier == nullptr)
                        continue;
                    auto identifierType = identifier->getType();
                    auto identifierName = identifier->get();

                    if (std::ranges::find(identifierTypes, identifierType) != identifierTypes.end()) {
                        std::string typeStr;

                        if (keyword != nullptr && (*keyword == Keyword::Enum || *keyword == Keyword::Bitfield)) {
                            typeStr = name;
                        } else if (isArgument) {
                            typeStr = getArgumentTypeName(range.start, delimiter1);
                        } else {
                            typeStr = getVariableTypeName();
                        }

                        if (typeStr.empty())
                            continue;
                        Definition definition(identifierType, typeStr, getTokenId(m_curr->location), m_curr->location);
                        variableMap[name][identifierName].push_back(definition);
                        continue;
                    }
                }
            }
        }
    }

// Definitions of user defined types and functions.
    void TextHighlighter::loadTypeDefinitions(
            UnorderedBlocks tokenRangeMap, std::vector<IdentifierType> identifierTypes, Definitions &types) {
        for (auto [name, range]: tokenRangeMap) {
            m_curr = m_startToken + range.start;
            auto limit = m_startToken + range.end;
            while (m_curr != limit) {

                if (peek(tkn::Literal::Identifier)) {

                    if (auto identifier = (Identifier *) getValue<Token::Identifier>(0); identifier != nullptr) {
                        auto identifierType = identifier->getType();

                        if (std::ranges::find(identifierTypes, identifierType) != identifierTypes.end())
                            types[name] = ParentDefinition(identifierType, getTokenId(m_curr->location),m_curr->location);
                    }
                }
                next();
            }
        }
    }

// Once types are loaded from parsed tokens we can create
// maps of variable names to their definitions.
    void TextHighlighter::getDefinitions() {
        using IdentifierType::LocalVariable;
        using IdentifierType::PatternVariable;
        using IdentifierType::CalculatedPointer;
        using IdentifierType::GlobalVariable;
        using IdentifierType::PlacedVariable;
        using IdentifierType::View;
        using IdentifierType::FunctionVariable;
        using IdentifierType::FunctionParameter;
        using IdentifierType::TemplateArgument;
        using IdentifierType::UDT;
        using IdentifierType::Function;

        if (!m_UDTDefinitions.empty())
            m_UDTDefinitions.clear();
        loadTypeDefinitions(m_UDTTokenRange, {UDT}, m_UDTDefinitions);

        if (!m_globalVariables.empty())
            m_globalVariables.clear();
        loadGlobalDefinitions(m_globalTokenRange, {GlobalVariable, PlacedVariable}, m_globalVariables);

        if (!m_UDTVariables.empty())
            m_UDTVariables.clear();
        loadVariableDefinitions(m_UDTTokenRange, tkn::Operator::BoolLessThan, tkn::Operator::BoolGreaterThan,
                                {TemplateArgument}, true, m_UDTVariables);

        loadVariableDefinitions(m_UDTTokenRange, tkn::Operator::BoolLessThan, tkn::Operator::BoolGreaterThan,
                                {LocalVariable, PatternVariable, CalculatedPointer}, false, m_UDTVariables);
        appendInheritances();

        if (!m_functionDefinitions.empty())
            m_functionDefinitions.clear();
        loadTypeDefinitions(m_functionTokenRange, {Function}, m_functionDefinitions);

        if (!m_functionVariables.empty())
            m_functionVariables.clear();
        loadVariableDefinitions(m_functionTokenRange, tkn::Separator::LeftParenthesis, tkn::Separator::RightParenthesis,
                                {FunctionParameter}, true, m_functionVariables);

        loadVariableDefinitions(m_functionTokenRange, tkn::Separator::LeftParenthesis, tkn::Separator::RightParenthesis,
                                {View,FunctionVariable}, false, m_functionVariables);
    }


// Load the source code into the text highlighter, splits
// the text into lines and creates a lookup table for the
// first token id of each line.
    void TextHighlighter::loadText() {

        if (!m_lines.empty())
            m_lines.clear();

        if (m_text.empty())
            m_text = m_viewPatternEditor->preprocessText(m_viewPatternEditor->getTextEditor().GetText());

        m_lines = hex::splitString(m_text, "\n");
        m_lines.push_back("");
        m_firstTokenIdOfLine.clear();
        m_firstTokenIdOfLine.resize(m_lines.size(), -1);

        i32 tokenId = 0;
        i32 tokenCount = m_tokens->size();
        i32 index;

        if (tokenCount > 0) {
            index = (*m_tokens)[0].location.line - 1;
            m_firstTokenIdOfLine[index] = 0;
        }
        i32 count = m_lines.size();
        for (i32 currentLine = 0; currentLine < count; currentLine++) {
            for (index = (*m_tokens)[tokenId].location.line - 1;
                 index <= currentLine && tokenId + 1 < tokenCount; tokenId++)
                index = (*m_tokens)[tokenId + 1].location.line - 1;

            if (index > currentLine)
                m_firstTokenIdOfLine[index] = tokenId;
        }

        if (m_firstTokenIdOfLine.back() != tokenCount)
            m_firstTokenIdOfLine.push_back(tokenCount);
    }

// Some tokens span many lines and some lines have no tokens. This
// function helps to find the next line number in the inner loop.
    u32 TextHighlighter::nextLine(u32 line) {
        auto currentTokenId = m_firstTokenIdOfLine[line];
        u32 i = 1;
        while (line + i < m_lines.size() &&
               (m_firstTokenIdOfLine[line + i] == currentTokenId || m_firstTokenIdOfLine[line + i] == (i32) 0xFFFFFFFF))
            i++;
        return i + line;
    }

    u32 TextHighlighter::previousLine(u32 line) {
        auto currentTokenId = m_firstTokenIdOfLine[line];
        u32 i = 1;
        while (line - i < m_lines.size() &&
               (m_firstTokenIdOfLine[line - i] == currentTokenId || m_firstTokenIdOfLine[line - i] == (i32) 0xFFFFFFFF))
            i++;
        return line - i;
    }

// global token ranges are the complement (aka inverse) of the union
// of the UDT and function token ranges
    void TextHighlighter::invertGlobalTokenRange() {
        std::set<Interval> ranges;
        auto size = m_globalTokenRange.size();
        auto tokenCount = m_tokens->size();

        if (size == 0) {
            ranges.insert(Interval(0, tokenCount));
        } else {
            auto it = m_globalTokenRange.begin();
            auto it2 = std::next(it);
            if (it->start != 0)
                ranges.insert(Interval(0, it->start));
            while (it2 != m_globalTokenRange.end()) {

                if (it->end < it2->start)
                    ranges.insert(Interval(it->end, it2->start));
                else
                    ranges.insert(Interval(it->start, it2->end));
                it = it2;
                it2 = std::next(it);
            }

            if (it->end < (i32) (tokenCount-1))
                ranges.insert(Interval(it->end, tokenCount-1));
        }
        m_globalTokenRange = ranges;
    }

// 0 for 1st argument, 1 for 2nd argument, etc. Obtained counting commas.
    i32 TextHighlighter::getArgumentNumber(i32 start, i32 arg) {
        i32 count = 0;
        m_curr = m_startToken;
        auto limit = m_startToken + arg;
        next(start);
        while (m_curr != limit) {

            if (peek(tkn::Separator::Comma))
                count++;
            next();
        }
        return count;
    }

// The inverse of getArgumentNumber.
    void TextHighlighter::getTokenIdForArgument(i32 start, i32 argNumber, Token delimiter) {
        m_curr = m_startToken;
        next(start);
        while (!peek(delimiter))
            next();
        next();
        i32 count = 0;
        while (count < argNumber && !peek(tkn::Separator::EndOfProgram)) {

            if (peek(tkn::Separator::Comma))
                count++;
            next();
        }
    }

// Changes auto type string in definitions to the actual type string.
    void TextHighlighter::resolveAutos(VariableMap &variableMap, UnorderedBlocks &tokenRange) {
        auto curr = m_curr;
        std::string UDTName;
        for (auto &[name, variables]: variableMap) {
            for (auto &[variableName, definitions]: variables) {
                for (auto &definition: definitions) {

                    if (definition.typeStr == "auto" && (definition.idType == Token::Identifier::IdentifierType::TemplateArgument || definition.idType == Token::Identifier::IdentifierType::FunctionParameter)) {
                        auto argumentIndex = getArgumentNumber(tokenRange[name].start, definition.tokenIndex);

                        if (tokenRange == m_UDTTokenRange || !m_attributeFunctionArgumentType.contains(name) ||
                            m_attributeFunctionArgumentType[name].empty()) {

                            //auto vectorString = hex::splitString(name, "::");
                            //auto shortName = vectorString.back();
                            auto instances = m_instances[name];
                            for (auto instance: instances) {

                                if (std::abs(definition.tokenIndex - instance) <= 5)
                                    continue;
                                Token delimiter;

                                if (tokenRange == m_UDTTokenRange)
                                    delimiter = tkn::Operator::BoolLessThan;
                                else
                                    delimiter = tkn::Separator::LeftParenthesis;
                                std::string fullName;
                                std::vector<Identifier *> identifiers;
                                getTokenIdForArgument(instance, argumentIndex,delimiter);
                                forwardIdentifierName(fullName, identifiers);

                                if (fullName.starts_with("Parent.")) {
                                    auto fixedDefinition = setChildrenTypes();

                                    if (fixedDefinition.has_value() &&
                                        m_UDTDefinitions.contains(fixedDefinition->typeStr)) {
                                        definition.typeStr = fixedDefinition->typeStr;
                                        continue;
                                    }
                                } else if (fullName.contains(".")) {
                                    Definition definitionTemp;
                                    resolveIdentifierType(definitionTemp,fullName);
                                    definition.typeStr = definitionTemp.typeStr;
                                } else {
                                    auto typeName = findIdentifierTypeStr(fullName);
                                    definition.typeStr = typeName;
                                }
                            }
                        } else {
                            UDTName = m_attributeFunctionArgumentType[name];
                            if (m_UDTDefinitions.contains(UDTName)) {
                                definition.typeStr = UDTName;
                                continue;
                            }
                        }
                    }
                }
            }
        }
        m_curr = curr;
    }

    void TextHighlighter::fixAutos() {
        resolveAutos(m_functionVariables, m_functionTokenRange);
        resolveAutos(m_UDTVariables, m_UDTTokenRange);
    }

    void TextHighlighter::fixChains() {

        if (!m_scopeChains.empty()) {
            for (auto chain: m_scopeChains) {
                m_curr = m_startToken + chain;
                colorSeparatorScopeChain();
            }
        }

        if (!m_memberChains.empty()) {
            for (auto chain: m_memberChains) {
                m_curr = m_startToken + chain;
                colorOperatorDotChain();
            }
        }
    }

// Calculates the union of all the UDT and function token ranges
// and inverts the result.
    void TextHighlighter::getGlobalTokenRanges() {
        std::set<Interval> ranges;
        for (auto [name, range]: m_UDTTokenRange)
            ranges.insert(range);
        for (auto [name, range]: m_functionTokenRange)
            ranges.insert(range);

        if (ranges.empty())
            return;

        auto it = ranges.begin();
        auto next = std::next(it);
        while (next != ranges.end()) {

            if (next->start - it->end < 2) {
                Interval &range = const_cast<Interval &>(*it);
                range.end = next->end;
                ranges.erase(next);
                next = std::next(it);
            } else {
                it++;
                next = std::next(it);
            }
        }
        m_globalTokenRange = ranges;
        invertGlobalTokenRange();
        for (auto tokenRange: m_globalTokenRange) {

            if ((u32) tokenRange.end == m_tokens->size()) {
                tokenRange.end -= 1;
                m_globalBlocks.insert(tokenRange);
            }
        }
    }

// Parser labels global variables that are not placed as
// function variables.
    void TextHighlighter::fixGlobalVariables() {
        for (auto range: m_globalTokenRange) {
            auto limit = m_startToken + range.end;
            for (m_curr =m_startToken + range.start; m_curr != limit; next()) {

                if (auto identifier = (Identifier *) getValue<Token::Identifier>(0); identifier != nullptr) {
                    auto identifierType = identifier->getType();
                    auto identifierName = identifier->get();

                    if (identifierType == IdentifierType::FunctionVariable) {
                        identifier->setType(IdentifierType::GlobalVariable, true);
                        setColor(-1, IdentifierType::GlobalVariable);
                    } else if (identifierType == IdentifierType::View) {
                        identifier->setType(IdentifierType::PlacedVariable, true);
                        setColor(-1,IdentifierType::PlacedVariable);
                    }
                }
            }
        }
    }


// Only update if needed. Must wait for the parser to finish first.
    void TextHighlighter::highlightSourceCode() {

        if (m_needsToUpdateColors && m_viewPatternEditor->getChangesWereParsed() &&
            (m_viewPatternEditor->getRunningParsers() + m_viewPatternEditor->getRunningEvaluators() + m_runningColorizers == 0)) {

            m_needsToUpdateColors = false;
            TaskManager::createBackgroundTask("hex.builtin.task.colorizing_pattern"_lang,
                                              [this](auto &) { this->colorizePatternEditor(); });
             m_viewPatternEditor->setChangesWereParsed(false);
        }
    }

    void TextHighlighter::colorizePatternEditor() {
        m_runningColorizers++;
        auto preprocessor = patternLanguage->get()->getInternals().preprocessor.get();
        auto parser = patternLanguage->get()->getInternals().parser.get();
        using Types = std::map<std::string, pl::hlp::safe_shared_ptr<pl::core::ast::ASTNodeTypeDecl>>;
        Types types = parser->getTypes();

        if (!m_UDTs.empty())
            m_UDTs.clear();
        for (auto &[name, type]: types)
            m_UDTs.push_back(name);
        m_tokens = preprocessor->getResult();
        if (m_tokens->empty())
            return;

        ON_SCOPE_EXIT {
            if (!m_tokenColors.empty())
                m_tokenColors.clear();
            m_runningColorizers--;
        };

        m_text = m_viewPatternEditor->preprocessText( m_viewPatternEditor->getTextEditor().GetText());

        if (m_text.empty() || m_text == "\n")
            return;
        loadText();

        if (!m_globalTokenRange.empty())
            m_globalTokenRange.clear();
        m_globalTokenRange.insert(Interval(0, m_tokens->size()-1));

        // Namespaces from included files.
        m_nameSpaces.clear();
        m_nameSpaces = preprocessor->getNamespaces();

        if (!m_inheritances.empty())
            m_inheritances.clear();

        if (!m_namespaceTokenRange.empty())
            m_namespaceTokenRange.clear();
        getAllTokenRanges(IdentifierType::NameSpace);

        if (!m_UDTBlocks.empty())
            m_UDTBlocks.clear();

        if (!m_UDTTokenRange.empty())
            m_UDTTokenRange.clear();
        getAllTokenRanges(IdentifierType::UDT);

        if (!m_functionBlocks.empty())
            m_functionBlocks.clear();

        if (!m_functionTokenRange.empty())
            m_functionTokenRange.clear();
        getAllTokenRanges(IdentifierType::Function);

        if (!m_globalBlocks.empty())
            m_globalBlocks.clear();
        getGlobalTokenRanges();

        fixGlobalVariables();
        if (!m_scopeChains.empty())
            m_scopeChains.clear();

        if (!m_memberChains.empty())
            m_memberChains.clear();

        setInitialColors();
        loadInstances();

        if (!m_attributeFunctionArgumentType.empty())
            m_attributeFunctionArgumentType.clear();

        getAllTokenRanges(IdentifierType::Attribute);
        getDefinitions();
        fixAutos();
        fixChains();

        m_excludedLocations = preprocessor->getExcludedLocations();

       colorRemainingTokens();
        setAllColors();

        m_viewPatternEditor->getTextEditor().ClearErrorMarkers();
        m_compileErrors = patternLanguage->get()->getCompileErrors();

        if (!m_compileErrors.empty())
            renderErrors();
        else
            m_viewPatternEditor->getTextEditor().ClearErrorMarkers();
    }
}