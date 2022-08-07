#include <hex/api/project_file_manager.hpp>

#include <hex/helpers/tar.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>

#include <hex/providers/provider.hpp>

namespace hex {

    constexpr static auto MetadataHeaderMagic = "HEX";
    constexpr static auto MetadataPath = "IMHEX_METADATA";

    std::vector<ProjectFile::Handler> ProjectFile::s_handlers;
    std::vector<ProjectFile::ProviderHandler> ProjectFile::s_providerHandlers;

    std::fs::path ProjectFile::s_currProjectPath;

    bool ProjectFile::load(const std::fs::path &filePath) {
        if (!fs::exists(filePath) || !fs::isRegularFile(filePath))
            return false;
        if (filePath.extension() != ".hexproj")
            return false;

        Tar tar(filePath, Tar::Mode::Read);
        if (!tar.isValid())
            return false;

        if (!tar.contains(MetadataPath))
            return false;

        {
            const auto metadataContent = tar.read(MetadataPath);

            if (!std::string(metadataContent.begin(), metadataContent.end()).starts_with(MetadataHeaderMagic))
                return false;
        }

        bool result = true;
        for (const auto &handler : ProjectFile::getHandlers()) {
            try {
                if (!handler.load(handler.basePath, tar))
                    result = false;
            } catch (std::exception &e) {
                log::info("{}", e.what());
                result = false;
            }
        }

        for (const auto &provider : ImHexApi::Provider::getProviders()) {
            const auto basePath = std::fs::path(std::to_string(provider->getID()));
            for (const auto &handler: ProjectFile::getProviderHandlers()) {
                try {
                    if (!handler.load(provider, basePath / handler.basePath, tar))
                        result = false;
                } catch (std::exception &e) {
                    log::info("{}", e.what());
                    result = false;
                }
            }
        }

        ProjectFile::s_currProjectPath = filePath;

        return result;
    }

    bool ProjectFile::store(std::optional<std::fs::path> filePath) {
        if (!filePath.has_value())
            filePath = ProjectFile::s_currProjectPath;

        Tar tar(*filePath, Tar::Mode::Create);
        if (!tar.isValid())
            return false;

        bool result = true;
        for (const auto &handler : ProjectFile::getHandlers()) {
            try {
                if (!handler.store(handler.basePath, tar))
                    result = false;
            } catch (std::exception &e) {
                log::info("{}", e.what());
                result = false;
            }
        }
        for (const auto &provider : ImHexApi::Provider::getProviders()) {
            const auto basePath = std::fs::path(std::to_string(provider->getID()));
            for (const auto &handler: ProjectFile::getProviderHandlers()) {
                try {
                    if (!handler.store(provider, basePath / handler.basePath, tar))
                        result = false;
                } catch (std::exception &e) {
                    log::info("{}", e.what());
                    result = false;
                }
            }
        }

        {
            const auto metadataContent = hex::format("{}\n{}@{}\n{}", MetadataHeaderMagic, GIT_BRANCH, GIT_COMMIT_HASH, IMHEX_VERSION);
            tar.write(MetadataPath, metadataContent);
        }

        return result;
    }

}