 #pragma once

#include <hex/ui/view.hpp>
#include <hex/providers/provider.hpp>
#include <hex/helpers/fs.hpp>

#include <pl/pattern_language.hpp>
#include <pl/core/errors/error.hpp>

#include <ui/text_editor.hpp>
#include <content/text_highlighting/pattern_language.hpp>
#include <hex/helpers/magic.hpp>
#include <ui/pattern_drawer.hpp>

 namespace pl::ptrn { class Pattern; }

namespace hex::plugin::builtin {

    class PatternSourceCode {
    public:
        const std::string& get(prv::Provider *provider) const;
        std::string& get(prv::Provider *provider);
        [[nodiscard]] bool hasProviderSpecificSource(prv::Provider *provider) const;

        bool isSynced() const;
        void enableSync(bool enabled);

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
        std::unique_ptr<pl::PatternLanguage> *getPatternLanguage() {
            return &m_editorRuntime;
        }

        ui::TextEditor *getTextEditor() {
            auto provider = ImHexApi::Provider::get();
            if (provider == nullptr)
                return nullptr;

            return &m_textEditor.get(provider);
        }

        bool getChangesWereParsed() const {
            return m_changesWereParsed;
        }

        u32  getRunningParsers () const {
            return m_runningParsers;
        }

        u32  getRunningEvaluators () const {
            return m_runningEvaluators;
        }

        void setChangesWereParsed(bool changesWereParsed) {
            m_changesWereParsed = changesWereParsed;
        }

        void drawContent() override;

        void setPopupWindowHeight(u32 height) { m_popupWindowHeight = height; }
        u32 getPopupWindowHeight() const { return m_popupWindowHeight; }

        enum class DangerousFunctionPerms : u8 {
            Ask,
            Allow,
            Deny
        };

        void drawHelpText() override;

    private:
        class PopupAcceptPattern;

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
        PerProvider<std::vector<magic::FoundPattern>> m_possiblePatternFiles;
        bool m_runAutomatically   = false;
        bool m_triggerEvaluation  = false;
        std::atomic<bool> m_triggerAutoEvaluate = false;

        volatile bool m_lastEvaluationProcessed = true;
        bool m_lastEvaluationResult    = false;

        std::atomic<u32> m_runningEvaluators = 0;
        std::atomic<u32> m_runningParsers    = 0;

        bool m_changesWereParsed = false;
        PerProvider<bool> m_hasUnevaluatedChanges;
        std::chrono::time_point<std::chrono::steady_clock> m_lastEditorChangeTime;

        PerProvider<ui::TextEditor> m_textEditor, m_consoleEditor;
        std::atomic<bool> m_consoleNeedsUpdate = false;

        std::atomic<bool> m_dangerousFunctionCalled = false;
        std::atomic<DangerousFunctionPerms> m_dangerousFunctionsAllowed = DangerousFunctionPerms::Ask;

        bool m_suggestSupportedPatterns = true;
        bool m_autoApplyPatterns = false;

        PerProvider<ui::VisualizerDrawer> m_visualizerDrawer;
        bool m_tooltipJustOpened = false;

        PatternSourceCode m_sourceCode;
        PerProvider<std::vector<std::string>> m_console;
        PerProvider<bool> m_executionDone;

        std::mutex m_logMutex;

        PerProvider<ui::TextEditor::Coordinates>  m_cursorPosition;
        PerProvider<ImVec2> m_scroll;
        PerProvider<ImVec2> m_consoleScroll;

        PerProvider<ui::TextEditor::Coordinates> m_consoleCursorPosition;
        PerProvider<ui::TextEditor::Range> m_selection;
        PerProvider<ui::TextEditor::Range> m_consoleSelection;
        PerProvider<size_t> m_consoleLongestLineLength;
        PerProvider<ui::TextEditor::Breakpoints> m_breakpoints;
        PerProvider<std::optional<pl::core::err::PatternLanguageError>> m_lastEvaluationError;
        PerProvider<std::vector<pl::core::err::CompileError>> m_lastCompileError;
        PerProvider<const std::vector<pl::core::Evaluator::StackTrace>*> m_callStack;
        PerProvider<std::map<std::string, pl::core::Token::Literal>> m_lastEvaluationOutVars;
        PerProvider<std::map<std::string, PatternVariable>> m_patternVariables;


        PerProvider<std::list<EnvVar>> m_envVarEntries;

        PerProvider<TaskHolder> m_analysisTask;
        PerProvider<bool> m_shouldAnalyze;
        PerProvider<bool> m_breakpointHit;
        PerProvider<bool> m_debuggerActive;
        PerProvider<std::unique_ptr<ui::PatternDrawer>> m_debuggerDrawer;
        std::atomic<bool> m_resetDebuggerVariables;
        int m_debuggerScopeIndex = 0;

        std::array<AccessData, 512> m_accessHistory = {};
        u32 m_accessHistoryIndex = 0;
        bool m_parentHighlightingEnabled = true;
        bool m_replaceMode = false;
        bool m_openFindReplacePopUp = false;
        bool m_openGotoLinePopUp = false;
        bool m_patternEvaluating = false;
        std::map<std::fs::path, std::string> m_patternNames;
        PerProvider<wolv::io::ChangeTracker> m_changeTracker;
        PerProvider<bool> m_ignoreNextChangeEvent;
        PerProvider<bool> m_changeEventAcknowledgementPending;
        PerProvider<bool> m_patternFileDirty;

        ImRect m_textEditorHoverBox;
        ImRect m_consoleHoverBox;
        std::string m_focusedSubWindowName;
        float m_popupWindowHeight = 0;
        float m_popupWindowHeightChange = 0;
        bool m_frPopupIsClosed = true;
        bool m_gotoPopupIsClosed = true;

        static inline std::array<std::string,256> m_findHistory;
        static inline u32 m_findHistorySize = 0;
        static inline u32 m_findHistoryIndex = 0;
        static inline std::array<std::string,256> m_replaceHistory;
        static inline u32 m_replaceHistorySize = 0;
        static inline u32 m_replaceHistoryIndex = 0;

        TextHighlighter m_textHighlighter = TextHighlighter(this,&this->m_editorRuntime);
    private:
        void drawConsole(ImVec2 size);
        void drawDebugger(ImVec2 size);

        void drawPatternSettings();
        void drawEnvVars(std::list<EnvVar> &envVars);
        void drawVariableSettings(std::map<std::string, PatternVariable> &patternVariables);

        void drawPatternTooltip(pl::ptrn::Pattern *pattern);

        void drawTextEditorFindReplacePopup(ui::TextEditor *textEditor);
        void drawTextEditorGotoLinePopup(ui::TextEditor *textEditor);

        void historyInsert(std::array<std::string, 256> &history, u32 &size, u32 &index, const std::string &value);

        void loadPatternFile(const std::fs::path &path, prv::Provider *provider, bool trackFile = false);
        bool isPatternDirty(prv::Provider *provider) { return m_patternFileDirty.get(provider); }
        void markPatternFileDirty(prv::Provider *provider) { m_patternFileDirty.get(provider) = true; }

        void parsePattern(const std::string &code, prv::Provider *provider);
        void evaluatePattern(const std::string &code, prv::Provider *provider);

        ui::TextEditor *getEditorFromFocusedWindow();
        void setupFindReplace(ui::TextEditor *editor);
        void setupGotoLine(ui::TextEditor *editor);

        void registerEvents();
        void registerMenuItems();
        void registerHandlers();

        void fileChangedCallback(prv::Provider *provider, const std::fs::path &path);
        void handleFileChange(prv::Provider *provider, const std::fs::path &path);

        void openPatternFile(bool trackFile);
        void savePatternToCurrentFile(bool trackFile);
        void savePatternAsNewFile(bool trackFile);

        void appendEditorText(const std::string &text);
        void appendVariable(const std::string &type);
        void appendArray(const std::string &type, size_t size);
    };

}
