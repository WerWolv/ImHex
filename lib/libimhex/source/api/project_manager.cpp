#include <hex/api/project_manager.hpp>

#include <hex/helpers/auto_reset.hpp>
#include <hex/helpers/default_paths.hpp>

#include <wolv/io/fs.hpp>

namespace hex {

    namespace {


        AutoReset<std::fs::path> s_currProjectPath;
        AutoReset<bool> s_folderProject, s_temporaryProject;
        AutoReset<std::fs::path> s_temporaryProjectPath;

        AutoReset<std::function<bool(const std::fs::path&)>> s_loadProjectFunction;
        AutoReset<std::function<bool(std::optional<std::fs::path>, bool)>> s_storeProjectFunction;

    }

    static std::fs::path resolveTemporaryProjectPath() {
        if (!s_temporaryProjectPath->empty())
            return *s_temporaryProjectPath;

        for (const auto &path : paths::Config.write()) {
            s_temporaryProjectPath = path / "default_project";
            return *s_temporaryProjectPath;
        }

        return {};
    }


    void ProjectManager::setProjectFunctions(
            const std::function<bool(const std::fs::path&)> &loadFun,
            const std::function<bool(std::optional<std::fs::path>, bool)> &storeFun
    ) {
        s_loadProjectFunction = loadFun;
        s_storeProjectFunction = storeFun;
    }

    bool ProjectManager::load(const std::fs::path &filePath) {
        const bool wasTemporary = *s_temporaryProject;
        s_temporaryProject = false;
        if ((*s_loadProjectFunction)(filePath))
            return true;

        s_temporaryProject = wasTemporary;
        return false;
    }

    bool ProjectManager::loadTemporaryProject() {
        if (*s_temporaryProject && hasPath())
            return true;

        const auto projectPath = resolveTemporaryProjectPath();
        if (projectPath.empty())
            return false;

        std::error_code error;
        const auto quarantineProject = [&] {
            error.clear();
            auto quarantinedPath = std::fs::path(projectPath.string() + ".corrupt");
            for (u32 index = 1; std::fs::exists(quarantinedPath, error) && !error; ++index)
                quarantinedPath = std::fs::path(projectPath.string() + ".corrupt-" + std::to_string(index));
            if (error)
                return false;
            std::fs::rename(projectPath, quarantinedPath, error);
            return !error;
        };

        for (const auto &entry : std::fs::directory_iterator(projectPath.parent_path(), error)) {
            if (error)
                break;
            if (entry.path().filename().string().starts_with(projectPath.filename().string() + ".promoted") &&
                std::fs::is_regular_file(entry.path() / ProjectDirectory / "promoted", error) && !error) {
                std::error_code cleanupError;
                std::fs::remove_all(entry.path(), cleanupError);
            }
            error.clear();
        }
        std::fs::remove(projectPath / ProjectDirectory / "promoted", error);
        error.clear();
        std::fs::create_directories(projectPath, error);
        if (error) {
            error.clear();
            const auto projectStatus = std::fs::symlink_status(projectPath, error);
            if (error || !std::fs::exists(projectStatus) || !quarantineProject())
                return false;
            std::fs::create_directories(projectPath, error);
            if (error)
                return false;
        }

        const bool wasTemporary = *s_temporaryProject;
        s_temporaryProject = true;
        if ((*s_loadProjectFunction)(projectPath))
            return true;

        if (!quarantineProject()) {
            s_temporaryProject = wasTemporary;
            return false;
        }

        std::fs::create_directories(projectPath, error);
        if (!error && (*s_loadProjectFunction)(projectPath))
            return true;

        s_temporaryProject = wasTemporary;
        return false;
    }

    bool ProjectManager::isTemporaryProject() {
        return *s_temporaryProject;
    }

    std::fs::path ProjectManager::getTemporaryProjectPath() {
        return resolveTemporaryProjectPath();
    }

    bool ProjectManager::store(std::optional<std::fs::path> filePath, bool updateLocation) {
        const bool promotingTemporaryProject = *s_temporaryProject && updateLocation && filePath.has_value() &&
            filePath->lexically_normal() != s_currProjectPath->lexically_normal();
        if (promotingTemporaryProject)
            s_temporaryProject = false;

        const bool result = (*s_storeProjectFunction)(std::move(filePath), updateLocation);
        if (!result && promotingTemporaryProject)
            s_temporaryProject = true;

        return result;
    }

    bool ProjectManager::hasPath() {
        return !s_currProjectPath->empty();
    }

    void ProjectManager::clearPath() {
        s_currProjectPath->clear();
        s_folderProject = false;
        s_temporaryProject = false;
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
