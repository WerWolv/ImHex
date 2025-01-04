#include "content/views/view_information.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/api/achievement_manager.hpp>
#include <hex/api/project_file_manager.hpp>

#include <hex/providers/provider.hpp>

#include <hex/helpers/magic.hpp>

#include <toasts/toast_notification.hpp>

namespace hex::plugin::builtin {

    using namespace hex::literals;

    ViewInformation::ViewInformation() : View::Window("hex.builtin.view.information.name", ICON_VS_GRAPH_LINE) {
        m_analysisData.setOnCreateCallback([](const prv::Provider *provider, AnalysisData &data) {
            data.analyzedProvider = provider;

            data.informationSections.clear();
            for (const auto &informationSectionConstructor : ContentRegistry::DataInformation::impl::getInformationSectionConstructors()) {
                data.informationSections.push_back(informationSectionConstructor());
            }
        });

        ProjectFile::registerPerProviderHandler({
            .basePath = "data_information.json",
            .required = false,
            .load = [this](prv::Provider *provider, const std::fs::path &basePath, const Tar &tar) {
                std::string save = tar.readString(basePath);
                nlohmann::json input = nlohmann::json::parse(save);

                for (const auto &section : m_analysisData.get(provider).informationSections) {
                    if (!input.contains(section->getUnlocalizedName().get()))
                        continue;

                    section->load(input[section->getUnlocalizedName().get()]);
                }

                return true;
            },
            .store = [this](prv::Provider *provider, const std::fs::path &basePath, const Tar &tar) {
                nlohmann::json output;
                for (const auto &section : m_analysisData.get(provider).informationSections) {
                    output[section->getUnlocalizedName().get()] = section->store();
                }

                tar.writeString(basePath, output.dump(4));

                return true;
            }
        });
    }

    void ViewInformation::analyze() {
        AchievementManager::unlockAchievement("hex.builtin.achievement.misc", "hex.builtin.achievement.misc.analyze_file.name");

        auto provider = ImHexApi::Provider::get();
        auto &analysis = m_analysisData.get(provider);

        // Reset all sections
        for (const auto &section : analysis.informationSections) {
            section->reset();
            section->markValid(false);
        }

        // Run analyzers for each section
        analysis.task = TaskManager::createTask("hex.builtin.view.information.analyzing"_lang, analysis.informationSections.size(), [provider, &analysis](Task &task) {
            u32 progress = 0;
            for (const auto &section : analysis.informationSections) {
                // Only process the section if it is enabled
                if (section->isEnabled()) {
                    // Set the section as analyzing so a spinner can be drawn
                    section->setAnalyzing(true);
                    ON_SCOPE_EXIT { section->setAnalyzing(false); };

                    try {
                        // Process the section
                        section->process(task, provider, analysis.analysisRegion);

                        // Mark the section as valid
                        section->markValid();
                    } catch (const std::exception &e) {
                        // Show a toast with the error if the section failed to process
                        ui::ToastError::open(hex::format("hex.builtin.view.information.error_processing_section"_lang, Lang(section->getUnlocalizedName()), e.what()));
                    }
                }

                // Update the task progress
                progress += 1;
                task.update(progress);
            }
        });
    }        

    void ViewInformation::drawContent() {
        if (ImGui::BeginChild("##scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav)) {

            auto provider = ImHexApi::Provider::get();
            if (ImHexApi::Provider::isValid() && provider->isReadable()) {
                auto &analysis = m_analysisData.get(provider);

                // Draw settings window
                ImGui::BeginDisabled(analysis.task.isRunning());
                if (ImGuiExt::BeginSubWindow("hex.ui.common.settings"_lang)) {
                    // Create a table so we can draw global settings on the left and section specific settings on the right
                    if (ImGui::BeginTable("SettingsTable", 2, ImGuiTableFlags_BordersInner | ImGuiTableFlags_SizingStretchProp, ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                        ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthStretch, 0.3F);
                        ImGui::TableSetupColumn("Right", ImGuiTableColumnFlags_WidthStretch, 0.7F);

                        ImGui::TableNextRow();

                        // Draw global settings
                        ImGui::TableNextColumn();
                        ui::regionSelectionPicker(&analysis.analysisRegion, provider, &analysis.selectionType, false);

                        // Draw analyzed section names
                        ImGui::TableNextColumn();
                        if (ImGui::BeginTable("AnalyzedSections", 1, ImGuiTableFlags_BordersInnerH, ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeightWithSpacing() * 5))) {
                            for (const auto &section : analysis.informationSections | std::views::reverse) {
                                if (section->isEnabled() && (section->isValid() || section->isAnalyzing())) {
                                    ImGui::TableNextRow();
                                    ImGui::TableNextColumn();

                                    ImGui::BeginDisabled();
                                    {
                                        ImGui::TextUnformatted(Lang(section->getUnlocalizedName()));

                                        if (section->isAnalyzing()) {
                                            ImGui::SameLine();
                                            ImGuiExt::TextSpinner("");
                                        }
                                    }
                                    ImGui::EndDisabled();
                                }
                            }

                            ImGui::EndTable();
                        }

                        ImGui::EndTable();
                    }
                    ImGui::NewLine();

                    // Draw the analyze button
                    ImGui::SetCursorPosX(50_scaled);
                    if (ImGuiExt::DimmedButton("hex.builtin.view.information.analyze"_lang, ImVec2(ImGui::GetContentRegionAvail().x - 50_scaled, 0)))
                        this->analyze();

                }
                ImGuiExt::EndSubWindow();
                ImGui::EndDisabled();

                if (analysis.analyzedProvider != nullptr) {
                    for (const auto &section : analysis.informationSections) {
                        ImGui::TableNextColumn();
                        ImGui::PushID(section.get());

                        bool enabled = section->isEnabled();

                        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0F);
                        if (ImGui::BeginChild(Lang(section->getUnlocalizedName()), ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_MenuBar)) {
                            if (ImGui::BeginMenuBar()) {

                                // Draw the enable checkbox of the section
                                // This is specifically split out so the checkbox does not get disabled when the section is disabled
                                ImGui::BeginGroup();
                                {
                                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y);
                                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                                    {
                                        if (ImGui::Checkbox("##enabled", &enabled)) {
                                            section->setEnabled(enabled);
                                        }
                                    }
                                    ImGui::PopStyleVar();
                                }
                                ImGui::EndGroup();

                                ImGui::SameLine();

                                // Draw the rest of the section header
                                ImGui::BeginDisabled(!enabled);
                                {
                                    ImGui::TextUnformatted(Lang(section->getUnlocalizedName()));
                                    ImGui::SameLine();
                                    if (auto description = section->getUnlocalizedDescription(); !description.empty()) {
                                        ImGui::SameLine();
                                        ImGuiExt::HelpHover(Lang(description), ICON_VS_INFO);
                                    }

                                    // Draw settings gear on the right
                                    if (section->hasSettings()) {
                                        ImGui::SameLine(0, ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(ICON_VS_SETTINGS_GEAR).x);
                                        if (ImGuiExt::DimmedIconButton(ICON_VS_SETTINGS_GEAR, ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                                            ImGui::OpenPopup("SectionSettings");
                                        }

                                        if (ImGui::BeginPopup("SectionSettings")) {
                                            ImGuiExt::Header("hex.ui.common.settings"_lang, true);
                                            section->drawSettings();
                                            ImGui::EndPopup();
                                        }
                                    }
                                }
                                ImGui::EndDisabled();

                                ImGui::EndMenuBar();
                            }

                            // Draw the section content
                            ImGui::BeginDisabled(!enabled);
                            if (section->isEnabled()) {
                                if (section->isValid())
                                    section->drawContent();
                                else if (section->isAnalyzing())
                                    ImGuiExt::TextSpinner("hex.builtin.view.information.analyzing"_lang);
                                else
                                    ImGuiExt::TextFormattedCenteredHorizontal("hex.builtin.view.information.not_analyzed"_lang);
                            }
                            ImGui::EndDisabled();
                        }
                        ImGui::EndChild();
                        ImGui::PopStyleVar();

                        ImGui::PopID();

                        ImGui::NewLine();
                    }
                }
            }
        }
        ImGui::EndChild();
    }

}
