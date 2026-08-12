#pragma once

#include <content/project.hpp>

#include <filesystem>

namespace hex::plugin::builtin {

    struct LegacyProjectParseResult {
        std::vector<project::ImportedProvider> providers;
        std::vector<project::ImportedProjectFile> projectFiles;
        std::string error;
        bool hasUnsupportedData = false;

        [[nodiscard]] bool isValid() const { return error.empty(); }
    };

    LegacyProjectParseResult parseLegacyProject(const std::filesystem::path &path);
    project::ImportResult importLegacyProject(const std::filesystem::path &path);
    void registerLegacyProjectImporter();

}
