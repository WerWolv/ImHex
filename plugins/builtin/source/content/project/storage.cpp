#include "internal.hpp"

#include <algorithm>

#include <hex/api/content_registry/provider.hpp>
#include <hex/api/project_manager.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/providers/provider.hpp>

#include <wolv/io/file.hpp>

#include <nlohmann/json.hpp>

#if defined(OS_WINDOWS)
    #include <windows.h>
#endif

namespace hex::plugin::builtin::project::impl {

    bool isSafeProjectPath(const std::fs::path &path) {
        if (path.empty() || path.is_absolute())
            return false;

        if (std::ranges::any_of(path, [](const auto &part) { return part == ".."; }))
            return false;

        std::error_code error;
        const auto root = std::fs::weakly_canonical(ProjectManager::getProjectRoot(), error);
        const auto resolvedPath = std::fs::weakly_canonical(root / path, error);
        if (error)
            return false;

        const auto relativePath = resolvedPath.lexically_relative(root);
        return !relativePath.empty() && !relativePath.is_absolute() &&
               std::ranges::none_of(relativePath, [](const auto &part) { return part == ".."; });
    }

    bool validateProjectSettings(const std::fs::path &root) {
        try {
            const auto settings = nlohmann::json::parse(readProjectFile(root, projectSettingsPath()));
            const auto associations = settings.value("associations", nlohmann::json::object());
            if (!associations.is_object())
                return false;

            for (const auto &[providerId, entries] : associations.items()) {
                std::ignore = std::stoul(providerId);
                if (!entries.is_object())
                    return false;
                for (const auto &[typeId, pathValue] : entries.items()) {
                    std::ignore = typeId;
                    if (!pathValue.is_string())
                        return false;
                    const auto path = std::fs::path(pathValue.get<std::string>());
                    if (path.empty() || path.is_absolute() ||
                        std::ranges::any_of(path, [](const auto &part) { return part == ".."; }) ||
                        path.generic_string().starts_with(".imhex/"))
                        return false;
                }
            }
            return true;
        } catch (const std::exception &error) {
            log::error("Failed to validate project settings at {}: {}", root.string(), error.what());
            return false;
        }
    }

    std::string readProjectFile(const std::fs::path &root, const std::fs::path &path) {
        return wolv::io::File(root / path, wolv::io::File::Mode::Read).readString();
    }

    bool writeProjectFile(const std::fs::path &root, const std::fs::path &path, const std::string &content) {
        const auto outputPath = root / path;
        auto temporaryPath = outputPath;
        temporaryPath += ".tmp";

        std::error_code error;
        std::fs::create_directories(outputPath.parent_path(), error);
        if (error)
            return false;

        std::fs::remove(temporaryPath, error);
        error.clear();
        wolv::io::File file(temporaryPath, wolv::io::File::Mode::Create);
        if (!file.isValid() || file.writeString(content) != static_cast<i64>(content.size()) || !file.flush()) {
            file.close();
            std::fs::remove(temporaryPath, error);
            return false;
        }
        file.close();

        #if defined(OS_WINDOWS)
            if (!MoveFileExW(temporaryPath.c_str(), outputPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                std::fs::remove(temporaryPath, error);
                return false;
            }
        #else
            std::fs::rename(temporaryPath, outputPath, error);
            if (error) {
                std::fs::remove(temporaryPath, error);
                return false;
            }
        #endif

        return true;
    }

    void loadAssociations(const std::fs::path &root) {
        auto &associations = state().associations;
        associations.clear();
        if (!std::fs::is_regular_file(root / projectSettingsPath()))
            return;

        try {
            const auto settings = nlohmann::json::parse(readProjectFile(root, projectSettingsPath()));
            const auto association = settings.value("associations", nlohmann::json::object());
            for (const auto &[providerId, entries] : association.items()) {
                for (const auto &[handler, pathValue] : entries.items()) {
                    auto path = std::fs::path(pathValue.get<std::string>());
                    if (isSafeProjectPath(path) && !path.generic_string().starts_with(".imhex/"))
                        associations[std::stoul(providerId)][handler] = std::move(path);
                }
            }
        } catch (const std::exception &error) {
            associations.clear();
            log::warn("Failed to load project associations: {}", error.what());
        }
    }

    bool storeAssociations(const std::fs::path &root) {
        nlohmann::json associations = nlohmann::json::object();
        for (const auto &[providerId, entries] : state().associations) {
            for (const auto &[handler, path] : entries)
                associations[std::to_string(providerId)][handler] = path.generic_string();
        }

        return writeProjectFile(root, projectSettingsPath(), nlohmann::json({
            { "version", 1 },
            { "associations", std::move(associations) }
        }).dump(4));
    }

    std::string rebaseStoredProviderSettings(const std::string &serializedSettings,
                                              const std::fs::path &sourceRoot,
                                              const std::fs::path &destinationRoot) {
        if (sourceRoot.empty() || sourceRoot.lexically_normal() == destinationRoot.lexically_normal())
            return serializedSettings;

        try {
            auto descriptor = nlohmann::json::parse(serializedSettings);
            const auto providerType = UnlocalizedString(descriptor.at("type").get<std::string>());
            const auto provider = ContentRegistry::Provider::impl::create(providerType);
            if (provider == nullptr || dynamic_cast<prv::IProviderFilePicker *>(provider.get()) == nullptr)
                return serializedSettings;

            auto &settings = descriptor.at("settings");
            if (!settings.contains("path") || !settings["path"].is_string())
                return serializedSettings;

            auto path = std::fs::path(settings["path"].get<std::string>());
            const bool storesAbsolutePath = path.is_absolute();

            std::error_code error;
            const auto sourcePath = std::fs::weakly_canonical(path.is_absolute() ? path : sourceRoot / path, error);
            const auto canonicalSourceRoot = std::fs::weakly_canonical(sourceRoot, error);
            if (!error) {
                const auto relativePath = sourcePath.lexically_relative(canonicalSourceRoot);
                if (!relativePath.empty() && !relativePath.is_absolute() &&
                    std::ranges::none_of(relativePath, [](const auto &part) { return part == ".."; }) &&
                    std::fs::exists(destinationRoot / relativePath, error) && !error) {
                    settings["path"] = wolv::io::fs::toNormalizedPathString(
                        storesAbsolutePath ? destinationRoot / relativePath : relativePath);
                    return descriptor.dump(4);
                }
            }

            if (path.is_absolute())
                return serializedSettings;

            error.clear();
            const auto rebasedPath = std::fs::proximate(sourceRoot / path, destinationRoot, error);
            if (error)
                return serializedSettings;

            settings["path"] = wolv::io::fs::toNormalizedPathString(rebasedPath);
            return descriptor.dump(4);
        } catch (const std::exception &error) {
            log::warn("Failed to rebase stored project provider settings: {}", error.what());
            return serializedSettings;
        }
    }

    bool copyTemporaryProjectFiles(const std::fs::path &source, const std::fs::path &destination,
                                   std::vector<std::fs::path> &copiedEntries) {
        std::error_code error;
        const auto canonicalSource = std::fs::weakly_canonical(source, error);
        if (error)
            return false;

        std::fs::create_directories(destination, error);
        if (error)
            return false;

        const auto canonicalDestination = std::fs::weakly_canonical(destination, error);
        if (error)
            return false;

        const auto destinationInSource = canonicalDestination.lexically_relative(canonicalSource);
        const auto sourceInDestination = canonicalSource.lexically_relative(canonicalDestination);
        const auto isDescendant = [](const std::fs::path &relative) {
            return !relative.empty() && !relative.is_absolute() &&
                std::ranges::none_of(relative, [](const auto &part) { return part == ".."; });
        };
        if (canonicalSource == canonicalDestination || isDescendant(destinationInSource) || isDescendant(sourceInDestination))
            return false;

        if (std::fs::exists(canonicalDestination / ProjectManager::ProjectDirectory, error) || error)
            return false;

        std::vector<std::fs::path> sourceEntries;
        for (const auto &entry : std::fs::directory_iterator(canonicalSource, error)) {
            if (error)
                return false;
            if (entry.path().filename() == ProjectManager::ProjectDirectory)
                continue;

            const auto destinationPath = canonicalDestination / entry.path().filename();
            if (std::fs::exists(destinationPath, error) || error)
                return false;
            sourceEntries.push_back(entry.path());
        }

        for (const auto &entry : sourceEntries) {
            const auto destinationPath = canonicalDestination / entry.filename();
            std::fs::copy(entry, destinationPath, std::fs::copy_options::recursive, error);
            if (error) {
                std::fs::remove_all(destinationPath, error);
                for (const auto &copiedEntry : copiedEntries)
                    std::fs::remove_all(copiedEntry, error);
                copiedEntries.clear();
                return false;
            }
            copiedEntries.push_back(destinationPath);
        }

        return true;
    }

    bool isPathInProject(const std::fs::path &path, const std::fs::path &root, std::fs::path &relativePath) {
        std::error_code error;
        const auto canonicalPath = std::fs::weakly_canonical(path, error);
        const auto canonicalRoot = std::fs::weakly_canonical(root, error);
        if (error)
            return false;

        relativePath = canonicalPath.lexically_relative(canonicalRoot);
        return !relativePath.empty() && !relativePath.is_absolute() &&
            std::ranges::none_of(relativePath, [](const auto &part) { return part == ".."; });
    }

}
