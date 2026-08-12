#pragma once

#include <hex.hpp>

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <vector>

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

    ImportResult importProviders(std::vector<ImportedProvider> providers, std::vector<ImportedProjectFile> projectFiles = {});

}
