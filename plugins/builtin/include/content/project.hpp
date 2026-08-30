#pragma once

#include <hex.hpp>

#include <nlohmann/json.hpp>

#include <map>
#include <filesystem>
#include <string>
#include <vector>

namespace hex::prv {
    class Provider;
}

namespace hex::plugin::builtin::project {

    struct ImportedFile {
        std::string typeId;
        std::string extension;
        std::vector<u8> contents;
    };

    struct ImportedProvider {
        u32 id;
        nlohmann::json descriptor;
        std::vector<ImportedFile> files;
        std::map<u64, u8> patches;
    };

    struct ImportedProjectFile {
        std::string name;
        std::vector<u8> contents;
    };

    struct ImportResult {
        bool success = false;
        std::string error;
        std::vector<u32> failedProviderIds;
        std::vector<std::string> warnings;
        size_t importedProviderCount = 0;
        size_t importedFileCount = 0;
    };

    std::string createEmptyProject(const std::filesystem::path &path);
    bool createProjectFile();
    bool moveProjectEntry(const std::filesystem::path &source, const std::filesystem::path &destination);
    ImportResult importProviders(std::vector<ImportedProvider> providers, std::vector<ImportedProjectFile> projectFiles = {});
    prv::Provider *openProviderForPath(const std::filesystem::path &path, const prv::Provider *excludedProvider = nullptr);
    bool prepareForShutdown(bool persist = true);

}
