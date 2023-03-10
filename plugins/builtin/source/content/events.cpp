#include <hex/api/event.hpp>
#include <hex/api/content_registry.hpp>

#include <hex/providers/provider.hpp>
#include <hex/ui/view.hpp>
#include <hex/api/localization.hpp>
#include <hex/helpers/file.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/api/project_file_manager.hpp>

#include <imgui.h>
#include <nlohmann/json.hpp>

#include <content/helpers/provider_extra_data.hpp>

#include <content/providers/file_provider.hpp>

namespace hex::plugin::builtin {

    static void openFile(const std::fs::path &path) {
        auto provider = ImHexApi::Provider::createProvider("hex.builtin.provider.file", true);
        if (auto *fileProvider = dynamic_cast<FileProvider*>(provider); fileProvider != nullptr) {
            fileProvider->setPath(path);
            if (fileProvider->open())
                EventManager::post<EventProviderOpened>(fileProvider);
        }
    }

    void registerEventHandlers() {

        EventManager::subscribe<EventWindowClosing>([](GLFWwindow *window) {
            if (ImHexApi::Provider::isDirty()) {
                glfwSetWindowShouldClose(window, GLFW_FALSE);
                TaskManager::doLater([] { ImGui::OpenPopup("hex.builtin.popup.exit_application.title"_lang); });
            } else if (TaskManager::getRunningTaskCount() > 0 || TaskManager::getRunningBackgroundTaskCount() > 0) {
                glfwSetWindowShouldClose(window, GLFW_FALSE);
                TaskManager::doLater([] {
                    for (auto &task : TaskManager::getRunningTasks())
                        task->interrupt();
                    ImGui::OpenPopup("hex.builtin.popup.waiting_for_tasks.title"_lang);
                });
            }
        });

        EventManager::subscribe<EventProviderClosing>([](hex::prv::Provider *provider, bool *shouldClose) {
            if (provider->isDirty()) {
                *shouldClose = false;
                TaskManager::doLater([] { ImGui::OpenPopup("hex.builtin.popup.close_provider.title"_lang); });
            }
        });

        EventManager::subscribe<EventProviderChanged>([](hex::prv::Provider *oldProvider, hex::prv::Provider *newProvider) {
            hex::unused(oldProvider);
            hex::unused(newProvider);

            EventManager::post<RequestUpdateWindowTitle>();
        });

        EventManager::subscribe<EventProviderOpened>([](hex::prv::Provider *provider) {
            if (provider != nullptr && ImHexApi::Provider::get() == provider)
                EventManager::post<RequestUpdateWindowTitle>();
        });

        EventManager::subscribe<RequestOpenFile>(openFile);

        EventManager::subscribe<RequestOpenWindow>([](const std::string &name) {
            if (name == "Create File") {
                auto newProvider = hex::ImHexApi::Provider::createProvider("hex.builtin.provider.mem_file", true);
                if (newProvider != nullptr && !newProvider->open())
                    hex::ImHexApi::Provider::remove(newProvider);
                else
                    EventManager::post<EventProviderOpened>(newProvider);
            } else if (name == "Open File") {
                ImHexApi::Provider::createProvider("hex.builtin.provider.file");
            } else if (name == "Open Project") {
                fs::openFileBrowser(fs::DialogMode::Open, { {"Project File", "hexproj"} },
                    [](const auto &path) {
                        if (!ProjectFile::load(path)) {
                            View::showErrorPopup("hex.builtin.popup.error.project.load"_lang);
                        }
                    });
            }
        });

        EventManager::subscribe<EventProviderChanged>([](auto, auto) {
            EventManager::post<EventHighlightingChanged>();
        });

        EventManager::subscribe<EventProviderCreated>([](hex::prv::Provider *provider) {
            if (provider->shouldSkipLoadInterface())
                return;

            if (provider->hasFilePicker()) {
                if (!provider->handleFilePicker()) {
                    TaskManager::doLater([provider] { ImHexApi::Provider::remove(provider); });
                    return;
                }
                if (!provider->open()) {
                    View::showErrorPopup("hex.builtin.popup.error.open"_lang);
                    TaskManager::doLater([provider] { ImHexApi::Provider::remove(provider); });
                    return;
                }

                EventManager::post<EventProviderOpened>(provider);
            }
            else if (provider->hasLoadInterface())
                EventManager::post<RequestOpenPopup>(View::toWindowName("hex.builtin.view.provider_settings.load_popup"));
            else {
                if (!provider->open() || !provider->isAvailable()) {
                    View::showErrorPopup("hex.builtin.popup.error.open"_lang);
                    TaskManager::doLater([provider] { ImHexApi::Provider::remove(provider); });
                    return;
                }

                EventManager::post<EventProviderOpened>(provider);
            }
        });

        EventManager::subscribe<EventProviderDeleted>([](hex::prv::Provider *provider) {
            ProviderExtraData::erase(provider);
        });

        EventManager::subscribe<EventRegionSelected>([](const ImHexApi::HexEditor::ProviderRegion &region) {
           ImHexApi::HexEditor::impl::setCurrentSelection(region);
        });

        fs::setFileBrowserErrorCallback([]{
            #if defined(NFD_PORTAL)
                View::showErrorPopup("hex.builtin.popup.error.file_dialog.portal"_lang);
            #else
                View::showErrorPopup("hex.builtin.popup.error.file_dialog.common"_lang);
            #endif
        });

    }

}
