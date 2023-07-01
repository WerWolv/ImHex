#pragma once

#include <list>
#include <string>
#include <string_view>

#include <hex/api/imhex_api.hpp>
#include <hex/api/event.hpp>

#include <hex/helpers/fs.hpp>
#include <hex/helpers/utils.hpp>
#include <hex/helpers/concepts.hpp>
#include <hex/helpers/tar.hpp>

/**
 * @brief Project file manager
 *
 * The project file manager is used to load and store project files. It is used by all features of ImHex
 * that want to store any data to a Project File.
 *
 */
namespace hex {

    class ProjectFile {
    public:
        struct Handler {
            using Function = std::function<bool(const std::fs::path &, Tar &tar)>;

            std::fs::path basePath; //< Base path for where to store the files in the project file
            bool required;          //< If true, ImHex will display an error if this handler fails to load or store data
            Function load, store;   //< Functions to load and store data
        };

        struct ProviderHandler {
            using Function = std::function<bool(prv::Provider *provider, const std::fs::path &, Tar &tar)>;

            std::fs::path basePath; //< Base path for where to store the files in the project file
            bool required;          //< If true, ImHex will display an error if this handler fails to load or store data
            Function load, store;   //< Functions to load and store data
        };

        /**
         * @brief Set implementations for loading and restoring a project
         *
         * @param loadFun function to use to load a project in ImHex
         * @param storeFun function to use to store a project to disk
         */
        static void setProjectFunctions(
            const std::function<bool(const std::fs::path&)> &loadFun,
            const std::function<bool(std::optional<std::fs::path>, bool)> &storeFun
        );

        /**
         * @brief Load a project file
         *
         * @param filePath Path to the project file
         * @return true if the project file was loaded successfully
         * @return false if the project file was not loaded successfully
         */
        static bool load(const std::fs::path &filePath);

        /**
         * @brief Store a project file
         *
         * @param filePath Path to the project file
         * @param updateLocation update the project location so subssequent saves will save there
         * @return true if the project file was stored successfully
         * @return false if the project file was not stored successfully
         */
        static bool store(std::optional<std::fs::path> filePath = std::nullopt, bool updateLocation = true);

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
        static std::fs::path getPath();

        /**
         * @brief Set the path to the currently loaded project file
         * @param path Path to the currently loaded project file
         */
        static void setPath(const std::fs::path &path);

        /**
         * @brief Register a handler for storing and loading global data from a project file
         *
         * @param handler The handler to register
         */
        static void registerHandler(const Handler &handler) {
            getHandlers().push_back(handler);
        }

        /**
         * @brief Register a handler for storing and loading per-provider data from a project file
         *
         * @param handler The handler to register
         */
        static void registerPerProviderHandler(const ProviderHandler &handler) {
            getProviderHandlers().push_back(handler);
        }

        /**
         * @brief Get the list of registered handlers
         * @return List of registered handlers
         */
        static std::vector<Handler>& getHandlers() {
            return s_handlers;
        }

        /**
         * @brief Get the list of registered per-provider handlers
         * @return List of registered per-provider handlers
         */
        static std::vector<ProviderHandler>& getProviderHandlers() {
            return s_providerHandlers;
        }

    private:
        ProjectFile() = default;

        static std::function<bool(const std::fs::path&)> s_loadProjectFunction;
        static std::function<bool(std::optional<std::fs::path>, bool)> s_storeProjectFunction;

        static std::fs::path s_currProjectPath;
        static std::vector<Handler> s_handlers;
        static std::vector<ProviderHandler> s_providerHandlers;
    };

}