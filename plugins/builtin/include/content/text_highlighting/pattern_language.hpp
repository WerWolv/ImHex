#pragma once
#include <pl/core/token.hpp>
#include <pl/core/preprocessor.hpp>
#include <pl/helpers/safe_iterator.hpp>
#include <ui/text_editor.hpp>
#include <hex/helpers/types.hpp>

namespace pl {
    class PatternLanguage;
}

namespace hex::plugin::builtin {
    class ViewPatternEditor;
    class TextHighlighter  {
    public:
        class Interval;
        using Token                 = pl::core::Token;
        using ASTNode               = pl::core::ast::ASTNode;
        using ExcludedLocation      = pl::core::Preprocessor::ExcludedLocation;
        using CompileError          = pl::core::err::CompileError;
        using Identifier            = Token::Identifier;
        using IdentifierType        = Identifier::IdentifierType;
        using UnorderedBlocks       = std::map<std::string,Interval>;
        using OrderedBlocks         = std::map<Interval,std::string>;
        using Scopes                = std::set<Interval>;
        using Location              = pl::core::Location;
        using TokenIter             = pl::hlp::SafeIterator<std::vector<Token>::const_iterator>;
        using VariableScopes        = std::map<std::string,Scopes>;
        using Inheritances          = std::map<std::string,std::vector<std::string>>;
        using IdentifierTypeColor   = std::map<Identifier::IdentifierType,ui::TextEditor::PaletteIndex>;
        using TokenTypeColor        = std::map<Token::Type,ui::TextEditor::PaletteIndex>;
        using TokenColor            = std::map<Token *,ui::TextEditor::PaletteIndex>;

        struct ParentDefinition;
        struct Definition {
            Definition()= default;
            Definition(IdentifierType identifierType, std::string typeStr,i32 tokenId, Location location) : idType(identifierType), typeStr(typeStr), tokenIndex(tokenId),location(location) {}
            IdentifierType idType;
            std::string typeStr;
            i32 tokenIndex;
            Location location;
        };

        struct ParentDefinition {
            ParentDefinition() = default;
            ParentDefinition(IdentifierType identifierType, i32 tokenId, Location location) : idType(identifierType), tokenIndex(tokenId), location(location) {}
            IdentifierType idType;
            i32 tokenIndex;
            Location location;
        };
        /// to define functions and types
        using Definitions = std::map<std::string,ParentDefinition>;
        /// to define global variables
        using Variables   = std::map<std::string,std::vector<Definition>>;
        /// to define UDT and function variables
        using VariableMap = std::map<std::string,Variables>;
    private:
        std::string m_text;
        std::vector<std::string> m_lines;
        std::vector<i32> m_firstTokenIdOfLine;
        ViewPatternEditor *m_viewPatternEditor;
        std::vector<ExcludedLocation> m_excludedLocations;
        std::vector<Token> m_tokens;
        TokenColor m_tokenColors;
        std::unique_ptr<pl::PatternLanguage> *patternLanguage;
        std::vector<CompileError> m_compileErrors;
        std::map<std::string,std::vector<i32>> m_instances;
        Definitions m_UDTDefinitions;
        Definitions m_functionDefinitions;

        OrderedBlocks m_namespaceTokenRange;
        UnorderedBlocks m_UDTTokenRange;
        UnorderedBlocks m_functionTokenRange;
        Scopes m_globalTokenRange;

        VariableMap m_UDTVariables;
        VariableMap m_ImportedUDTVariables;
        VariableMap m_functionVariables;
        Variables m_globalVariables;

        std::map<std::string, std::vector<Token>> m_parsedImports;
        std::map<std::string,std::string> m_attributeFunctionArgumentType;
        std::map<std::string,std::string> m_typeDefMap;
        std::map<std::string,std::string> m_typeDefInvMap;
        std::vector<std::string> m_nameSpaces;
        std::vector<std::string> m_UDTs;
        std::set<i32> m_taggedIdentifiers;
        std::set<i32> m_memberChains;
        std::set<i32> m_scopeChains;

        TokenIter m_curr;
        TokenIter m_startToken, m_originalPosition, m_partOriginalPosition;

        VariableScopes m_UDTBlocks;
        VariableScopes m_functionBlocks;
        Scopes m_globalBlocks;
        Inheritances m_inheritances;
        const static IdentifierTypeColor m_identifierTypeColor;
        const static TokenTypeColor m_tokenTypeColor;

        i32 m_runningColorizers=0;
    public:

        /// Intervals are the sets finite contiguous non-negative integer that
        /// are described by their endpoints. The sets must have the following
        /// properties:
        /// 1. Any two elements of the set can either have an empty intersection or
        /// 2. their intersection is equal to one of the two sets (i.e. one is
        /// a subset of the other).
        /// An interval is defined to be smaller than another if:
        /// 1. The interval lies entirely to the left of the other interval or
        /// 2. The interval is a proper subset of the other interval.
        /// Two intervals are equal if they have identical start and end values.
        /// This ordering is used for things like code blocks or the token
        /// ranges that are defined by the blocks.
        class Interval {
        public:
            i32 start;
            i32 end;
            Interval() : start(0), end(0) {}
            Interval(i32 start, i32 end) : start(start), end(end) {
                if (start > end)
                    throw std::invalid_argument("Interval start must be less than or equal to end");
            }
            bool operator<(const Interval &other) const {
                return other.end > end;
            }
            bool operator>(const Interval &other) const {
                return end > other.end;
            }
            bool operator==(const Interval &other) const {
                return start == other.start && end == other.end;
            }
            bool operator!=(const Interval &other) const {
                return start != other.start || end != other.end;
            }
            bool operator<=(const Interval &other) const {
                return other.end >= end;
            }
            bool operator>=(const Interval &other) const {
                return end >= other.end;
            }
            bool contains(const Interval &other) const {
                return other.start >= start && other.end <= end;
            }
            bool contains(i32 value) const {
                return value >= start && value <= end;
            }
            bool contiguous(const Interval &other) const {
                return ((start - other.end) == 1 || (other.start - end) == 1);
            }
        };
        std::atomic<bool>  m_needsToUpdateColors = true;
        std::atomic<bool>  m_wasInterrupted = false;
        std::atomic<bool>  m_interrupt = false;


        void interrupt() {
            m_interrupt = true;
        }

        TextHighlighter(ViewPatternEditor *viewPatternEditor, std::unique_ptr<pl::PatternLanguage> *patternLanguage ) :
                m_viewPatternEditor(viewPatternEditor), patternLanguage(patternLanguage), m_needsToUpdateColors(true) {}
        /**
         * @brief Entry point to syntax highlighting
         */
        void highlightSourceCode(const std::string& text);
        void processSource();
        void clearVariables();

        /**
        * @brief Syntax highlighting from parser
        */
        void setInitialColors();
        /**
        * @brief Create data to pass to text editor
        */
        void setRequestedIdentifierColors();
        /**
        * @brief Set the color of a token
        */
        void setColor(i32 tokenId=-1, const IdentifierType &type = IdentifierType::Unknown);
        void setIdentifierColor(i32 tokenId=-1, const IdentifierType &type = IdentifierType::Unknown);
        /**
        * @brief Only identifiers not in chains should remain
        */
        void colorRemainingIdentifierTokens();
        /**
         * @brief Renders compile errors in real time
         */
        void renderErrors();
        /// A token range is the set of token indices of a definition. The namespace token
        /// ranges are obtained first because they are needed to obtain unique identifiers.
        void getAllTokenRanges(IdentifierType identifierTypeToSearch);
        /// The global scope is the complement of the union of all the function and UDT token ranges
        void getGlobalTokenRanges();
        /// If the current token is a function or UDT, creates a map entry from the name to the token range. These are ordered alphabetically by name.
        /// If the current token is a namespace, creates a map entry from the token range to the name. Namespace entries are stored in the order they occur in the source code.
        bool getTokenRange(std::vector<Token> keywords,UnorderedBlocks &tokenRange, OrderedBlocks &tokenRangeInv, bool fullName, VariableScopes *blocks);
        /// Global variables are the variables that are not inside a function or UDT
        void fixGlobalVariables();
        /// Creates the definition maps for UDTs, functions, their variables and global variables
        void getDefinitions();
        void loadGlobalDefinitions(Scopes tokenRangeSet, std::vector<IdentifierType> identifierTypes, Variables &variables);
        void loadVariableDefinitions(UnorderedBlocks tokenRangeMap, Token delimiter1, Token delimiter2, std::vector<IdentifierType> identifierTypes, bool isArgument, VariableMap &variableMap);
        void loadTypeDefinitions(UnorderedBlocks tokenRangeMap, std::vector<IdentifierType> identifierTypes, Definitions &types);
        std::string getArgumentTypeName(i32 rangeStart, Token delimiter2);
        std::string getVariableTypeName();
        /// Append the variable definitions of the parent to the child
        void appendInheritances();
        void recurseInheritances(std::string name);
        ///Loads a map of identifiers to their token id instances
        void loadInstances();
        /// Replace auto with the actual type for template arguments and function parameters
        void fixAutos();
        void resolveAutos(VariableMap &variableMap, UnorderedBlocks &tokenRange);
        /// Chains are sequences of identifiers separated by scope resolution or dot operators.
        void fixChains();
        bool colorSeparatorScopeChain();
        bool colorOperatorDotChain();
        /// Returns the next/previous valid source code line
        u32 nextLine(u32 line);
        u32 previousLine(u32 line);
        /// Loads the source code and calculates the first token index of each line
        void loadText();
        /// Used to obtain the color to be applied.
        ui::TextEditor::PaletteIndex getPaletteIndex(Token::Literal *literal);
        /// The complement of a set is also known as its inverse
        void invertGlobalTokenRange();
        /// Starting at the identifier, it tracks all the scope resolution and dot operators and returns the full chain without arrays, templates, pointers,...
        bool getFullName(std::string &identifierName, std::vector<Identifier *> &identifiers, bool preserveCurr = true);
        /// Returns the identifier value.
        bool getIdentifierName(std::string &identifierName, Identifier *identifier);
        /// Adds namespaces to the full name if they exist
        bool getQualifiedName(std::string &identifierName, std::vector<Identifier *> &identifiers, bool useDefinitions = false, bool preserveCurr = true);
        /// As it moves forward it loads the result to the argument. Used by getFullName
        bool forwardIdentifierName(std::string &identifierName, std::vector<Identifier *> &identifiers, bool preserveCurr = true);
        /// Takes as input the full name and returns the type of the last element.
        bool resolveIdentifierType(Definition &result, std::string identifierName);
        /// like previous functions but returns the type of the variable that is a member of a UDT
        std::string findIdentifierTypeStr(const std::string &identifierName, std::string context="");
        IdentifierType findIdentifierType(const std::string &identifierName, std::string context);
        /// If context is empty search for the variable, if it isnt use the variable map.
        bool findOrContains(std::string &context, UnorderedBlocks tokenRange, VariableMap variableMap);
        /// Search for instances inside some block
        void setBlockInstancesColor(const std::string &name, const Definition &definition, const Interval &block);
        /// Convenience functions.
        void skipAttribute();
        void skipArray(i32 maxSkipCount, bool forward = true);
        void skipTemplate(i32 maxSkipCount, bool forward = true);
        void skipDelimiters(i32 maxSkipCount, Token delimiter[2], i8 increment);
        void skipToken(Token token, i8 step=1);
        /// from given or current names find the corresponding definition
        bool findIdentifierDefinition(Definition &result, const std::string &optionalIdentifierName = "",  std::string optionalName = "", bool setInstances = false);
        /// To deal with the Parent keyword
        std::optional<Definition> setChildrenTypes();
        bool findParentTypes(std::vector<std::string> &parentTypes, const std::string &optionalUDTName="");
        bool findAllParentTypes(std::vector<std::string> &parentTypes, std::vector<Identifier *> &identifiers, std::string &optionalFullName);
        bool tryParentType(const std::string &parentType, std::string &variableName, std::optional<Definition> &result, std::vector<Identifier *> &identifiers);
        /// Convenience function
        bool isTokenIdValid(i32 tokenId);
        bool isLocationValid(Location location);
        /// Returns the name of the context where the current or given token is located
        bool findScope(std::string &name, const UnorderedBlocks &map, i32 optionalTokenId=-1);
        /// Returns the name of the namespace where the current or given token is located
        bool findNamespace(std::string &nameSpace, i32 optionalTokenId=-1);
        /// Calculate the source code, line and column numbers of a token index
        pl::core::Location getLocation(i32 tokenId);
        /// Calculate the token index of a source code, line and column numbers
        i32 getTokenId(pl::core::Location location);
        /// Calculate the function or template argument position from token indices
        i32  getArgumentNumber(i32 start,i32 arg);
        /// Calculate the token index of a function or template argument position
        void getTokenIdForArgument(i32 start, i32 argNumber, Token delimiter);
        ///Creates a map from function name to argument type
        void linkAttribute();
        /// Comment and strings usethese function to determine their coordinates
        template<typename T> ui::TextEditor::Coordinates commentCoordinates(Token *token);
        ui::TextEditor::Coordinates stringCoordinates();
        /// Returns the number of tasks highlighting code. Shouldn't be > 1
        i32 getRunningColorizers() {
            return m_runningColorizers;
        }

        enum class HighlightStage {
            Starting,
            NamespaceTokenRanges,
            UDTTokenRanges,
            FunctionTokenRanges,
            GlobalTokenRanges,
            FixGlobalVariables,
            SetInitialColors,
            LoadInstances,
            AttributeTokenRanges,
            Definitions,
            FixAutos,
            FixChains,
            ExcludedLocations,
            ColorRemainingTokens,
            SetRequestedIdentifierColors,
            Stage1,
            Stage2,
            Stage3,
            Stage4,
            Stage5,
            Stage6,
            Stage7,
            Stage8,
            Stage9,
            Stage10,
            Stage11,
        };

        HighlightStage m_highlightStage = HighlightStage::Starting;

        /// The following functions were copied from the parser and some were modified

        template<typename T>
        T *getValue(const i32 index) {
            return const_cast<T*>(std::get_if<T>(&m_curr[index].value));
        }

        void next(i32 count = 1) {
            if (m_interrupt) {
                m_interrupt = false;
                throw std::out_of_range("Highlights were deliberately interrupted");
            }
            if (count == 0)
                return;
            i32 id = getTokenId(m_curr->location);
            i32 maxChange;
            if (count > 0)
                maxChange = std::min(count,static_cast<i32>(m_tokens.size() - id));
            else
                maxChange = -std::min(-count,id);
            m_curr += maxChange;
        }
        constexpr static u32 Normal = 0;
        constexpr static u32 Not    = 1;

        bool begin() {
            m_originalPosition = m_curr;

            return true;
        }

        void partBegin() {
            m_partOriginalPosition = m_curr;
        }

        void reset() {
            m_curr = m_originalPosition;
        }

        void partReset() {
            m_curr = m_partOriginalPosition;
        }

        bool resetIfFailed(const bool value) {
            if (!value) reset();

            return value;
        }

        template<auto S = Normal>
        bool sequenceImpl() {
            if constexpr (S == Normal)
            return true;
            else if constexpr (S == Not)
            return false;
            else
            std::unreachable();
        }

        template<auto S = Normal>
        bool matchOne(const Token &token) {
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

        template<auto S = Normal>
        bool sequenceImpl(const auto &... args) {
            return (matchOne<S>(args) && ...);
        }

        template<auto S = Normal>
        bool sequence(const Token &token, const auto &... args) {
            partBegin();
            return sequenceImpl<S>(token, args...);
        }

        bool isValid() {
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
            if (!isLocationValid(token.location))
                return false;
            return true;
        }

        bool peek(const Token &token, const i32 index = 0) {
            if (!isValid())
                return false;
            i32 id = getTokenId(m_curr->location);
            if (id+index < 0 || id+index >= (i32)m_tokens.size())
                return false;
            return m_curr[index].type == token.type && m_curr[index] == token.value;
        }

    };
}