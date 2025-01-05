#include <hex/api/event_manager.hpp>

#include <hex/api/localization_manager.hpp>
#include <hex/api/content_registry.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/api/achievement_manager.hpp>
#include <hex/api/workspace_manager.hpp>

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

namespace hex::plugin::builtin {

    static void openFile(const std::fs::path &path) {
        if (path.extension() == ".hexproj") {
            if (!ProjectFile::load(path)) {
                ui::ToastError::open(hex::format("hex.builtin.popup.error.project.load"_lang, wolv::util::toUTF8String(path)));
            } else {
                return;
            }
        }

        auto provider = ImHexApi::Provider::createProvider("hex.builtin.provider.file", true);
        if (auto *fileProvider = dynamic_cast<FileProvider*>(provider); fileProvider != nullptr) {
            fileProvider->setPath(path);
            if (!provider->open() || !provider->isAvailable()) {
                ui::ToastError::open(hex::format("hex.builtin.provider.error.open"_lang, provider->getErrorMessage()));
                TaskManager::doLater([provider] { ImHexApi::Provider::remove(provider); });
            } else {
                EventProviderOpened::post(fileProvider);
                AchievementManager::unlockAchievement("hex.builtin.achievement.starting_out", "hex.builtin.achievement.starting_out.open_file.name");
            }
        }
    }

    void registerEventHandlers() {

        static bool imhexClosing = false;
        EventCrashRecovered::subscribe([](const std::exception &e) {
            PopupCrashRecovered::open(e);
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
                    PopupTasksWaiting::open();
                });
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
                auto newProvider = hex::ImHexApi::Provider::createProvider("hex.builtin.provider.mem_file", true);
                if (newProvider != nullptr && !newProvider->open())
                    hex::ImHexApi::Provider::remove(newProvider);
                else
                    EventProviderOpened::post(newProvider);
            } else if (name == "Open File") {
                fs::openFileBrowser(fs::DialogMode::Open, { }, [](const auto &path) {
                    if (path.extension() == ".hexproj") {
                        if (!ProjectFile::load(path)) {
                            ui::ToastError::open(hex::format("hex.builtin.popup.error.project.load"_lang, wolv::util::toUTF8String(path)));
                        } else {
                            return;
                        }
                    }

                    auto newProvider = static_cast<FileProvider*>(
                        ImHexApi::Provider::createProvider("hex.builtin.provider.file", true)
                    );

                    if (newProvider == nullptr)
                        return;

                    newProvider->setPath(path);
                    if (!newProvider->open()) {
                        hex::ImHexApi::Provider::remove(newProvider);
                    } else {
                        EventProviderOpened::post(newProvider);
                        AchievementManager::unlockAchievement("hex.builtin.achievement.starting_out", "hex.builtin.achievement.starting_out.open_file.name");
                    }
                }, {}, true);
            } else if (name == "Open Project") {
                fs::openFileBrowser(fs::DialogMode::Open, { {"Project File", "hexproj"} },
                    [](const auto &path) {
                        if (!ProjectFile::load(path)) {
                            ui::ToastError::open(hex::format("hex.builtin.popup.error.project.load"_lang, wolv::util::toUTF8String(path)));
                        }
                    });
            }
        });

        EventProviderChanged::subscribe([](auto, auto) {
            EventHighlightingChanged::post();
        });

        // Handles the provider initialization, and calls EventProviderOpened if successful
        EventProviderCreated::subscribe([](hex::prv::Provider *provider) {
            if (provider->shouldSkipLoadInterface())
                return;

            if (provider->hasFilePicker()) {
                if (!provider->handleFilePicker()) {
                    TaskManager::doLater([provider] { ImHexApi::Provider::remove(provider); });
                    return;
                }
                if (!provider->open()) {
                    ui::ToastError::open(hex::format("hex.builtin.provider.error.open"_lang, provider->getErrorMessage()));
                    TaskManager::doLater([provider] { ImHexApi::Provider::remove(provider); });
                    return;
                }

                EventProviderOpened::post(provider);
            }
            else if (!provider->hasLoadInterface()) {
                if (!provider->open() || !provider->isAvailable()) {
                    ui::ToastError::open(hex::format("hex.builtin.provider.error.open"_lang, provider->getErrorMessage()));
                    TaskManager::doLater([provider] { ImHexApi::Provider::remove(provider); });
                    return;
                }

                EventProviderOpened::post(provider);
            }
        });

        EventRegionSelected::subscribe([](const ImHexApi::HexEditor::ProviderRegion &region) {
           ImHexApi::HexEditor::impl::setCurrentSelection(region);
        });

        EventFileDropped::subscribe([](const std::fs::path &path) {
             // Check if a custom file handler can handle the file
             bool handled = false;
             for (const auto &[extensions, handler] : ContentRegistry::FileHandler::impl::getEntries()) {
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

        RequestStartMigration::subscribe([] {
            const auto currVersion = ImHexApi::System::getImHexVersion();
            const auto prevLaunchVersion = ContentRegistry::Settings::read<std::string>("hex.builtin.setting.general", "hex.builtin.setting.general.prev_launch_version", "");
            if (prevLaunchVersion == "") {
                EventFirstLaunch::post();
            }

            EventImHexUpdated::post(SemanticVersion(prevLaunchVersion), currVersion);

            ContentRegistry::Settings::write<std::string>("hex.builtin.setting.general", "hex.builtin.setting.general.prev_launch_version", currVersion.get(false));
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
                LocalizationManager::loadLanguage(it->second);
        });

        EventWindowFocused::subscribe([](bool focused) {
            const auto ctx = ImGui::GetCurrentContext();
            if (ctx == nullptr)
                return;

            if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopup))
                return;
            if (ImGui::IsAnyItemHovered())
                return;

            static ImGuiWindow *lastFocusedWindow = nullptr;

            if (focused) {
                // If the main window gains focus again, restore the last focused window
                ImGui::FocusWindow(lastFocusedWindow);
                ImGui::FocusWindow(lastFocusedWindow, ImGuiFocusRequestFlags_RestoreFocusedChild);

                if (lastFocusedWindow != nullptr)
                    log::debug("Restoring focus on window '{}'", lastFocusedWindow->Name ? lastFocusedWindow->Name : "Unknown Window");
            } else {
                // If the main window loses focus, store the currently focused window
                // and remove focus from it so it doesn't look like it's focused and
                // cursor blink animations don't play
                lastFocusedWindow =  ctx->NavWindow;
                ImGui::FocusWindow(nullptr);

                if (lastFocusedWindow != nullptr)
                    log::debug("Removing focus from window '{}'", lastFocusedWindow->Name ? lastFocusedWindow->Name : "Unknown Window");
            }
        });

        fs::setFileBrowserErrorCallback([](const std::string& errMsg){
            #if defined(NFD_PORTAL)
                ui::PopupError::open(hex::format("hex.builtin.popup.error.file_dialog.portal"_lang, errMsg));
            #else
                ui::PopupError::open(hex::format("hex.builtin.popup.error.file_dialog.common"_lang, errMsg));
            #endif
        });

    }

}
