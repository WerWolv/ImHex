#include <hex/api/project_file_manager.hpp>

#include <hex/helpers/auto_reset.hpp>

#include <wolv/io/fs.hpp>

namespace hex {

    namespace {

        AutoReset<std::vector<ProjectFile::Handler>> s_handlers;
        AutoReset<std::vector<ProjectFile::ProviderHandler>> s_providerHandlers;

        AutoReset<std::fs::path> s_currProjectPath;

        AutoReset<std::function<bool(const std::fs::path&)>> s_loadProjectFunction;
        AutoReset<std::function<bool(std::optional<std::fs::path>, bool)>> s_storeProjectFunction;

    }


    void ProjectFile::setProjectFunctions(
            const std::function<bool(const std::fs::path&)> &loadFun,
            const std::function<bool(std::optional<std::fs::path>, bool)> &storeFun
    ) {
        s_loadProjectFunction = loadFun;
        s_storeProjectFunction = storeFun;
    }

    bool ProjectFile::load(const std::fs::path &filePath) {
      return (*s_loadProjectFunction)(filePath);
    }

    bool ProjectFile::store(std::optional<std::fs::path> filePath, bool updateLocation) {
       return (*s_storeProjectFunction)(std::move(filePath), updateLocation);
    }

    bool ProjectFile::hasPath() {
        return !s_currProjectPath->empty();
    }

    void ProjectFile::clearPath() {
        s_currProjectPath->clear();
    }

    std::fs::path ProjectFile::getPath() {
        return s_currProjectPath;
    }

    void ProjectFile::setPath(const std::fs::path &path) {
        s_currProjectPath = path;
    }

    void ProjectFile::registerHandler(const Handler &handler) {
        s_handlers->push_back(handler);
    }

    void ProjectFile::registerPerProviderHandler(const ProviderHandler &handler) {
        s_providerHandlers->push_back(handler);
    }

    const std::vector<ProjectFile::Handler>& ProjectFile::getHandlers() {
        return s_handlers;
    }

    const std::vector<ProjectFile::ProviderHandler>& ProjectFile::getProviderHandlers() {
        return s_providerHandlers;
    }

}