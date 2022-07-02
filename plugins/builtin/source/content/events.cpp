#include <hex/api/event.hpp>
#include <hex/api/content_registry.hpp>

#include <hex/providers/provider.hpp>
#include <hex/ui/view.hpp>
#include <hex/helpers/project_file_handler.hpp>
#include <hex/api/localization.hpp>
#include <hex/helpers/file.hpp>
#include <hex/helpers/logger.hpp>

#include <imgui.h>

#include "content/providers/file_provider.hpp"

namespace hex::plugin::builtin {

    static void openFile(const std::fs::path &path) {
        hex::prv::Provider *provider = nullptr;
        EventManager::post<RequestCreateProvider>("hex.builtin.provider.file", &provider);

        if (auto fileProvider = dynamic_cast<prv::FileProvider *>(provider)) {
            fileProvider->setPath(path);
            if (!fileProvider->open()) {
                View::showErrorPopup("hex.builtin.popup.error.open"_lang);
                ImHexApi::Provider::remove(provider);

                return;
            }
        }

        if (!provider->isWritable()) {
            View::showErrorPopup("hex.builtin.popup.error.read_only"_lang);
        }

        if (!provider->isAvailable()) {
            View::showErrorPopup("hex.builtin.popup.error.open"_lang);
            ImHexApi::Provider::remove(provider);

            return;
        }

        ProjectFile::setFilePath(path);

        EventManager::post<EventFileLoaded>(path);
        EventManager::post<EventDataChanged>();
        EventManager::post<EventHighlightingChanged>();
    }

    void registerEventHandlers() {
        EventManager::subscribe<EventProjectFileLoad>([]() {
            EventManager::post<RequestOpenFile>(ProjectFile::getFilePath());
        });

        EventManager::subscribe<EventWindowClosing>([](GLFWwindow *window) {
            if (ProjectFile::hasUnsavedChanges()) {
                glfwSetWindowShouldClose(window, GLFW_FALSE);
                ImHexApi::Tasks::doLater([] { ImGui::OpenPopup("hex.builtin.popup.exit_application.title"_lang); });
            }
        });

        EventManager::subscribe<RequestOpenFile>(openFile);

        EventManager::subscribe<RequestOpenWindow>([](const std::string &name) {
            if (name == "Create File") {
                fs::openFileBrowser(fs::DialogMode::Save, {}, [](const auto &path) {
                    fs::File file(path, fs::File::Mode::Create);

                    if (!file.isValid()) {
                        View::showErrorPopup("hex.builtin.popup.error.create"_lang);
                        return;
                    }

                    file.setSize(1);

                    EventManager::post<RequestOpenFile>(path);
                });
            } else if (name == "Open File") {
                fs::openFileBrowser(fs::DialogMode::Open, {}, [](const auto &path) {
                    EventManager::post<RequestOpenFile>(path);
                });
            } else if (name == "Open Project") {
                fs::openFileBrowser(fs::DialogMode::Open, { {"Project File", "hexproj"} },
                    [](const auto &path) {
                        ProjectFile::load(path);
                    });
            }
        });

        EventManager::subscribe<EventProviderChanged>([](auto, auto) {
            EventManager::post<EventHighlightingChanged>();
        });

        EventManager::subscribe<EventProviderCreated>([](hex::prv::Provider *provider) {
            if (provider->hasLoadInterface())
                EventManager::post<RequestOpenPopup>(View::toWindowName("hex.builtin.view.provider_settings.load_popup"));
        });
    }

}