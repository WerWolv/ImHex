 #pragma once

#include <hex/ui/view.hpp>
#include <hex/providers/provider.hpp>
#include <hex/ui/popup.hpp>

#include <pl/pattern_language.hpp>
#include <pl/core/errors/error.hpp>

#include <ui/hex_editor.hpp>
#include <ui/pattern_drawer.hpp>

#include <filesystem>
#include <functional>

#include <TextEditor.h>
#include "popups/popup_file_chooser.hpp"
#include "hex/api/achievement_manager.hpp"
#include "pl/core/preprocessor.hpp"
#include "pl/core/ast/ast_node_function_definition.hpp"
#include "pl/helpers/safe_iterator.hpp"


 namespace pl::ptrn { class Pattern; }

namespace hex::plugin::builtin {

    class PatternSourceCode {
    public:
        const std::string& get(prv::Provider *provider) {
            if (m_synced)
                return m_sharedSource;

            return m_perProviderSource.get(provider);
        }

        void set(prv::Provider *provider, std::string source) {
            source = wolv::util::trim(source);

            m_perProviderSource.set(source, provider);
            m_sharedSource = std::move(source);
        }

        bool isSynced() const {
            return m_synced;
        }

        void enableSync(bool enabled) {
            m_synced = enabled;
        }

    private:
        bool m_synced = false;
        PerProvider<std::string> m_perProviderSource;
        std::string m_sharedSource;
    };

    class ViewPatternEditor : public View::Window {
    public:
        ViewPatternEditor();
        ~ViewPatternEditor() override;

        void drawAlwaysVisibleContent() override;
        void drawContent() override;
        [[nodiscard]] ImGuiWindowFlags getWindowFlags() const override {
            return ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        }
        TextEditor::PaletteIndex getPaletteIndex(char type);
    public:
        struct VirtualFile {
            std::fs::path path;
            std::vector<u8> data;
            Region region;
        };

        enum class DangerousFunctionPerms : u8 {
            Ask,
            Allow,
            Deny
        };

        class TextHighlighter  {
        public:
            class Interval;
            using Token = pl::core::Token;
            using ASTNode = pl::core::ast::ASTNode;
            using ExcludedLocation = pl::core::Preprocessor::ExcludedLocation;
            using CompileError = pl::core::err::CompileError;
            using Identifier = Token::Identifier;
            using IdentifierType = Identifier::IdentifierType;
            using UnorderedBlocks = std::map<std::string,Interval>;
            using OrderedBlocks = std::map<Interval,std::string>;
            using Scopes  = std::set<Interval>;
            using Location = pl::core::Location;
            using TokenIter = pl::hlp::SafeIterator<std::vector<Token>::const_iterator>;
            using VariableScopes = std::map<std::string,Scopes>;
            using Inheritances = std::map<std::string,std::vector<std::string>>;

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
            ViewPatternEditor &m_viewPatternEditor;
            std::vector<ExcludedLocation> m_excludedLocations;
            std::vector<std::shared_ptr<ASTNode>> m_ast;
            std::vector<Token> m_tokens;
            std::unique_ptr<pl::PatternLanguage> &patternLanguage;
            std::vector<CompileError> m_compileErrors;

            std::map<std::string,std::vector<i32>> m_instances;

            Definitions m_UDTDefinitions;
            Definitions m_functionDefinitions;

            OrderedBlocks m_namespaceTokenRange;
            UnorderedBlocks m_UDTTokenRange;
            UnorderedBlocks m_functionTokenRange;
            Scopes m_globalTokenRange;

            VariableMap m_UDTVariables;
            VariableMap m_functionVariables;
            Variables m_globalVariables;

            std::map<std::string,std::string> m_attributeFunctionArgumentType;

            std::vector<std::string> m_typeDefs;
            std::vector<std::string> m_nameSpaces;
            std::vector<std::string> m_UDTs;

            std::map<std::string, pl::hlp::safe_shared_ptr<pl::core::ast::ASTNodeTypeDecl>>  m_types;

            TokenIter m_curr;
            TokenIter m_startToken, m_originalPosition, m_partOriginalPosition;

            VariableScopes m_UDTBlocks;
            VariableScopes m_functionBlocks;
            Scopes m_globalBlocks;

            Inheritances m_inheritances;
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
                    if (start >= end)
                        throw std::invalid_argument("Interval start must be less than end");
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

            TextHighlighter(ViewPatternEditor &viewPatternEditor ) : m_viewPatternEditor(viewPatternEditor), patternLanguage(viewPatternEditor.m_editorRuntime) {
                m_needsToUpdateColors = true;
            }

            /**
             * @brief Entry point to syntax highlighting
             */
            void highlightSourceCode();
            /**
             * @brief Renders compile errors in real time
             */
            void renderErrors();
            /// The argument to this function must be the token sequence that was processed
            /// by the parser which loaded the identifiers of variable definitions with their types.
            /// This function then loads the types to the un-preprocessed token sequence.
            void getIdentifierTypes(const std::optional<std::vector<pl::core::Token>> &tokens);
            /// A token range is the set of token indices of a definition. The namespace token
            /// ranges are obtained first because they are needed to obtain unique identifiers.
            void getAllTokenRanges(IdentifierType idtype);

            /// The global scope is the complement of the union of all the function and UDT token ranges
            void getGlobalTokenRanges();
            /// If the current token is a function or UDT or namespace, creates a map entry from the name to the token range
            /// for the first two and from the token range to the name for the last. This ensures that namespaces are stored in the order they occur in the source code.
            bool getTokenRange(std::vector<Token> keywords,UnorderedBlocks &tokenRange, OrderedBlocks &tokenRangeInv, bool fullName, bool invertMap, VariableScopes *blocks);
            /// Code blocks are delimited by braces so they are created by loops, conditionals, functions, UDTs and namespaces
            void loadGlobalBlocks();
            /// Global variables are the variables that are not inside a function or UDT
            void fixGlobalVariables();
            /// Creates the definition maps for UDTs, functions, their variables and global variables
            void getDefinitions();
            void loadGlobalDefinitions(Scopes tokenRangeSet, std::vector<IdentifierType> identifierTypes, Variables &variables);
            void loadVariableDefinitions(UnorderedBlocks tokenRangeMap, Token delimiter, std::vector<IdentifierType> identifierTypes, bool isArgument, VariableMap &variableMap);
            void loadTypeDefinitions(UnorderedBlocks tokenRangeMap, std::vector<IdentifierType> identifierTypes, Definitions &types);
            std::string getArgumentTypeName(i32 rangeStart, Token delimiter2);
            std::string getVariableTypeName();
            void appendInheritances();
            ///Loads a map of identifiers to their token id instances
            void loadInstances();
            /// Replace auto with the actual type
            void resolveAutos(VariableMap &variableMap, UnorderedBlocks &tokenRange);
            void fixAutos();
            /// for every function token range creates a map of identifier types from
            /// their definitions and uses it to assign the correct type to the function variables.
            void fixFunctionVariables();
            /// Processes the tokens one line of source code at a time
            void processLineTokens();
            /// Returns the next/previous valid source code line
            u32 nextLine(u32 line);
            u32 previousLine(u32 line);
            /// Returns the number of escape characters in a string
            i32 escapeCharCount(const std::string &str);
            /// Loads the source code and calculates the first token index of each line
            void loadText();
            /// Used to obtain the color to be applied.
            TextEditor::PaletteIndex getPaletteIndex();
            TextEditor::PaletteIndex getPaletteIndex(Identifier::IdentifierType type);
            TextEditor::PaletteIndex getPaletteIndex(Token::Literal *literal);
            TextEditor::PaletteIndex getPaletteIndex(Identifier *identifier);
            /// The complement of a set is also known as its inverse
            void invertGlobalTokenRange();
            /// Starting at the identifier, it tracks back all the scope resolution and dot operators and returns the full chain without arrays, templates, pointers,...
            bool getFullName(std::string &identifierName);
            /// Returns the identifier value.
            bool getIdentifierName(std::string &identifierName);
            /// Adds namespaces ti the full name if they exist
            bool getQualifiedName(std::string &identifierName);
            /// Rewinds the token iterator to the start of the identifier. Used by getFullName
            void rewindIdentifierName();
            /// As it moves forward it loads the result to the argument. Used by getFullName
            bool forwardIdentifierName(std::string &identifierName, std::optional<TokenIter> curr, std::vector<Identifier *> &identifiers);
            /// Takes as input the full name and returns the type of the last element.
            bool resolveIdentifierType(Definition &result);
            /// Assumes that there is a map entry for the identifier in the current token and that the token is inside the corresponding token range
            bool findIdentifierType(IdentifierType &type );
            /// Scope resolution can be namespace or enumeration.
            bool findScopeResolutionType(Identifier::IdentifierType &type);
           /// like previous functions but returns the type of the variable that is a member of a UDT
            std::string findIdentifierTypeStr(const std::string &identifierName, std::string context="");
            bool findOrContains(std::string &context, UnorderedBlocks tokenRange, VariableMap variableMap);
            /// Convenience functions.
            void skipAttribute();
            void skipArray(i32 maxSkipCount, bool forward = true);
            void skipTemplate(i32 maxSkipCount, bool forward = true);
            void skipDelimiters(i32 maxSkipCount, Token delimiter[2], i8 increment);
            void skipToken(Token token, i8 step=1);
            /// from given or current names find the corresponding definition
            bool findIdentifierDefinition(Definition &result, const std::string &optionalIdentifierName = "",  std::string optionalName = "");
            /// To deal with the Parent keyword
            std::optional<Definition> setChildrenTypes(std::string &optionalFullName, std::vector<Identifier *> &optionalIdentifiers);
            bool findParentTypes(std::vector<std::string> &parentTypes, const std::string &optionalName="");
            bool findAllParentTypes(std::vector<std::string> &parentTypes, std::vector<Identifier *> &identifiers, std::string &optionalFullName);
            bool tryParentType(const std::string &parentType, std::string &variableName, std::optional<Definition> &result, std::vector<Identifier *> &identifiers);
            /// Clear all the data structures
            void clear();
            /// Convenience function
            bool isTokenIdValid(i32 tokenId);
            /// These 3 use the types created from ast, but only as las resort or for importes
            std::optional<std::shared_ptr<pl::core::ast::ASTNode>> parseChildren(const std::shared_ptr<pl::core::ast::ASTNode> &node, const std::string &typeName);
            bool isMemberOf(const std::string &memberName, const std::string &UDTName, IdentifierType &identifierType);
            std::optional<std::shared_ptr<ASTNode>> findType(const std::string &typeName, IdentifierType &identifierType);
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
            void getTokenIdForArgument(i32 start, i32 argNumber);
            ///Creates amap from function name to argument type
            void linkAttribute();
            /// Token consuming

            template<typename T>
            const T *getValue(const i32 index) {
                return std::get_if<T>(&m_curr[index].value);
            }

            void restart() {
                m_curr = m_startToken;
            }

            void next(i32 count = 1) {
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

            bool peek(const Token &token, const i32 index = 0) {
                i32 id = getTokenId(m_curr->location);
                if (id+index < 0 || id+index >= (i32)m_tokens.size())
                    return false;
                return m_curr[index].type == token.type && m_curr[index] == token.value;
            }

        };

    private:
        class PopupAcceptPattern : public Popup<PopupAcceptPattern> {
        public:
            explicit PopupAcceptPattern(ViewPatternEditor *view) : Popup("hex.builtin.view.pattern_editor.accept_pattern"), m_view(view) {}

            void drawContent() override {
                std::scoped_lock lock(m_view->m_possiblePatternFilesMutex);

                auto* provider = ImHexApi::Provider::get();

                ImGuiExt::TextFormattedWrapped("{}", static_cast<const char *>("hex.builtin.view.pattern_editor.accept_pattern.desc"_lang));

                std::vector<std::string> entries;
                entries.resize(m_view->m_possiblePatternFiles.get(provider).size());

                for (u32 i = 0; i < entries.size(); i++) {
                    entries[i] = wolv::util::toUTF8String(m_view->m_possiblePatternFiles.get(provider)[i].filename());
                }

                if (ImGui::BeginListBox("##patterns_accept", ImVec2(-FLT_MIN, 0))) {
                    u32 index = 0;
                    for (auto &path : m_view->m_possiblePatternFiles.get(provider)) {
                        if (ImGui::Selectable(wolv::util::toUTF8String(path.filename()).c_str(), index == m_selectedPatternFile, ImGuiSelectableFlags_DontClosePopups))
                            m_selectedPatternFile = index;

                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                            m_view->loadPatternFile(m_view->m_possiblePatternFiles.get(provider)[m_selectedPatternFile], provider);

                        ImGuiExt::InfoTooltip(wolv::util::toUTF8String(path).c_str());

                        index++;
                    }

                    // Close the popup if there aren't any patterns available
                    if (index == 0)
                        this->close();

                    ImGui::EndListBox();
                }

                ImGui::NewLine();
                ImGui::TextUnformatted("hex.builtin.view.pattern_editor.accept_pattern.question"_lang);

                ImGuiExt::ConfirmButtons("hex.ui.common.yes"_lang, "hex.ui.common.no"_lang,
                    [this, provider] {
                        m_view->loadPatternFile(m_view->m_possiblePatternFiles.get(provider)[m_selectedPatternFile], provider);
                        this->close();
                    },
                    [this] {
                        this->close();
                    }
                );

                if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                    this->close();
            }

            [[nodiscard]] ImGuiWindowFlags getFlags() const override {
                return ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize;
            }

        private:
            ViewPatternEditor *m_view;
            u32 m_selectedPatternFile = 0;
        };

    private:
        struct PatternVariable {
            bool inVariable;
            bool outVariable;

            pl::core::Token::ValueType type;
            pl::core::Token::Literal value;
        };

        enum class EnvVarType
        {
            Integer,
            Float,
            String,
            Bool
        };

        struct EnvVar {
            u64 id;
            std::string name;
            pl::core::Token::Literal value;
            EnvVarType type;

            bool operator==(const EnvVar &other) const {
                return this->id == other.id;
            }
        };

        struct AccessData {
            float progress;
            u32 color;
        };

        std::unique_ptr<pl::PatternLanguage> m_editorRuntime;

        std::mutex m_possiblePatternFilesMutex;
        PerProvider<std::vector<std::fs::path>> m_possiblePatternFiles;
        bool m_runAutomatically   = false;
        bool m_triggerEvaluation  = false;
        std::atomic<bool> m_triggerAutoEvaluate = false;

        volatile bool m_lastEvaluationProcessed = true;
        bool m_lastEvaluationResult    = false;

        std::atomic<u32> m_runningEvaluators = 0;
        std::atomic<u32> m_runningParsers    = 0;

        bool m_hasUnevaluatedChanges = false;

        TextEditor m_textEditor, m_consoleEditor;
        std::atomic<bool> m_consoleNeedsUpdate = false;

        std::atomic<bool> m_dangerousFunctionCalled = false;
        std::atomic<DangerousFunctionPerms> m_dangerousFunctionsAllowed = DangerousFunctionPerms::Ask;

        bool m_autoLoadPatterns = true;

        std::map<prv::Provider*, std::function<void()>> m_sectionWindowDrawer;

        ui::HexEditor m_sectionHexEditor;

        PatternSourceCode m_sourceCode;
        PerProvider<std::vector<std::string>> m_console;
        PerProvider<bool> m_executionDone;

        std::mutex m_logMutex;

        PerProvider<std::optional<pl::core::err::PatternLanguageError>> m_lastEvaluationError;
        PerProvider<std::vector<pl::core::err::CompileError>> m_lastCompileError;
        PerProvider<std::vector<const pl::core::ast::ASTNode*>> m_callStack;
        PerProvider<std::map<std::string, pl::core::Token::Literal>> m_lastEvaluationOutVars;
        PerProvider<std::map<std::string, PatternVariable>> m_patternVariables;
        PerProvider<std::map<u64, pl::api::Section>> m_sections;

        PerProvider<std::vector<VirtualFile>> m_virtualFiles;

        PerProvider<std::list<EnvVar>> m_envVarEntries;

        PerProvider<TaskHolder> m_analysisTask;
        PerProvider<bool> m_shouldAnalyze;
        PerProvider<bool> m_breakpointHit;
        PerProvider<std::unique_ptr<ui::PatternDrawer>> m_debuggerDrawer;
        std::atomic<bool> m_resetDebuggerVariables;
        i32 m_debuggerScopeIndex = 0;

        std::array<AccessData, 512> m_accessHistory = {};
        u32 m_accessHistoryIndex = 0;
        bool m_parentHighlightingEnabled = true;
        bool m_replaceMode = false;
        bool m_openFindReplacePopUp = false;

        static inline std::array<std::string,256> m_findHistory;
        static inline u32 m_findHistorySize = 0;
        static inline u32 m_findHistoryIndex = 0;
        static inline std::array<std::string,256> m_replaceHistory;
        static inline u32 m_replaceHistorySize = 0;
        static inline u32 m_replaceHistoryIndex = 0;

        TextHighlighter m_textHighlighter = TextHighlighter(*this);

    private:
        void drawConsole(ImVec2 size);
        void drawEnvVars(ImVec2 size, std::list<EnvVar> &envVars);
        void drawVariableSettings(ImVec2 size, std::map<std::string, PatternVariable> &patternVariables);
        void drawSectionSelector(ImVec2 size, const std::map<u64, pl::api::Section> &sections);
        void drawVirtualFiles(ImVec2 size, const std::vector<VirtualFile> &virtualFiles) const;
        void drawDebugger(ImVec2 size);
        std::string preprocessText(const std::string &code);
        void drawPatternTooltip(pl::ptrn::Pattern *pattern);

        void drawFindReplaceDialog(std::string &findWord, bool &requestFocus, u64 &position, u64 &count, bool &updateCount);

        void historyInsert(std::array<std::string,256> &history,u32 &size,  u32 &index, const std::string &value);

        void loadPatternFile(const std::fs::path &path, prv::Provider *provider);

        void parsePattern(const std::string &code, prv::Provider *provider);
        void evaluatePattern(const std::string &code, prv::Provider *provider);

        void registerEvents();
        void registerMenuItems();
        void registerHandlers();

        std::function<void()> importPatternFile = [&] {
            auto provider = ImHexApi::Provider::get();
            const auto basePaths = fs::getDefaultPaths(fs::ImHexPath::Patterns);
            std::vector<std::fs::path> paths;

            for (const auto &imhexPath : basePaths) {
                if (!wolv::io::fs::exists(imhexPath)) continue;

                std::error_code error;
                for (auto &entry : std::fs::recursive_directory_iterator(imhexPath, error)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".hexpat")
                        paths.push_back(entry.path());
                }
            }
            ui::PopupFileChooser::open(
                    basePaths, paths, std::vector<hex::fs::ItemFilter>{ { "Pattern File", "hexpat" } }, false,
                    [this, provider](const std::fs::path &path) {
                        this->loadPatternFile(path, provider);
                        AchievementManager::unlockAchievement("hex.builtin.achievement.patterns", "hex.builtin.achievement.patterns.load_existing.name");
                    }
            );
        };

        std::function<void()> exportPatternFile = [&] {
            fs::openFileBrowser(
                    fs::DialogMode::Save, { {"Pattern", "hexpat"} },
                    [this](const auto &path) {
                        wolv::io::File file(path, wolv::io::File::Mode::Create);
                        file.writeString(wolv::util::trim(m_textEditor.GetText()));
                    }
            );
        };

        void appendEditorText(const std::string &text);
        void appendVariable(const std::string &type);
        void appendArray(const std::string &type, size_t size);
    };

}
