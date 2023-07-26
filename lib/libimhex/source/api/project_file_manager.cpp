#include <hex/api/project_file_manager.hpp>

#include <hex/helpers/tar.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>

#include <hex/providers/provider.hpp>

#include <wolv/utils/guards.hpp>
#include <wolv/io/fs.hpp>

namespace hex {

    namespace {

        std::vector<ProjectFile::Handler> s_handlers;
        std::vector<ProjectFile::ProviderHandler> s_providerHandlers;

        std::fs::path s_currProjectPath;

        std::function<bool(const std::fs::path&)> s_loadProjectFunction;
        std::function<bool(std::optional<std::fs::path>, bool)> s_storeProjectFunction;

    }


    void ProjectFile::setProjectFunctions(
            const std::function<bool(const std::fs::path&)> &loadFun,
            const std::function<bool(std::optional<std::fs::path>, bool)> &storeFun
    ) {
        s_loadProjectFunction = loadFun;
        s_storeProjectFunction = storeFun;
    }

    bool ProjectFile::load(const std::fs::path &filePath) {
      return s_loadProjectFunction(filePath);
    }

    bool ProjectFile::store(std::optional<std::fs::path> filePath, bool updateLocation) {
       return s_storeProjectFunction(filePath, updateLocation);
    }

    bool ProjectFile::hasPath() {
        return !s_currProjectPath.empty();
    }

    void ProjectFile::clearPath() {
        s_currProjectPath.clear();
    }

    std::fs::path ProjectFile::getPath() {
        return s_currProjectPath;
    }

    void ProjectFile::setPath(const std::fs::path &path) {
        s_currProjectPath = path;
    }

    std::vector<ProjectFile::Handler> &ProjectFile::getHandlers() {
        return s_handlers;
    }

    std::vector<ProjectFile::ProviderHandler> &ProjectFile::getProviderHandlers() {
        return s_providerHandlers;
    }

}