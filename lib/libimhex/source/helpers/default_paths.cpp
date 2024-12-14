#include <hex/helpers/default_paths.hpp>

#include <hex/api/imhex_api.hpp>
#include <hex/api/project_file_manager.hpp>

#if defined(OS_WINDOWS)
    #include <windows.h>
    #include <shlobj.h>
#elif defined(OS_LINUX) || defined(OS_WEB)
    #include <xdg.hpp>
# endif

namespace hex::paths {

    std::vector<std::fs::path> getDataPaths(bool includeSystemFolders) {
        std::vector<std::fs::path> paths;

        #if defined(OS_WINDOWS)

            // In the portable Windows version, we just use the executable directory
            // Prevent the use of the AppData folder here
            if (!ImHexApi::System::isPortableVersion()) {
                PWSTR wAppDataPath = nullptr;
                if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &wAppDataPath))) {
                    paths.emplace_back(wAppDataPath);
                    CoTaskMemFree(wAppDataPath);
                }
            }

        #elif defined(OS_MACOS)

            paths.push_back(wolv::io::fs::getApplicationSupportDirectoryPath() / "imhex");

        #elif defined(OS_LINUX) || defined(OS_WEB)

            paths.push_back(xdg::DataHomeDir());

            auto dataDirs = xdg::DataDirs();
            std::copy(dataDirs.begin(), dataDirs.end(), std::back_inserter(paths));

        #endif

        #if defined(OS_MACOS)

            if (includeSystemFolders) {
                if (auto executablePath = wolv::io::fs::getExecutablePath(); executablePath.has_value()) {
                    paths.push_back(*executablePath);
                }
            }

        #else

            for (auto &path : paths)
                path = path / "imhex";

            if (ImHexApi::System::isPortableVersion() || includeSystemFolders) {
                if (auto executablePath = wolv::io::fs::getExecutablePath(); executablePath.has_value())
                    paths.push_back(executablePath->parent_path());
            }

        #endif


        // Add additional data directories to the path
        auto additionalDirs = ImHexApi::System::getAdditionalFolderPaths();
        std::ranges::copy(additionalDirs, std::back_inserter(paths));

        // Add the project file directory to the path, if one is loaded
        if (ProjectFile::hasPath()) {
            paths.push_back(ProjectFile::getPath().parent_path());
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
        std::vector<std::fs::path> paths = getDataPaths(true);

        // Add the system plugin directory to the path if one was provided at compile time
        #if defined(OS_LINUX) && defined(SYSTEM_PLUGINS_LOCATION)
            paths.push_back(SYSTEM_PLUGINS_LOCATION);
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