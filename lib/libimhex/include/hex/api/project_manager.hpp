#pragma once

#include <hex.hpp>

#include <filesystem>
#include <functional>
#include <optional>

/**
 * @brief Folder project manager
 *
 * The project file manager is used to load and store project files. It is used by all features of ImHex
 * that want to store any data to a Project File.
 *
 */
EXPORT_MODULE namespace hex {

    namespace prv {
        class Provider;
    }

    class ProjectManager {
    public:
        constexpr static auto ProjectDirectory = ".imhex";

        /**
         * @brief Set implementations for loading and restoring a project
         *
         * @param loadFun function to use to load a project in ImHex
         * @param storeFun function to use to store a project to disk
         */
        static void setProjectFunctions(
            const std::function<bool(const std::filesystem::path&)> &loadFun,
            const std::function<bool(std::optional<std::filesystem::path>, bool)> &storeFun
        );


        /**
         * @brief Load a project file
         *
         * @param filePath Path to the project file
         * @return true if the project file was loaded successfully
         * @return false if the project file was not loaded successfully
         */
        static bool load(const std::filesystem::path &filePath);

        /**
         * @brief Load the persistent project used when no explicit project is open.
         */
        static bool loadTemporaryProject();

        /**
         * @brief Check whether the current project is ImHex's implicit temporary project.
         */
        static bool isTemporaryProject();

        /**
         * @brief Get the storage location of ImHex's implicit temporary project.
         */
        static std::filesystem::path getTemporaryProjectPath();

        /**
         * @brief Store a project file
         *
         * @param filePath Path to the project file
         * @param updateLocation update the project location so subssequent saves will save there
         * @return true if the project file was stored successfully
         * @return false if the project file was not stored successfully
         */
        static bool store(std::optional<std::filesystem::path> filePath = std::nullopt, bool updateLocation = true);

        /**
         * @brief Check if a project file is currently loaded
         *
         * @return true if a project file is currently loaded
         * @return false if no project file is currently loaded
         */
        static bool hasPath();

        /**
         * @brief Clear the currently loaded project file
         */
        static void clearPath();

        /**
         * @brief Get the path to the currently loaded project file
         * @return Path to the currently loaded project file
         */
        static std::filesystem::path getPath();

        /**
         * @brief Set the path to the currently loaded project file
         * @param path Path to the currently loaded project file
         */
        static void setPath(const std::filesystem::path &path);

        /**
         * @brief Get the directory containing user-visible project files.
         */
        static std::filesystem::path getProjectRoot();

        /**
         * @brief Check whether the current project is backed by a directory.
         */
        static bool isFolderProject();

        static void setFolderProject(bool folderProject);

    private:
        ProjectManager() = default;
    };

}
