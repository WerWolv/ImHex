#include <hex/api/project_manager.hpp>

#include "hex/helpers/default_paths.hpp"

#include <hex/helpers/auto_reset.hpp>

#include <wolv/io/fs.hpp>

namespace hex {

    namespace {

        AutoReset<std::fs::path> s_currProjectPath;
        AutoReset<bool> s_folderProject, s_defaultProject;

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
        s_defaultProject = false;
        return (*s_loadProjectFunction)(filePath);
    }

    bool ProjectManager::loadDefaultProject() {
        for (const auto &path : paths::Backups.write()) {
            const auto projectPath = path / "default_project";
            s_defaultProject = true;
            wolv::io::fs::createDirectories(projectPath);
            const auto result = (*s_loadProjectFunction)(projectPath);

            if (!result)
                s_defaultProject = false;

            return result;
        }

        return false;
    }

    bool ProjectManager::isDefaultProject() {
        return s_defaultProject;
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
