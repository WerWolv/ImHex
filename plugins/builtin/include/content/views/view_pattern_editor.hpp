 #pragma once

#include <hex/api/achievement_manager.hpp>
#include <hex/ui/view.hpp>
#include <hex/ui/popup.hpp>
#include <hex/providers/provider.hpp>
#include <hex/helpers/default_paths.hpp>

#include <pl/pattern_language.hpp>
#include <pl/core/errors/error.hpp>

#include <ui/hex_editor.hpp>
#include <ui/pattern_drawer.hpp>
#include <ui/visualizer_drawer.hpp>

#include <filesystem>
#include <functional>

#include <TextEditor.h>
#include <popups/popup_file_chooser.hpp>

namespace pl::ptrn { class Pattern; }

namespace hex::plugin::builtin {


    constexpr static auto textEditorView    = "/Pattern editor_";
    constexpr static auto consoleView       = "/##console_";
    constexpr static auto variablesView     = "/##env_vars_";
    constexpr static auto settingsView      = "/##settings_";
    constexpr static auto sectionsView      = "/##sections_table_";
    constexpr static auto virtualFilesView  = "/Virtual File Tree_";
    constexpr static auto debuggerView      = "/##debugger_";

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

        void setPopupWindowHeight(u32 height) { m_popupWindowHeight = height; }
        u32 getPopupWindowHeight() const { return m_popupWindowHeight; }

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

    private:
        class PopupAcceptPattern : public Popup<PopupAcceptPattern> {
        public:
            explicit PopupAcceptPattern(ViewPatternEditor *view) : Popup("hex.builtin.view.pattern_editor.accept_pattern"), m_view(view) {}

            void drawContent() override {
                std::scoped_lock lock(m_view->m_possiblePatternFilesMutex);

                auto* provider = ImHexApi::Provider::get();

                ImGuiExt::TextFormattedWrapped("{}", static_cast<const char *>("hex.builtin.view.pattern_editor.accept_pattern.desc"_lang));

                if (ImGui::BeginListBox("##patterns_accept", ImVec2(400_scaled, 0))) {
                    u32 index = 0;
                    for (const auto &[path, author, description] : m_view->m_possiblePatternFiles.get(provider)) {
                        auto fileName = wolv::util::toUTF8String(path.filename());

                        std::string displayValue;
                        if (!description.empty()) {
                            displayValue = fmt::format("{} ({})", description, fileName);
                        } else {
                            displayValue = fileName;
                        }

                        if (ImGui::Selectable(displayValue.c_str(), index == m_selectedPatternFile, ImGuiSelectableFlags_NoAutoClosePopups))
                            m_selectedPatternFile = index;

                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_Stationary | ImGuiHoveredFlags_DelayNormal)) {
                            if (ImGui::BeginTooltip()) {
                                ImGui::TextUnformatted(fileName.c_str());

                                if (!author.empty()) {
                                    ImGui::SameLine();
                                    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                                    ImGui::SameLine();
                                    ImGui::TextUnformatted(author.c_str());
                                }

                                if (!description.empty()) {
                                    ImGui::Separator();
                                    ImGui::TextUnformatted(description.c_str());
                                }

                                ImGui::EndTooltip();
                            }
                        }

                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                            m_view->loadPatternFile(m_view->m_possiblePatternFiles.get(provider)[m_selectedPatternFile].path, provider);

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
                ImGui::NewLine();

                ImGuiExt::ConfirmButtons("hex.ui.common.yes"_lang, "hex.ui.common.no"_lang,
                    [this, provider] {
                        m_view->loadPatternFile(m_view->m_possiblePatternFiles.get(provider)[m_selectedPatternFile].path, provider);
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

        struct PossiblePattern {
            std::fs::path path;
            std::string author;
            std::string description;
        };

        std::unique_ptr<pl::PatternLanguage> m_editorRuntime;

        std::mutex m_possiblePatternFilesMutex;
        PerProvider<std::vector<PossiblePattern>> m_possiblePatternFiles;
        bool m_runAutomatically   = false;
        bool m_triggerEvaluation  = false;
        std::atomic<bool> m_triggerAutoEvaluate = false;

        volatile bool m_lastEvaluationProcessed = true;
        bool m_lastEvaluationResult    = false;

        std::atomic<u32> m_runningEvaluators = 0;
        std::atomic<u32> m_runningParsers    = 0;

        bool m_hasUnevaluatedChanges = false;
        std::chrono::time_point<std::chrono::steady_clock> m_lastEditorChangeTime;

        TextEditor m_textEditor, m_consoleEditor;
        std::atomic<bool> m_consoleNeedsUpdate = false;

        std::atomic<bool> m_dangerousFunctionCalled = false;
        std::atomic<DangerousFunctionPerms> m_dangerousFunctionsAllowed = DangerousFunctionPerms::Ask;

        bool m_autoLoadPatterns = true;

        std::map<prv::Provider*, std::function<void()>> m_sectionWindowDrawer;

        ui::HexEditor m_sectionHexEditor;
        PerProvider<ui::VisualizerDrawer> m_visualizerDrawer;
        bool m_tooltipJustOpened = false;

        PatternSourceCode m_sourceCode;
        PerProvider<std::vector<std::string>> m_console;
        PerProvider<bool> m_executionDone;

        std::mutex m_logMutex;

        PerProvider<TextEditor::Coordinates>  m_cursorPosition;

        PerProvider<TextEditor::Coordinates> m_consoleCursorPosition;
        PerProvider<TextEditor::Selection> m_selection;
        PerProvider<TextEditor::Selection> m_consoleSelection;
        PerProvider<TextEditor::Breakpoints> m_breakpoints;
        PerProvider<std::optional<pl::core::err::PatternLanguageError>> m_lastEvaluationError;
        PerProvider<std::vector<pl::core::err::CompileError>> m_lastCompileError;
        PerProvider<const std::vector<std::unique_ptr<pl::core::ast::ASTNode>>*> m_callStack;
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
        int m_debuggerScopeIndex = 0;

        std::array<AccessData, 512> m_accessHistory = {};
        u32 m_accessHistoryIndex = 0;
        bool m_parentHighlightingEnabled = true;
        bool m_replaceMode = false;
        bool m_openFindReplacePopUp = false;
        bool m_openGotoLinePopUp = false;
        std::map<std::fs::path, std::string> m_patternNames;

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

    private:
        void drawConsole(ImVec2 size);
        void drawEnvVars(ImVec2 size, std::list<EnvVar> &envVars);
        void drawVariableSettings(ImVec2 size, std::map<std::string, PatternVariable> &patternVariables);
        void drawSectionSelector(ImVec2 size, const std::map<u64, pl::api::Section> &sections);
        void drawVirtualFiles(ImVec2 size, const std::vector<VirtualFile> &virtualFiles) const;
        void drawDebugger(ImVec2 size);

        void drawPatternTooltip(pl::ptrn::Pattern *pattern);

        void drawTextEditorFindReplacePopup(TextEditor *textEditor);
        void drawTextEditorGotoLinePopup(TextEditor *textEditor);

        void historyInsert(std::array<std::string, 256> &history, u32 &size, u32 &index, const std::string &value);

        void loadPatternFile(const std::fs::path &path, prv::Provider *provider);

        void parsePattern(const std::string &code, prv::Provider *provider);
        void evaluatePattern(const std::string &code, prv::Provider *provider);

        TextEditor *getEditorFromFocusedWindow();
        void setupFindReplace(TextEditor *editor);
        void setupGotoLine(TextEditor *editor);

        void registerEvents();
        void registerMenuItems();
        void registerHandlers();

        std::function<void()> m_importPatternFile = [this] {
            auto provider = ImHexApi::Provider::get();
            const auto basePaths = paths::Patterns.read();
            std::vector<std::fs::path> paths;

            for (const auto &imhexPath : basePaths) {
                if (!wolv::io::fs::exists(imhexPath)) continue;

                std::error_code error;
                for (auto &entry : std::fs::recursive_directory_iterator(imhexPath, error)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".hexpat")
                        paths.push_back(entry.path());
                }
            }

            ui::PopupNamedFileChooser::open(
                basePaths, paths, std::vector<hex::fs::ItemFilter>{ { "Pattern File", "hexpat" } }, false,
                [this, provider](const std::fs::path &path, const std::fs::path &adjustedPath) mutable -> std::string {
                    auto it = m_patternNames.find(path);
                    if (it != m_patternNames.end()) {
                        return it->second;
                    }

                    const auto fileName = wolv::util::toUTF8String(adjustedPath.filename());
                    m_patternNames[path] = fileName;

                    pl::PatternLanguage runtime;
                    ContentRegistry::PatternLanguage::configureRuntime(runtime, provider);
                    runtime.addPragma("description", [&](pl::PatternLanguage &, const std::string &value) -> bool {
                        m_patternNames[path] = hex::format("{} ({})", value, fileName);
                        return true;
                    });

                    wolv::io::File file(path, wolv::io::File::Mode::Read);
                    std::ignore = runtime.preprocessString(file.readString(), pl::api::Source::DefaultSource);

                    return m_patternNames[path];
                },
                [this, provider](const std::fs::path &path) {
                    this->loadPatternFile(path, provider);
                    AchievementManager::unlockAchievement("hex.builtin.achievement.patterns", "hex.builtin.achievement.patterns.load_existing.name");
                }
            );
        };

        std::function<void()> m_exportPatternFile = [this] {
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
