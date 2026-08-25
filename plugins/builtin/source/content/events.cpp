#include <hex/api/events/events_provider.hpp>
#include <hex/api/events/events_lifecycle.hpp>
#include <hex/api/events/events_interaction.hpp>
#include <hex/api/events/events_gui.hpp>
#include <hex/api/events/requests_lifecycle.hpp>
#include <hex/api/events/requests_interaction.hpp>
#include <hex/api/events/requests_gui.hpp>

#include <hex/api/localization_manager.hpp>
#include <hex/api/content_registry/settings.hpp>
#include <hex/api/content_registry/file_type_handler.hpp>
#include <hex/api/content_registry/provider.hpp>
#include <hex/api/project_manager.hpp>
#include <hex/api/achievement_manager.hpp>
#include <hex/api/workspace_manager.hpp>

#include <hex/trace/exceptions.hpp>

#include <hex/providers/provider.hpp>
#include <hex/providers/file_backed_provider_data.hpp>
#include <hex/ui/view.hpp>

#include <imgui.h>
#include <content/global_actions.hpp>
#include <content/legacy_project_importer.hpp>

#include <content/providers/file_provider.hpp>

#include <wolv/io/fs.hpp>
#include <wolv/utils/string.hpp>

#include <toasts/toast_notification.hpp>
#include <popups/popup_notification.hpp>
#include <content/popups/popup_tasks_waiting.hpp>
#include <content/popups/popup_unsaved_changes.hpp>
#include <content/project.hpp>
#include <content/popups/popup_crash_recovered.hpp>

#include <GLFW/glfw3.h>
#include <hex/api/theme_manager.hpp>
#include <hex/helpers/default_paths.hpp>

namespace hex::plugin::builtin {

    static bool hasPendingFileBackedData(const prv::Provider *provider) {
        return std::ranges::any_of(FileBackedProviderDataRegistry::getTypes(), [provider](const auto *data) {
            return data->hasPendingData(provider);
        });
    }

    static bool flushProjectFileBackedData() {
        if (!ProjectManager::isFolderProject())
            return false;

        bool result = true;
        for (auto *data : FileBackedProviderDataRegistry::getTypes())
            result = data->flush() && result;
        return result;
    }

    static bool hasUnsavedProviderChanges(bool includeFileBackedData) {
        return std::ranges::any_of(ImHexApi::Provider::getProviders(), [includeFileBackedData](const auto *provider) {
            return provider->isDataDirty() || provider->isMetadataDirty() ||
                (includeFileBackedData && hasPendingFileBackedData(provider));
        });
    }

    static bool openFileWithProvider(UnlocalizedString providerName, const std::fs::path &path) {
        auto provider = ImHexApi::Provider::createProvider(providerName, true);
        if (auto *fileProvider = dynamic_cast<prv::IProviderFilePicker*>(provider.get()); fileProvider != nullptr) {
            fileProvider->setPickedPath(path);

            ImHexApi::Provider::openProvider(provider);

            AchievementManager::unlockAchievement("hex.builtin.achievement.starting_out"_unlocalized, "hex.builtin.achievement.starting_out.open_file.name"_unlocalized);

            glfwRequestWindowAttention(ImHexApi::System::getMainWindowHandle());
            glfwFocusWindow(ImHexApi::System::getMainWindowHandle());
            return true;
        }

        ImHexApi::Provider::remove(provider.get());

        return false;
    }

    static void openFile(const std::fs::path &path) {
        TaskManager::doLater([path] {
            if (isLegacyProjectFile(path)) {
                openLegacyProjectMigration(path);
                return;
            }

            for (const auto &entry : ContentRegistry::Provider::impl::getEntries()) {
                for (const auto &extension : entry.validFileExtensions) {
                    if (path.extension() == fmt::format(".{}", extension.spec)) {
                        if (openFileWithProvider(entry.unlocalizedName, path))
                            return;
                    }
                }
            }

            openFileWithProvider("hex.builtin.provider.file"_unlocalized, path);
        });
    }

    void registerEventHandlers() {

        static bool imhexClosing = false;

        // Shared exit popup logic used by both EventWindowClosing and EventCloseButtonPressed.
        // Collects all dirty providers, shows a table with their data/metadata dirty state,
        // and offers Save / Discard / Cancel. Save persists metadata to project if any is dirty,
        // Discard removes all providers and closes the window, Cancel does nothing.
        static auto showExitPopup = [](GLFWwindow *window, bool includeFileBackedData) {
            // Build list of providers that have unsaved changes
            std::vector<ProviderDirtyState> dirtyStates;
            for (const auto &provider : ImHexApi::Provider::getProviders()) {
                const bool metadataDirty = provider->isMetadataDirty() ||
                    (includeFileBackedData && hasPendingFileBackedData(provider));
                if (provider->isDataDirty() || metadataDirty)
                    dirtyStates.push_back({ .provider = provider, .dataDirty = provider->isDataDirty(), .metadataDirty = metadataDirty });
            }

            PopupUnsavedChanges::open(std::move(dirtyStates),
                [window] {
                    // Save data: write file data to disk for each dirty provider
                    if (!saveProviderData()) {
                        ui::ToastError::open("hex.builtin.popup.error.project.save"_lang);
                        return;
                    }

                    if (!project::prepareForShutdown()) {
                        ui::ToastError::open("hex.builtin.popup.error.project.save"_lang);
                        return;
                    }
                    imhexClosing = true;
                    for (const auto &provider : ImHexApi::Provider::getProviders())
                        ImHexApi::Provider::remove(provider, true);
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                },
                [window] {
                    // Save providers data
                    if (!saveProviderData()) {
                        ui::ToastError::open("hex.builtin.popup.error.project.save"_lang);
                        return;
                    }

                    // Save project metadata
                    if (!(ProjectManager::hasPath() ? saveProject() : saveProjectAs()))
                        return;

                    // Close
                    if (!project::prepareForShutdown()) {
                        ui::ToastError::open("hex.builtin.popup.error.project.save"_lang);
                        return;
                    }
                    imhexClosing = true;
                    for (const auto &provider : ImHexApi::Provider::getProviders())
                        ImHexApi::Provider::remove(provider, true);
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                },
                [window] {
                    // Discard: remove all providers and close without saving
                    std::ignore = project::prepareForShutdown(false);
                    imhexClosing = true;
                    for (const auto &provider : ImHexApi::Provider::getProviders())
                        ImHexApi::Provider::remove(provider, true);
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                },
                [] { }
            );
        };

        EventCrashRecovered::subscribe([](const std::exception &e) {
            PopupCrashRecovered::open(e);

            auto stackTrace = hex::trace::getLastExceptionStackTrace();
            if (stackTrace.has_value()) {
                for (const auto &entry : stackTrace->stackFrames) {
                    hex::log::fatal("  {} at {}:{}", entry.function, entry.file, entry.line);
                }
            }
        });

        EventWindowClosing::subscribe([](GLFWwindow *window) {
            imhexClosing = false;
            const bool includeFileBackedData = !ProjectManager::isFolderProject() || !flushProjectFileBackedData();
            if (hasUnsavedProviderChanges(includeFileBackedData) && !imhexClosing) {
                glfwSetWindowShouldClose(window, GLFW_FALSE);
                showExitPopup(window, includeFileBackedData);
            } else if (TaskManager::getRunningTaskCount() > 0 || TaskManager::getRunningBackgroundTaskCount() > 0) {
                glfwSetWindowShouldClose(window, GLFW_FALSE);
                TaskManager::doLater([] {
                    for (auto &task : TaskManager::getRunningTasks())
                        task->interrupt();
                    PopupTasksWaiting::open([]() {
                        ImHexApi::System::closeImHex();
                    });
                });
            }
        });

        EventCloseButtonPressed::subscribe([]() {
            if (ImHexApi::Provider::isValid()) {
                const bool includeFileBackedData = !ProjectManager::isFolderProject() || !flushProjectFileBackedData();
                if (hasUnsavedProviderChanges(includeFileBackedData)) {
                    showExitPopup(ImHexApi::System::getMainWindowHandle(), includeFileBackedData);
                } else if (TaskManager::getRunningTaskCount() > 0 || TaskManager::getRunningBackgroundTaskCount() > 0) {
                    TaskManager::doLater([] {
                        for (auto &task : TaskManager::getRunningTasks())
                            task->interrupt();
                        PopupTasksWaiting::open([]() {
                            EventCloseButtonPressed::post();
                        });
                    });
                } else {
                    if (!project::prepareForShutdown()) {
                        ui::ToastError::open("hex.builtin.popup.error.project.save"_lang);
                        return;
                    }
                    for (const auto &provider : ImHexApi::Provider::getProviders())
                        ImHexApi::Provider::remove(provider);
                    glfwSetWindowShouldClose(ImHexApi::System::getMainWindowHandle(), GLFW_TRUE);
                }
            } else {
                ImHexApi::System::closeImHex();
            }
        });

        EventProviderClosing::subscribe([](const prv::Provider *provider, bool *shouldClose) {
            const bool includeFileBackedData = !ProjectManager::isFolderProject() || !flushProjectFileBackedData();
            const bool metadataDirty = provider->isMetadataDirty() ||
                (includeFileBackedData && hasPendingFileBackedData(provider));
            if (provider->isDataDirty() || metadataDirty) {
                // Block the close until the user responds to the popup
                *shouldClose = false;

                // Build dirty state for the popup table (single provider in this case)
                std::vector<ProviderDirtyState> dirtyStates = {
                    { .provider = const_cast<prv::Provider*>(provider), .dataDirty = provider->isDataDirty(), .metadataDirty = metadataDirty }
                };

                PopupUnsavedChanges::open(dirtyStates,
                    [dirtyStates]{
                        // Save data: write file data to disk for each dirty provider
                        for (const auto &entry : dirtyStates) {
                            if (entry.dataDirty && entry.provider->isSavable())
                                entry.provider->save();
                        }

                        // Once saved, close requested providers and close the window if it was closing
                        for (const auto &provider : ImHexApi::Provider::impl::getClosingProviders())
                            ImHexApi::Provider::remove(provider, true);

                        if (imhexClosing)
                            ImHexApi::System::closeImHex(true);
                    },
                    [dirtyStates]{
                        // Save data + project: write file data to disk, then persist metadata
                        for (const auto &entry : dirtyStates) {
                            if (entry.dataDirty && entry.provider->isSavable())
                                entry.provider->save();
                        }

                        bool saved = true;
                        bool anyMetadataDirty = false;
                        for (const auto &entry : dirtyStates) {
                            if (entry.metadataDirty) anyMetadataDirty = true;
                        }

                        if (anyMetadataDirty)
                            saved = ProjectManager::hasPath() ? saveProject() : saveProjectAs();

                        if (saved) {
                            for (const auto &provider : ImHexApi::Provider::impl::getClosingProviders())
                                ImHexApi::Provider::remove(provider, true);

                            if (imhexClosing)
                                ImHexApi::System::closeImHex(true);
                        } else {
                            // Save failed — abort the close and reset state
                            ImHexApi::Provider::impl::resetClosingProviders();
                            imhexClosing = false;
                        }
                    },
                    [] {
                        // Discard: remove all queued providers without saving
                        for (const auto &provider : ImHexApi::Provider::impl::getClosingProviders())
                            ImHexApi::Provider::remove(provider, true);

                        if (imhexClosing)
                            ImHexApi::System::closeImHex(true);
                    },
                    [] {
                        ImHexApi::Provider::impl::resetClosingProviders();
                        imhexClosing = false;
                    }
                );
            }
        });

        EventProviderChanged::subscribe([](hex::prv::Provider *oldProvider, hex::prv::Provider *newProvider) {
            std::ignore = oldProvider;
            std::ignore = newProvider;

            RequestUpdateWindowTitle::post();
        });

        EventProviderOpened::subscribe([](hex::prv::Provider *provider) {
            if (provider != nullptr && ImHexApi::Provider::get() == provider) {
                RequestUpdateWindowTitle::post();
            }
        });

        RequestOpenFile::subscribe(openFile);

        RequestOpenWindow::subscribe([](const std::string &name) {
            if (name == "Create File") {
                auto newProvider = ImHexApi::Provider::createProvider("hex.builtin.provider.mem_file"_unlocalized, true);
                ImHexApi::Provider::openProvider(newProvider);
            } else if (name == "Open File") {
                fs::openFileBrowser(fs::DialogMode::Open, { }, [](const auto &path) {
                    openFile(path);
                }, {}, true);
            } else if (name == "Open Project") {
                fs::openFileBrowser(fs::DialogMode::Folder, { },
                    [](const auto &path) {
                        if (!ProjectManager::load(path)) {
                            ui::ToastError::open(fmt::format("hex.builtin.popup.error.project.load"_lang, wolv::util::toUTF8String(path)));
                        }
                    });
            } else if (name == "Open Folder") {
                fs::openFileBrowser(fs::DialogMode::Folder, {  },
                    [](const auto &path) {
                        if (!ProjectManager::load(path)) {
                            ui::ToastError::open(fmt::format("hex.builtin.popup.error.project.load"_lang, wolv::util::toUTF8String(path)));
                        }
                    });
            }
        });

        EventProviderChanged::subscribe([](auto, auto) {
            EventHighlightingChanged::post();
        });

        // Handles the provider initialization, and calls EventProviderOpened if successful
        EventProviderCreated::subscribe([](std::shared_ptr<prv::Provider> provider) {
            if (provider->shouldSkipLoadInterface())
                return;

            if (auto *filePickerProvider = dynamic_cast<prv::IProviderFilePicker*>(provider.get()); filePickerProvider != nullptr) {
                std::fs::path filePath;
                auto filePicked = fs::openFileBrowser(
                    fs::DialogMode::Open,
                    filePickerProvider->getValidExtensions(),
                    [&filePath](const std::fs::path &path) {
                        filePath = path;
                    }
                );

                if (!filePicked || !filePickerProvider->canOpenFile(filePath)) {
                    TaskManager::doLater([provider] {
                        ImHexApi::Provider::remove(provider.get());
                    });
                    return;
                }

                filePickerProvider->setPickedPath(filePath);
                ImHexApi::Provider::openProvider(provider);
            }
            else if (dynamic_cast<prv::IProviderLoadInterface*>(provider.get()) == nullptr) {
                ImHexApi::Provider::openProvider(provider);
            }
        });

        EventRegionSelected::subscribe([](const ImHexApi::HexEditor::ProviderRegion &region) {
           ImHexApi::HexEditor::impl::setCurrentSelection(region);
        });

        EventFileDropped::subscribe([](const std::fs::path &path) {
             // Check if a custom file handler can handle the file
             bool handled = false;
             for (const auto &[extensions, handler, icon] : ContentRegistry::FileTypeHandler::impl::getEntries()) {
                 std::ignore = icon;
                 for (const auto &extension : extensions) {
                     if (path.extension() == extension) {
                         // Pass the file to the handler and check if it was successful
                         if (!handler(path)) {
                             log::error("Handler for extensions '{}' failed to process file!", extension);
                             break;
                         }

                         handled = true;
                     }
                 }
             }

             // If no custom handler was found, just open the file regularly
             if (!handled)
                 RequestOpenFile::post(path);
        });

        EventImHexStartupFinished::subscribe([] {
            const auto& currVersion = ImHexApi::System::getImHexVersion();
            const auto prevLaunchVersion = ContentRegistry::Settings::read<std::string>("hex.builtin.setting.general"_unlocalized, "hex.builtin.setting.general.prev_launch_version"_untranslated, "");

            const auto forceOobe = getEnvironmentVariable("IMHEX_FORCE_OOBE");
            if (prevLaunchVersion.empty() || (forceOobe.has_value() && *forceOobe != "0")) {
                EventFirstLaunch::post();
                return;
            }

            const auto prevLaunchVersionParsed = SemanticVersion(prevLaunchVersion);

            if (currVersion != prevLaunchVersionParsed) {
                EventImHexUpdated::post(prevLaunchVersionParsed, currVersion);

                ContentRegistry::Settings::write<std::string>("hex.builtin.setting.general"_unlocalized, "hex.builtin.setting.general.prev_launch_version"_untranslated, currVersion.get(false));
            }
        });

        EventWindowDeinitializing::subscribe([](GLFWwindow *window) {
            WorkspaceManager::exportToFile();
            if (auto workspace = WorkspaceManager::getCurrentWorkspace(); workspace != WorkspaceManager::getWorkspaces().end())
                ContentRegistry::Settings::write<std::string>("hex.builtin.setting.general"_unlocalized, "hex.builtin.setting.general.curr_workspace"_untranslated, workspace->first);

            {
                int x = 0, y = 0, width = 0, height = 0, maximized = 0;
                glfwGetWindowPos(window, &x, &y);
                glfwGetWindowSize(window, &width, &height);
                maximized = glfwGetWindowAttrib(window, GLFW_MAXIMIZED);

                ContentRegistry::Settings::write<int>("hex.builtin.setting.interface"_unlocalized, "hex.builtin.setting.interface.window.x"_untranslated, x);
                ContentRegistry::Settings::write<int>("hex.builtin.setting.interface"_unlocalized, "hex.builtin.setting.interface.window.y"_untranslated, y);
                ContentRegistry::Settings::write<int>("hex.builtin.setting.interface"_unlocalized, "hex.builtin.setting.interface.window.width"_untranslated, width);
                ContentRegistry::Settings::write<int>("hex.builtin.setting.interface"_unlocalized, "hex.builtin.setting.interface.window.height"_untranslated, height);
                ContentRegistry::Settings::write<int>("hex.builtin.setting.interface"_unlocalized, "hex.builtin.setting.interface.window.maximized"_untranslated, maximized);
            }
        });

        EventImHexStartupFinished::subscribe([] {
            const auto &initArgs = ImHexApi::System::getInitArguments();
            if (auto it = initArgs.find("language"); it != initArgs.end())
                LocalizationManager::setLanguage(it->second);

            // Set the user-defined post-processing shader if one exists
            #if !defined(OS_WEB)
                for (const auto &folder : paths::Resources.all()) {
                    auto vertexShaderPath = folder / "shader.vert";
                    auto fragmentShaderPath = folder / "shader.frag";

                    if (!wolv::io::fs::exists(vertexShaderPath))
                        continue;
                    if (!wolv::io::fs::exists(fragmentShaderPath))
                        continue;

                    auto vertexShaderFile = wolv::io::File(vertexShaderPath, wolv::io::File::Mode::Read);
                    if (!vertexShaderFile.isValid())
                        continue;

                    auto fragmentShaderFile = wolv::io::File(fragmentShaderPath, wolv::io::File::Mode::Read);
                    if (!fragmentShaderFile.isValid())
                        continue;

                    const auto vertexShaderSource = vertexShaderFile.readString();
                    const auto fragmentShaderSource = fragmentShaderFile.readString();

                    ImHexApi::System::setPostProcessingShader(vertexShaderSource, fragmentShaderSource);

                    break;
                }
            #endif
        });

        EventWindowFocused::subscribe([](bool focused) {
            const auto ctx = ImGui::GetCurrentContext();
            if (ctx == nullptr)
                return;

            static ImGuiWindow *lastFocusedWindow = nullptr;

            if (ImGui::IsAnyItemHovered()) {
                lastFocusedWindow = nullptr;
                return;
            }

            if (focused) {
                if (lastFocusedWindow == nullptr)
                    return;

                // If the main window gains focus again, restore the last focused window
                ImGui::FocusWindow(lastFocusedWindow);
                ImGui::FocusWindow(lastFocusedWindow, ImGuiFocusRequestFlags_RestoreFocusedChild);

                if (lastFocusedWindow != nullptr)
                    log::debug("Restoring focus on window '{}'", lastFocusedWindow->Name ? lastFocusedWindow->Name : "Unknown Window");
                lastFocusedWindow = nullptr;
            } else {
                if (ctx->NavWindow != nullptr && (ctx->NavWindow->Flags & ImGuiWindowFlags_Modal))
                    return;

                // If the main window loses focus, store the currently focused window
                // and remove focus from it so it doesn't look like it's focused and
                // cursor blink animations don't play
                lastFocusedWindow = ctx->NavWindow;
                ImGui::FocusWindow(nullptr);

                if (lastFocusedWindow != nullptr)
                    log::debug("Removing focus from window '{}'", lastFocusedWindow->Name ? lastFocusedWindow->Name : "Unknown Window");
            }
        });

        RequestChangeTheme::subscribe([](const std::string &theme) {
            ThemeManager::changeTheme(theme);
        });

        static std::mutex s_popupMutex;
        static std::list<std::string> s_popupsToOpen;
        RequestOpenPopup::subscribe([](auto name) {
            std::scoped_lock lock(s_popupMutex);

            s_popupsToOpen.push_back(name);
        });

        EventFrameBegin::subscribe([]() {
            // Open popups when plugins requested it
            // We retry every frame until the popup actually opens
            // It might not open the first time because another popup is already open

            std::scoped_lock lock(s_popupMutex);
            s_popupsToOpen.remove_if([](const auto &name) {
                if (ImGui::IsPopupOpen(name.c_str()))
                    return true;
                else
                    ImGui::OpenPopup(name.c_str());

                return false;
            });
        });

        RequestOpenProvider::subscribe([](std::shared_ptr<prv::Provider> provider, TaskHolder *task) {
            auto *filePicker = dynamic_cast<prv::IProviderFilePicker *>(provider.get());
            auto *existingProvider = filePicker == nullptr ? nullptr : project::openProviderForPath(filePicker->getPickedPath(), provider.get());
            if (existingProvider != nullptr) {
                ImHexApi::Provider::remove(provider.get(), true);
                ImHexApi::Provider::setCurrentProvider(existingProvider);
                return;
            }

            *task = TaskManager::createBlockingTask("hex.builtin.provider.opening"_unlocalized, ProgressValue::None(), [provider]() {
                auto result = provider->open();
                if (result.isFailure()) {
                    ui::ToastError::open(fmt::format("hex.builtin.provider.error.open"_lang, result.getErrorMessage()));
                    TaskManager::doLater([provider] { ImHexApi::Provider::remove(provider.get()); });
                } else if (result.isRedirecting()) {
                    TaskManager::doLater([result, provider] {
                        ImHexApi::Provider::remove(provider.get());
                        ImHexApi::Provider::setCurrentProvider(result.getRedirectProvider());
                    });
                } else {
                    if (result.isWarning())
                        ui::ToastWarning::open(std::string(result.getErrorMessage()));
                    TaskManager::doLater([provider]{ EventProviderOpened::post(provider.get()); });
                }
            });
        });

        fs::setFileBrowserErrorCallback([](const std::string& errMsg){
            #if defined(NFD_PORTAL)
                ui::PopupError::open(fmt::format("hex.builtin.popup.error.file_dialog.portal"_lang, errMsg));
            #else
                ui::PopupError::open(fmt::format("hex.builtin.popup.error.file_dialog.common"_lang, errMsg));
            #endif
        });

        static ContentRegistry::Settings::SettingsVariable<bool, "hex.builtin.setting.interface", "hex.builtin.setting.interface.show_task_finish_notification"> taskFinishedNotificationEnabled = false;
        TaskManager::addTaskCompletionCallback([](Task &task) {
            if (!taskFinishedNotificationEnabled)
                return;

            if (task.isBackgroundTask())
                return;

            #if !defined(OS_WEB)
                if (!ImHexApi::System::isMainWindowFocused())
                    hex::showToastMessage("ImHex", fmt::format("hex.builtin.os_toast_message.task_finished"_lang, Lang(task.getUnlocalizedName())));
            #endif
        });

    }

}
