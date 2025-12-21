#include <content/views/fullscreen/view_fullscreen_file_info.hpp>
#include <fonts/fonts.hpp>

#include <hex/api/content_registry/pattern_language.hpp>
#include <hex/helpers/magic.hpp>
#include <hex/helpers/literals.hpp>

#include <pl/pattern_language.hpp>
#include <pl/core/evaluator.hpp>
#include <toasts/toast_notification.hpp>

namespace hex::plugin::builtin {

    using namespace hex::literals;

    ViewFullScreenFileInfo::ViewFullScreenFileInfo(std::fs::path filePath) : m_filePath(std::move(filePath)) {
        this->m_provider.setPath(m_filePath);
        if (this->m_provider.open().isFailure()) {
            ui::ToastError::open("hex.builtin.view.fullscreen.file_info.error.file_not_readable"_lang);
            return;
        }

        m_analysisTask = TaskManager::createBlockingTask("hex.builtin.view.fullscreen.file_info.analyzing", TaskManager::NoProgress, [this](Task &task) {
            m_mimeType = magic::getMIMEType(&m_provider);
            if (!magic::isValidMIMEType(m_mimeType)) {
                m_mimeType.clear();
            } else {
                m_fileDescription = magic::getDescription(&m_provider, 0, 100_KiB, true);
            }

            m_foundPatterns = magic::findViablePatterns(&m_provider, &task);
            if (!m_foundPatterns.empty()) {
                pl::PatternLanguage runtime;
                ContentRegistry::PatternLanguage::configureRuntime(runtime, &m_provider);

                constexpr static auto DataDescriptionFunction = "get_data_description";
                if (runtime.executeFile(m_foundPatterns.front().patternFilePath) == 0) {
                    const auto &evaluator = runtime.getInternals().evaluator;
                    const auto &functions = evaluator->getCustomFunctions();
                    if (const auto function = functions.find(DataDescriptionFunction); function != functions.end()) {
                        if (const auto value = function->second.func(evaluator.get(), {}); value.has_value()) {
                            if (value->isString()) {
                                m_fullDescription = ui::Markdown(value->toString());
                            }
                        }
                    }
                }
            }
        });

    }

    void ViewFullScreenFileInfo::drawContent() {
        if (!m_provider.isReadable()) {
            return;
        }

        if (m_analysisTask.isRunning()) {
            return;
        }

        const bool foundPatterns = !m_foundPatterns.empty();
        const bool hasMimeType = !m_mimeType.empty();
        if (!foundPatterns && !hasMimeType) {
            ImGuiExt::TextFormattedCentered("hex.builtin.view.fullscreen.file_info.error.not_identified"_lang);
            return;
        }

        if (foundPatterns) {
            const auto &firstMatch = m_foundPatterns.front();

            ImGui::BeginGroup();

            fonts::Default().pushBold(1.2F);
            ImGuiExt::TextFormattedCenteredHorizontal("{}", firstMatch.description);
            fonts::Default().pop();

            if (hasMimeType)
                ImGuiExt::TextFormattedCenteredHorizontal("{}", m_mimeType);
            if (!m_fileDescription.empty())
                ImGuiExt::TextFormattedCenteredHorizontal("{}", m_fileDescription);

            ImGui::EndGroup();

            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowSize().x - 300_scaled);

            if (ImGuiExt::BeginSubWindow("hex.builtin.view.fullscreen.file_info.match_info"_lang)) {
                if (firstMatch.mimeType.has_value())
                    ImGuiExt::TextFormattedWrapped("hex.builtin.view.fullscreen.file_info.match_info.mime"_lang);
                else if (firstMatch.magicOffset.has_value())
                    ImGuiExt::TextFormattedWrapped("hex.builtin.view.fullscreen.file_info.match_info.magic"_lang, *firstMatch.magicOffset);
            }
            ImGuiExt::EndSubWindow();
        } else {
            fonts::Default().pushBold(1.2F);
            ImGuiExt::TextFormattedCenteredHorizontal("{}", m_mimeType);
            fonts::Default().pop();
        }


        ImGui::NewLine();

        if (ImGuiExt::BeginSubWindow("hex.builtin.view.fullscreen.file_info.information"_lang, nullptr, ImGui::GetContentRegionAvail())) {
            if (m_fullDescription.has_value())
                m_fullDescription->draw();
            else
                ImGuiExt::TextFormattedCentered("hex.builtin.view.fullscreen.file_info.no_information"_lang);
        }
        ImGuiExt::EndSubWindow();
    }

}
