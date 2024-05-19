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
