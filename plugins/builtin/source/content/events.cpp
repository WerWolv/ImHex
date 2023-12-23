#include <hex/api/event_manager.hpp>

#include <hex/api/localization_manager.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/api/achievement_manager.hpp>

#include <hex/providers/provider.hpp>
#include <hex/ui/view.hpp>

#include <imgui.h>

#include <content/providers/file_provider.hpp>

#include <wolv/io/fs.hpp>
#include <wolv/utils/string.hpp>

#include <popups/popup_notification.hpp>
#include <popups/popup_question.hpp>
#include <content/popups/popup_tasks_waiting.hpp>
#include <content/popups/popup_unsaved_changes.hpp>

namespace hex::plugin::builtin {

    static void openFile(const std::fs::path &path) {
        auto provider = ImHexApi::Provider::createProvider("hex.builtin.provider.file", true);
        if (auto *fileProvider = dynamic_cast<FileProvider*>(provider); fileProvider != nullptr) {
            fileProvider->setPath(path);
            if (!provider->open() || !provider->isAvailable()) {
                ui::PopupError::open(hex::format("hex.builtin.provider.error.open"_lang, provider->getErrorMessage()));
                TaskManager::doLater([provider] { ImHexApi::Provider::remove(provider); });
            } else {
                EventProviderOpened::post(fileProvider);
            }
        }
    }

    void registerEventHandlers() {

        static bool imhexClosing = false;
        EventWindowClosing::subscribe([](GLFWwindow *window) {
            imhexClosing = false;
            if (ImHexApi::Provider::isDirty() && !imhexClosing) {
                glfwSetWindowShouldClose(window, GLFW_FALSE);
                ui::PopupQuestion::open("hex.builtin.popup.exit_application.desc"_lang,
                    [] {
                        imhexClosing = true;
                        for (const auto &provider : auto(ImHexApi::Provider::getProviders()))
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
            hex::unused(oldProvider);
            hex::unused(newProvider);

            RequestUpdateWindowTitle::post();
        });

        EventProviderOpened::subscribe([](hex::prv::Provider *provider) {
            if (provider != nullptr && ImHexApi::Provider::get() == provider)
                RequestUpdateWindowTitle::post();
            EventProviderChanged::post(nullptr, provider);
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
                            ui::PopupError::open(hex::format("hex.builtin.popup.error.project.load"_lang, wolv::util::toUTF8String(path)));
                        }
                    } else {
                        FileProvider* newProvider = static_cast<FileProvider*>(
                            ImHexApi::Provider::createProvider("hex.builtin.provider.file", true)
                        );

                        if (newProvider == nullptr)
                            return;

                        newProvider->setPath(path);
                        if (!newProvider->open())
                            hex::ImHexApi::Provider::remove(newProvider);
                        else {
                            EventProviderOpened::post(newProvider);
                            AchievementManager::unlockAchievement("hex.builtin.achievement.starting_out", "hex.builtin.achievement.starting_out.open_file.name");
                        }

                    }
                });
            } else if (name == "Open Project") {
                fs::openFileBrowser(fs::DialogMode::Open, { {"Project File", "hexproj"} },
                    [](const auto &path) {
                        if (!ProjectFile::load(path)) {
                            ui::PopupError::open(hex::format("hex.builtin.popup.error.project.load"_lang, wolv::util::toUTF8String(path)));
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
                    ui::PopupError::open(hex::format("hex.builtin.provider.error.open"_lang, provider->getErrorMessage()));
                    TaskManager::doLater([provider] { ImHexApi::Provider::remove(provider); });
                    return;
                }

                EventProviderOpened::post(provider);
            }
            else if (!provider->hasLoadInterface()) {
                if (!provider->open() || !provider->isAvailable()) {
                    ui::PopupError::open(hex::format("hex.builtin.provider.error.open"_lang, provider->getErrorMessage()));
                    TaskManager::doLater([provider] { ImHexApi::Provider::remove(provider); });
                    return;
                }

                EventProviderOpened::post(provider);
            }
        });

        EventRegionSelected::subscribe([](const ImHexApi::HexEditor::ProviderRegion &region) {
           ImHexApi::HexEditor::impl::setCurrentSelection(region);
        });

        RequestOpenInfoPopup::subscribe([](const std::string &message) {
            ui::PopupInfo::open(message);
        });

        RequestOpenErrorPopup::subscribe([](const std::string &message) {
            ui::PopupError::open(message);
        });

        RequestOpenFatalPopup::subscribe([](const std::string &message) {
            ui::PopupFatal::open(message);
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
