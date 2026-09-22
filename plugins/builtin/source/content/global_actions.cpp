
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

    static bool hasPendingFileBackedData(const prv::Provider *provider) {
        return std::ranges::any_of(FileBackedProviderDataRegistry::getTypes(), [provider](const auto *data) {
            return data->hasPendingData(provider);
        });
    }

    static bool flushFileBackedData() {
        bool result = true;
        for (auto *data : FileBackedProviderDataRegistry::getTypes())
            result = data->flush() && result;
        return result;
    }

    bool saveProviderData() {
        bool result = true;
        for (auto *provider : ImHexApi::Provider::getProviders()) {
            if (!provider->isDataDirty())
                continue;

            if (provider->isSavable()) {
                provider->save();
            } else if (auto *filePicker = dynamic_cast<prv::IProviderFilePicker *>(provider); filePicker != nullptr && filePicker->flushFile()) {
                provider->markDataDirty(false);
            }

            result = !provider->isDataDirty() && result;
        }
        return result;
    }

    void openProject() {
        fs::openFileBrowser(fs::DialogMode::Folder, { },
                            [](const auto &path) {
                                const auto finishOpening = [path](bool saveData, bool saveMetadata) {
                                    if ((saveData || saveMetadata) && !flushFileBackedData()) {
                                        ui::ToastError::open("hex.builtin.popup.error.project.save"_lang);
                                        return;
                                    }
                                    if (saveData && !saveProviderData()) {
                                        ui::ToastError::open("hex.builtin.popup.error.project.save"_lang);
                                        return;
                                    }
                                    if (saveMetadata && !saveProject())
                                        return;
                                    if (!ProjectManager::load(path))
                                        ui::ToastError::open(fmt::format("hex.builtin.popup.error.project.load"_lang, wolv::util::toUTF8String(path)));
                                };

                                std::vector<ProviderDirtyState> dirtyStates;
                                for (auto *provider : ImHexApi::Provider::getProviders()) {
                                    const bool metadataDirty = provider->isMetadataDirty() || hasPendingFileBackedData(provider);
                                    if (provider->isDataDirty() || metadataDirty) {
                                        dirtyStates.push_back({
                                            .provider = provider,
                                            .dataDirty = provider->isDataDirty(),
                                            .metadataDirty = metadataDirty
                                        });
                                    }
                                }
                                if (dirtyStates.empty()) {
                                    finishOpening(false, false);
                                    return;
                                }

                                PopupUnsavedChanges::open(std::move(dirtyStates),
                                    [finishOpening] { finishOpening(true, false); },
                                    [finishOpening] { finishOpening(true, true); },
                                    [finishOpening] { finishOpening(false, false); },
                                    [] { });
                            });
    }

    void closeProject() {
        if (!ProjectManager::isFolderProject() || ProjectManager::isTemporaryProject())
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
            if (saveData || saveMetadata) {
                if (!flushFileBackedData()) {
                    ui::ToastError::open("hex.builtin.popup.error.project.save"_lang);
                    return;
                }
            }

            if (saveData) {
                if (!saveProviderData()) {
                    ui::ToastError::open("hex.builtin.popup.error.project.save"_lang);
                    return;
                }
            }

            if (saveMetadata && !saveProject())
                return;

            const auto providers = ImHexApi::Provider::getProviders();
            ProjectManager::clearPath();
            EventProjectClosed::post();
            for (auto *provider : providers)
                ImHexApi::Provider::remove(provider, true);
            if (!ProjectManager::loadTemporaryProject())
                log::error("Failed to restore the temporary project");
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
        bool saved = false;
        if (!fs::openFileBrowser(fs::DialogMode::Folder, { }, [&saved](const std::fs::path &path) {
            saved = ProjectManager::store(path);
            if (!saved)
                ui::ToastError::open("hex.builtin.popup.error.project.save"_lang);
        }))
            return false;
        return saved;
    }

}
