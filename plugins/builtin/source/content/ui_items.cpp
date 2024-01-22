#include <hex/api/content_registry.hpp>
#include <hex/api/imhex_api.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/task_manager.hpp>

#include <hex/ui/view.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>

#include <fonts/codicons_font.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <toasts/toast_notification.hpp>

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
        // Task exception toast
        for (const auto &task : TaskManager::getRunningTasks()) {
            if (task->hadException()) {
                ui::ToastError::open(hex::format("hex.builtin.popup.error.task_exception"_lang, Lang(task->getUnlocalizedName()), task->getExceptionMessage()));
                task->clearException();
                break;
            }
        }
    }

    static bool s_drawDragDropOverlay = false;
    static void drawDragNDropOverlay() {
        if (!s_drawDragDropOverlay)
            return;

        auto drawList = ImGui::GetForegroundDrawList();

        drawList->PushClipRectFullScreen();
        {
            const auto windowPos = ImHexApi::System::getMainWindowPosition();
            const auto windowSize = ImHexApi::System::getMainWindowSize();
            const auto center = windowPos + (windowSize / 2.0F) - scaled({ 0, 50 });

            // Draw background
            {
                const ImVec2 margin = scaled({ 15, 15 });
                drawList->AddRectFilled(windowPos, windowPos + windowSize, ImGui::GetColorU32(ImGuiCol_WindowBg, 200.0/255.0));
                drawList->AddRect(windowPos + margin, (windowPos + windowSize) - margin, ImGuiExt::GetCustomColorU32(ImGuiCustomCol_Highlight), 10_scaled, ImDrawFlags_None, 7.5_scaled);
            }

            // Draw drag n drop icon
            {
                const ImVec2 iconSize = scaled({ 64, 64 });
                const auto offset = scaled({ 15, 15 });
                const auto margin = scaled({ 20, 20 });

                const auto text = "hex.builtin.drag_drop.text"_lang;
                const auto textSize = ImGui::CalcTextSize(text);

                drawList->AddShadowRect(center - ImVec2(textSize.x, iconSize.y + 40_scaled) / 2.0F - offset - margin, center + ImVec2(textSize.x, iconSize.y + 75_scaled) / 2.0F + offset + ImVec2(0, textSize.y) + margin, ImGui::GetColorU32(ImGuiCol_WindowShadow), 20_scaled, ImVec2(), ImDrawFlags_None, 10_scaled);
                drawList->AddRectFilled(center - ImVec2(textSize.x, iconSize.y + 40_scaled) / 2.0F - offset - margin, center + ImVec2(textSize.x, iconSize.y + 75_scaled) / 2.0F + offset + ImVec2(0, textSize.y) + margin, ImGui::GetColorU32(ImGuiCol_MenuBarBg, 10), 1_scaled, ImDrawFlags_None);
                drawList->AddRect(center - iconSize / 2.0F - offset, center + iconSize / 2.0F - offset, ImGui::GetColorU32(ImGuiCol_Text), 5_scaled, ImDrawFlags_None, 7.5_scaled);
                drawList->AddRect(center - iconSize / 2.0F + offset, center + iconSize / 2.0F + offset, ImGui::GetColorU32(ImGuiCol_Text), 5_scaled, ImDrawFlags_None, 7.5_scaled);

                drawList->AddText(center + ImVec2(-textSize.x / 2, 85_scaled), ImGui::GetColorU32(ImGuiCol_Text), text);
            }
        }
        drawList->PopClipRect();
    }

    void addGlobalUIItems() {
        EventFrameEnd::subscribe(drawGlobalPopups);
        EventFrameEnd::subscribe(drawDragNDropOverlay);

        EventFileDragged::subscribe([](bool entered) {
            s_drawDragDropOverlay = entered;
        });
    }

    void addFooterItems() {
        if (hex::isProcessElevated()) {
            ContentRegistry::Interface::addFooterItem([] {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGuiExt::GetCustomColorU32(ImGuiCustomCol_Highlight));
                ImGui::TextUnformatted(ICON_VS_SHIELD);
                ImGui::PopStyleColor();
            });
        }

        #if defined(DEBUG)
            ContentRegistry::Interface::addFooterItem([] {
                static float framerate = 0;
                if (ImGuiExt::HasSecondPassed()) {
                    framerate = 1.0F / ImGui::GetIO().DeltaTime;
                }

                ImGuiExt::TextFormatted("FPS {0:3}.{1:02}", u32(framerate), u32(framerate * 100) % 100);

                if (ImGui::IsItemHovered()) {
                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2());
                    if (ImGui::BeginTooltip()) {

                        static u32 frameCount = 0;
                        static double largestFrameTime = 0;
                        if (ImPlot::BeginPlot("##frame_time_graph", scaled({ 200, 100 }), ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame | ImPlotFlags_NoInputs)) {
                            ImPlot::SetupAxes("", "", ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_LockMin | ImPlotAxisFlags_AutoFit);
                            ImPlot::SetupAxisLimits(ImAxis_Y1, 0, largestFrameTime * 1.25F, ImPlotCond_Always);
                            ImPlot::SetupAxisFormat(ImAxis_Y1, [](double value, char* buff, int size, void*) -> int {
                                return snprintf(buff, size, "%dms", int(value * 1000.0));
                            }, nullptr);
                            ImPlot::SetupAxisTicks(ImAxis_Y1, 0, largestFrameTime * 1.25F, 3);

                            static std::vector<double> values(100);

                            values.push_back(ImHexApi::System::getLastFrameTime());
                            if (values.size() > 100)
                                values.erase(values.begin());

                            if (frameCount % 100 == 0)
                                largestFrameTime = *std::ranges::max_element(values);
                            frameCount += 1;

                            ImPlot::PlotLine("FPS", values.data(), values.size());
                            ImPlot::EndPlot();
                        }
                        ImGui::EndTooltip();
                    }
                    ImGui::PopStyleVar();
                }
            });
        #endif

        ContentRegistry::Interface::addFooterItem([] {
            static bool shouldResetProgress = false;

            auto taskCount = TaskManager::getRunningTaskCount();
            if (taskCount > 0) {
                const auto &tasks = TaskManager::getRunningTasks();
                const auto &frontTask = tasks.front();

                if (frontTask == nullptr)
                    return;

                const auto progress = frontTask->getMaxValue() == 0 ? -1 : float(frontTask->getValue()) / float(frontTask->getMaxValue());

                ImHexApi::System::setTaskBarProgress(ImHexApi::System::TaskProgressState::Progress, ImHexApi::System::TaskProgressType::Normal, u32(progress * 100));

                const auto widgetStart = ImGui::GetCursorPos();
                {
                    ImGuiExt::TextSpinner(hex::format("({})", taskCount).c_str());
                    ImGui::SameLine();
                    ImGuiExt::SmallProgressBar(progress, (ImGui::GetCurrentWindowRead()->MenuBarHeight() - 10_scaled) / 2.0);
                    ImGui::SameLine();
                }
                const auto widgetEnd = ImGui::GetCursorPos();

                ImGui::SetCursorPos(widgetStart);
                ImGui::InvisibleButton("RestTasks", ImVec2(widgetEnd.x - widgetStart.x, ImGui::GetCurrentWindowRead()->MenuBarHeight()));
                ImGui::SetCursorPos(widgetEnd);

                ImGuiExt::InfoTooltip(hex::format("[{:.1f}%] {}", progress * 100.0F, Lang(frontTask->getUnlocalizedName())).c_str());

                if (ImGui::BeginPopupContextItem("RestTasks", ImGuiPopupFlags_MouseButtonLeft)) {
                    for (const auto &task : tasks) {
                        if (task->isBackgroundTask())
                            continue;

                        ImGui::PushID(&task);
                        ImGuiExt::TextFormatted("{}", Lang(task->getUnlocalizedName()));
                        ImGui::SameLine();
                        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                        ImGui::SameLine();
                        ImGuiExt::SmallProgressBar(task->getMaxValue() == 0 ? -1 : (float(task->getValue()) / float(task->getMaxValue())), (ImGui::GetTextLineHeightWithSpacing() - 5_scaled) / 2);
                        ImGui::SameLine();

                        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                        if (ImGuiExt::ToolBarButton(ICON_VS_DEBUG_STOP, ImGui::GetStyleColorVec4(ImGuiCol_Text)))
                            task->interrupt();
                        ImGui::PopStyleVar();

                        ImGui::PopID();
                    }
                    ImGui::EndPopup();
                }

                ImGui::SameLine();

                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, scaled(ImVec2(1, 2)));
                if (ImGuiExt::ToolBarButton(ICON_VS_DEBUG_STOP, ImGui::GetStyleColorVec4(ImGuiCol_Text)))
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

    static void drawProviderContextMenu(prv::Provider *provider) {
        for (const auto &menuEntry : provider->getMenuEntries()) {
            if (ImGui::MenuItem(menuEntry.name.c_str())) {
                menuEntry.callback();
            }
        }
    }

    struct MenuItemSorter {
        bool operator()(const auto *a, const auto *b) const {
            return a->toolbarIndex < b->toolbarIndex;
        }
    };

    void addToolbarItems() {
        ShortcutManager::addGlobalShortcut(AllowWhileTyping + ALT + CTRLCMD + Keys::Left, "hex.builtin.shortcut.prev_provider", []{
            auto currIndex = ImHexApi::Provider::getCurrentProviderIndex();

            if (currIndex > 0)
                ImHexApi::Provider::setCurrentProvider(currIndex - 1);
        });

        ShortcutManager::addGlobalShortcut(AllowWhileTyping + ALT + CTRLCMD + Keys::Right, "hex.builtin.shortcut.next_provider", []{
            auto currIndex = ImHexApi::Provider::getCurrentProviderIndex();

            const auto &providers = ImHexApi::Provider::getProviders();
            if (currIndex < i64(providers.size() - 1))
                ImHexApi::Provider::setCurrentProvider(currIndex + 1);
        });

        static bool providerJustChanged = true;
        EventProviderChanged::subscribe([](auto, auto) { providerJustChanged = true; });

        static prv::Provider *rightClickedProvider = nullptr;
        EventSearchBoxClicked::subscribe([](ImGuiMouseButton button){
            if (button == ImGuiMouseButton_Right) {
                rightClickedProvider = ImHexApi::Provider::get();
                RequestOpenPopup::post("ProviderMenu");
            }
        });

        EventFrameBegin::subscribe([] {
            if (rightClickedProvider != nullptr && !rightClickedProvider->getMenuEntries().empty()) {
                if (ImGui::BeginPopup("ProviderMenu")) {
                    drawProviderContextMenu(rightClickedProvider);
                    ImGui::EndPopup();
                }
            }
        });

        EventProviderChanged::subscribe([](auto, auto){
            rightClickedProvider = nullptr;
        });

        ContentRegistry::Interface::addToolbarItem([] {
            std::set<const ContentRegistry::Interface::impl::MenuItem*, MenuItemSorter> menuItems;

            for (const auto &[priority, menuItem] : ContentRegistry::Interface::impl::getMenuItems()) {
                if (menuItem.toolbarIndex != -1) {
                    menuItems.insert(&menuItem);
                }
            }

            for (const auto &menuItem : menuItems) {
                if (menuItem->unlocalizedNames.back().get() == ContentRegistry::Interface::impl::SeparatorValue) {
                    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                    continue;
                }

                ImGui::BeginDisabled(!menuItem->enabledCallback());
                if (ImGuiExt::ToolBarButton(menuItem->icon.glyph.c_str(), ImGuiExt::GetCustomColorVec4(ImGuiCustomCol(menuItem->icon.color)))) {
                    menuItem->callback();
                }
                ImGui::EndDisabled();
            }
        });

        // Provider switcher
        ContentRegistry::Interface::addToolbarItem([] {
            const auto provider      = ImHexApi::Provider::get();
            const bool providerValid = provider != nullptr;
            const bool tasksRunning  = TaskManager::getRunningTaskCount() > 0;

            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            ImGui::BeginDisabled(!providerValid || tasksRunning);
            {
                auto &providers = ImHexApi::Provider::getProviders();

                ImGui::PushStyleColor(ImGuiCol_TabActive, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
                ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
                auto providerSelectorVisible = ImGui::BeginTabBar("provider_switcher", ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs);
                ImGui::PopStyleColor(2);

                if (providerSelectorVisible) {
                    for (size_t i = 0; i < providers.size(); i++) {
                        if (providers.size() == 1)
                            break;

                        auto &tabProvider = providers[i];
                        const auto selectedProviderIndex = ImHexApi::Provider::getCurrentProviderIndex();

                        bool open = true;
                        ImGui::PushID(tabProvider);

                        ImGuiTabItemFlags flags = ImGuiTabItemFlags_NoTooltip;
                        if (tabProvider->isDirty())
                            flags |= ImGuiTabItemFlags_UnsavedDocument;
                        if (i64(i) == selectedProviderIndex && providerJustChanged) {
                            flags |= ImGuiTabItemFlags_SetSelected;
                            providerJustChanged = false;
                        }

                        static size_t lastSelectedProvider = 0;

                        bool isSelected = false;
                        if (ImGui::BeginTabItem(tabProvider->getName().c_str(), &open, flags)) {
                            isSelected = true;
                            ImGui::EndTabItem();
                        }

                        if (isSelected && lastSelectedProvider != i) {
                            ImHexApi::Provider::setCurrentProvider(i);
                            lastSelectedProvider = i;
                        }

                        if (ImGuiExt::InfoTooltip()) {
                            ImGui::BeginTooltip();

                            ImGuiExt::TextFormatted("{}", tabProvider->getName().c_str());

                            const auto &description = tabProvider->getDataDescription();
                            if (!description.empty()) {
                                ImGui::Separator();
                                if (ImGui::GetIO().KeyShift && !description.empty()) {

                                    if (ImGui::BeginTable("information", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoKeepColumnsVisible, ImVec2(400_scaled, 0))) {
                                        ImGui::TableSetupColumn("type");
                                        ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                                        ImGui::TableNextRow();

                                        for (auto &[name, value] : description) {
                                            ImGui::TableNextColumn();
                                            ImGuiExt::TextFormatted("{}", name);
                                            ImGui::TableNextColumn();
                                            ImGuiExt::TextFormattedWrapped("{}", value);
                                        }

                                        ImGui::EndTable();
                                    }
                                } else {
                                    ImGuiExt::TextFormattedDisabled("hex.builtin.provider.tooltip.show_more"_lang);
                                }
                            }

                            ImGui::EndTooltip();
                        }

                        ImGui::PopID();

                        if (!open) {
                            ImHexApi::Provider::remove(providers[i]);
                            break;
                        }

                        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) && ImGui::IsItemHovered()) {
                            rightClickedProvider = tabProvider;
                            RequestOpenPopup::post("ProviderMenu");
                        }
                    }
                    ImGui::EndTabBar();
                }
            }
            ImGui::EndDisabled();
        });

        ContentRegistry::Interface::addMenuItemToToolbar("hex.builtin.menu.edit.undo", ImGuiCustomCol_ToolbarBlue);
        ContentRegistry::Interface::addMenuItemToToolbar("hex.builtin.menu.edit.redo", ImGuiCustomCol_ToolbarBlue);
        ContentRegistry::Interface::addMenuItemToToolbar("hex.builtin.menu.file.create_file", ImGuiCustomCol_ToolbarGray);
        ContentRegistry::Interface::addMenuItemToToolbar("hex.builtin.menu.file.open_file", ImGuiCustomCol_ToolbarBrown);
        ContentRegistry::Interface::addMenuItemToToolbar("hex.builtin.view.hex_editor.menu.file.save", ImGuiCustomCol_ToolbarBlue);
        ContentRegistry::Interface::addMenuItemToToolbar("hex.builtin.view.hex_editor.menu.file.save_as", ImGuiCustomCol_ToolbarBlue);
        ContentRegistry::Interface::addMenuItemToToolbar("hex.builtin.menu.edit.bookmark.create", ImGuiCustomCol_ToolbarGreen);
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