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
#include <hex/api/project_file_manager.hpp>
#include <hex/api/achievement_manager.hpp>
#include <hex/api/workspace_manager.hpp>

#include <hex/trace/exceptions.hpp>

#include <hex/providers/provider.hpp>
#include <hex/ui/view.hpp>

#include <imgui.h>
#include <content/global_actions.hpp>

#include <content/providers/file_provider.hpp>

#include <wolv/io/fs.hpp>
#include <wolv/utils/string.hpp>

#include <toasts/toast_notification.hpp>
#include <popups/popup_notification.hpp>
#include <popups/popup_question.hpp>
#include <content/popups/popup_tasks_waiting.hpp>
#include <content/popups/popup_unsaved_changes.hpp>
#include <content/popups/popup_crash_recovered.hpp>

#include <GLFW/glfw3.h>
#include <hex/api/theme_manager.hpp>
#include <hex/helpers/default_paths.hpp>

namespace hex::plugin::builtin {

    static void openFile(const std::fs::path &path) {
        TaskManager::doLater([path] {
            if (path.extension() == ".hexproj") {
                if (!ProjectFile::load(path)) {
                    ui::ToastError::open(fmt::format("hex.builtin.popup.error.project.load"_lang, wolv::util::toUTF8String(path)));
                }

                return;
            }

            auto provider = ImHexApi::Provider::createProvider("hex.builtin.provider.file", true);
            if (auto *fileProvider = dynamic_cast<FileProvider*>(provider.get()); fileProvider != nullptr) {
                fileProvider->setPath(path);

                ImHexApi::Provider::openProvider(provider);

                AchievementManager::unlockAchievement("hex.builtin.achievement.starting_out", "hex.builtin.achievement.starting_out.open_file.name");

                glfwRequestWindowAttention(ImHexApi::System::getMainWindowHandle());
                glfwFocusWindow(ImHexApi::System::getMainWindowHandle());
            }
        });
    }

    void registerEventHandlers() {

        static bool imhexClosing = false;
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
            if (ImHexApi::Provider::isDirty() && !imhexClosing) {
                glfwSetWindowShouldClose(window, GLFW_FALSE);
                ui::PopupQuestion::open("hex.builtin.popup.exit_application.desc"_lang,
                    [] {
                        imhexClosing = true;
                        for (const auto &provider : ImHexApi::Provider::getProviders())
                            ImHexApi::Provider::remove(provider);
                    },
                    [] { }
                );
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
                if (ImHexApi::Provider::isDirty()) {
                    ui::PopupQuestion::open("hex.builtin.popup.exit_application.desc"_lang,
                        [] {
                            for (const auto &provider : ImHexApi::Provider::getProviders())
                                ImHexApi::Provider::remove(provider);
                        },
                        [] { }
                    );
                } else if (TaskManager::getRunningTaskCount() > 0 || TaskManager::getRunningBackgroundTaskCount() > 0) {
                    TaskManager::doLater([] {
                        for (auto &task : TaskManager::getRunningTasks())
                            task->interrupt();
                        PopupTasksWaiting::open([]() {
                            EventCloseButtonPressed::post();
                        });
                    });
                } else {
                    for (const auto &provider : ImHexApi::Provider::getProviders())
                        ImHexApi::Provider::remove(provider);
                }
            } else {
                ImHexApi::System::closeImHex();
            }
        });

        EventProviderClosing::subscribe([](const prv::Provider *provider, bool *shouldClose) {
            if (provider->isDirty()) {
                *shouldClose = false;
                PopupUnsavedChanges::open("hex.builtin.popup.close_provider.desc"_lang,
                    []{
                        const bool projectSaved = ProjectFile::hasPath() ? saveProject() : saveProjectAs();
                        if (projectSaved) {
                            for (const auto &provider : ImHexApi::Provider::impl::getClosingProviders())
                                ImHexApi::Provider::remove(provider, true);

                            if (imhexClosing)
                                ImHexApi::System::closeImHex(true);
                        } else {
                            ImHexApi::Provider::impl::resetClosingProvider();
                            imhexClosing = false;
                        }
                    },
                    [] {
                        for (const auto &provider : ImHexApi::Provider::impl::getClosingProviders())
                            ImHexApi::Provider::remove(provider, true);

                        if (imhexClosing)
                            ImHexApi::System::closeImHex(true);
                    },
                    [] {
                        ImHexApi::Provider::impl::resetClosingProvider();
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
                auto newProvider = ImHexApi::Provider::createProvider("hex.builtin.provider.mem_file", true);
                ImHexApi::Provider::openProvider(newProvider);
            } else if (name == "Open File") {
                fs::openFileBrowser(fs::DialogMode::Open, { }, [](const auto &path) {
                    if (path.extension() == ".hexproj") {
                        if (!ProjectFile::load(path)) {
                            ui::ToastError::open(fmt::format("hex.builtin.popup.error.project.load"_lang, wolv::util::toUTF8String(path)));
                        } else {
                            return;
                        }
                    }

                    auto provider = ImHexApi::Provider::createProvider("hex.builtin.provider.file", true);
                    auto newProvider = static_cast<FileProvider*>(provider.get());

                    if (newProvider == nullptr)
                        return;

                    newProvider->setPath(path);
                    ImHexApi::Provider::openProvider(provider);
                    AchievementManager::unlockAchievement("hex.builtin.achievement.starting_out", "hex.builtin.achievement.starting_out.open_file.name");
                }, {}, true);
            } else if (name == "Open Project") {
                fs::openFileBrowser(fs::DialogMode::Open, { {"Project File", "hexproj"} },
                    [](const auto &path) {
                        if (!ProjectFile::load(path)) {
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
                if (!filePickerProvider->handleFilePicker()) {
                    TaskManager::doLater([provider] { ImHexApi::Provider::remove(provider.get()); });
                    return;
                }

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
             for (const auto &[extensions, handler] : ContentRegistry::FileTypeHandler::impl::getEntries()) {
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
            const auto prevLaunchVersion = ContentRegistry::Settings::read<std::string>("hex.builtin.setting.general", "hex.builtin.setting.general.prev_launch_version", "");

            const auto forceOobe = getEnvironmentVariable("IMHEX_FORCE_OOBE");
            if (prevLaunchVersion.empty() || (forceOobe.has_value() && *forceOobe != "0")) {
                EventFirstLaunch::post();
                return;
            }

            const auto prevLaunchVersionParsed = SemanticVersion(prevLaunchVersion);

            if (currVersion != prevLaunchVersionParsed) {
                EventImHexUpdated::post(prevLaunchVersionParsed, currVersion);

                ContentRegistry::Settings::write<std::string>("hex.builtin.setting.general", "hex.builtin.setting.general.prev_launch_version", currVersion.get(false));
            }
        });

        EventWindowDeinitializing::subscribe([](GLFWwindow *window) {
            WorkspaceManager::exportToFile();
            if (auto workspace = WorkspaceManager::getCurrentWorkspace(); workspace != WorkspaceManager::getWorkspaces().end())
                ContentRegistry::Settings::write<std::string>("hex.builtin.setting.general", "hex.builtin.setting.general.curr_workspace", workspace->first);

            {
                int x = 0, y = 0, width = 0, height = 0, maximized = 0;
                glfwGetWindowPos(window, &x, &y);
                glfwGetWindowSize(window, &width, &height);
                maximized = glfwGetWindowAttrib(window, GLFW_MAXIMIZED);

                ContentRegistry::Settings::write<int>("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.x", x);
                ContentRegistry::Settings::write<int>("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.y", y);
                ContentRegistry::Settings::write<int>("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.width", width);
                ContentRegistry::Settings::write<int>("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.height", height);
                ContentRegistry::Settings::write<int>("hex.builtin.setting.interface", "hex.builtin.setting.interface.window.maximized", maximized);
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

            if (ImGui::IsAnyItemHovered())
                return;

            static ImGuiWindow *lastFocusedWindow = nullptr;

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

        RequestOpenProvider::subscribe([](std::shared_ptr<prv::Provider> provider) {
            TaskManager::createBlockingTask("hex.builtin.provider.opening", TaskManager::NoProgress, [provider]() {
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

    }

}
