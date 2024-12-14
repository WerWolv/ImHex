#include "content/views/view_tutorials.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/api/tutorial_manager.hpp>
#include <hex/api/task_manager.hpp>

#include <fonts/vscode_icons.hpp>

#include <ranges>

namespace hex::plugin::builtin {

    ViewTutorials::ViewTutorials() : View::Floating("hex.builtin.view.tutorials.name") {
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.help", "hex.builtin.view.tutorials.name" }, ICON_VS_COMPASS, 4000, Shortcut::None, [&, this] {
            this->getWindowOpenState() = true;
        });

        RequestOpenWindow::subscribe(this, [this](const std::string &name) {
            if (name == "Tutorials") {
                TaskManager::doLater([this] {
                    this->getWindowOpenState() = true;
                });
            }
        });
    }


    void ViewTutorials::drawContent() {
        const auto& tutorials = TutorialManager::getTutorials();
        const auto& currTutorial = TutorialManager::getCurrentTutorial();

        if (ImGui::BeginTable("TutorialLayout", 2, ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.3F);
            ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch, 0.7F);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            if (ImGui::BeginTable("Tutorials", 1, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg, ImGui::GetContentRegionAvail())) {
                for (const auto &tutorial : tutorials | std::views::values) {
                    if (m_selectedTutorial == nullptr)
                        m_selectedTutorial = &tutorial;

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    if (ImGui::Selectable(Lang(tutorial.getUnlocalizedName()), m_selectedTutorial == &tutorial, ImGuiSelectableFlags_SpanAllColumns)) {
                        m_selectedTutorial = &tutorial;
                    }
                }

                ImGui::EndTable();
            }

            ImGui::TableNextColumn();

            if (m_selectedTutorial != nullptr) {
                if (ImGuiExt::BeginSubWindow("hex.builtin.view.tutorials.description"_lang, nullptr, ImGui::GetContentRegionAvail() - ImVec2(0, ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y * 2))) {
                    ImGuiExt::TextFormattedWrapped(Lang(m_selectedTutorial->getUnlocalizedDescription()));
                }
                ImGuiExt::EndSubWindow();

                ImGui::BeginDisabled(currTutorial != tutorials.end());
                if (ImGuiExt::DimmedButton("hex.builtin.view.tutorials.start"_lang, ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                    TutorialManager::startTutorial(m_selectedTutorial->getUnlocalizedName());
                    this->getWindowOpenState() = false;
                }
                ImGui::EndDisabled();
            }

            ImGui::EndTable();
        }
    }

}