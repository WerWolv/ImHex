#include <hex/helpers/fs.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/helpers/fs_macos.hpp>
#include <hex/helpers/file.hpp>
#include <hex/helpers/intrinsics.hpp>
#include <hex/helpers/net.hpp>

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

namespace hex::fs {

    std::optional<std::fs::path> getExecutablePath() {
#if defined(OS_WINDOWS)
        std::wstring exePath(MAX_PATH, '\0');
        if (GetModuleFileNameW(nullptr, exePath.data(), exePath.length()) == 0)
            return std::nullopt;

        return exePath;
#elif defined(OS_LINUX)
        std::string exePath(PATH_MAX, '\0');
        if (readlink("/proc/self/exe", exePath.data(), PATH_MAX) < 0)
            return std::nullopt;

        return exePath;
#elif defined(OS_MACOS)
        std::string result;

        {
            auto string = getMacExecutableDirectoryPath();
            result = string;
            macFree(string);
        }

        return result;
#else
        return std::nullopt;
#endif
    }


    bool isPathWritable(const std::fs::path &path) {
        constexpr static auto TestFileName = "__imhex__tmp__";
        {
            File file(path / TestFileName, File::Mode::Read);
            if (file.isValid()) {
                if (!file.remove())
                    return false;
            }
        }

        File file(path / TestFileName, File::Mode::Create);
        bool result = file.isValid();
        if (!file.remove())
            return false;

        return result;
    }

    bool openFileBrowser(DialogMode mode, const std::vector<nfdfilteritem_t> &validExtensions, const std::function<void(std::fs::path)> &callback, const std::string &defaultPath) {
        NFD::Init();

        nfdchar_t *outPath = nullptr;
        nfdresult_t result;
        switch (mode) {
            case DialogMode::Open:
                result = NFD::OpenDialog(outPath, validExtensions.data(), validExtensions.size(), defaultPath.c_str());
                break;
            case DialogMode::Save:
                result = NFD::SaveDialog(outPath, validExtensions.data(), validExtensions.size(), defaultPath.c_str());
                break;
            case DialogMode::Folder:
                result = NFD::PickFolder(outPath, defaultPath.c_str());
                break;
            default:
                hex::unreachable();
        }

        if (result == NFD_OKAY && outPath != nullptr) {
            callback(reinterpret_cast<char8_t*>(outPath));
            NFD::FreePath(outPath);
        }

        NFD::Quit();

        return result == NFD_OKAY;
    }


    std::vector<std::fs::path> getDefaultPaths(ImHexPath path, bool listNonExisting) {
        std::vector<std::fs::path> result;
        const auto exePath = getExecutablePath();
        auto userDirs = ImHexApi::System::getAdditionalFolderPaths();

        [[maybe_unused]]
        auto addUserDirs = [&userDirs](auto &paths) {
            std::transform(userDirs.begin(), userDirs.end(), std::back_inserter(paths), [](auto &item) {
                return std::move(item);
            });
        };

#if defined(OS_WINDOWS)
        std::vector<std::fs::path> paths;

        if (!ImHexApi::System::isPortableVersion()) {
            std::fs::path appDataDir;
            {
                PWSTR wAppDataPath = nullptr;
                if (!SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &wAppDataPath)))
                    throw std::runtime_error("Failed to get APPDATA folder path");

                appDataDir = std::wstring(wAppDataPath);
                CoTaskMemFree(wAppDataPath);
            }

            paths.push_back(appDataDir / "imhex");
        }

        if (exePath)
            paths.push_back(exePath->parent_path());

        switch (path) {
            case ImHexPath::Patterns:
                addUserDirs(paths);
                std::transform(paths.begin(), paths.end(), std::back_inserter(result), [](auto &path) {
                    return path / "patterns";
                });
                break;
            case ImHexPath::PatternsInclude:
                addUserDirs(paths);
                std::transform(paths.begin(), paths.end(), std::back_inserter(result), [](auto &path) {
                    return path / "includes";
                });
                break;
            case ImHexPath::Magic:
                addUserDirs(paths);
                std::transform(paths.begin(), paths.end(), std::back_inserter(result), [](auto &path) {
                    return path / "magic";
                });
                break;
            case ImHexPath::Python:
                addUserDirs(paths);
                std::transform(paths.begin(), paths.end(), std::back_inserter(result), [](auto &path) {
                    return path / "python";
                });
                break;
            case ImHexPath::Plugins:
                std::transform(paths.begin(), paths.end(), std::back_inserter(result), [](auto &path) {
                    return path / "plugins";
                });
                break;
            case ImHexPath::Yara:
                addUserDirs(paths);
                std::transform(paths.begin(), paths.end(), std::back_inserter(result), [](auto &path) {
                    return path / "yara";
                });
                break;
            case ImHexPath::Config:
                std::transform(paths.begin(), paths.end(), std::back_inserter(result), [](auto &path) {
                    return path / "config";
                });
                break;
            case ImHexPath::Resources:
                std::transform(paths.begin(), paths.end(), std::back_inserter(result), [](auto &path) {
                    return path / "resources";
                });
                break;
            case ImHexPath::Constants:
                addUserDirs(paths);
                std::transform(paths.begin(), paths.end(), std::back_inserter(result), [](auto &path) {
                    return path / "constants";
                });
                break;
            case ImHexPath::Encodings:
                addUserDirs(paths);
                std::transform(paths.begin(), paths.end(), std::back_inserter(result), [](auto &path) {
                    return path / "encodings";
                });
                break;
            case ImHexPath::Logs:
                std::transform(paths.begin(), paths.end(), std::back_inserter(result), [](auto &path) {
                    return path / "logs";
                });
                break;
            default:
                hex::unreachable();
        }
#elif defined(OS_MACOS)
        // Get path to special directories
        std::string applicationSupportDir;
        {
            auto string = getMacApplicationSupportDirectoryPath();
            applicationSupportDir = string;
            macFree(string);
        }
        const std::fs::path applicationSupportDirPath(applicationSupportDir);

        std::vector<std::fs::path> paths = { applicationSupportDirPath };

        if (exePath.has_value())
            paths.push_back(exePath.value());

        switch (path) {
            case ImHexPath::Patterns:
                result.push_back(applicationSupportDirPath / "patterns");
                break;
            case ImHexPath::PatternsInclude:
                result.push_back(applicationSupportDirPath / "includes");
                break;
            case ImHexPath::Magic:
                result.push_back(applicationSupportDirPath / "magic");
                break;
            case ImHexPath::Python:
                result.push_back(applicationSupportDirPath / "python");
                break;
            case ImHexPath::Plugins:
                std::transform(paths.begin(), paths.end(), std::back_inserter(result), [](auto &path) {
                    return path / "plugins";
                });
                break;
            case ImHexPath::Yara:
                result.push_back(applicationSupportDirPath / "yara");
                break;
            case ImHexPath::Config:
                result.push_back(applicationSupportDirPath / "config");
                break;
            case ImHexPath::Resources:
                result.push_back(applicationSupportDirPath / "resources");
                break;
            case ImHexPath::Constants:
                result.push_back(applicationSupportDirPath / "constants");
                break;
            case ImHexPath::Encodings:
                result.push_back(applicationSupportDirPath / "encodings");
                break;
            case ImHexPath::Logs:
                result.push_back(applicationSupportDirPath / "logs");
                break;
            default:
                hex::unreachable();
        }
#else
        std::vector<std::fs::path> configDirs = xdg::ConfigDirs();
        std::vector<std::fs::path> dataDirs   = xdg::DataDirs();

        configDirs.insert(configDirs.begin(), xdg::ConfigHomeDir());
        dataDirs.insert(dataDirs.begin(), xdg::DataHomeDir());

        for (auto &dir : dataDirs)
            dir = dir / "imhex";

        if (exePath && !exePath->empty())
            dataDirs.push_back(exePath->parent_path());

        switch (path) {
            case ImHexPath::Patterns:
                addUserDirs(dataDirs);
                std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result), [](auto p) { return p / "patterns"; });
                break;
            case ImHexPath::PatternsInclude:
                addUserDirs(dataDirs);
                std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result), [](auto p) { return p / "includes"; });
                break;
            case ImHexPath::Magic:
                addUserDirs(dataDirs);
                std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result), [](auto p) { return p / "magic"; });
                break;
            case ImHexPath::Python:
                addUserDirs(dataDirs);
                std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result), [](auto p) { return p; });
                break;
            case ImHexPath::Plugins:
                std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result), [](auto p) { return p / "plugins"; });
                break;
            case ImHexPath::Yara:
                addUserDirs(dataDirs);
                std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result), [](auto p) { return p / "yara"; });
                break;
            case ImHexPath::Config:
                std::transform(configDirs.begin(), configDirs.end(), std::back_inserter(result), [](auto p) { return p / "imhex"; });
                break;
            case ImHexPath::Resources:
                std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result), [](auto p) { return p / "resources"; });
                break;
            case ImHexPath::Constants:
                addUserDirs(dataDirs);
                std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result), [](auto p) { return p / "constants"; });
                break;
            case ImHexPath::Encodings:
                addUserDirs(dataDirs);
                std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result), [](auto p) { return p / "encodings"; });
                break;
            case ImHexPath::Logs:
                std::transform(dataDirs.begin(), dataDirs.end(), std::back_inserter(result), [](auto p) { return p / "logs"; });
                break;
            default:
                hex::unreachable();
        }

#endif

        if (!listNonExisting) {
            result.erase(std::remove_if(result.begin(), result.end(), [](const auto &path) {
                return !fs::isDirectory(path);
            }),
                result.end());
        }

        return result;
    }

    std::fs::path toShortPath(const std::fs::path &path) {
        #if defined(OS_WINDOWS)
            size_t size = GetShortPathNameW(path.c_str(), nullptr, 0) * sizeof(TCHAR);
            if (size == 0)
                return path;

            std::wstring newPath(size, 0x00);
            GetShortPathNameW(path.c_str(), newPath.data(), newPath.size());

            return newPath;
        #else
            return path;
        #endif
    }


}
