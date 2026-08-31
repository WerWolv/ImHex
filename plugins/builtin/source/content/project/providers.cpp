#include "internal.hpp"

#include <content/project.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>

#include <hex/api/content_registry/provider.hpp>
#include <hex/api/events/events_provider.hpp>
#include <hex/api/imhex_api/provider.hpp>
#include <hex/api/localization_manager.hpp>
#include <hex/api/project_manager.hpp>
#include <hex/api/task_manager.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/providers/file_backed_provider_data.hpp>
#include <hex/providers/provider.hpp>

#include <fonts/vscode_icons.hpp>
#include <toasts/toast_notification.hpp>

#include <wolv/io/file.hpp>
#include <wolv/utils/guards.hpp>
#include <wolv/utils/string.hpp>

#include <nlohmann/json.hpp>

namespace hex::plugin::builtin::project::impl {

    std::string getProviderName(const nlohmann::json &descriptor) {
        if (const auto settings = descriptor.find("settings"); settings != descriptor.end() && settings->is_object()) {
            for (const auto *key : { "displayName", "name" }) {
                if (const auto value = settings->find(key); value != settings->end() && value->is_string() && !value->empty())
                    return value->get<std::string>();
            }

            if (const auto path = settings->find("path"); path != settings->end() && path->is_string() && !path->empty())
                return wolv::util::toUTF8String(std::fs::path(path->get<std::string>()).filename());
        }

        if (const auto type = descriptor.find("type"); type != descriptor.end() && type->is_string())
            return Lang(type->get<std::string>());

        return "hex.builtin.sidebar.project.provider_fallback"_lang;
    }

    std::string getStoredProviderName(u32 id) {
        const auto &settings = state().projectProviderSettings;
        const auto storedSettings = settings.find(id);
        if (storedSettings == settings.end())
            return "hex.builtin.sidebar.project.provider_fallback"_lang;

        try {
            return getProviderName(nlohmann::json::parse(storedSettings->second));
        } catch (const std::exception &) {
            return "hex.builtin.sidebar.project.provider_fallback"_lang;
        }
    }

    bool canPersistProvider(const prv::Provider *provider) {
        return provider != nullptr && provider->isSavableAsRecent();
    }

    bool canPersistProvider(const nlohmann::json &descriptor) {
        try {
            const auto providerType = UnlocalizedString(descriptor.at("type").get<std::string>());
            const auto provider = ContentRegistry::Provider::impl::create(providerType);
            if (provider == nullptr)
                return true;
            if (!canPersistProvider(provider.get()))
                return false;
            provider->loadSettings(descriptor.at("settings"));
            return canPersistProvider(provider.get());
        } catch (const std::exception &) {
            return true;
        }
    }

    void snapshotProviderSettings(prv::Provider *provider) {
        if (!canPersistProvider(provider))
            return;
        try {
            state().projectProviderSettings[provider->getID()] = nlohmann::json({
                { "type", provider->getTypeName() },
                { "settings", provider->storeSettings({}) }
            }).dump(4);
        } catch (const std::exception &error) {
            log::warn("Failed to snapshot project provider {}: {}", provider->getID(), error.what());
        }
    }

    prv::Provider *getProviderById(u32 id) {
        const auto providers = ImHexApi::Provider::getProviders();
        const auto provider = std::ranges::find_if(providers, [id](const auto *entry) {
            return entry->getID() == id;
        });
        return provider == providers.end() ? nullptr : *provider;
    }

    namespace {

        std::string getProviderFileStem(prv::Provider *provider) {
            auto name = provider->getName();
            for (auto &character : name) {
                const auto value = static_cast<unsigned char>(character);
                if (!std::isalnum(value) && character != '-' && character != '_')
                    character = '-';
            }
            if (name.empty())
                name = "provider";
            return fmt::format("{}-{}", name, provider->getID());
        }

        void removeProviderForProjectLoad(prv::Provider *provider) {
            ImHexApi::Provider::remove(provider, true);
        }

    }

    void bindRegisteredData() {
        const auto root = ProjectManager::getProjectRoot();
        const auto &associations = state().associations;
        for (auto *provider : ImHexApi::Provider::getProviders()) {
            const auto providerAssociations = associations.find(provider->getID());
            if (providerAssociations == associations.end())
                continue;

            for (const auto &[typeId, relativePath] : providerAssociations->second) {
                if (FileBackedProviderDataRegistry::get(typeId) != nullptr)
                    std::ignore = FileBackedProviderDataRegistry::bind(provider, typeId, root / relativePath);
            }
        }
    }

    void materializeRegisteredData() {
        const auto root = ProjectManager::getProjectRoot();
        auto &projectState = state();
        bool associationsChanged = false;
        for (auto *provider : ImHexApi::Provider::getProviders()) {
            for (auto *data : FileBackedProviderDataRegistry::getTypes()) {
                const auto &type = data->getType();
                if (data->isBound(provider) || !data->hasPendingData(provider) || type.extensions.empty())
                    continue;

                const auto extension = type.extensions.front().spec;
                const auto relativePath = std::fs::path(fmt::format("{}.{}", getProviderFileStem(provider), extension));
                if (data->bind(provider, root / relativePath)) {
                    projectState.associations[provider->getID()][type.typeId] = relativePath;
                    associationsChanged = true;
                }
            }
        }
        if (associationsChanged)
            std::ignore = storeAssociations(ProjectManager::getProjectRoot());
    }

    prv::Provider::OpenResult openStoredProjectProvider(u32 id) {
        if (auto *provider = getProviderById(id); provider != nullptr) {
            ImHexApi::Provider::setCurrentProvider(provider);
            return {};
        }

        auto &projectState = state();
        const auto storedSettings = projectState.projectProviderSettings.find(id);
        if (storedSettings == projectState.projectProviderSettings.end())
            return prv::Provider::OpenResult::failure(fmt::format("Project data source '{}' has no stored settings", getStoredProviderName(id)));

        projectState.providerOpenAttempts.insert(id);
        auto finishOpenAttempt = SCOPE_GUARD { projectState.providerOpenAttempts.erase(id); };

        try {
            const auto providerSettings = nlohmann::json::parse(storedSettings->second);
            const auto providerType = UnlocalizedString(providerSettings.at("type").get<std::string>());
            auto provider = ImHexApi::Provider::createProvider(providerType, true, true);
            if (provider == nullptr)
                return prv::Provider::OpenResult::failure(fmt::format("Failed to create provider of type {}", providerType.get()));

            provider->setID(id);
            provider->loadSettings(providerSettings.at("settings"));
            const auto result = provider->open();
            if (result.isFailure() || result.isRedirecting() || !provider->isAvailable() || !provider->isReadable()) {
                removeProviderForProjectLoad(provider.get());
                return prv::Provider::OpenResult::failure(fmt::format("Failed to open data source {}", getStoredProviderName(id)));
            }

            EventProviderOpened::post(provider.get());
            bindRegisteredData();
            return {};
        } catch (const std::exception &error) {
            log::warn("Failed to reopen project provider {}: {}", id, error.what());
            if (auto *provider = getProviderById(id); provider != nullptr)
                removeProviderForProjectLoad(provider);
            return prv::Provider::OpenResult::failure(fmt::format("Failed to open data source {}: {}", getStoredProviderName(id), error.what()));
        }
    }

    bool isSameFile(const std::fs::path &left, const std::fs::path &right) {
        std::error_code error;
        return std::fs::equivalent(left, right, error) && !error;
    }

    std::optional<u32> findClosedProviderForPath(const std::fs::path &path) {
        if (!ProjectManager::isFolderProject())
            return std::nullopt;

        const auto &projectState = state();
        for (const auto id : projectState.closedProjectProviderIds) {
            const auto settings = projectState.projectProviderSettings.find(id);
            if (settings == projectState.projectProviderSettings.end())
                continue;

            try {
                const auto descriptor = nlohmann::json::parse(settings->second);
                const auto providerType = UnlocalizedString(descriptor.at("type").get<std::string>());
                const auto provider = ContentRegistry::Provider::impl::create(providerType);
                auto *filePicker = provider == nullptr ? nullptr : dynamic_cast<prv::IProviderFilePicker *>(provider.get());
                if (filePicker == nullptr)
                    continue;

                provider->loadSettings(descriptor.at("settings"));
                if (isSameFile(filePicker->getPickedPath(), path))
                    return id;
            } catch (const std::exception &exception) {
                log::warn("Failed to inspect closed project provider {}: {}", id, exception.what());
            }
        }

        return std::nullopt;
    }

    bool isProviderReplacement(const prv::Provider *provider) {
        return state().providerReplacements.contains(provider);
    }

    StoredProviderDisplayInfo getStoredProviderDisplayInfo(u32 id) {
        StoredProviderDisplayInfo result { .name = getStoredProviderName(id), .icon = ICON_VS_FILE_BINARY };
        const auto &settings = state().projectProviderSettings;
        const auto storedSettings = settings.find(id);
        if (storedSettings == settings.end())
            return result;

        try {
            const auto providerSettings = nlohmann::json::parse(storedSettings->second);
            const auto providerType = providerSettings.at("type").get<std::string>();
            for (const auto &entry : ContentRegistry::Provider::impl::getEntries()) {
                if (entry.unlocalizedName.get() == providerType) {
                    result.icon = entry.icon;
                    break;
                }
            }
        } catch (const std::exception &) {
        }
        return result;
    }

    std::optional<ProviderManifest> readProviderManifest(const std::fs::path &root, bool showError) {
        const auto basePath = std::fs::path(ProjectManager::ProjectDirectory) / "providers";
        try {
            const auto manifest = nlohmann::json::parse(readProjectFile(root, basePath / "providers.json"));
            auto providerIds = manifest.at("providers").get<std::vector<u32>>();
            auto closedProviderIds = manifest.at("closedProviders").get<std::vector<u32>>();

            const std::set<u32> providerIdSet(providerIds.begin(), providerIds.end());
            const std::set<u32> closedProviderIdSet(closedProviderIds.begin(), closedProviderIds.end());
            if (providerIdSet.size() != providerIds.size() || closedProviderIdSet.size() != closedProviderIds.size())
                throw std::runtime_error("Project data source manifest contains duplicate entries");
            if (std::ranges::any_of(providerIds, [](u32 id) { return id >= std::numeric_limits<u32>::max() - 1; }))
                throw std::runtime_error("Project data source manifest contains an invalid entry");
            if (!std::ranges::all_of(closedProviderIdSet, [&providerIdSet](u32 id) { return providerIdSet.contains(id); }))
                throw std::runtime_error("Closed provider is not part of the project");

            return ProviderManifest { std::move(providerIds), std::move(closedProviderIds) };
        } catch (const std::exception &error) {
            log::error("Failed to load project provider manifest at {}: {}", root.string(), error.what());
            if (showError)
                ui::ToastError::open(fmt::format("hex.builtin.popup.error.project.load"_lang, error.what()));
            return std::nullopt;
        }
    }

    bool loadProviders(const std::fs::path &root) {
        const auto basePath = std::fs::path(ProjectManager::ProjectDirectory) / "providers";
        auto manifest = readProviderManifest(root, true);
        if (!manifest.has_value())
            return false;
        const auto &providerIds = manifest->providerIds;
        const std::set<u32> closedProviderIds(manifest->closedProviderIds.begin(), manifest->closedProviderIds.end());
        auto &projectState = state();

        projectState.projectProviderIds.clear();
        projectState.closedProjectProviderIds.clear();
        projectState.projectProviderSettings.clear();

        if (providerIds.empty())
            return true;

        std::vector<std::string> failedProviderNames;
        for (const auto id : providerIds) {
            std::string providerName = "hex.builtin.sidebar.project.provider_fallback"_lang;
            try {
                auto serializedSettings = readProjectFile(root, basePath / fmt::format("{}.json", id));
                const auto providerSettings = nlohmann::json::parse(std::move(serializedSettings));
                providerName = getProviderName(providerSettings);
                if (!canPersistProvider(providerSettings))
                    continue;

                projectState.projectProviderIds.insert(id);
                if (closedProviderIds.contains(id))
                    projectState.closedProjectProviderIds.insert(id);
                projectState.projectProviderSettings[id] = providerSettings.dump(4);
                prv::Provider::reserveID(id);

                const auto providerType = UnlocalizedString(providerSettings.at("type").get<std::string>());
                if (projectState.closedProjectProviderIds.contains(id))
                    continue;

                auto provider = ImHexApi::Provider::createProvider(providerType, true, false);
                if (provider == nullptr) {
                    log::warn("Failed to create project provider {} of type {}", id, providerType.get());
                    failedProviderNames.push_back(providerName);
                    continue;
                }

                provider->setID(id);
                provider->loadSettings(providerSettings.at("settings"));
                providerName = provider->getName();
                const auto result = provider->open();
                if (result.isFailure() || result.isRedirecting() || !provider->isAvailable() || !provider->isReadable()) {
                    removeProviderForProjectLoad(provider.get());
                    failedProviderNames.push_back(providerName);
                    continue;
                }
                EventProviderOpened::post(provider.get());
            } catch (const std::exception &error) {
                log::warn("Failed to load project provider {}: {}", id, error.what());
                if (auto *provider = getProviderById(id); provider != nullptr) {
                    providerName = provider->getName();
                    removeProviderForProjectLoad(provider);
                }
                failedProviderNames.push_back(providerName);
            }
        }

        if (!failedProviderNames.empty()) {
            ui::ToastWarning::open(fmt::format(
                "hex.builtin.popup.error.project.load.some_providers_failed"_lang,
                fmt::join(failedProviderNames, ", ")));
        }

        return true;
    }

    bool storeProviders(const std::fs::path &root, const std::fs::path &sourceRoot, ProviderSettings &storedSettings) {
        const auto basePath = std::fs::path(ProjectManager::ProjectDirectory) / "providers";
        auto &projectState = state();

        std::set<u32> excludedProviderIds;
        for (const auto id : projectState.projectProviderIds) {
            if (const auto *provider = getProviderById(id); provider != nullptr) {
                if (!canPersistProvider(provider))
                    excludedProviderIds.insert(id);
            } else if (const auto settings = projectState.projectProviderSettings.find(id);
                       settings != projectState.projectProviderSettings.end()) {
                try {
                    if (!canPersistProvider(nlohmann::json::parse(settings->second)))
                        excludedProviderIds.insert(id);
                } catch (const std::exception &) {
                }
            }
        }
        for (const auto id : excludedProviderIds) {
            projectState.projectProviderIds.erase(id);
            projectState.closedProjectProviderIds.erase(id);
            projectState.projectProviderSettings.erase(id);
            projectState.associations.erase(id);
            std::error_code error;
            std::fs::remove(root / basePath / fmt::format("{}.json", id), error);
        }

        auto providerSettings = projectState.projectProviderSettings;
        std::set<u32> serializedActiveProviderIds;
        for (const auto *provider : ImHexApi::Provider::getProviders()) {
            if (!canPersistProvider(provider) || !projectState.projectProviderIds.contains(provider->getID()))
                continue;
            if (isProviderReplacement(provider))
                continue;

            serializedActiveProviderIds.insert(provider->getID());
            providerSettings[provider->getID()] = nlohmann::json({
                { "type", provider->getTypeName() },
                { "settings", provider->storeSettings({}) }
            }).dump(4);
        }

        for (const auto id : projectState.projectProviderIds) {
            auto settings = providerSettings.find(id);
            if (settings == providerSettings.end())
                continue;

            if (!serializedActiveProviderIds.contains(id))
                settings->second = rebaseStoredProviderSettings(settings->second, sourceRoot, root);
            if (!writeProjectFile(root, basePath / fmt::format("{}.json", id), settings->second))
                return false;
        }

        if (!writeProjectFile(root, basePath / "providers.json", nlohmann::json({
            { "providers", projectState.projectProviderIds },
            { "closedProviders", projectState.closedProjectProviderIds }
        }).dump(4)))
            return false;

        storedSettings = std::move(providerSettings);
        return true;
    }

    void removeProviderFromProject(u32 id) {
        auto &projectState = state();
        auto *provider = getProviderById(id);
        projectState.projectProviderIds.erase(id);
        projectState.closedProjectProviderIds.erase(id);
        projectState.projectProviderSettings.erase(id);

        if (const auto associations = projectState.associations.find(id); associations != projectState.associations.end()) {
            std::vector<std::string> typeIds;
            for (const auto &[typeId, path] : associations->second) {
                std::ignore = path;
                typeIds.push_back(typeId);
            }
            projectState.associations.erase(associations);

            if (provider != nullptr) {
                for (const auto &typeId : typeIds)
                    std::ignore = FileBackedProviderDataRegistry::unbind(provider, typeId);
            }
        }

        std::error_code error;
        std::fs::remove(ProjectManager::getProjectRoot() / ProjectManager::ProjectDirectory / "providers" /
            fmt::format("{}.json", id), error);
        if (error)
            log::warn("Failed to remove project provider metadata: {}", error.message());

        std::ignore = storeAssociations(ProjectManager::getProjectRoot());
        scheduleProjectMetadataSave();
    }

    bool persistProjectMetadata() {
        auto &projectState = state();
        if (projectState.loadingProject || projectState.storingProject || !ProjectManager::isFolderProject())
            return false;

        const auto root = ProjectManager::getProjectRoot();
        ProviderSettings storedSettings;
        if (!storeProviders(root, root, storedSettings) || !storeAssociations(root)) {
            log::error("Failed to update project metadata at {}", root.string());
            return false;
        }
        projectState.projectProviderSettings = std::move(storedSettings);
        for (auto *provider : ImHexApi::Provider::getProviders())
            provider->markMetadataDirty(false);
        return true;
    }

    void scheduleProjectMetadataSave() {
        std::ignore = persistProjectMetadata();
    }

    bool reopenProviderWithNewSettings(u32 id) {
        auto &projectState = state();
        if (!ProjectManager::isFolderProject() || !projectState.projectProviderIds.contains(id) ||
            getProviderById(id) != nullptr ||
            std::ranges::any_of(projectState.providerReplacements, [id](const auto &entry) { return entry.second == id; }) ||
            TaskManager::getRunningTaskCount() != 0)
            return false;

        const auto storedSettings = projectState.projectProviderSettings.find(id);
        if (storedSettings == projectState.projectProviderSettings.end())
            return false;

        try {
            const auto descriptor = nlohmann::json::parse(storedSettings->second);
            const auto providerType = UnlocalizedString(descriptor.at("type").get<std::string>());
            auto provider = ImHexApi::Provider::createProvider(providerType);
            if (provider == nullptr || !std::ranges::contains(ImHexApi::Provider::getProviders(), provider.get()))
                return false;

            projectState.providerReplacements[provider.get()] = id;
            return true;
        } catch (const std::exception &error) {
            log::warn("Failed to reconfigure project provider {}: {}", id, error.what());
            return false;
        }
    }

}

namespace hex::plugin::builtin::project {

    bool createProjectFile() {
        const auto reportFailure = [] {
            ui::ToastError::open(fmt::format("hex.builtin.popup.error.project.entry.create_file"_lang, "Untitled.bin"));
            return false;
        };
        if (!ProjectManager::isFolderProject() && !ProjectManager::loadTemporaryProject())
            return reportFailure();

        const auto root = ProjectManager::getProjectRoot();
        auto relativePath = std::fs::path("Untitled.bin");
        for (u32 suffix = 2; ; suffix += 1) {
            std::error_code error;
            const auto status = std::fs::symlink_status(root / relativePath, error);
            if (error && error != std::errc::no_such_file_or_directory)
                return reportFailure();
            if (status.type() == std::fs::file_type::not_found)
                break;
            relativePath = fmt::format("Untitled-{}.bin", suffix);
        }

        const auto path = root / relativePath;
        wolv::io::File file(path, wolv::io::File::Mode::Create);
        const std::array<u8, 1> contents = { 0x00 };
        if (!file.isValid() || file.writeBuffer(contents.data(), contents.size()) != static_cast<i64>(contents.size()) || !file.flush()) {
            file.close();
            std::error_code error;
            std::fs::remove(path, error);
            return reportFailure();
        }
        file.close();

        auto provider = ImHexApi::Provider::createProvider("hex.builtin.provider.file"_unlocalized, true);
        auto *filePicker = dynamic_cast<prv::IProviderFilePicker *>(provider.get());
        if (filePicker == nullptr) {
            if (provider != nullptr)
                ImHexApi::Provider::remove(provider.get(), true);
            std::error_code error;
            std::fs::remove(path, error);
            return reportFailure();
        }

        filePicker->setPickedPath(path);
        ImHexApi::Provider::openProvider(std::move(provider));
        return true;
    }

    prv::Provider *openProviderForPath(const std::filesystem::path &path, const prv::Provider *excludedProvider) {
        for (auto *provider : ImHexApi::Provider::getProviders()) {
            if (provider == excludedProvider)
                continue;
            auto *filePicker = dynamic_cast<prv::IProviderFilePicker *>(provider);
            if (filePicker == nullptr)
                continue;

            if (impl::isSameFile(filePicker->getPickedPath(), path))
                return provider;
        }

        const auto providerId = impl::findClosedProviderForPath(path);
        if (!providerId.has_value())
            return nullptr;

        auto result = impl::openStoredProjectProvider(*providerId);
        if (result.isFailure()) {
            ui::ToastError::open(fmt::format("hex.builtin.provider.error.open"_lang, result.getErrorMessage()));
            return nullptr;
        }

        return impl::getProviderById(*providerId);
    }

}
