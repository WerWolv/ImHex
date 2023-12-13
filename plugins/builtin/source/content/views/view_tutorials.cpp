#include "content/views/view_tutorials.hpp"

#include <hex/api/content_registry.hpp>
#include <hex/api/tutorial_manager.hpp>

#include <ranges>

namespace hex::plugin::builtin {

    ViewTutorials::ViewTutorials() : View::Floating("hex.builtin.view.tutorials.name") {
        ContentRegistry::Interface::addMenuItem({ "hex.builtin.menu.help", "hex.builtin.view.tutorials.name" }, 4000, Shortcut::None, [&, this] {
            this->getWindowOpenState() = true;
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
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    if (ImGui::Selectable(Lang(tutorial.getUnlocalizedName()), this->m_selectedTutorial == &tutorial, ImGuiSelectableFlags_SpanAllColumns)) {
                        this->m_selectedTutorial = &tutorial;
                    }
                }

                ImGui::EndTable();
            }

            ImGui::TableNextColumn();

            if (this->m_selectedTutorial != nullptr) {
                ImGuiExt::BeginSubWindow("hex.builtin.view.tutorials.description"_lang, ImGui::GetContentRegionAvail() - ImVec2(0, ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y * 2));
                {
                    ImGuiExt::TextFormattedWrapped(Lang(this->m_selectedTutorial->getUnlocalizedDescription()));
                }
                ImGuiExt::EndSubWindow();

                ImGui::BeginDisabled(currTutorial != tutorials.end());
                if (ImGuiExt::DimmedButton("hex.builtin.view.tutorials.start"_lang, ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                    TutorialManager::startTutorial(this->m_selectedTutorial->getUnlocalizedName());
                }
                ImGui::EndDisabled();
            }

            ImGui::EndTable();
        }
    }

}