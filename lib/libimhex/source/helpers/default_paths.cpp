#include <algorithm>
#include <hex/helpers/default_paths.hpp>

#include <hex/api/imhex_api/system.hpp>
#include <hex/api/project_file_manager.hpp>

#include <ranges>
#include <set>

#if defined(OS_WINDOWS)
    #include <windows.h>
    #include <shlobj.h>
#elif defined(OS_LINUX) || defined(OS_WEB)
    #include <xdg.hpp>
# endif

namespace hex::paths {

    std::vector<std::fs::path> getDataPaths(bool includeSystemFolders) {
        std::vector<std::fs::path> paths;
        std::set<std::fs::path> uniquePaths;

        const auto emplaceUniquePath = [&](const std::fs::path &path) {
            auto duplicate = uniquePaths.insert(path);
            if (duplicate.second)
                paths.emplace_back(path);
        };

        #if defined(OS_WINDOWS)

            // In the portable Windows version, we just use the executable directory
            // Prevent the use of the AppData folder here
            if (!ImHexApi::System::isPortableVersion()) {
                PWSTR wAppDataPath = nullptr;
                if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &wAppDataPath))) {
                    emplaceUniquePath(wAppDataPath);
                    CoTaskMemFree(wAppDataPath);
                }
            }

        #elif defined(OS_MACOS)

            emplaceUniquePath(wolv::io::fs::getApplicationSupportDirectoryPath() / "imhex");

        #elif defined(OS_LINUX) || defined(OS_WEB)

            emplaceUniquePath(xdg::DataHomeDir());

            auto dataDirs = xdg::DataDirs();
            std::ranges::for_each(dataDirs, [&](auto &path) { emplaceUniquePath(path); });

        #endif

        #if defined(OS_MACOS)

            if (includeSystemFolders) {
                if (auto executablePath = wolv::io::fs::getExecutablePath(); executablePath.has_value()) {
                    emplaceUniquePath(executablePath->parent_path());
                }
            }

        #else

            uniquePaths.clear();
            std::ranges::for_each(paths, [&](auto &path) {
                path = path / "imhex";
                uniquePaths.insert(path);
            });

            if (ImHexApi::System::isPortableVersion() || includeSystemFolders) {
                if (auto executablePath = wolv::io::fs::getExecutablePath(); executablePath.has_value())
                    emplaceUniquePath(executablePath->parent_path());
            }

        #endif


        // Add additional data directories to the path
        auto additionalDirs = ImHexApi::System::getAdditionalFolderPaths();
        std::ranges::for_each(additionalDirs, [&](auto &path) { emplaceUniquePath(path); });

        // Add the project file directory to the path, if one is loaded
        if (ProjectFile::hasPath()) {
            emplaceUniquePath(ProjectFile::getPath().parent_path());
        }

        return paths;
    }

    std::vector<std::fs::path> getConfigPaths(bool includeSystemFolders) {
        #if defined(OS_WINDOWS)
            return getDataPaths(includeSystemFolders);
        #elif defined(OS_MACOS)
            return getDataPaths(includeSystemFolders);
        #elif defined(OS_LINUX) || defined(OS_WEB)
            std::ignore = includeSystemFolders;
            return {xdg::ConfigHomeDir() / "imhex"};
        #endif
    }

    static std::vector<std::fs::path> appendPath(std::vector<std::fs::path> paths, std::fs::path folder) {
        folder.make_preferred();
        
        for (auto &path : paths)
            path = path / folder;

        return paths;
    }

    static std::vector<std::fs::path> getPluginPaths() {
        // If running from an AppImage, only allow loaded plugins from inside it
        #if defined(OS_LINUX)
        if(const char* appdir = std::getenv("APPDIR")) { // check for AppImage environment
            return {std::string(appdir) + "/usr/lib/imhex"};
        }
        #endif

        std::vector<std::fs::path> paths = getDataPaths(true);

        // Add the system plugin directory to the path if one was provided at compile time
        #if defined(OS_LINUX) && defined(SYSTEM_PLUGINS_LOCATION)
            paths.emplace_back(SYSTEM_PLUGINS_LOCATION);
        #endif

        return paths;
    }

    namespace impl {

        std::vector<std::fs::path> DefaultPath::read() const {
            auto result = this->all();

            std::erase_if(result, [](const auto &entryPath) {
                return !wolv::io::fs::isDirectory(entryPath);
            });

            return result;
        }

        std::vector<std::fs::path> DefaultPath::write() const {
            auto result = this->read();

            std::erase_if(result, [](const auto &entryPath) {
                return !hex::fs::isPathWritable(entryPath);
            });

            return result;
        }

        std::vector<std::fs::path> ConfigPath::all() const {
            return appendPath(getConfigPaths(false), m_postfix);
        }

        std::vector<std::fs::path> DataPath::all() const {
            return appendPath(getDataPaths(true), m_postfix);
        }

        std::vector<std::fs::path> DataPath::write() const {
            auto result = appendPath(getDataPaths(false), m_postfix);

            std::erase_if(result, [](const auto &entryPath) {
                return !hex::fs::isPathWritable(entryPath);
            });

            return result;
        }

        std::vector<std::fs::path> PluginPath::all() const {
            return appendPath(getPluginPaths(), m_postfix);
        }

    }

}