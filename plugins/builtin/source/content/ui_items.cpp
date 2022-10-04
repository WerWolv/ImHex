#include <hex/api/content_registry.hpp>
#include <hex/api/localization.hpp>

#include <hex/ui/view.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>

#include <fonts/codicons_font.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <hex/ui/imgui_imhex_extensions.h>

namespace hex::plugin::builtin {

    static std::string s_popupMessage;
    static std::function<void()> s_yesCallback, s_noCallback;
    static u32 s_selectableFileIndex;
    static std::vector<std::fs::path> s_selectableFiles;
    static std::function<void(std::fs::path)> s_selectableFileOpenCallback;
    static std::vector<nfdfilteritem_t> s_selectableFilesValidExtensions;

    static void drawGlobalPopups() {

        // "Are you sure you want to exit?" Popup
        if (ImGui::BeginPopupModal("hex.builtin.popup.exit_application.title"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::NewLine();
            ImGui::TextUnformatted("hex.builtin.popup.exit_application.desc"_lang);
            ImGui::NewLine();

            View::confirmButtons("hex.builtin.common.yes"_lang, "hex.builtin.common.no"_lang,
                                 [] { ImHexApi::Common::closeImHex(true); },
                                 [] { ImGui::CloseCurrentPopup(); }
                                 );

            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        // "Are you sure you want to close provider?" Popup
        if (ImGui::BeginPopupModal("hex.builtin.popup.close_provider.title"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::NewLine();
            ImGui::TextUnformatted("hex.builtin.popup.close_provider.desc"_lang);
            ImGui::NewLine();

            View::confirmButtons("hex.builtin.common.yes"_lang, "hex.builtin.common.no"_lang,
                                 [] { ImHexApi::Provider::remove(ImHexApi::Provider::impl::getClosingProvider(), true); ImGui::CloseCurrentPopup(); },
                                 [] { ImGui::CloseCurrentPopup(); }
                                 );

            if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        auto windowSize = ImHexApi::System::getMainWindowSize();

        // Info popup
        ImGui::SetNextWindowSizeConstraints(scaled(ImVec2(400, 100)), scaled(ImVec2(600, 300)));
        if (ImGui::BeginPopupModal("hex.builtin.common.info"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextFormattedWrapped("{}", s_popupMessage.c_str());
            ImGui::NewLine();
            ImGui::Separator();
            if (ImGui::Button("hex.builtin.common.okay"_lang) || ImGui::IsKeyDown(ImGuiKey_Escape))
                ImGui::CloseCurrentPopup();

            ImGui::SetWindowPos((windowSize - ImGui::GetWindowSize()) / 2, ImGuiCond_Appearing);
            ImGui::EndPopup();
        }

        // Error popup
        ImGui::SetNextWindowSizeConstraints(scaled(ImVec2(400, 100)), scaled(ImVec2(600, 300)));
        if (ImGui::BeginPopupModal("hex.builtin.common.error"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextFormattedWrapped("{}", s_popupMessage.c_str());
            ImGui::NewLine();
            ImGui::Separator();
            if (ImGui::Button("hex.builtin.common.okay"_lang) || ImGui::IsKeyDown(ImGuiKey_Escape))
                ImGui::CloseCurrentPopup();

            ImGui::SetWindowPos((windowSize - ImGui::GetWindowSize()) / 2, ImGuiCond_Appearing);
            ImGui::EndPopup();
        }

        // Fatal error popup
        ImGui::SetNextWindowSizeConstraints(scaled(ImVec2(400, 100)), scaled(ImVec2(600, 300)));
        if (ImGui::BeginPopupModal("hex.builtin.common.fatal"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextFormattedWrapped("{}", s_popupMessage.c_str());
            ImGui::NewLine();
            ImGui::Separator();
            if (ImGui::Button("hex.builtin.common.okay"_lang) || ImGui::IsKeyDown(ImGuiKey_Escape)) {
                ImHexApi::Common::closeImHex();
                ImGui::CloseCurrentPopup();
            }

            ImGui::SetWindowPos((windowSize - ImGui::GetWindowSize()) / 2, ImGuiCond_Appearing);
            ImGui::EndPopup();
        }

        // Yes/No question popup
        ImGui::SetNextWindowSizeConstraints(scaled(ImVec2(400, 100)), scaled(ImVec2(600, 300)));
        if (ImGui::BeginPopupModal("hex.builtin.common.question"_lang, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextFormattedWrapped("{}", s_popupMessage.c_str());
            ImGui::NewLine();
            ImGui::Separator();

            View::confirmButtons(
                    "hex.builtin.common.yes"_lang, "hex.builtin.common.no"_lang, [] {
                        s_yesCallback();
                        ImGui::CloseCurrentPopup(); }, [] {
                        s_noCallback();
                        ImGui::CloseCurrentPopup(); });

            ImGui::SetWindowPos((windowSize - ImGui::GetWindowSize()) / 2, ImGuiCond_Appearing);
            ImGui::EndPopup();
        }

        // File chooser popup
        bool opened = true;
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5F, 0.5F));
        if (ImGui::BeginPopupModal("hex.builtin.common.choose_file"_lang, &opened, ImGuiWindowFlags_AlwaysAutoResize)) {

            if (ImGui::BeginListBox("##files", ImVec2(300_scaled, 0))) {

                u32 index = 0;
                for (auto &path : s_selectableFiles) {
                    ImGui::PushID(index);
                    if (ImGui::Selectable(hex::toUTF8String(path.filename()).c_str(), index == s_selectableFileIndex))
                        s_selectableFileIndex = index;
                    ImGui::PopID();

                    index++;
                }

                ImGui::EndListBox();
            }

            if (ImGui::Button("hex.builtin.common.open"_lang)) {
                s_selectableFileOpenCallback(s_selectableFiles[s_selectableFileIndex]);
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (ImGui::Button("hex.builtin.common.browse"_lang)) {
                fs::openFileBrowser(fs::DialogMode::Open, s_selectableFilesValidExtensions, [](const auto &path) {
                    s_selectableFileOpenCallback(path);
                    ImGui::CloseCurrentPopup();
                });
            }

            ImGui::EndPopup();
        }

        // Task exception popup
        for (const auto &task : TaskManager::getRunningTasks()) {
            if (task->hadException()) {
                EventManager::post<RequestShowErrorPopup>(hex::format("hex.builtin.popup.error.task_exception"_lang, LangEntry(task->getUnlocalizedName()), task->getExceptionMessage()));
                task->clearException();
                break;
            }
        }
    }

    void addGlobalUIItems() {
        EventManager::subscribe<EventFrameEnd>(drawGlobalPopups);

        EventManager::subscribe<RequestShowInfoPopup>([](const std::string &message) {
            s_popupMessage = message;

            TaskManager::doLater([] { ImGui::OpenPopup("hex.builtin.common.info"_lang); });
        });

        EventManager::subscribe<RequestShowErrorPopup>([](const std::string &message) {
            s_popupMessage = message;

            TaskManager::doLater([] { ImGui::OpenPopup("hex.builtin.common.error"_lang); });
        });

        EventManager::subscribe<RequestShowFatalErrorPopup>([](const std::string &message) {
            s_popupMessage = message;

            TaskManager::doLater([] { ImGui::OpenPopup("hex.builtin.common.fatal"_lang); });
        });

        EventManager::subscribe<RequestShowYesNoQuestionPopup>([](const std::string &message, const std::function<void()> &yesCallback, const std::function<void()> &noCallback) {
            s_popupMessage = message;

            s_yesCallback = yesCallback;
            s_noCallback  = noCallback;

            TaskManager::doLater([] { ImGui::OpenPopup("hex.builtin.common.question"_lang); });
        });

        EventManager::subscribe<RequestShowFileChooserPopup>([](const std::vector<std::fs::path> &paths, const std::vector<nfdfilteritem_t> &validExtensions, const std::function<void(std::fs::path)> &callback) {
            s_selectableFileIndex            = 0;
            s_selectableFiles                = paths;
            s_selectableFilesValidExtensions = validExtensions;
            s_selectableFileOpenCallback     = callback;

            TaskManager::doLater([] { ImGui::OpenPopup("hex.builtin.common.choose_file"_lang); });
        });
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
            auto taskCount = TaskManager::getRunningTaskCount();
            if (taskCount > 0) {
                auto &tasks = TaskManager::getRunningTasks();
                auto frontTask = tasks.front();

                auto widgetStart = ImGui::GetCursorPos();

                ImGui::TextSpinner(hex::format("({})", taskCount).c_str());
                ImGui::SameLine();
                ImGui::SmallProgressBar(frontTask->getMaxValue() == 0 ? 1 : (float(frontTask->getValue()) / frontTask->getMaxValue()), (ImGui::GetCurrentWindow()->MenuBarHeight() - 10_scaled) / 2.0);
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
                if (ImGui::ToolBarButton(ICON_VS_FILE, ImGui::GetCustomColorVec4(ImGuiCustomCol_ToolbarGray)))
                    EventManager::post<RequestOpenWindow>("Create File");

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
                        if (ImGui::BeginTabItem(tabProvider->getName().c_str(), &open, tabProvider->isDirty() ? ImGuiTabItemFlags_UnsavedDocument : ImGuiTabItemFlags_None)) {
                            ImHexApi::Provider::setCurrentProvider(i);
                            ImGui::EndTabItem();
                        }
                        ImGui::PopID();

                        if (!open) {
                            ImHexApi::Provider::remove(providers[i]);
                            break;
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