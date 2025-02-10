#include <imgui.h>
#include <imgui_internal.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <fonts/vscode_icons.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/task_manager.hpp>

#include <hex/project/project.hpp>
#include <hex/project/project_manager.hpp>
#include <hex/helpers/utils.hpp>

namespace hex::plugin::builtin {

    namespace {

        void drawContent(proj::Content &content) {
            static proj::Content *rightClickedContent = nullptr;
            static proj::Content *renamingContent = nullptr;
            static std::string renameText;

            ImGui::TableNextColumn();

            if (renamingContent != &content) {
                ImGui::Selectable(content.getName().c_str(), content.isOpen(), ImGuiSelectableFlags_SpanAllColumns);
            } else {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2());
                if (ImGui::InputText("##ContentName", renameText)) {
                    renamingContent->setName(renameText);
                }
                ImGui::SetKeyboardFocusHere(-1);
                ImGui::PopStyleVar();

                if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
                    renamingContent->setName(renameText);
                    renamingContent = nullptr;
                }
                if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                    renamingContent = nullptr;
                }
            }

            if (ImGui::IsItemHovered()) {
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    proj::ProjectManager::loadContent(content);
                }
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                    rightClickedContent = &content;
                    ImGui::OpenPopup("ContentContextMenu");
                }
            } else {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && renamingContent == &content)
                    renamingContent = nullptr;
            }

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(Lang(content.getType()));

            if (rightClickedContent == &content) {
                if (ImGui::BeginPopup("ContentContextMenu")) {
                    if (ImGui::MenuItemEx("Open", ICON_VS_OPEN_PREVIEW)) {
                        proj::ProjectManager::loadContent(content);
                    }
                    if (ImGui::MenuItemEx("Rename", ICON_VS_DIFF_RENAMED)) {
                        renamingContent = &content;
                        renameText = content.getName();
                    }
                    ImGui::EndPopup();
                }
            }
        }

        void drawProject(proj::Project &project) {
            static proj::Project *rightClickedProject = nullptr;

            bool open = ImGui::TreeNodeEx(project.getName().c_str(), ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAllColumns);

            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                rightClickedProject = &project;
                ImGui::OpenPopup("ProjectContextMenu");
            }
            if (rightClickedProject == &project) {
                if (ImGui::BeginPopup("ProjectContextMenu")) {
                    if (ImGui::BeginMenuEx("Add", ICON_VS_FILE_ADD)) {
                        for (const auto &handler : proj::ProjectManager::getContentHandlers()) {
                            if (ImGui::MenuItem(Lang(handler.type))) {
                                rightClickedProject->addContent(handler.type);
                            }
                        }
                        ImGui::EndMenu();
                    }

                    if (ImGui::MenuItemEx("Close", ICON_VS_CLOSE)) {
                        TaskManager::doLater([project = rightClickedProject] {
                            proj::ProjectManager::removeProject(*project);
                        });
                    }
                    ImGui::EndPopup();
                }
            }

            if (open) {
                for (const auto &content : project.getContents()) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    ImGui::PushID(content.get());

                    drawContent(*content);

                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
        }

    }

    void registerProjectExplorer() {
        ContentRegistry::Interface::addSidebarItem(ICON_VS_PROJECT, [] {
            if (ImGui::BeginTable("Projects", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
                ImGui::TableSetupColumn("##Icon", ImGuiTableColumnFlags_WidthFixed, 20_scaled);
                ImGui::TableSetupColumn("##Name", ImGuiTableColumnFlags_WidthStretch, 20_scaled);
                ImGui::TableSetupColumn("##Type", ImGuiTableColumnFlags_WidthFixed, 100_scaled);
                for (auto &project : proj::ProjectManager::getProjects()) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();

                    ImGui::PushID(project.get());
                    drawProject(*project);
                    ImGui::PopID();

                    ImGui::NewLine();
                }
                ImGui::EndTable();
            }
        });

        proj::ProjectManager::createProject("Project 1");
        proj::ProjectManager::createProject("Project 2");
        proj::ProjectManager::createProject("Free Items");
    }

}