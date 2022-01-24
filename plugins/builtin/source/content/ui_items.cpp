#include <hex/api/content_registry.hpp>
#include <hex/helpers/shared_data.hpp>

#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>

#include <codicons_font.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <atomic>

namespace hex::plugin::builtin {

    void addFooterItems() {

        if (hex::isProcessElevated()) {
            ContentRegistry::Interface::addFooterItem([] {
                ImGui::TextUnformatted(ICON_VS_SHIELD);
            });
        }

        ContentRegistry::Interface::addFooterItem([] {
            static float framerate = 0;
            if (ImGui::HasSecondPassed()) {
                framerate = 1.0F / ImGui::GetIO().DeltaTime;
            }

            ImGui::TextFormatted("FPS {0:2}.{1:02}", u32(framerate), u32(framerate * 100) % 100);
        });

        ContentRegistry::Interface::addFooterItem([] {
            size_t taskCount = 0;
            double taskProgress = 0.0;
            std::string taskName;

            {
                std::scoped_lock lock(SharedData::tasksMutex);

                taskCount = SharedData::runningTasks.size();
                if (taskCount > 0) {
                    taskProgress = SharedData::runningTasks.front()->getProgress();
                    taskName = SharedData::runningTasks.front()->getName();
                }
            }

            if (taskCount > 0) {
                if (taskCount > 0)
                    ImGui::TextSpinner(hex::format("({})", taskCount).c_str());
                else
                    ImGui::TextSpinner("");

                ImGui::SameLine();

                ImGui::SmallProgressBar(taskProgress, (ImGui::GetCurrentWindow()->MenuBarHeight() - 10_scaled) / 2.0);
                ImGui::InfoTooltip(taskName.c_str());
            }
        });
    }

    void addToolbarItems() {
        ContentRegistry::Interface::addToolbarItem([] {
            auto provider = ImHexApi::Provider::get();

            // Undo
            ImGui::Disabled([&provider] {
                if (ImGui::ToolBarButton(ICON_VS_DISCARD, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue)))
                    provider->undo();
            },
                            !ImHexApi::Provider::isValid() || !provider->canUndo());

            // Redo
            ImGui::Disabled([&provider] {
                if (ImGui::ToolBarButton(ICON_VS_REDO, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue)))
                    provider->redo();
            },
                            !ImHexApi::Provider::isValid() || !provider->canRedo());


            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

            // Create new file
            if (ImGui::ToolBarButton(ICON_VS_FILE, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarGray)))
                EventManager::post<RequestOpenWindow>("Create File");

            // Open file
            if (ImGui::ToolBarButton(ICON_VS_FOLDER_OPENED, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBrown)))
                EventManager::post<RequestOpenWindow>("Open File");


            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

            // Save file
            ImGui::Disabled([&provider] {
                if (ImGui::ToolBarButton(ICON_VS_SAVE, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue)))
                    provider->save();
            },
                            !ImHexApi::Provider::isValid() || !provider->isWritable() || !provider->isSavable());

            // Save file as
            ImGui::Disabled([&provider] {
                if (ImGui::ToolBarButton(ICON_VS_SAVE_AS, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue)))
                    hex::openFileBrowser("hex.builtin.view.hexeditor.save_as"_lang, DialogMode::Save, {}, [&provider](auto path) {
                        provider->saveAs(path);
                    });
            },
                            !ImHexApi::Provider::isValid() || !provider->isSavable());


            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);


            // Create bookmark
            ImGui::Disabled([] {
                if (ImGui::ToolBarButton(ICON_VS_BOOKMARK, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarGreen))) {
                    Region region = { 0 };
                    EventManager::post<QuerySelection>(region);

                    ImHexApi::Bookmarks::add(region.address, region.size, {}, {});
                }
            },
                            !ImHexApi::Provider::isValid() || !provider->isReadable());


            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::Spacing();

            // Provider switcher
            ImGui::Disabled([] {
                auto &providers = ImHexApi::Provider::getProviders();

                std::string preview;
                if (ImHexApi::Provider::isValid())
                    preview = providers[SharedData::currentProvider]->getName();

                ImGui::SetNextItemWidth(200_scaled);
                if (ImGui::BeginCombo("", preview.c_str())) {

                    for (int i = 0; i < providers.size(); i++) {
                        if (ImGui::Selectable(providers[i]->getName().c_str())) {
                            SharedData::currentProvider = i;
                        }
                    }

                    ImGui::EndCombo();
                }
            },
                            !ImHexApi::Provider::isValid());
        });
    }

}