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

#include "provider_extra_data.hpp"

#include "content/providers/file_provider.hpp"

namespace hex::plugin::builtin {

    static void openFile(const std::fs::path &path) {
        auto provider = ImHexApi::Provider::createProvider("hex.builtin.provider.file", true);
        if (auto *fileProvider = dynamic_cast<prv::FileProvider*>(provider); fileProvider != nullptr) {
            fileProvider->setPath(path);
            (void)fileProvider->open();
        }
    }

    void registerEventHandlers() {

        EventManager::subscribe<EventWindowClosing>([](GLFWwindow *window) {
            if (ImHexApi::Provider::isDirty()) {
                glfwSetWindowShouldClose(window, GLFW_FALSE);
                ImHexApi::Tasks::doLater([] { ImGui::OpenPopup("hex.builtin.popup.exit_application.title"_lang); });
            }
        });

        EventManager::subscribe<EventProviderClosing>([](hex::prv::Provider *provider, bool *shouldClose) {
            if (provider->isDirty()) {
                *shouldClose = false;
                ImHexApi::Tasks::doLater([] { ImGui::OpenPopup("hex.builtin.popup.close_provider.title"_lang); });
            }
        });

        EventManager::subscribe<EventProviderChanged>([](hex::prv::Provider *oldProvider, hex::prv::Provider *newProvider) {
            hex::unused(oldProvider);

            if (newProvider != nullptr) {
                EventManager::post<RequestChangeWindowTitle>(newProvider->getName());
            } else {
                EventManager::post<RequestChangeWindowTitle>("");
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
                ImHexApi::Provider::createProvider("hex.builtin.provider.file");
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
            if (provider->shouldSkipLoadInterface())
                return;

            if (provider->hasFilePicker()) {
                if (!provider->handleFilePicker()) {
                    ImHexApi::Tasks::doLater([provider] { ImHexApi::Provider::remove(provider); });
                    return;
                }
                if (!provider->open()) {
                    View::showErrorPopup("hex.builtin.popup.error.open"_lang);
                    ImHexApi::Tasks::doLater([provider] { ImHexApi::Provider::remove(provider); });
                    return;
                }
            }
            else if (provider->hasLoadInterface())
                EventManager::post<RequestOpenPopup>(View::toWindowName("hex.builtin.view.provider_settings.load_popup"));
            else {
                if (!provider->open() || !provider->isAvailable()) {
                    View::showErrorPopup("hex.builtin.popup.error.open"_lang);
                    ImHexApi::Tasks::doLater([provider] { ImHexApi::Provider::remove(provider); });
                }
            }
        });

        EventManager::subscribe<EventProviderDeleted>([](hex::prv::Provider *provider) {
            ProviderExtraData::erase(provider);
        });

    }

}