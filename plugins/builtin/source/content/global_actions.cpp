
#include <content/global_actions.hpp>
#include <hex/ui/view.hpp>
#include <hex/api/project_manager.hpp>
#include <hex/api/events/events_lifecycle.hpp>
#include <hex/api/events/requests_gui.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/providers/provider.hpp>
#include <hex/providers/file_backed_provider_data.hpp>

#include <toasts/toast_notification.hpp>
#include <content/popups/popup_unsaved_changes.hpp>

#include <wolv/utils/string.hpp>

namespace hex::plugin::builtin {

    void openProject() {
        fs::openFileBrowser(fs::DialogMode::Folder, { },
                            [](const auto &path) {
                                if (!ProjectManager::load(path)) {
                                    ui::ToastError::open(fmt::format("hex.builtin.popup.error.project.load"_lang, wolv::util::toUTF8String(path)));
                                }
                            });
    }

    void closeProject() {
        if (!ProjectManager::isFolderProject())
            return;

        std::vector<ProviderDirtyState> dirtyStates;
        for (auto *provider : ImHexApi::Provider::getProviders()) {
            if (provider->isDataDirty() || provider->isMetadataDirty()) {
                dirtyStates.push_back({
                    .provider = provider,
                    .dataDirty = provider->isDataDirty(),
                    .metadataDirty = provider->isMetadataDirty()
                });
            }
        }

        const auto finishClosing = [](bool saveData, bool saveMetadata) {
            for (auto *data : FileBackedProviderDataRegistry::getTypes())
                std::ignore = data->flush();

            if (saveData) {
                for (auto *provider : ImHexApi::Provider::getProviders()) {
                    if (provider->isDataDirty() && provider->isSavable())
                        provider->save();
                }
            }

            if (saveMetadata && !saveProject())
                return;

            const auto providers = ImHexApi::Provider::getProviders();
            ProjectManager::store();
            ProjectManager::clearPath();
            EventProjectClosed::post();
            for (auto *provider : providers)
                ImHexApi::Provider::remove(provider, true);
            RequestUpdateWindowTitle::post();
        };

        if (dirtyStates.empty()) {
            finishClosing(false, true);
            return;
        }

        PopupUnsavedChanges::open(std::move(dirtyStates),
            [finishClosing] { finishClosing(true, false); },
            [finishClosing] { finishClosing(true, true); },
            [finishClosing] { finishClosing(false, false); },
            [] { });
    }

    bool saveProject() {
        if (!ProjectManager::hasPath() && !ImHexApi::Provider::isValid()) {
            log::info("Cannot save project because no project or provider is open");
            return false;
        }

        if (ProjectManager::hasPath()) {
            log::info("Saving project to: {}", wolv::util::toUTF8String(ProjectManager::getPath()));
            if (!ProjectManager::store()) {
                ui::ToastError::open("hex.builtin.popup.error.project.save"_lang);
                return false;
            } else {
                log::debug("Project saved");
                return true;
            }
        } else {
            return saveProjectAs();
        }
    }

    bool saveProjectAs() {
        return fs::openFileBrowser(fs::DialogMode::Folder, { },
                            [](const std::fs::path &path) {
                                if (!ProjectManager::store(path)) {
                                    ui::ToastError::open("hex.builtin.popup.error.project.save"_lang);
                                }
                            });
    }

}
