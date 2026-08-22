#include <hex/api/project_manager.hpp>

#include <hex/helpers/auto_reset.hpp>

#include <wolv/io/fs.hpp>

namespace hex {

    namespace {


        AutoReset<std::fs::path> s_currProjectPath;
        AutoReset<bool> s_folderProject;

        AutoReset<std::function<bool(const std::fs::path&)>> s_loadProjectFunction;
        AutoReset<std::function<bool(std::optional<std::fs::path>, bool)>> s_storeProjectFunction;

    }


    void ProjectManager::setProjectFunctions(
            const std::function<bool(const std::fs::path&)> &loadFun,
            const std::function<bool(std::optional<std::fs::path>, bool)> &storeFun
    ) {
        s_loadProjectFunction = loadFun;
        s_storeProjectFunction = storeFun;
    }

    bool ProjectManager::load(const std::fs::path &filePath) {
      return (*s_loadProjectFunction)(filePath);
    }

    bool ProjectManager::store(std::optional<std::fs::path> filePath, bool updateLocation) {
       return (*s_storeProjectFunction)(std::move(filePath), updateLocation);
    }

    bool ProjectManager::hasPath() {
        return !s_currProjectPath->empty();
    }

    void ProjectManager::clearPath() {
        s_currProjectPath->clear();
        s_folderProject = false;
    }

    std::fs::path ProjectManager::getPath() {
        return *s_currProjectPath;
    }

    void ProjectManager::setPath(const std::fs::path &path) {
        s_currProjectPath = path;
        std::error_code error;
        s_folderProject = std::fs::is_directory(path, error);
    }

    std::fs::path ProjectManager::getProjectRoot() {
        if (isFolderProject())
            return *s_currProjectPath;
        return s_currProjectPath->parent_path();
    }

    bool ProjectManager::isFolderProject() {
        return *s_folderProject;
    }

    void ProjectManager::setFolderProject(bool folderProject) {
        s_folderProject = folderProject;
    }



}
