#include <hex/api/content_registry.hpp>

#include <hex/ui/view.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/api/localization.hpp>

#include <hex/providers/provider.hpp>

#include <codicons_font.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <atomic>

namespace hex::plugin::builtin {

    static void drawGlobalPopups() {

        // "Are you sure you want to exit?" Popup
        if (ImGui::BeginPopupModal("hex.builtin.popup.exit_application.title"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::NewLine();
            ImGui::TextUnformatted("hex.builtin.popup.exit_application.desc"_lang);
            ImGui::NewLine();

            View::confirmButtons(
                "hex.builtin.common.yes"_lang, "hex.builtin.common.no"_lang, [] { ImHexApi::Common::closeImHex(true); }, [] { ImGui::CloseCurrentPopup(); });

            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
    }

    void addGlobalUIItems() {
        EventManager::subscribe<EventFrameEnd>(drawGlobalPopups);
    }

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
            size_t taskCount    = 0;
            double taskProgress = 0.0;
            std::string taskName;

            {
                std::scoped_lock lock(Task::getTaskMutex());

                taskCount = Task::getRunningTasks().size();
                if (taskCount > 0) {
                    auto frontTask = Task::getRunningTasks().front();
                    taskProgress   = frontTask->getProgress();
                    taskName       = frontTask->getName();
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
            auto provider      = ImHexApi::Provider::get();
            bool providerValid = provider != nullptr;

            // Undo
            ImGui::Disabled([&provider] {
                if (ImGui::ToolBarButton(ICON_VS_DISCARD, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue)))
                    provider->undo();
            },
                !providerValid || !provider->canUndo());

            // Redo
            ImGui::Disabled([&provider] {
                if (ImGui::ToolBarButton(ICON_VS_REDO, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue)))
                    provider->redo();
            },
                !providerValid || !provider->canRedo());


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
                !providerValid || !provider->isWritable() || !provider->isSavable());

            // Save file as
            ImGui::Disabled([&provider] {
                if (ImGui::ToolBarButton(ICON_VS_SAVE_AS, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue)))
                    fs::openFileBrowser(fs::DialogMode::Save, {}, [&provider](auto path) {
                        provider->saveAs(path);
                    });
            },
                !providerValid || !provider->isSavable());


            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);


            // Create bookmark
            ImGui::Disabled([] {
                if (ImGui::ToolBarButton(ICON_VS_BOOKMARK, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarGreen))) {
                    auto region = ImHexApi::HexEditor::getSelection();

                    if (region.has_value())
                        ImHexApi::Bookmarks::add(region->address, region->size, {}, {});
                }
            },
                !providerValid || !provider->isReadable() || ImHexApi::HexEditor::isSelectionValid());


            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::Spacing();

            // Provider switcher
            ImGui::Disabled([] {
                auto &providers = ImHexApi::Provider::getProviders();

                std::string preview;
                if (ImHexApi::Provider::isValid())
                    preview = ImHexApi::Provider::get()->getName();

                ImGui::SetNextItemWidth(200_scaled);
                if (ImGui::BeginCombo("", preview.c_str())) {

                    for (size_t i = 0; i < providers.size(); i++) {
                        if (ImGui::Selectable(providers[i]->getName().c_str())) {
                            ImHexApi::Provider::setCurrentProvider(i);
                        }
                    }

                    ImGui::EndCombo();
                }
            },
                !providerValid);
        });
    }

}