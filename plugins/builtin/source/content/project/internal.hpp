#pragma once

#include "hex/providers/provider.hpp"

#include <hex.hpp>

#include <wolv/io/fs.hpp>

#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace hex {

    class FileBackedProviderDataBase;

    namespace prv {
        class Provider;
    }

}

namespace hex::plugin::builtin::project::impl {

    constexpr u32 ProjectFormatVersion = 1;

    using Associations = std::map<u32, std::map<std::string, std::fs::path>>;
    using ProviderSettings = std::map<u32, std::string>;

    struct ProjectState {
        Associations associations;
        std::set<u32> projectProviderIds;
        std::set<u32> closedProjectProviderIds;
        std::set<u32> providerOpenAttempts;
        std::map<const prv::Provider *, u32> providerReplacements;
        ProviderSettings projectProviderSettings;
        bool loadingProject = false;
        bool storingProject = false;
        bool skipShutdownStore = false;
    };

    struct ProviderManifest {
        std::vector<u32> providerIds;
        std::vector<u32> closedProviderIds;
    };

    struct StoredProviderDisplayInfo {
        std::string name;
        const char *icon;
    };

    ProjectState &state();
    const std::fs::path &projectSettingsPath();

    std::string getProviderName(const nlohmann::json &descriptor);
    std::string getStoredProviderName(u32 id);
    bool canPersistProvider(const prv::Provider *provider);
    bool canPersistProvider(const nlohmann::json &descriptor);

    bool isSafeProjectPath(const std::fs::path &path);
    bool validateProjectSettings(const std::fs::path &root);
    std::string readProjectFile(const std::fs::path &root, const std::fs::path &path);
    bool writeProjectFile(const std::fs::path &root, const std::fs::path &path, const std::string &content);
    void loadAssociations(const std::fs::path &root);
    bool storeAssociations(const std::fs::path &root);
    std::string rebaseStoredProviderSettings(const std::string &serializedSettings, const std::fs::path &sourceRoot, const std::fs::path &destinationRoot);
    bool copyTemporaryProjectFiles(const std::fs::path &source, const std::fs::path &destination, std::vector<std::fs::path> &copiedEntries);
    bool isPathInProject(const std::fs::path &path, const std::fs::path &root, std::fs::path &relativePath);

    void snapshotProviderSettings(prv::Provider *provider);
    prv::Provider *getProviderById(u32 id);
    void bindRegisteredData();
    void materializeRegisteredData();
    prv::Provider::OpenResult openStoredProjectProvider(u32 id);
    bool isSameFile(const std::fs::path &left, const std::fs::path &right);
    std::optional<u32> findClosedProviderForPath(const std::fs::path &path);
    bool isProviderReplacement(const prv::Provider *provider);
    StoredProviderDisplayInfo getStoredProviderDisplayInfo(u32 id);
    std::optional<ProviderManifest> readProviderManifest(const std::fs::path &root, bool showError);
    bool loadProviders(const std::fs::path &root);
    bool storeProviders(const std::fs::path &root, const std::fs::path &sourceRoot, ProviderSettings &storedSettings);
    void removeProviderFromProject(u32 id);
    bool persistProjectMetadata();
    void scheduleProjectMetadataSave();
    bool reopenProviderWithNewSettings(u32 id);

    void drawProjectSidebar();
    void resetSidebarState();

    bool load(const std::fs::path &filePath);
    bool store(std::optional<std::fs::path> filePath = std::nullopt, bool updateLocation = true);

}
