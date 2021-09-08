#include <hex/helpers/paths.hpp>
#include <hex/helpers/paths_mac.h>

#if defined(OS_WINDOWS)
    #include <windows.h>
    #include <shlobj.h>
#elif defined(OS_LINUX)
    #include <xdg.hpp>
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

            switch (path) {
                case ImHexPath::Patterns:
                    return { (parentDir / "pattern_language").string() };
                case ImHexPath::PatternsInclude:
                    return { (parentDir / "includes").string() };
                case ImHexPath::Magic:
                    return { (parentDir / "magic").string() };
                case ImHexPath::Python:
                    return { parentDir.string() };
                case ImHexPath::Plugins:
                    return { (parentDir / "plugins").string() };
                case ImHexPath::Yara:
                    return { (parentDir / "yara").string() };
                case ImHexPath::Config:
                    return { (appDataDir / "imhex" / "config").string() };
                case ImHexPath::Resources:
                    return { (parentDir / "resources").string() };
                case ImHexPath::Constants:
                    return { (parentDir / "constants").string() };
                default: __builtin_unreachable();
            }
        #elif defined(OS_MACOS)
            return { getPathForMac(path) };
        #else
            std::vector<std::filesystem::path> configDirs = xdg::ConfigDirs();
            std::vector<std::filesystem::path> dataDirs = xdg::DataDirs();

            configDirs.insert(configDirs.begin(), xdg::ConfigHomeDir());
            dataDirs.insert(dataDirs.begin(), xdg::DataHomeDir());

            std::vector<std::string> result;

            switch (path) {
                case ImHexPath::Patterns:
                    std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "imhex" / "patterns").string(); });
                    return result;
                case ImHexPath::PatternsInclude:
                    std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "imhex" / "includes").string(); });
                    return result;
                case ImHexPath::Magic:
                    std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "imhex" / "magic").string(); });
                    return result;
                case ImHexPath::Python:
                    std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "imhex").string(); });
                    return result;
                case ImHexPath::Plugins:
                    std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "imhex" / "plugins").string(); });
                    return result;
                case ImHexPath::Yara:
                    std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "imhex" / "yara").string(); });
                    return result;
                case ImHexPath::Config:
                    std::transform(configDirs.begin(), configDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "imhex").string(); });
                    return result;
                case ImHexPath::Resources:
                    std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "imhex" / "resources").string(); });
                    return result;
                case ImHexPath::Constants:
                    std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result),
                        [](auto p) { return (p / "imhex" / "constants").string(); });
                    return result;
                default: __builtin_unreachable();
            }
        #endif
    }

}