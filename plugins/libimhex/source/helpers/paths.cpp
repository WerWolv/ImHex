#include <hex/helpers/paths.hpp>
#include <hex/helpers/paths_mac.h>

#if defined(OS_WINDOWS)
    #include <windows.h>
    #include <shlobj.h>
#elif defined(OS_LINUX)
    #include <xdg.hpp>
    #include <linux/limits.h>
#endif

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace hex {

    std::vector<std::string> getPath(ImHexPath path) {
        #if defined(OS_WINDOWS)
            std::string exePath(MAX_PATH, '\0');
            GetModuleFileName(nullptr, exePath.data(), exePath.length());
            auto parentDir = std::filesystem::path(exePath).parent_path();

            std::filesystem::path appDataDir;
            {
                LPWSTR wAppDataPath = nullptr;
                if (!SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &wAppDataPath)))
                    throw std::runtime_error("Failed to get APPDATA folder path");

                appDataDir = wAppDataPath;
                CoTaskMemFree(wAppDataPath);
            }

            std::vector<std::filesystem::path> paths = { parentDir, appDataDir / "imhex" };
            std::vector<std::string> results;

            switch (path) {
                case ImHexPath::Patterns:
                    std::transform(paths.begin(), paths.end(), std::back_inserter(results), [](auto &path){
                        return (path / "patterns").string();
                    });
                    break;
                case ImHexPath::PatternsInclude:
                    std::transform(paths.begin(), paths.end(), std::back_inserter(results), [](auto &path){
                        return (path / "includes").string();
                    });
                    break;
                case ImHexPath::Magic:
                    std::transform(paths.begin(), paths.end(), std::back_inserter(results), [](auto &path){
                        return (path / "magic").string();
                    });
                    break;
                case ImHexPath::Python:
                    std::transform(paths.begin(), paths.end(), std::back_inserter(results), [](auto &path){
                        return (path / "python").string();
                    });
                    break;
                case ImHexPath::Plugins:
                    std::transform(paths.begin(), paths.end(), std::back_inserter(results), [](auto &path){
                        return (path / "plugins").string();
                    });
                    break;
                case ImHexPath::Yara:
                    std::transform(paths.begin(), paths.end(), std::back_inserter(results), [](auto &path){
                        return (path / "yara").string();
                    });
                    break;
                case ImHexPath::Config:
                    return { (appDataDir / "imhex" / "config").string() };
                case ImHexPath::Resources:
                    std::transform(paths.begin(), paths.end(), std::back_inserter(results), [](auto &path){
                        return (path / "resources").string();
                    });
                    break;
                case ImHexPath::Constants:
                    std::transform(paths.begin(), paths.end(), std::back_inserter(results), [](auto &path){
                        return (path / "constants").string();
                    });
                    break;
                default: __builtin_unreachable();
            }

            return results;
        #elif defined(OS_MACOS)
            // Get path to special directories
            const std::filesystem::path exeDir(getMacExecutableDirectoryPath());
            const std::filesystem::path applicationSupportDir(getMacApplicationSupportDirectoryPath());

            std::vector<std::filesystem::path> paths = { exeDir, applicationSupportDir };
            std::vector<std::string> results;

            switch (path) {
            case ImHexPath::Patterns:
                return { (applicationSupportDir / "patterns").string() };
            case ImHexPath::PatternsInclude:
                return { (applicationSupportDir / "includes").string() };
            case ImHexPath::Magic:
                return { (applicationSupportDir / "magic").string() };
            case ImHexPath::Python:
                return { (applicationSupportDir / "python").string() };
            case ImHexPath::Plugins:
                std::transform(paths.begin(), paths.end(), std::back_inserter(results), [](auto &path){
                    return (path / "plugins").string();
                });
                break;
            case ImHexPath::Yara:
                return { (applicationSupportDir / "yara").string() };
            case ImHexPath::Config:
                return { (applicationSupportDir / "config").string() };
            case ImHexPath::Resources:
                return { (applicationSupportDir / "resources").string() };
            case ImHexPath::Constants:
                return { (applicationSupportDir / "constants").string() };
            default: __builtin_unreachable();
            }

            return results;
        #else
            std::vector<std::filesystem::path> configDirs = xdg::ConfigDirs();
            std::vector<std::filesystem::path> dataDirs = xdg::DataDirs();

            configDirs.insert(configDirs.begin(), xdg::ConfigHomeDir());
            dataDirs.insert(dataDirs.begin(), xdg::DataHomeDir());

            for (auto &dir : dataDirs)
                dir = dir / "imhex";

            std::array<char, PATH_MAX> executablePath = { 0 };
            if (readlink("/proc/self/exe", executablePath.data(), PATH_MAX) != -1)
                dataDirs.emplace(dataDirs.begin(), std::filesystem::path(executablePath.data()).parent_path());

            std::vector<std::string> result;

            switch (path) {
                case ImHexPath::Patterns:
                    std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "patterns").string(); });
                    break;
                case ImHexPath::PatternsInclude:
                    std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "includes").string(); });
                    break;
                case ImHexPath::Magic:
                    std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "magic").string(); });
                    break;
                case ImHexPath::Python:
                    std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p).string(); });
                    break;
                case ImHexPath::Plugins:
                    std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "plugins").string(); });
                    break;
                case ImHexPath::Yara:
                    std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "yara").string(); });
                    break;
                case ImHexPath::Config:
                    std::transform(configDirs.begin(), configDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "imhex").string(); });
                    break;
                case ImHexPath::Resources:
                    std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "resources").string(); });
                    break;
                case ImHexPath::Constants:
                    std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "constants").string(); });
                    break;
                default: __builtin_unreachable();
            }

            return result;
        #endif
    }

}