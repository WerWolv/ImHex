#include <content/legacy_project_importer.hpp>

#include <hex/api/content_registry/user_interface.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/project_manager.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/fs.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/tar.hpp>

#include <fonts/vscode_icons.hpp>
#include <nlohmann/json.hpp>
#include <popups/popup_question.hpp>
#include <toasts/toast_notification.hpp>
#include <wolv/io/fs.hpp>

#include <array>
#include <map>
#include <set>

namespace hex::plugin::builtin {

    namespace {

        constexpr auto MetadataPath = "IMHEX_METADATA";
        constexpr auto MetadataMagic = "HEX";

        struct LegacyFileMapping {
            const char *archiveName;
            const char *typeId;
            const char *extension;
        };

        constexpr std::array LegacyFileMappings = {
            LegacyFileMapping { "bookmarks.json",              "hex.builtin.bookmarks",        "hexbm" },
            LegacyFileMapping { "data_processor.json",         "hex.builtin.data-processor",   "hexnode" },
            LegacyFileMapping { "custom_encoding.tbl",         "hex.builtin.custom-encoding",  "tbl" },
            LegacyFileMapping { "highlight_rules.json",        "hex.builtin.highlight-rules",  "hexhl" },
            LegacyFileMapping { "data_information.json",       "hex.builtin.data-information", "hexinfo" },
            LegacyFileMapping { "pattern_source_code.hexpat",  "hex.builtin.pattern-source",   "hexpat" },
            LegacyFileMapping { "hashes.json",                 "hex.hashes.functions",         "hexhashes" },
            LegacyFileMapping { "yara.json",                   "hex.yara.rules",               "hexyara" },
        };

        bool hasSafeLegacyProviderId(u32 id, const std::set<u32> &ids) {
            return ids.contains(id);
        }

        void normalizeLegacyProviderPath(const std::fs::path &projectPath, nlohmann::json &descriptor) {
            static const std::set<std::string> PathProviderTypes = {
                "hex.builtin.provider.file",
                "hex.builtin.provider.base64",
            };

            const auto type = descriptor.at("type").get<std::string>();
            auto &settings = descriptor.at("settings");
            if (!PathProviderTypes.contains(type) || !settings.contains("path") || !settings["path"].is_string())
                return;

            auto path = std::fs::path(settings["path"].get<std::string>());
            if (path.is_relative())
                path = (projectPath.parent_path() / path).lexically_normal();
            settings["path"] = wolv::io::fs::toNormalizedPathString(path);
        }

    }

    LegacyProjectParseResult parseLegacyProject(const std::fs::path &path) {
        LegacyProjectParseResult result;
        if (!wolv::io::fs::isRegularFile(path)) {
            result.error = fmt::format("Legacy project file '{}' does not exist", path.string());
            return result;
        }

        Tar tar(path, Tar::Mode::Read);
        if (!tar.isValid()) {
            result.error = fmt::format("Could not open legacy project archive: {}", tar.getOpenErrorString());
            return result;
        }
        const auto archiveSize = wolv::io::fs::getFileSize(path);
        const auto readString = [&tar, archiveSize](const std::fs::path &entry) {
            return tar.readString(entry, archiveSize);
        };
        const auto readVector = [&tar, archiveSize](const std::fs::path &entry) {
            return tar.readVector(entry, archiveSize);
        };

        if (!tar.contains(MetadataPath) || !readString(MetadataPath).starts_with(MetadataMagic)) {
            result.error = "The selected file is not a valid legacy ImHex project";
            return result;
        }
        result.hasUnsupportedData = tar.contains("challenge/achievements.json") ||
            tar.contains("challenge/description.txt") || tar.contains("challenge/unlocked.json");

        try {
            constexpr auto ManifestPath = "providers/providers.json";
            if (!tar.contains(ManifestPath)) {
                result.error = "The legacy project does not contain a provider manifest";
                return result;
            }

            const auto manifest = nlohmann::json::parse(readString(ManifestPath));
            const auto providerIds = manifest.at("providers").get<std::vector<u32>>();
            const std::set<u32> providerIdSet(providerIds.begin(), providerIds.end());
            if (providerIdSet.size() != providerIds.size()) {
                result.error = "The legacy project contains duplicate provider IDs";
                return result;
            }

            for (const auto id : providerIds) {
                const auto settingsPath = std::fs::path("providers") / fmt::format("{}.json", id);
                if (!tar.contains(settingsPath)) {
                    result.error = fmt::format("Legacy project provider {} has no settings", id);
                    return result;
                }

                auto descriptor = nlohmann::json::parse(readString(settingsPath));
                if (!descriptor.contains("type") || !descriptor["type"].is_string() ||
                    !descriptor.contains("settings") || !descriptor["settings"].is_object()) {
                    result.error = fmt::format("Legacy project provider {} has invalid settings", id);
                    return result;
                }
                normalizeLegacyProviderPath(path, descriptor);

                project::ImportedProvider provider {
                    .id = id,
                    .descriptor = std::move(descriptor),
                    .files = {},
                    .patches = {},
                };

                for (const auto &mapping : LegacyFileMappings) {
                    const auto archivePath = std::fs::path(std::to_string(id)) / mapping.archiveName;
                    if (tar.contains(archivePath)) {
                        provider.files.push_back({
                            .typeId = mapping.typeId,
                            .extension = mapping.extension,
                            .contents = readVector(archivePath),
                        });
                    }
                }

                const auto patchesPath = std::fs::path(std::to_string(id)) / "patches.json";
                if (tar.contains(patchesPath)) {
                    const auto patchContents = readVector(patchesPath);
                    const auto patches = nlohmann::json::parse(patchContents.begin(), patchContents.end());
                    if (const auto entries = patches.find("patches"); entries != patches.end() && entries->is_object()) {
                        for (const auto &[address, value] : entries->items()) {
                            size_t parsedCharacters = 0;
                            const auto parsedAddress = std::stoull(address, &parsedCharacters);
                            if (parsedCharacters != address.size() || !value.is_number_unsigned() || value.get<u64>() > 0xFF)
                                throw std::runtime_error("Invalid patch entry");
                            provider.patches[parsedAddress] = static_cast<u8>(value.get<u64>());
                        }
                    }
                    result.projectFiles.push_back({
                        .name = fmt::format("legacy-provider-{}-patches.json", id),
                        .contents = patchContents,
                    });
                }

                result.providers.push_back(std::move(provider));
            }

            constexpr std::array ChallengeFiles = {
                "achievements.json",
                "description.txt",
                "unlocked.json",
            };
            for (const auto *name : ChallengeFiles) {
                const auto archivePath = std::fs::path("challenge") / name;
                if (tar.contains(archivePath)) {
                    result.projectFiles.push_back({
                        .name = fmt::format("legacy-challenge-{}", name),
                        .contents = readVector(archivePath),
                    });
                }
            }

            for (const auto &provider : result.providers) {
                if (provider.descriptor["type"] != "hex.builtin.provider.view")
                    continue;
                const auto &settings = provider.descriptor["settings"];
                if (!settings.contains("id") || !hasSafeLegacyProviderId(settings["id"].get<u32>(), providerIdSet)) {
                    result.error = fmt::format("Legacy view provider {} references a missing provider", provider.id);
                    result.providers.clear();
                    return result;
                }
            }
        } catch (const std::exception &error) {
            result.providers.clear();
            result.error = fmt::format("Failed to parse legacy project: {}", error.what());
        }

        return result;
    }

    project::ImportResult importLegacyProject(const std::fs::path &path) {
        auto parsed = parseLegacyProject(path);
        if (!parsed.isValid()) {
            project::ImportResult result;
            result.error = std::move(parsed.error);
            return result;
        }
        auto result = project::importProviders(std::move(parsed.providers), std::move(parsed.projectFiles));
        if (result.success && parsed.hasUnsupportedData)
            result.warnings.push_back("Legacy challenge files were copied but are not activated");
        return result;
    }

    void registerLegacyProjectImporter() {
        ContentRegistry::UserInterface::addMenuItem(
            { "hex.builtin.menu.file", "hex.builtin.menu.file.import", "hex.builtin.menu.file.import.legacy_project" },
            ICON_VS_PROJECT, 5145, Shortcut::None,
            [] {
                fs::openFileBrowser(fs::DialogMode::Open, { { "Legacy ImHex Project", "hexproj" } }, [](const auto &path) {
                    ui::PopupQuestion::open("hex.builtin.popup.project.import_legacy.confirm"_lang,
                        [path] {
                            auto result = importLegacyProject(path);
                            if (!result.success) {
                                ui::ToastError::open(fmt::format("hex.builtin.popup.error.project.load"_lang, result.error));
                                return;
                            }

                            if (result.failedProviderIds.empty() && result.warnings.empty()) {
                                ui::ToastInfo::open(fmt::format("hex.builtin.popup.project.import_legacy.success"_lang,
                                    result.importedProviderCount, result.importedFileCount));
                            } else {
                                ui::ToastWarning::open(fmt::format(
                                    "hex.builtin.popup.project.import_legacy.partial"_lang,
                                    result.failedProviderIds.size(), fmt::join(result.warnings, "\n")));
                            }
                        }, [] { });
                });
            },
            [] {
                return TaskManager::getRunningTaskCount() == 0 && ProjectManager::isFolderProject();
            });
    }

}
