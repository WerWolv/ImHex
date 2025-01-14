#include <hex/api/content_registry.hpp>
#include <hex/api/imhex_api.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/task_manager.hpp>

#include <hex/ui/view.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/debugging.hpp>

#include <fonts/vscode_icons.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>
#include <implot3d.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <toasts/toast_notification.hpp>

#include <csignal>

namespace hex::plugin::builtin {

    void addTitleBarButtons() {
        #if defined(DEBUG)
            ContentRegistry::Interface::addTitleBarButton(ICON_VS_DEBUG, "hex.builtin.title_bar_button.debug_build", []{
                if (ImGui::GetIO().KeyShift) {
                    RequestOpenPopup::post("DebugMenu");
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

    #if defined(DEBUG)

        static void drawDebugPopup() {
            static bool showImGuiDemo = false;
            static bool showImPlotDemo = false;
            static bool showImPlot3DDemo = false;

            ImGui::SetNextWindowSize(scaled({ 300, 150 }), ImGuiCond_Always);
            if (ImGui::BeginPopup("DebugMenu")) {
                if (ImGui::BeginTabBar("DebugTabBar")) {
                    if (ImGui::BeginTabItem("ImHex")) {
                        if (ImGui::BeginChild("Scrolling", ImGui::GetContentRegionAvail())) {
                            ImGui::Checkbox("Show Debug Variables", &dbg::impl::getDebugWindowState());

                            ImGuiExt::Header("Information");
                            ImGuiExt::TextFormatted("Running Tasks: {0}", TaskManager::getRunningTaskCount());
                            ImGuiExt::TextFormatted("Running Background Tasks: {0}", TaskManager::getRunningBackgroundTaskCount());
                            ImGuiExt::TextFormatted("Last Frame Time: {0:.3f}ms", ImHexApi::System::getLastFrameTime() * 1000.0F);
                        }
                        ImGui::EndChild();

                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("ImGui")) {
                        if (ImGui::BeginChild("Scrolling", ImGui::GetContentRegionAvail())) {
                            auto ctx = ImGui::GetCurrentContext();
                            ImGui::Checkbox("Show ImGui Demo", &showImGuiDemo);
                            ImGui::Checkbox("Show ImPlot Demo", &showImPlotDemo);
                            ImGui::Checkbox("Show ImPlot3D Demo", &showImPlot3DDemo);

                            if (ImGui::Button("Trigger Breakpoint in Item") || ctx->DebugItemPickerActive)
                                ImGui::DebugStartItemPicker();
                        }
                        ImGui::EndChild();

                        ImGui::EndTabItem();
                    }
                    if (ImGui::BeginTabItem("Crashes")) {
                        if (ImGui::BeginChild("Scrolling", ImGui::GetContentRegionAvail())) {
                            if (ImGui::Button("Throw Exception")) {
                                TaskManager::doLater([] {
                                    throw std::runtime_error("Test exception");
                                });
                            }
                            if (ImGui::Button("Access Invalid Memory")) {
                                TaskManager::doLater([] {
                                    *reinterpret_cast<u32*>(0x10) = 0x10;
                                    std::unreachable();
                                });
                            }
                            if (ImGui::Button("Raise SIGSEGV")) {
                                TaskManager::doLater([] {
                                    raise(SIGSEGV);
                                });
                            }
                            if (ImGui::Button("Corrupt Memory")) {
                                TaskManager::doLater([] {
                                    auto bytes = new u8[0xFFFFF];

                                    delete[] bytes;
                                    delete[] bytes;
                                });
                            }
                        }
                        ImGui::EndChild();

                        ImGui::EndTabItem();
                    }

                    ImGui::EndTabBar();
                }
                ImGui::EndPopup();
            }

            if (showImGuiDemo)
                ImGui::ShowDemoWindow(&showImGuiDemo);
            if (showImPlotDemo)
                ImPlot::ShowDemoWindow(&showImPlotDemo);
            if (showImPlot3DDemo)
                ImPlot3D::ShowDemoWindow(&showImPlot3DDemo);
        }

    #endif

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

        #if defined(DEBUG)
            EventFrameEnd::subscribe(drawDebugPopup);
        #endif

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
                    ImGuiExt::SmallProgressBar(progress, (ImGui::GetCurrentWindowRead()->MenuBarHeight - 10_scaled) / 2.0);
                    ImGui::SameLine();
                }
                const auto widgetEnd = ImGui::GetCursorPos();

                ImGui::SetCursorPos(widgetStart);
                ImGui::InvisibleButton("RestTasks", ImVec2(widgetEnd.x - widgetStart.x, ImGui::GetCurrentWindowRead()->MenuBarHeight));
                ImGui::SetCursorPos(widgetEnd);

                std::string progressString;
                if (progress < 0)
                    progressString = "";
                else
                    progressString = hex::format("[ {}/{} ({:.1f}%) ] ", frontTask->getValue(), frontTask->getMaxValue(), std::min(progress, 1.0F) * 100.0F);

                ImGuiExt::InfoTooltip(hex::format("{}{}", progressString, Lang(frontTask->getUnlocalizedName())).c_str());

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
            if (ImGui::MenuItemEx(menuEntry.name.c_str(), menuEntry.icon)) {
                menuEntry.callback();
            }
        }
    }

    void drawProviderTooltip(const prv::Provider *provider) {
        if (ImGuiExt::InfoTooltip()) {
            ImGui::BeginTooltip();

            ImGuiExt::TextFormatted("{}", provider->getName().c_str());

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
    }

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

        static bool alwaysShowProviderTabs = false;
        ContentRegistry::Settings::onChange("hex.builtin.setting.interface", "hex.builtin.setting.interface.always_show_provider_tabs", [](const ContentRegistry::Settings::SettingsValue &value) {
            alwaysShowProviderTabs = value.get<bool>(false);
        });

        // Toolbar items
        ContentRegistry::Interface::addToolbarItem([] {

            for (const auto &menuItem : ContentRegistry::Interface::impl::getToolbarMenuItems()) {
                const auto &unlocalizedItemName = menuItem->unlocalizedNames.back();
                if (unlocalizedItemName.get() == ContentRegistry::Interface::impl::SeparatorValue) {
                    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                    continue;
                }

                ImGui::BeginDisabled(!menuItem->enabledCallback());
                if (ImGuiExt::ToolBarButton(menuItem->icon.glyph.c_str(), ImGuiExt::GetCustomColorVec4(ImGuiCustomCol(menuItem->icon.color)))) {
                    menuItem->callback();
                }
                ImGuiExt::InfoTooltip(Lang(unlocalizedItemName));

                ImGui::EndDisabled();
            }
        });

        // Provider switcher
        ContentRegistry::Interface::addToolbarItem([] {
            const bool providerValid = ImHexApi::Provider::get() != nullptr;
            const bool tasksRunning  = TaskManager::getRunningTaskCount() > 0;

            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Spacing();

            ImGui::BeginDisabled(!providerValid || tasksRunning);
            {
                auto providers = ImHexApi::Provider::getProviders();

                ImGui::PushStyleColor(ImGuiCol_TabSelected, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
                ImGui::PushStyleColor(ImGuiCol_TabDimmedSelected, ImGui::GetColorU32(ImGuiCol_MenuBarBg));

                ImGui::GetCurrentWindow()->WorkRect.Max.x -= 25_scaled;
                auto providerSelectorVisible = ImGui::BeginTabBar("provider_switcher", ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs);
                ImGui::PopStyleColor(2);

                if (providerSelectorVisible) {
                    for (size_t i = 0; i < providers.size(); i++) {
                        if (providers.size() == 1 && !alwaysShowProviderTabs)
                            break;

                        auto &tabProvider = providers[i];
                        const auto selectedProviderIndex = ImHexApi::Provider::getCurrentProviderIndex();

                        const auto closingProviders = ImHexApi::Provider::impl::getClosingProviders();
                        if (std::ranges::find(closingProviders, tabProvider) != closingProviders.end())
                            continue;

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

                        drawProviderTooltip(tabProvider);

                        ImGui::PopID();

                        if (!open) {
                            ImHexApi::Provider::remove(providers[i]);
                            break;
                        }

                        if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && ImGui::IsItemHovered() && !ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
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

}