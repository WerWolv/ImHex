#include <hex/api/content_registry.hpp>
#include <hex/helpers/paths_mac.h>

#include <xdg.hpp>

#if defined(OS_WINDOWS)
    #include <windows.h>
    #include <shlobj.h>
#elif defined(OS_LINUX)
    #include <xdg.hpp>
    #include <linux/limits.h>
#endif

#include <algorithm>
#include <filesystem>

namespace hex {

    std::string getExecutablePath() {
#if defined(OS_WINDOWS)
        std::string exePath(MAX_PATH, '\0');
        GetModuleFileName(nullptr, exePath.data(), exePath.length());

        return exePath;
#elif defined(OS_LINUX)
        std::string exePath(PATH_MAX, '\0');
        readlink("/proc/self/exe", exePath.data(), PATH_MAX);

        return exePath;
#elif defined(OS_MACOS)
        return getMacExecutableDirectoryPath();
#else
        return "";
#endif
    }

    std::vector<fs::path> getPath(ImHexPath path, bool listNonExisting) {
        std::vector<fs::path> result;
        const auto exePath = getExecutablePath();
        const std::string settingName { "hex.builtin.setting.folders" };
        auto userDirs = ContentRegistry::Settings::read(settingName, settingName, std::vector<std::string> {});

        auto addUserDirs = [&userDirs](auto &paths) {
            std::transform(userDirs.begin(), userDirs.end(), std::back_inserter(paths), [](auto &item) {
                return std::move(item);
            });
        };

#if defined(OS_WINDOWS)
        const auto parentDir = fs::path(exePath).parent_path();

        fs::path appDataDir;
        {
            LPWSTR wAppDataPath = nullptr;
            if (!SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &wAppDataPath)))
                throw std::runtime_error("Failed to get APPDATA folder path");

            appDataDir = wAppDataPath;
            CoTaskMemFree(wAppDataPath);
        }

        std::vector<fs::path> paths = { appDataDir / "imhex", parentDir };

        switch (path) {
            case ImHexPath::Patterns:
                addUserDirs(paths);
                std::transform(paths.begin(), paths.end(), std::back_inserter(result), [](auto &path) {
                    return (path / "patterns").string();
                });
                break;
            case ImHexPath::PatternsInclude:
                addUserDirs(paths);
                std::transform(paths.begin(), paths.end(), std::back_inserter(result), [](auto &path) {
                    return (path / "includes").string();
                });
                break;
            case ImHexPath::Magic:
                addUserDirs(paths);
                std::transform(paths.begin(), paths.end(), std::back_inserter(result), [](auto &path) {
                    return (path / "magic").string();
                });
                break;
            case ImHexPath::Python:
                addUserDirs(paths);
                std::transform(paths.begin(), paths.end(), std::back_inserter(result), [](auto &path) {
                    return (path / "python").string();
                });
                break;
            case ImHexPath::Plugins:
                std::transform(paths.begin(), paths.end(), std::back_inserter(result), [](auto &path) {
                    return (path / "plugins").string();
                });
                break;
            case ImHexPath::Yara:
                addUserDirs(paths);
                std::transform(paths.begin(), paths.end(), std::back_inserter(result), [](auto &path) {
                    return (path / "yara").string();
                });
                break;
            case ImHexPath::Config:
                return { (appDataDir / "imhex" / "config").string() };
            case ImHexPath::Resources:
                std::transform(paths.begin(), paths.end(), std::back_inserter(result), [](auto &path) {
                    return (path / "resources").string();
                });
                break;
            case ImHexPath::Constants:
                addUserDirs(paths);
                std::transform(paths.begin(), paths.end(), std::back_inserter(result), [](auto &path) {
                    return (path / "constants").string();
                });
                break;
            case ImHexPath::Encodings:
                addUserDirs(paths);
                std::transform(paths.begin(), paths.end(), std::back_inserter(result), [](auto &path) {
                    return (path / "encodings").string();
                });
                break;
            case ImHexPath::Logs:
                std::transform(paths.begin(), paths.end(), std::back_inserter(result), [](auto &path) {
                    return (path / "logs").string();
                });
                break;
            default:
                __builtin_unreachable();
        }
#elif defined(OS_MACOS)
        // Get path to special directories
        const fs::path applicationSupportDir(getMacApplicationSupportDirectoryPath());

        std::vector<fs::path> paths = { applicationSupportDir, exePath };

        switch (path) {
            case ImHexPath::Patterns:
                result.push_back((applicationSupportDir / "patterns").string());
                break;
            case ImHexPath::PatternsInclude:
                result.push_back((applicationSupportDir / "includes").string());
                break;
            case ImHexPath::Magic:
                result.push_back((applicationSupportDir / "magic").string());
                break;
            case ImHexPath::Python:
                result.push_back((applicationSupportDir / "python").string());
                break;
            case ImHexPath::Plugins:
                std::transform(paths.begin(), paths.end(), std::back_inserter(result), [](auto &path) {
                    return (path / "plugins").string();
                });
                break;
            case ImHexPath::Yara:
                result.push_back((applicationSupportDir / "yara").string());
                break;
            case ImHexPath::Config:
                result.push_back((applicationSupportDir / "config").string());
                break;
            case ImHexPath::Resources:
                result.push_back((applicationSupportDir / "resources").string());
                break;
            case ImHexPath::Constants:
                result.push_back((applicationSupportDir / "constants").string());
                break;
            case ImHexPath::Encodings:
                result.push_back((applicationSupportDir / "encodings").string());
                break;
            case ImHexPath::Logs:
                result.push_back((applicationSupportDir / "logs").string());
                break;
            default:
                __builtin_unreachable();
        }
#else
        std::vector<fs::path> configDirs = xdg::ConfigDirs();
        std::vector<fs::path> dataDirs   = xdg::DataDirs();

        configDirs.insert(configDirs.begin(), xdg::ConfigHomeDir());
        dataDirs.insert(dataDirs.begin(), xdg::DataHomeDir());

        for (auto &dir : dataDirs)
            dir = dir / "imhex";

        switch (path) {
            case ImHexPath::Patterns:
                addUserDirs(dataDirs);
                std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result), [](auto p) { return (p / "patterns").string(); });
                break;
            case ImHexPath::PatternsInclude:
                addUserDirs(dataDirs);
                std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result), [](auto p) { return (p / "includes").string(); });
                break;
            case ImHexPath::Magic:
                addUserDirs(dataDirs);
                std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result), [](auto p) { return (p / "magic").string(); });
                break;
            case ImHexPath::Python:
                addUserDirs(dataDirs);
                std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result), [](auto p) { return (p).string(); });
                break;
            case ImHexPath::Plugins:
                std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result), [](auto p) { return (p / "plugins").string(); });
                break;
            case ImHexPath::Yara:
                addUserDirs(dataDirs);
                std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result), [](auto p) { return (p / "yara").string(); });
                break;
            case ImHexPath::Config:
                std::transform(configDirs.begin(), configDirs.end(), std::back_inserter(result), [](auto p) { return (p / "imhex").string(); });
                break;
            case ImHexPath::Resources:
                std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result), [](auto p) { return (p / "resources").string(); });
                break;
            case ImHexPath::Constants:
                addUserDirs(dataDirs);
                std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result), [](auto p) { return (p / "constants").string(); });
                break;
            case ImHexPath::Encodings:
                addUserDirs(dataDirs);
                std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result), [](auto p) { return (p / "encodings").string(); });
                break;
            case ImHexPath::Logs:
                std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result), [](auto p) { return (p / "logs").string(); });
                break;
            default:
                __builtin_unreachable();
        }

        if (!exePath.empty())
            dataDirs.emplace(dataDirs.begin(), fs::path(exePath.data()).parent_path());
#endif

        if (!listNonExisting) {
            result.erase(std::remove_if(result.begin(), result.end(), [](const auto &path) {
                return !fs::is_directory(path);
            }),
                result.end());
        }

        return result;
    }

}
