#include <hex/api/project_file_manager.hpp>

#include <hex/helpers/tar.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/logger.hpp>

#include <hex/providers/provider.hpp>

#include <wolv/utils/guards.hpp>
#include <wolv/io/fs.hpp>

namespace hex {

    std::vector<ProjectFile::Handler> ProjectFile::s_handlers;
    std::vector<ProjectFile::ProviderHandler> ProjectFile::s_providerHandlers;

    std::fs::path ProjectFile::s_currProjectPath;

    std::function<bool(const std::fs::path&)> ProjectFile::s_loadProjectFunction;
    std::function<bool(std::optional<std::fs::path>, bool)> ProjectFile::s_storeProjectFunction;

    void ProjectFile::setProjectFunctions(
            const std::function<bool(const std::fs::path&)> &loadFun,
            const std::function<bool(std::optional<std::fs::path>, bool)> &storeFun
    ) {
        ProjectFile::s_loadProjectFunction = loadFun;
        ProjectFile::s_storeProjectFunction = storeFun;
    }

    bool ProjectFile::load(const std::fs::path &filePath) {
      return s_loadProjectFunction(filePath);
    }

    bool ProjectFile::store(std::optional<std::fs::path> filePath, bool updateLocation) {
       return s_storeProjectFunction(filePath, updateLocation);
    }

    bool ProjectFile::hasPath() {
        return !ProjectFile::s_currProjectPath.empty();
    }

    void ProjectFile::clearPath() {
        ProjectFile::s_currProjectPath.clear();
    }

    std::fs::path ProjectFile::getPath() {
        return ProjectFile::s_currProjectPath;
    }

    void ProjectFile::setPath(const std::fs::path &path) {
        ProjectFile::s_currProjectPath = path;
    }

}