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

                if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape)))
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

        std::unique_ptr<pl::PatternLanguage> m_parserRuntime;

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
        PerProvider<std::map<std::string, pl::core::Token::Literal>> m_lastEvaluationOutVars;
        PerProvider<std::map<std::string, PatternVariable>> m_patternVariables;
        PerProvider<std::map<u64, pl::api::Section>> m_sections;

        PerProvider<std::vector<VirtualFile>> m_virtualFiles;

        PerProvider<std::list<EnvVar>> m_envVarEntries;

        PerProvider<bool> m_shouldAnalyze;
        PerProvider<bool> m_breakpointHit;
        PerProvider<std::unique_ptr<ui::PatternDrawer>> m_debuggerDrawer;
        std::atomic<bool> m_resetDebuggerVariables;
        int m_debuggerScopeIndex = 0;

        std::array<AccessData, 512> m_accessHistory = {};
        u32 m_accessHistoryIndex = 0;

    private:
        void drawConsole(ImVec2 size);
        void drawEnvVars(ImVec2 size, std::list<EnvVar> &envVars);
        void drawVariableSettings(ImVec2 size, std::map<std::string, PatternVariable> &patternVariables);
        void drawSectionSelector(ImVec2 size, const std::map<u64, pl::api::Section> &sections);
        void drawVirtualFiles(ImVec2 size, const std::vector<VirtualFile> &virtualFiles) const;
        void drawDebugger(ImVec2 size);

        void drawPatternTooltip(pl::ptrn::Pattern *pattern);

        void loadPatternFile(const std::fs::path &path, prv::Provider *provider);

        void parsePattern(const std::string &code, prv::Provider *provider);
        void evaluatePattern(const std::string &code, prv::Provider *provider);

        void registerEvents();
        void registerMenuItems();
        void registerHandlers();

        void appendEditorText(const std::string &text);
        void appendVariable(const std::string &type);
        void appendArray(const std::string &type, size_t size);
    };

}
