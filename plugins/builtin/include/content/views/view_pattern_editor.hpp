#pragma once

#include <hex/ui/view.hpp>
#include <hex/providers/provider.hpp>
#include <hex/ui/popup.hpp>

#include <pl/pattern_language.hpp>
#include <pl/core/errors/error.hpp>

#include <content/helpers/provider_extra_data.hpp>
#include <content/providers/memory_file_provider.hpp>

#include <ui/hex_editor.hpp>
#include <ui/pattern_drawer.hpp>


#include <cstring>
#include <filesystem>
#include <string_view>
#include <thread>
#include <vector>
#include <functional>

#include <TextEditor.h>

namespace pl::ptrn { class Pattern; }

namespace hex::plugin::builtin {

    class ViewPatternEditor : public View {
    public:
        ViewPatternEditor();
        ~ViewPatternEditor() override;

        void drawAlwaysVisible() override;
        void drawContent() override;

    private:
        enum class DangerousFunctionPerms : u8 {
            Ask,
            Allow,
            Deny
        };

        class PopupAcceptPattern : public Popup<PopupAcceptPattern> {
        public:
            PopupAcceptPattern(ViewPatternEditor *view) : Popup("hex.builtin.view.pattern_editor.accept_pattern"), m_view(view) {}

            void drawContent() override {
                ImGui::TextFormattedWrapped("{}", static_cast<const char *>("hex.builtin.view.pattern_editor.accept_pattern.desc"_lang));

                std::vector<std::string> entries;
                entries.resize(this->m_view->m_possiblePatternFiles.size());

                for (u32 i = 0; i < entries.size(); i++) {
                    entries[i] = wolv::util::toUTF8String(this->m_view->m_possiblePatternFiles[i].filename());
                }

                if (ImGui::BeginListBox("##patterns_accept", ImVec2(-FLT_MIN, 0))) {
                    u32 index = 0;
                    for (auto &path : this->m_view->m_possiblePatternFiles) {
                        if (ImGui::Selectable(wolv::util::toUTF8String(path.filename()).c_str(), index == this->m_view->m_selectedPatternFile))
                            this->m_view->m_selectedPatternFile = index;
                        index++;
                    }

                    ImGui::EndListBox();
                }

                ImGui::NewLine();
                ImGui::TextUnformatted("hex.builtin.view.pattern_editor.accept_pattern.question"_lang);

                auto provider = ImHexApi::Provider::get();

                confirmButtons(
                        "hex.builtin.common.yes"_lang, "hex.builtin.common.no"_lang, [this, provider] {
                            this->m_view->loadPatternFile(this->m_view->m_possiblePatternFiles[this->m_view->m_selectedPatternFile], provider);
                            Popup::close();
                        },
                        [] {
                            Popup::close();
                        }
                );

                if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                    Popup::close();
            }

            [[nodiscard]] ImGuiWindowFlags getFlags() const override {
                return ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize;
            }

        private:
            ViewPatternEditor *m_view;
        };

    private:
        using PlData = ProviderExtraData::Data::PatternLanguage;

        std::unique_ptr<pl::PatternLanguage> m_parserRuntime;

        std::vector<std::fs::path> m_possiblePatternFiles;
        u32 m_selectedPatternFile = 0;
        bool m_runAutomatically   = false;
        bool m_triggerEvaluation  = false;

        bool m_lastEvaluationProcessed = true;
        bool m_lastEvaluationResult    = false;

        std::atomic<u32> m_runningEvaluators = 0;
        std::atomic<u32> m_runningParsers    = 0;

        bool m_hasUnevaluatedChanges = false;

        bool m_acceptPatternWindowOpen = false;

        TextEditor m_textEditor;

        std::atomic<bool> m_dangerousFunctionCalled = false;
        std::atomic<DangerousFunctionPerms> m_dangerousFunctionsAllowed = DangerousFunctionPerms::Ask;

        bool m_syncPatternSourceCode = false;
        bool m_autoLoadPatterns = true;

        std::map<prv::Provider*, std::move_only_function<void()>> m_sectionWindowDrawer;

        ui::HexEditor m_sectionHexEditor;

    private:
        void drawConsole(ImVec2 size, const std::vector<std::pair<pl::core::LogConsole::Level, std::string>> &console);
        void drawEnvVars(ImVec2 size, std::list<PlData::EnvVar> &envVars);
        void drawVariableSettings(ImVec2 size, std::map<std::string, PlData::PatternVariable> &patternVariables);
        void drawSectionSelector(ImVec2 size, std::map<u64, pl::api::Section> &sections);

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
