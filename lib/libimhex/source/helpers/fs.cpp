#include <hex/helpers/fs.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/helpers/fs_macos.hpp>
#include <hex/helpers/file.hpp>
#include <hex/helpers/intrinsics.hpp>
#include <hex/helpers/net.hpp>
#include <hex/helpers/utils.hpp>

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

        hex::trim(exePath);

        return exePath;
#elif defined(OS_LINUX)
        std::string exePath(PATH_MAX, '\0');
        if (readlink("/proc/self/exe", exePath.data(), PATH_MAX) < 0)
            return std::nullopt;

        hex::trim(exePath);

        return exePath;
#elif defined(OS_MACOS)
        std::string exePath;

        {
            auto string = getMacExecutableDirectoryPath();
            exePath = string;
            macFree(string);
        }

        hex::trim(exePath);

        return exePath;
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

    static std::function<void()> s_fileBrowserErrorCallback;
    void setFileBrowserErrorCallback(const std::function<void()> &callback) {
        s_fileBrowserErrorCallback = callback;
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

        if (result == NFD_OKAY){
            if(outPath != nullptr) {
                callback(reinterpret_cast<char8_t*>(outPath));
                NFD::FreePath(outPath);
            }
        } else if (result==NFD_ERROR) {
            if (s_fileBrowserErrorCallback != nullptr)
                    s_fileBrowserErrorCallback();
        }

        NFD::Quit();

        return result == NFD_OKAY;
    }

    static std::vector<std::fs::path> getDataPaths() {
        std::vector<std::fs::path> paths;

        #if defined(OS_WINDOWS)

            if (!ImHexApi::System::isPortableVersion()) {
                PWSTR wAppDataPath = nullptr;
                if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &wAppDataPath))) {
                    paths.emplace_back(wAppDataPath);
                    CoTaskMemFree(wAppDataPath);
                }
            }

        #elif defined(OS_MACOS)

            std::fs::path applicationSupportDirPath;
            {
                auto string = getMacApplicationSupportDirectoryPath();
                applicationSupportDirPath = std::string(string);
                macFree(string);
            }

            paths.push_back(applicationSupportDirPath);

        #elif defined(OS_LINUX)

            paths.push_back(xdg::DataHomeDir());

            auto dataDirs = xdg::DataDirs();
            std::copy(dataDirs.begin(), dataDirs.end(), std::back_inserter(paths));

        #endif

        for (auto &path : paths)
            path = path / "imhex";

        #if defined(OS_MACOS)

            if (auto executablePath = fs::getExecutablePath(); executablePath.has_value())
                paths.push_back(*executablePath);

        #else

            if (auto executablePath = fs::getExecutablePath(); executablePath.has_value())
                paths.push_back(executablePath->parent_path());

        #endif


        auto additionalDirs = ImHexApi::System::getAdditionalFolderPaths();
        std::copy(additionalDirs.begin(), additionalDirs.end(), std::back_inserter(paths));

        return paths;
    }

    static std::vector<std::fs::path> getConfigPaths() {
        #if defined(OS_WINDOWS)
            return getDataPaths();
        #elif defined(OS_MACOS)
            return getDataPaths();
        #elif defined(OS_LINUX)
            std::vector<std::fs::path> paths;

            paths.push_back(xdg::DataHomeDir());

            auto dataDirs = xdg::DataDirs();
            std::copy(dataDirs.begin(), dataDirs.end(), std::back_inserter(paths));

            for (auto &path : paths)
                path = path / "imhex";

            return paths;
        #endif
    }

    constexpr std::vector<std::fs::path> appendPath(std::vector<std::fs::path> paths, const std::fs::path &folder) {
        for (auto &path : paths)
            path = path / folder;

        return paths;
    }

    std::vector<std::fs::path> getPluginPaths() {
        std::vector<std::fs::path> paths = getDataPaths();
        #if defined(OS_LINUX) && defined(SYSTEM_PLUGINS_LOCATION)
        paths.push_back(SYSTEM_PLUGINS_LOCATION);
        #endif
        return paths;
    }


    std::vector<std::fs::path> getDefaultPaths(ImHexPath path, bool listNonExisting) {
        std::vector<std::fs::path> result;

        switch (path) {
            case ImHexPath::END:
                return { };
            case ImHexPath::Constants:
                result = appendPath(getDataPaths(), "constants");
                break;
            case ImHexPath::Config:
                result = appendPath(getConfigPaths(), "config");
                break;
            case ImHexPath::Encodings:
                result = appendPath(getDataPaths(), "encodings");
                break;
            case ImHexPath::Logs:
                result = appendPath(getConfigPaths(), "logs");
                break;
            case ImHexPath::Plugins:
                result = appendPath(getPluginPaths(), "plugins");
                break;
            case ImHexPath::Resources:
                result = appendPath(getDataPaths(), "resources");
                break;
            case ImHexPath::Magic:
                result = appendPath(getDataPaths(), "magic");
                break;
            case ImHexPath::Patterns:
                result = appendPath(getDataPaths(), "patterns");
                break;
            case ImHexPath::PatternsInclude:
                result = appendPath(getDataPaths(), "includes");
                break;
            case ImHexPath::Python:
                result = appendPath(getDataPaths(), "python");
                break;
            case ImHexPath::Yara:
                result = appendPath(getDataPaths(), "yara");
                break;
            case ImHexPath::Recent:
                result = appendPath(getConfigPaths(), "recent");
                break;
            case ImHexPath::Scripts:
                result = appendPath(getDataPaths(), "scripts");
                break;
            case ImHexPath::Inspectors:
                result = appendPath(getDefaultPaths(ImHexPath::Scripts), "inspectors");
                break;
        }

        if (!listNonExisting) {
            result.erase(std::remove_if(result.begin(), result.end(), [](const auto &path) {
                return !fs::isDirectory(path);
            }), result.end());
        }

        return result;
    }

    std::fs::path toShortPath(const std::fs::path &path) {
        #if defined(OS_WINDOWS)
            size_t size = GetShortPathNameW(path.c_str(), nullptr, 0);
            if (size == 0)
                return path;

            std::wstring newPath(size, 0x00);
            GetShortPathNameW(path.c_str(), newPath.data(), newPath.size());
            newPath.pop_back();

            return newPath;
        #else
            return path;
        #endif
    }


}
