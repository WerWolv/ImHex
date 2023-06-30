#include <hex/api/content_registry.hpp>
#include <hex/api/imhex_api.hpp>
#include <hex/api/localization.hpp>

#include <hex/ui/view.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>

#include <fonts/codicons_font.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <content/popups/popup_tasks_waiting.hpp>
#include <content/popups/popup_notification.hpp>

namespace hex::plugin::builtin {

    void addTitleBarButtons() {
#if defined(DEBUG)
        ContentRegistry::Interface::addTitleBarButton(ICON_VS_DEBUG, "hex.builtin.title_bar_button.debug_build", []{
            if (ImGui::GetIO().KeyCtrl) {
                // Explicitly trigger a segfault by writing to an invalid memory location
                // Used for debugging crashes
                *reinterpret_cast<u8 *>(0x10) = 0x10;
                std::unreachable();
            } else if (ImGui::GetIO().KeyShift) {
                // Explicitly trigger an abort by throwing an uncaught exception
                // Used for debugging exception errors
                throw std::runtime_error("Debug Error");
                std::unreachable();
            } else {
                hex::openWebpage("https://imhex.werwolv.net/debug");
            }
        });
#endif

        ContentRegistry::Interface::addTitleBarButton(ICON_VS_SMILEY, "hex.builtin.title_bar_button.feedback", []{
            hex::openWebpage("https://github.com/WerWolv/ImHex/discussions/categories/feedback");
        });

    }

    static void drawGlobalPopups() {
        // Task exception popup
        for (const auto &task : TaskManager::getRunningTasks()) {
            if (task->hadException()) {
                PopupError::open(hex::format("hex.builtin.popup.error.task_exception"_lang, LangEntry(task->getUnlocalizedName()), task->getExceptionMessage()));
                task->clearException();
                break;
            }
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

        #if defined(DEBUG)
            ContentRegistry::Interface::addFooterItem([] {
                static float framerate = 0;
                if (ImGui::HasSecondPassed()) {
                    framerate = 1.0F / ImGui::GetIO().DeltaTime;
                }

                ImGui::TextFormatted("FPS {0:2}.{1:02}", u32(framerate), u32(framerate * 100) % 100);
            });
        #endif

        ContentRegistry::Interface::addFooterItem([] {
            static bool shouldResetProgress = false;

            auto taskCount = TaskManager::getRunningTaskCount();
            if (taskCount > 0) {
                const auto &tasks = TaskManager::getRunningTasks();
                auto frontTask = tasks.front();

                auto progress = frontTask->getMaxValue() == 0 ? 1 : float(frontTask->getValue()) / frontTask->getMaxValue();

                ImHexApi::System::setTaskBarProgress(ImHexApi::System::TaskProgressState::Progress, ImHexApi::System::TaskProgressType::Normal, progress * 100);

                auto widgetStart = ImGui::GetCursorPos();

                ImGui::TextSpinner(hex::format("({})", taskCount).c_str());
                ImGui::SameLine();
                ImGui::SmallProgressBar(progress, (ImGui::GetCurrentWindow()->MenuBarHeight() - 10_scaled) / 2.0);
                ImGui::SameLine();

                auto widgetEnd = ImGui::GetCursorPos();
                ImGui::SetCursorPos(widgetStart);
                ImGui::InvisibleButton("FrontTask", ImVec2(widgetEnd.x - widgetStart.x, ImGui::GetCurrentWindow()->MenuBarHeight()));
                ImGui::SetCursorPos(widgetEnd);

                ImGui::InfoTooltip(LangEntry(frontTask->getUnlocalizedName()).get().c_str());

                if (ImGui::BeginPopupContextItem("FrontTask", ImGuiPopupFlags_MouseButtonLeft)) {
                    for (const auto &task : tasks) {
                        if (task->isBackgroundTask())
                            continue;

                        ImGui::PushID(&task);
                        ImGui::TextFormatted("{}", LangEntry(task->getUnlocalizedName()));
                        ImGui::SameLine();
                        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                        ImGui::SameLine();
                        ImGui::SmallProgressBar(frontTask->getMaxValue() == 0 ? 1 : (float(frontTask->getValue()) / frontTask->getMaxValue()), (ImGui::GetTextLineHeightWithSpacing() - 5_scaled) / 2);
                        ImGui::SameLine();

                        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                        if (ImGui::ToolBarButton(ICON_VS_DEBUG_STOP, ImGui::GetStyleColorVec4(ImGuiCol_Text)))
                            task->interrupt();
                        ImGui::PopStyleVar();

                        ImGui::PopID();
                    }
                    ImGui::EndPopup();
                }

                ImGui::SameLine();

                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, scaled(ImVec2(1, 2)));
                if (ImGui::ToolBarButton(ICON_VS_DEBUG_STOP, ImGui::GetStyleColorVec4(ImGuiCol_Text)))
                    frontTask->interrupt();
                ImGui::PopStyleVar();

                shouldResetProgress = true;
            } else {
                if (shouldResetProgress) {
                    ImHexApi::System::setTaskBarProgress(ImHexApi::System::TaskProgressState::Reset, ImHexApi::System::TaskProgressType::Normal, 0);
                    shouldResetProgress = false;
                }
            }
        });
    }

    void addToolbarItems() {
        ContentRegistry::Interface::addToolbarItem([] {
            auto provider      = ImHexApi::Provider::get();
            bool providerValid = provider != nullptr;
            bool tasksRunning  = TaskManager::getRunningTaskCount() > 0;

            // Undo
            ImGui::BeginDisabled(!providerValid || !provider->canUndo());
            {
                if (ImGui::ToolBarButton(ICON_VS_DISCARD, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue)))
                    provider->undo();
            }
            ImGui::EndDisabled();

            // Redo
            ImGui::BeginDisabled(!providerValid || !provider->canRedo());
            {
                if (ImGui::ToolBarButton(ICON_VS_REDO, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue)))
                    provider->redo();
            }
            ImGui::EndDisabled();

            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

            ImGui::BeginDisabled(tasksRunning);
            {
                // Create new file
                if (ImGui::ToolBarButton(ICON_VS_FILE, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarGray))) {
                    auto newProvider = hex::ImHexApi::Provider::createProvider("hex.builtin.provider.mem_file", true);
                    if (newProvider != nullptr && !newProvider->open())
                        hex::ImHexApi::Provider::remove(newProvider);
                    else
                        EventManager::post<EventProviderOpened>(newProvider);
                }

                // Open file
                if (ImGui::ToolBarButton(ICON_VS_FOLDER_OPENED, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBrown)))
                    EventManager::post<RequestOpenWindow>("Open File");
            }
            ImGui::EndDisabled();

            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

            // Save file
            ImGui::BeginDisabled(!providerValid || !provider->isWritable() || !provider->isSavable());
            {
                if (ImGui::ToolBarButton(ICON_VS_SAVE, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue)))
                    provider->save();
            }
            ImGui::EndDisabled();

            // Save file as
            ImGui::BeginDisabled(!providerValid || !provider->isSavable());
            {
                if (ImGui::ToolBarButton(ICON_VS_SAVE_AS, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarBlue)))
                    fs::openFileBrowser(fs::DialogMode::Save, {}, [&provider](auto path) {
                        provider->saveAs(path);
                    });
            }
            ImGui::EndDisabled();


            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);


            // Create bookmark
            ImGui::BeginDisabled(!providerValid || !provider->isReadable() || !ImHexApi::HexEditor::isSelectionValid());
            {
                if (ImGui::ToolBarButton(ICON_VS_BOOKMARK, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarGreen))) {
                    auto region = ImHexApi::HexEditor::getSelection();

                    if (region.has_value())
                        ImHexApi::Bookmarks::add(region->getStartAddress(), region->getSize(), {}, {});
                }
            }
            ImGui::EndDisabled();

            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            // Provider switcher
            ImGui::BeginDisabled(!providerValid || tasksRunning);
            {
                auto &providers = ImHexApi::Provider::getProviders();

                ImGui::PushStyleColor(ImGuiCol_TabActive, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
                ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
                auto providerSelectorVisible = ImGui::BeginTabBar("provider_switcher", ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs);
                ImGui::PopStyleColor(2);

                if (providerSelectorVisible) {
                    for (size_t i = 0; i < providers.size(); i++) {
                        auto &tabProvider = providers[i];

                        bool open = true;
                        ImGui::PushID(tabProvider);
                        if (ImGui::BeginTabItem(tabProvider->getName().c_str(), &open, ImGuiTabItemFlags_NoTooltip | (tabProvider->isDirty() ? ImGuiTabItemFlags_UnsavedDocument : ImGuiTabItemFlags_None))) {
                            ImHexApi::Provider::setCurrentProvider(i);
                            ImGui::EndTabItem();
                        }

                        if (ImGui::InfoTooltip()) {
                            ImGui::BeginTooltip();

                            ImGui::TextFormatted("{}", tabProvider->getName().c_str());

                            const auto &description = provider->getDataDescription();
                            if (!description.empty()) {
                                ImGui::Separator();
                                if (ImGui::GetIO().KeyShift && !description.empty()) {

                                    if (ImGui::BeginTable("information", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoKeepColumnsVisible, ImVec2(400_scaled, 0))) {
                                        ImGui::TableSetupColumn("type");
                                        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                                        ImGui::TableNextRow();

                                        for (auto &[name, value] : description) {
                                            ImGui::TableNextColumn();
                                            ImGui::TextFormatted("{}", name);
                                            ImGui::TableNextColumn();
                                            ImGui::TextFormattedWrapped("{}", value);
                                        }

                                        ImGui::EndTable();
                                    }
                                } else {
                                    ImGui::TextFormattedDisabled("hex.builtin.provider.tooltip.show_more"_lang);
                                }
                            }

                            ImGui::EndTooltip();
                        }

                        ImGui::PopID();

                        if (!open) {
                            ImHexApi::Provider::remove(providers[i]);
                            break;
                        }

                        std::string popupID = std::string("ProviderMenu.") + std::to_string(tabProvider->getID());
                        if (ImGui::IsMouseReleased(1) && ImGui::IsItemHovered()) {
                            ImGui::OpenPopup(popupID.c_str());
                        }

                        if (ImGui::BeginPopup(popupID.c_str())) {
                            for (const auto &menuEntry : tabProvider->getMenuEntries()) {
                                if (ImGui::MenuItem(menuEntry.name.c_str())) {
                                    menuEntry.callback();
                                }
                            }
                            ImGui::EndPopup();
                        }
                    }
                    ImGui::EndTabBar();
                }
            }
            ImGui::EndDisabled();
        });
    }

    void handleBorderlessWindowMode() {
        // Intel's OpenGL driver has weird bugs that cause the drawn window to be offset to the bottom right.
        // This can be fixed by either using Mesa3D's OpenGL Software renderer or by simply disabling it.
        // If you want to try if it works anyways on your GPU, set the hex.builtin.setting.interface.force_borderless_window_mode setting to 1
        if (ImHexApi::System::isBorderlessWindowModeEnabled()) {
            bool isIntelGPU = hex::containsIgnoreCase(ImHexApi::System::getGPUVendor(), "Intel");
            ImHexApi::System::impl::setBorderlessWindowMode(!isIntelGPU);

            if (isIntelGPU)
                log::warn("Intel GPU detected! Intel's OpenGL driver has bugs that can cause issues when using ImHex. If you experience any rendering bugs, please try the Mesa3D Software Renderer");
        }
    }

}