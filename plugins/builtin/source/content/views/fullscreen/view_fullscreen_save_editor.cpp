#include <content/views/fullscreen/view_fullscreen_save_editor.hpp>

#include <hex/api/content_registry/pattern_language.hpp>

#include <fonts/vscode_icons.hpp>
#include <toasts/toast_notification.hpp>
#include <popups/popup_question.hpp>

#include <wolv/utils/lock.hpp>
#include <pl/patterns/pattern.hpp>

#include <imgui_internal.h>

namespace hex::plugin::builtin {

    ViewFullScreenSaveEditor::ViewFullScreenSaveEditor(std::string sourceCode) : m_sourceCode(std::move(sourceCode)) {
        m_runtime.addPragma("name", [this](auto &, const std::string &value) -> bool {
            m_saveEditorName = wolv::util::trim(value);
            return true;
        });

        m_runtime.addPragma("author", [this](auto &, const std::string &value) -> bool {
            m_saveEditorAuthors.push_back(wolv::util::trim(value));
            return true;
        });

        m_runtime.addPragma("description", [this](auto &, const std::string &value) -> bool {
            m_saveEditorDescriptions.push_back(wolv::util::trim(value));
            return true;
        });

        std::ignore = m_runtime.parseString(m_sourceCode, pl::api::Source::DefaultSource);
    }


    void ViewFullScreenSaveEditor::drawContent() {
        if (!m_provider.isReadable()) {
            drawFileSelectScreen();
        } else {
            drawSaveEditorScreen();
        }
    }

    void ViewFullScreenSaveEditor::drawFileSelectScreen() {
        const auto windowSize = ImGui::GetWindowSize();
        const auto optionsWindowSize = ImVec2(windowSize.x * 2 / 3, 0);
        ImGui::NewLine();
        ImGui::SetCursorPosX((windowSize.x - optionsWindowSize.x) / 2.0F);
        if (ImGuiExt::BeginSubWindow(m_saveEditorName.empty() ? "hex.builtin.view.fullscreen.save_editor.name"_lang : m_saveEditorName.c_str(), nullptr, optionsWindowSize)) {
            for (const auto &author : m_saveEditorAuthors) {
                ImGui::TextUnformatted(author.c_str());
                ImGui::SameLine();
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                ImGui::SameLine();
            }
            ImGui::NewLine();
            ImGui::Separator();
            ImGui::NewLine();

            for (const auto &description : m_saveEditorDescriptions) {
                ImGui::TextWrapped("%s", description.c_str());
                ImGui::NewLine();
            }

            ImGui::NewLine();

            if (ImGuiExt::DimmedButton(fmt::format("{} {}", ICON_VS_OPEN_PREVIEW, "hex.builtin.view.fullscreen.save_editor.select_file"_lang).c_str(), ImVec2(-1, 0))) {
                fs::openFileBrowser(fs::DialogMode::Open, {}, [this](const std::fs::path &path) {
                    this->m_provider.setPath(path);
                    if (this->m_provider.open().isFailure()) {
                        ui::ToastError::open("hex.builtin.view.fullscreen.save_editor.error.not_readable"_lang);
                        return;
                    }

                    ContentRegistry::PatternLanguage::configureRuntime(m_runtime, &m_provider);
                    if (m_runtime.executeString(this->m_sourceCode) != 0) {
                        ui::ToastError::open("hex.builtin.view.fullscreen.save_editor.error.failed_execution"_lang);
                        for (const auto &error : m_runtime.getCompileErrors()) {
                            log::error("Save Editor Error: {}", error.format());
                        }

                        if (const auto &error = m_runtime.getEvalError(); error.has_value()) {
                            log::error("Evaluation Error: {}:{} | {}", error->line, error->column, error->message);
                        }
                    }
                });
            }

            ImGuiExt::EndSubWindow();
        }
    }

    void ViewFullScreenSaveEditor::drawSaveEditorScreen() {
        constexpr static auto SimplifiedEditorAttribute = "hex::editor_export";
        if (TRY_LOCK(ContentRegistry::PatternLanguage::getRuntimeLock()) && m_runtime.arePatternsValid()) {
            const auto &patternSet = m_runtime.getPatternsWithAttribute(SimplifiedEditorAttribute);
            std::vector<pl::ptrn::Pattern*> patterns = { patternSet.begin(), patternSet.end() };
            std::ranges::sort(patterns, [](const pl::ptrn::Pattern *a, const pl::ptrn::Pattern *b) {
                return a->getOffset() < b->getOffset() || a->getDisplayName() < b->getDisplayName();
            });

            ImGui::SameLine(ImGui::GetWindowSize().x - 75_scaled);
            if (ImGuiExt::DimmedIconButton(ICON_VS_SAVE_AS, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue))) {
                fs::openFileBrowser(fs::DialogMode::Save, {}, [this](const std::fs::path &path) {
                    this->m_provider.saveAs(path);
                    this->m_provider.close();
                    this->m_runtime.reset();
                });
            }
            ImGui::SameLine();
            if (ImGuiExt::DimmedIconButton(ICON_VS_CLOSE, ImGuiExt::GetCustomColorVec4(ImGuiCustomCol_ToolbarRed))) {
                ui::PopupQuestion::open("hex.builtin.view.fullscreen.save_editor.should_close"_lang, [this] {
                    this->m_provider.close();
                }, []{});
            }

            if (!patterns.empty()) {
                if (ImGui::BeginChild("##editor")) {
                    for (const auto &pattern : patterns) {
                        ImGui::PushID(pattern);
                        try {
                            const auto attribute = pattern->getAttributeArguments(SimplifiedEditorAttribute);

                            const auto name = !attribute.empty() ? attribute[0].toString() : pattern->getDisplayName();
                            const auto description = attribute.size() >= 2 ? attribute[1].toString() : pattern->getComment();

                            const auto widgetPos = 200_scaled;
                            ImGui::TextUnformatted(name.c_str());
                            ImGui::SameLine(0, 20_scaled);
                            if (ImGui::GetCursorPosX() < widgetPos)
                                ImGui::SetCursorPosX(widgetPos);

                            ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, 0);
                            ImGui::PushItemWidth(-50_scaled);
                            pattern->accept(m_saveEditor);
                            ImGui::PopItemWidth();
                            ImGui::PopStyleVar();

                            if (!description.empty()) {
                                ImGui::PushFont(nullptr, ImGui::GetFontSize() * 0.8F);
                                ImGui::BeginDisabled();
                                ImGui::Indent();
                                ImGui::TextWrapped("%s", description.c_str());
                                ImGui::Unindent();
                                ImGui::EndDisabled();
                                ImGui::PopFont();
                            }

                            ImGui::Separator();

                        } catch (const std::exception &e) {
                            ImGui::TextUnformatted(pattern->getDisplayName().c_str());
                            ImGui::TextUnformatted(e.what());
                        }

                        ImGui::PopID();
                    }

                    ImGui::EndChild();
                }
            }
        }
    }

}
