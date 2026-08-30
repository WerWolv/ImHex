#include "internal.hpp"

#include <content/project.hpp>

#include <algorithm>
#include <cctype>
#include <limits>

#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/project_manager.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/providers/file_backed_provider_data.hpp>
#include <hex/providers/provider.hpp>

#include <wolv/io/file.hpp>

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin::project {

    ImportResult importProviders(std::vector<ImportedProvider> providers, std::vector<ImportedProjectFile> projectFiles) {
        ImportResult result;
        if (!ProjectManager::isFolderProject()) {
            result.error = "hex.builtin.popup.error.project.import_legacy.no_open_project"_lang.get();
            return result;
        }

        for (const auto &provider : providers) {
            if (!provider.descriptor.contains("type") || !provider.descriptor["type"].is_string() ||
                !provider.descriptor.contains("settings") || !provider.descriptor["settings"].is_object()) {
                result.error = fmt::format("hex.builtin.popup.error.project.import_legacy.invalid_provider_settings"_lang,
                    impl::getProviderName(provider.descriptor));
                return result;
            }
        }
        auto &projectState = impl::state();
        std::set<u32> usedIds = projectState.projectProviderIds;
        for (const auto *provider : ImHexApi::Provider::getProviders())
            usedIds.insert(provider->getID());

        std::set<u32> legacyIds;
        std::map<u32, u32> remappedIds;
        u32 nextId = 0;
        for (const auto &provider : providers) {
            const auto providerName = impl::getProviderName(provider.descriptor);
            if (!legacyIds.insert(provider.id).second) {
                result.error = fmt::format("hex.builtin.popup.error.project.import_legacy.duplicate_provider_id"_lang, providerName);
                return result;
            }
            auto id = provider.id;
            if (id >= std::numeric_limits<u32>::max() - 1) {
                result.error = fmt::format("hex.builtin.popup.error.project.import_legacy.reserved_provider_id"_lang, providerName);
                return result;
            }
            if (usedIds.contains(id)) {
                while (usedIds.contains(nextId) && nextId < std::numeric_limits<u32>::max() - 1)
                    nextId += 1;
                if (nextId >= std::numeric_limits<u32>::max() - 1) {
                    result.error = fmt::format("hex.builtin.popup.error.project.import_legacy.no_provider_ids_available"_lang, providerName);
                    return result;
                }
                id = nextId++;
            }
            usedIds.insert(id);
            remappedIds[provider.id] = id;
        }

        for (auto &provider : providers) {
            if (provider.descriptor["type"] == "hex.builtin.provider.view") {
                auto &settings = provider.descriptor["settings"];
                if (settings.contains("id")) {
                    const auto sourceId = settings["id"].get<u32>();
                    if (const auto remapped = remappedIds.find(sourceId); remapped != remappedIds.end())
                        settings["id"] = remapped->second;
                }
            }
            provider.id = remappedIds.at(provider.id);
            prv::Provider::reserveID(provider.id);
        }

        const auto root = ProjectManager::getProjectRoot();
        struct PreparedFile {
            u32 providerId;
            std::string typeId;
            std::fs::path relativePath;
        };
        std::vector<PreparedFile> preparedFiles;
        std::vector<std::fs::path> preparedProjectFiles;
        std::set<std::fs::path> reservedPaths;
        const auto cleanupPreparedFiles = [&] {
            for (const auto &prepared : preparedFiles) {
                std::error_code error;
                std::fs::remove(root / prepared.relativePath, error);
            }
            for (const auto &prepared : preparedProjectFiles) {
                std::error_code error;
                std::fs::remove(root / prepared, error);
            }
        };
        const auto makeProviderStem = [](const ImportedProvider &provider) {
            auto stem = fmt::format("provider-{}", provider.id);
            const auto settings = provider.descriptor.find("settings");
            if (settings == provider.descriptor.end())
                return stem;

            auto name = settings->value("displayName", std::string());
            if (name.empty())
                name = settings->value("name", std::string());
            for (auto &character : name) {
                const auto value = static_cast<unsigned char>(character);
                if (!std::isalnum(value) && character != '-' && character != '_')
                    character = '-';
            }
            if (!name.empty())
                stem = fmt::format("{}-{}", name, provider.id);
            return stem;
        };
        const auto makeUniquePath = [&](const std::string &stem, const std::string &extension) -> std::optional<std::fs::path> {
            auto relativePath = std::fs::path(fmt::format("{}.{}", stem, extension));
            for (u32 suffix = 2; ; suffix += 1) {
                std::error_code statusError;
                const auto status = std::fs::symlink_status(root / relativePath, statusError);
                if (statusError && statusError != std::errc::no_such_file_or_directory) {
                    cleanupPreparedFiles();
                    result.error = fmt::format("hex.builtin.popup.error.project.import_legacy.inspect_path_failed"_lang,
                        relativePath.generic_string(), statusError.message());
                    return std::nullopt;
                }
                if (status.type() == std::fs::file_type::not_found && !reservedPaths.contains(relativePath))
                    break;
                relativePath = std::fs::path(fmt::format("{}-{}.{}", stem, suffix, extension));
            }
            reservedPaths.insert(relativePath);
            return relativePath;
        };
        const auto writeProjectFile = [&](const std::fs::path &relativePath, const std::vector<u8> &contents) {
            wolv::io::File output(root / relativePath, wolv::io::File::Mode::Create);
            if (output.isValid() && output.writeVector(contents) == static_cast<i64>(contents.size()) && output.flush())
                return true;

            output.close();
            std::error_code error;
            std::fs::remove(root / relativePath, error);
            cleanupPreparedFiles();
            result.error = fmt::format("hex.builtin.popup.error.project.import_legacy.write_file_failed"_lang, relativePath.generic_string());
            return false;
        };

        for (auto &provider : providers) {
            if (provider.descriptor["type"] != "hex.builtin.provider.mem_file")
                continue;

            auto &settings = provider.descriptor["settings"];
            std::vector<u8> contents;
            bool readOnly = false;
            try {
                contents = settings.at("data").get<std::vector<u8>>();
                readOnly = settings.value("readOnly", false);
            } catch (const std::exception &) {
                cleanupPreparedFiles();
                result.error = fmt::format("hex.builtin.popup.error.project.import_legacy.invalid_provider_settings"_lang,
                    impl::getProviderName(provider.descriptor));
                return result;
            }

            const auto relativePath = makeUniquePath(makeProviderStem(provider), "bin");
            if (!relativePath.has_value() || !writeProjectFile(*relativePath, contents))
                return result;
            if (readOnly) {
                std::error_code permissionsError;
                std::fs::permissions(root / *relativePath,
                    std::fs::perms::owner_write | std::fs::perms::group_write | std::fs::perms::others_write,
                    std::fs::perm_options::remove, permissionsError);
                if (permissionsError) {
                    std::error_code removeError;
                    std::fs::remove(root / *relativePath, removeError);
                    cleanupPreparedFiles();
                    result.error = fmt::format("hex.builtin.popup.error.project.import_legacy.write_file_failed"_lang,
                        relativePath->generic_string());
                    return result;
                }
            }
            preparedProjectFiles.push_back(*relativePath);

            provider.descriptor["type"] = "hex.builtin.provider.file";
            settings["path"] = relativePath->generic_string();
            settings.erase("data");
        }

        for (const auto &importedProvider : providers) {
            const auto stem = makeProviderStem(importedProvider);

            for (const auto &file : importedProvider.files) {
                if (file.typeId.empty() || file.extension.empty())
                    continue;

                const auto relativePath = makeUniquePath(stem, file.extension);
                if (!relativePath.has_value() || !writeProjectFile(*relativePath, file.contents))
                    return result;

                preparedFiles.push_back({ importedProvider.id, file.typeId, *relativePath });
            }
        }

        for (const auto &file : projectFiles) {
            auto relativePath = std::fs::path(file.name).filename();
            if (relativePath.empty())
                continue;
            const auto stem = relativePath.stem().string();
            const auto extension = relativePath.extension().string();
            for (u32 suffix = 2; ; suffix += 1) {
                std::error_code statusError;
                const auto status = std::fs::symlink_status(root / relativePath, statusError);
                if (statusError && statusError != std::errc::no_such_file_or_directory) {
                    cleanupPreparedFiles();
                    result.error = fmt::format("hex.builtin.popup.error.project.import_legacy.inspect_path_failed"_lang,
                        relativePath.generic_string(), statusError.message());
                    return result;
                }
                if (status.type() == std::fs::file_type::not_found && !reservedPaths.contains(relativePath))
                    break;
                relativePath = std::fs::path(fmt::format("{}-{}{}", stem, suffix, extension));
            }
            reservedPaths.insert(relativePath);

            wolv::io::File output(root / relativePath, wolv::io::File::Mode::Create);
            if (!output.isValid() || output.writeVector(file.contents) != static_cast<i64>(file.contents.size()) || !output.flush()) {
                std::error_code currentFileError;
                std::fs::remove(root / relativePath, currentFileError);
                for (const auto &prepared : preparedFiles) {
                    std::error_code error;
                    std::fs::remove(root / prepared.relativePath, error);
                }
                for (const auto &prepared : preparedProjectFiles) {
                    std::error_code error;
                    std::fs::remove(root / prepared, error);
                }
                result.error = fmt::format("hex.builtin.popup.error.project.import_legacy.write_file_failed"_lang, relativePath.generic_string());
                return result;
            }
            preparedProjectFiles.push_back(std::move(relativePath));
        }

        result.importedProviderCount = std::ranges::count_if(providers, [](const auto &provider) {
            return impl::canPersistProvider(provider.descriptor);
        });
        result.importedFileCount = preparedFiles.size() + preparedProjectFiles.size();
        std::ranges::stable_sort(providers, [](const auto &left, const auto &right) {
            return left.descriptor["type"] != "hex.builtin.provider.view" &&
                right.descriptor["type"] == "hex.builtin.provider.view";
        });
        for (auto &importedProvider : providers) {
            const auto id = importedProvider.id;
            if (!impl::canPersistProvider(importedProvider.descriptor))
                continue;

            auto providerName = impl::getProviderName(importedProvider.descriptor);
            projectState.projectProviderIds.insert(id);
            projectState.projectProviderSettings[id] = importedProvider.descriptor.dump(4);

            std::ignore = impl::openStoredProjectProvider(id);
            auto *provider = impl::getProviderById(id);
            if (provider == nullptr)
                result.failedProviderIds.push_back(id);
            else
                providerName = provider->getName();

            for (const auto &file : preparedFiles) {
                if (file.providerId != id)
                    continue;
                projectState.associations[id][file.typeId] = file.relativePath;
                if (provider != nullptr && FileBackedProviderDataRegistry::get(file.typeId) != nullptr &&
                    !FileBackedProviderDataRegistry::bind(provider, file.typeId, root / file.relativePath)) {
                    log::warn("Failed to bind imported project file '{}' to provider {}", file.relativePath.string(), id);
                }
            }

            if (provider != nullptr && provider->isWritable() && !importedProvider.patches.empty()) {
                size_t appliedPatches = 0;
                for (const auto &[address, value] : importedProvider.patches) {
                    if (provider->getRegionValidity(address).second) {
                        provider->write(address, &value, sizeof(value));
                        appliedPatches += 1;
                    }
                }
                if (appliedPatches > 0)
                    provider->getUndoStack().groupOperations(appliedPatches, "hex.builtin.undo_operation.patches"_unlocalized);
                if (appliedPatches != importedProvider.patches.size())
                    result.warnings.push_back(fmt::format("hex.builtin.popup.project.import_legacy.warning.patches_not_applied"_lang, providerName));
            } else if (!importedProvider.patches.empty()) {
                result.warnings.push_back(fmt::format("hex.builtin.popup.project.import_legacy.warning.patches_not_imported"_lang, providerName));
            }
        }

        result.success = impl::persistProjectMetadata();
        if (!result.success)
            result.error = "hex.builtin.popup.error.project.import_legacy.save_metadata_failed"_lang.get();
        return result;
    }

}
