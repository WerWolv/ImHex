#include "internal.hpp"

#include <content/project.hpp>

#include <hex/api/achievement_manager.hpp>
#include <hex/api/events/events_lifecycle.hpp>
#include <hex/api/events/requests_gui.hpp>
#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/project_manager.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/providers/file_backed_provider_data.hpp>
#include <hex/providers/provider.hpp>

#include <toasts/toast_notification.hpp>

#include <wolv/utils/guards.hpp>
#include <wolv/utils/string.hpp>

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin::project::impl {

    bool load(const std::fs::path &filePath) {
        resetSidebarState();
        if (!wolv::io::fs::isDirectory(filePath)) {
            ui::ToastError::open(fmt::format("hex.builtin.popup.error.project.load"_lang,
                fmt::format("hex.builtin.popup.error.project.load.file_not_found"_lang,
                    wolv::util::toUTF8String(filePath))));
            return false;
        }

        auto &projectState = state();
        if (!std::fs::is_regular_file(filePath / projectSettingsPath())) {
            std::error_code metadataError;
            if (ProjectManager::isTemporaryProject() && !std::fs::is_empty(filePath, metadataError))
                return false;
            if (metadataError)
                return false;

            const auto metadataDirectory = filePath / ProjectManager::ProjectDirectory;
            if (std::fs::exists(metadataDirectory, metadataError) &&
                (!std::fs::is_directory(metadataDirectory, metadataError) || !std::fs::is_empty(metadataDirectory, metadataError)))
                return false;
            if (metadataError)
                return false;

            projectState.associations.clear();
            projectState.projectProviderIds.clear();
            projectState.closedProjectProviderIds.clear();
            projectState.projectProviderSettings.clear();
            for (const auto *provider : ImHexApi::Provider::getProviders())
                projectState.projectProviderIds.insert(provider->getID());

            std::error_code error;
            std::fs::create_directories(filePath / ProjectManager::ProjectDirectory, error);
            if (error) {
                log::error("Failed to create project metadata directory at {}: {}", (filePath / ProjectManager::ProjectDirectory).string(), error.message());
                return false;
            }

            ProjectManager::setPath(filePath);
            ProjectManager::setFolderProject(true);
            materializeRegisteredData();
            return store(std::nullopt, true);
        }

        if (!validateProjectSettings(filePath))
            return false;
        if (!readProviderManifest(filePath, true).has_value())
            return false;

        auto originalPath = ProjectManager::getPath();
        projectState.loadingProject = true;
        auto resetLoading = SCOPE_GUARD { projectState.loadingProject = false; };
        for (const auto &provider : ImHexApi::Provider::getProviders())
            ImHexApi::Provider::remove(provider, true);

        ProjectManager::setPath(filePath);
        auto resetPath = SCOPE_GUARD { ProjectManager::setPath(originalPath); };

        if (!loadProviders(filePath))
            return false;

        loadAssociations(filePath);
        bindRegisteredData();

        resetLoading.release();
        projectState.loadingProject = false;
        resetPath.release();
        EventProjectOpened::post();
        RequestUpdateWindowTitle::post();

        return true;
    }

    bool store(std::optional<std::fs::path> filePath, bool updateLocation) {
        auto originalPath = ProjectManager::getPath();
        const auto originalFolderProject = ProjectManager::isFolderProject();

        if (!filePath.has_value())
            filePath = originalPath;

        std::error_code directoryError;
        const bool destinationExisted = std::fs::exists(*filePath, directoryError);
        if (directoryError)
            return false;
        std::fs::create_directories(*filePath, directoryError);
        if (directoryError)
            return false;

        auto &projectState = state();
        const bool promotingTemporaryProject = updateLocation &&
            originalPath.lexically_normal() == ProjectManager::getTemporaryProjectPath().lexically_normal() &&
            filePath->lexically_normal() != originalPath.lexically_normal();
        const auto originalProviderSettings = promotingTemporaryProject ? projectState.projectProviderSettings : ProviderSettings {};
        struct RelocatedProviderPath {
            prv::IProviderFilePicker *provider;
            std::fs::path oldPath;
        };
        std::vector<std::fs::path> copiedProjectEntries;
        std::vector<RelocatedProviderPath> relocatedProviderPaths;
        const auto cleanupPromotion = [&] {
            for (const auto &entry : copiedProjectEntries) {
                std::error_code error;
                std::fs::remove_all(entry, error);
            }
            std::error_code error;
            std::fs::remove_all(*filePath / ProjectManager::ProjectDirectory, error);
            if (!destinationExisted)
                std::fs::remove(*filePath, error);
        };
        if (promotingTemporaryProject) {
            for (auto *provider : ImHexApi::Provider::getProviders()) {
                auto *filePicker = dynamic_cast<prv::IProviderFilePicker *>(provider);
                std::fs::path relativePath;
                if (filePicker != nullptr && isPathInProject(filePicker->getPickedPath(), originalPath, relativePath) && !filePicker->flushFile()) {
                    cleanupPromotion();
                    return false;
                }
            }
            for (auto *data : FileBackedProviderDataRegistry::getTypes()) {
                if (!data->flush()) {
                    cleanupPromotion();
                    return false;
                }
            }
            if (!copyTemporaryProjectFiles(originalPath, *filePath, copiedProjectEntries)) {
                cleanupPromotion();
                return false;
            }

            for (auto *provider : ImHexApi::Provider::getProviders()) {
                auto *filePicker = dynamic_cast<prv::IProviderFilePicker *>(provider);
                if (filePicker == nullptr || filePicker->getPickedPath().empty())
                    continue;

                std::fs::path relativePath;
                if (isPathInProject(filePicker->getPickedPath(), originalPath, relativePath)) {
                    const auto oldPath = filePicker->getPickedPath();
                    if (!filePicker->relocateFile(*filePath / relativePath)) {
                        for (auto relocation = relocatedProviderPaths.rbegin(); relocation != relocatedProviderPaths.rend(); ++relocation)
                            std::ignore = relocation->provider->relocateFile(relocation->oldPath);
                        cleanupPromotion();
                        return false;
                    }
                    relocatedProviderPaths.push_back({ filePicker, oldPath });
                }
            }
        }

        ProjectManager::setPath(filePath.value());
        ProjectManager::setFolderProject(true);
        auto resetPath = SCOPE_GUARD {
            ProjectManager::setPath(originalPath);
            ProjectManager::setFolderProject(originalFolderProject);
            for (auto relocation = relocatedProviderPaths.rbegin(); relocation != relocatedProviderPaths.rend(); ++relocation)
                std::ignore = relocation->provider->relocateFile(relocation->oldPath);
            if (promotingTemporaryProject) {
                projectState.projectProviderSettings = originalProviderSettings;
                for (auto *provider : ImHexApi::Provider::getProviders())
                    provider->markMetadataDirty(true);
                cleanupPromotion();
            }
        };

        if (!originalFolderProject) {
            projectState.projectProviderIds.clear();
            projectState.closedProjectProviderIds.clear();
            projectState.projectProviderSettings.clear();
            for (const auto *provider : ImHexApi::Provider::getProviders())
                projectState.projectProviderIds.insert(provider->getID());
        }

        materializeRegisteredData();

        ProviderSettings storedSettings;
        bool result = storeProviders(*filePath, originalPath, storedSettings) && storeAssociations(*filePath);

        for (auto *data : FileBackedProviderDataRegistry::getTypes())
            result = data->flush() && result;

        if (!result)
            log::error("Failed to write project storage at {}", filePath->string());
        if (!result)
            return false;

        if (updateLocation)
            projectState.projectProviderSettings = std::move(storedSettings);

        for (const auto &provider : ImHexApi::Provider::getProviders())
            provider->markMetadataDirty(false);

        if (updateLocation) {
            if (promotingTemporaryProject) {
                struct RelocatedBinding {
                    prv::Provider *provider;
                    std::string typeId;
                    std::fs::path oldPath;
                };
                std::vector<RelocatedBinding> relocatedBindings;
                const auto rollbackBindings = [&] {
                    for (auto binding = relocatedBindings.rbegin(); binding != relocatedBindings.rend(); ++binding)
                        std::ignore = FileBackedProviderDataRegistry::relocate(binding->provider, binding->typeId, binding->oldPath);
                };
                for (auto *provider : ImHexApi::Provider::getProviders()) {
                    const auto associations = projectState.associations.find(provider->getID());
                    if (associations == projectState.associations.end())
                        continue;

                    for (const auto &[typeId, relativePath] : associations->second) {
                        const auto oldBinding = FileBackedProviderDataRegistry::getBinding(provider, typeId);
                        if (!oldBinding.has_value())
                            continue;
                        if (!FileBackedProviderDataRegistry::relocate(provider, typeId, *filePath / relativePath)) {
                            rollbackBindings();
                            return false;
                        }
                        relocatedBindings.push_back({ provider, typeId, *oldBinding });
                    }
                }

                std::error_code cleanupError;
                auto promotedPath = std::fs::path(originalPath.string() + ".promoted");
                for (u32 index = 1; std::fs::exists(promotedPath, cleanupError) && !cleanupError; ++index)
                    promotedPath = std::fs::path(originalPath.string() + ".promoted-" + std::to_string(index));
                if (cleanupError) {
                    rollbackBindings();
                    return false;
                }
                if (!writeProjectFile(originalPath, std::fs::path(ProjectManager::ProjectDirectory) / "promoted", {})) {
                    rollbackBindings();
                    return false;
                }
                std::fs::rename(originalPath, promotedPath, cleanupError);
                if (cleanupError) {
                    std::error_code markerError;
                    std::fs::remove(originalPath / ProjectManager::ProjectDirectory / "promoted", markerError);
                    rollbackBindings();
                    return false;
                }

                resetPath.release();
                std::fs::remove_all(promotedPath, cleanupError);
                if (cleanupError)
                    log::warn("Failed to remove promoted temporary project at {}: {}", promotedPath.string(), cleanupError.message());
            } else {
                resetPath.release();
            }

            RequestUpdateWindowTitle::post();
            AchievementManager::unlockAchievement("hex.builtin.achievement.starting_out"_unlocalized, "hex.builtin.achievement.starting_out.save_project.name"_unlocalized);
            EventProjectSaved::post();
        }

        return result;
    }

}

namespace hex::plugin::builtin::project {

    std::string createEmptyProject(const std::filesystem::path &path) {
        if (path.empty())
            return "hex.builtin.popup.error.project.create.no_folder_selected"_lang;

        const auto metadataDirectory = path / ProjectManager::ProjectDirectory;
        std::error_code error;
        std::filesystem::create_directories(path, error);
        if (error)
            return fmt::format("hex.builtin.popup.error.project.create.folder_failed"_lang, error.message());
        if (!std::filesystem::is_directory(path, error))
            return "hex.builtin.popup.error.project.create.not_directory"_lang;

        if (!std::filesystem::create_directory(metadataDirectory, error)) {
            if (error)
                return fmt::format("hex.builtin.popup.error.project.create.metadata_failed"_lang, error.message());
            return "hex.builtin.popup.error.project.create.metadata_exists"_lang;
        }

        if (!std::filesystem::create_directory(metadataDirectory / "providers", error)) {
            std::error_code cleanupError;
            std::filesystem::remove(metadataDirectory, cleanupError);
            if (error)
                return fmt::format("hex.builtin.popup.error.project.create.metadata_failed"_lang, error.message());
            return "hex.builtin.popup.error.project.create.metadata_directory_exists"_lang;
        }

        const bool initialized = impl::writeProjectFile(path, impl::projectSettingsPath(), nlohmann::json({
                { "version", 1 },
                { "associations", nlohmann::json::object() }
            }).dump(4)) &&
            impl::writeProjectFile(path, std::filesystem::path(ProjectManager::ProjectDirectory) / "providers/providers.json", nlohmann::json({
                { "providers", nlohmann::json::array() },
                { "closedProviders", nlohmann::json::array() }
            }).dump(4));
        if (!initialized) {
            std::filesystem::remove_all(metadataDirectory, error);
            return "hex.builtin.popup.error.project.create.initialize_failed"_lang;
        }

        if (!impl::load(path))
            return "hex.builtin.popup.error.project.create.open_failed"_lang;

        return {};
    }

    bool prepareForShutdown(bool persist) {
        auto &projectState = impl::state();
        if (ProjectManager::isFolderProject()) {
            if (persist) {
                bool result = true;
                for (auto *data : FileBackedProviderDataRegistry::getTypes())
                    result = data->flush() && result;
                if (!impl::persistProjectMetadata() || !result)
                    return false;
            }
            projectState.skipShutdownStore = true;
        }
        projectState.loadingProject = true;
        return true;
    }

}
