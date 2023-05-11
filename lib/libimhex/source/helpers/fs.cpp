#include <hex/helpers/fs.hpp>

#include <hex/api/content_registry.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/helpers/logger.hpp>

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

#include <wolv/io/file.hpp>
#include <wolv/io/fs.hpp>

namespace hex::fs {

    static std::function<void(const std::string&)> s_fileBrowserErrorCallback;
    void setFileBrowserErrorCallback(const std::function<void(const std::string&)> &callback) {
        s_fileBrowserErrorCallback = callback;
    }

    bool openFileBrowser(DialogMode mode, const std::vector<nfdfilteritem_t> &validExtensions, const std::function<void(std::fs::path)> &callback, const std::string &defaultPath, bool multiple) {
        NFD::ClearError();

        if (NFD::Init() != NFD_OKAY) {
            log::error("NFD init returned an error: {}", NFD::GetError());
            if (s_fileBrowserErrorCallback != nullptr)
                s_fileBrowserErrorCallback(NFD::GetError() ? NFD::GetError() : "No details");
            return false;
        }

        NFD::UniquePathU8 outPath;
        NFD::UniquePathSet outPaths;
        nfdresult_t result;
        switch (mode) {
            case DialogMode::Open:
                if (multiple)
                    result = NFD::OpenDialogMultiple(outPaths, validExtensions.data(), validExtensions.size(), defaultPath.empty() ? nullptr : defaultPath.c_str());
                else
                    result = NFD::OpenDialog(outPath, validExtensions.data(), validExtensions.size(), defaultPath.empty() ? nullptr : defaultPath.c_str());
                break;
            case DialogMode::Save:
                result = NFD::SaveDialog(outPath, validExtensions.data(), validExtensions.size(), defaultPath.empty() ? nullptr : defaultPath.c_str());
                break;
            case DialogMode::Folder:
                result = NFD::PickFolder(outPath, defaultPath.empty() ? nullptr : defaultPath.c_str());
                break;
            default:
                std::unreachable();
        }

        if (result == NFD_OKAY){
            if(outPath != nullptr) {
                callback(reinterpret_cast<char8_t*>(outPath.get()));
            }
            if (outPaths != nullptr) {
                nfdpathsetsize_t numPaths = 0;
                if (NFD::PathSet::Count(outPaths, numPaths) == NFD_OKAY) {
                    for (size_t i = 0; i < numPaths; i++) {
                        NFD::UniquePathSetPath path;
                        if (NFD::PathSet::GetPath(outPaths, i, path) == NFD_OKAY)
                            callback(reinterpret_cast<char8_t*>(path.get()));
                    }
                }
            }
        } else if (result == NFD_ERROR) {
            log::error("Requested file dialog returned an error: {}", NFD::GetError());
            if (s_fileBrowserErrorCallback != nullptr)
                s_fileBrowserErrorCallback(NFD::GetError() ? NFD::GetError() : "No details");
        }

        NFD::Quit();

        return result == NFD_OKAY;
    }

    std::vector<std::fs::path> getDataPaths() {
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

            paths.push_back(wolv::io::fs::getApplicationSupportDirectoryPath());

        #elif defined(OS_LINUX)

            paths.push_back(xdg::DataHomeDir());

            auto dataDirs = xdg::DataDirs();
            std::copy(dataDirs.begin(), dataDirs.end(), std::back_inserter(paths));

        #endif

        for (auto &path : paths)
            path = path / "imhex";

        #if defined(OS_MACOS)

            if (auto executablePath = wolv::io::fs::getExecutablePath(); executablePath.has_value())
                paths.push_back(*executablePath);

        #else

            if (auto executablePath = wolv::io::fs::getExecutablePath(); executablePath.has_value())
                paths.push_back(executablePath->parent_path());

        #endif


        auto additionalDirs = ImHexApi::System::getAdditionalFolderPaths();
        std::copy(additionalDirs.begin(), additionalDirs.end(), std::back_inserter(paths));

        if (ProjectFile::hasPath()) {
            paths.push_back(ProjectFile::getPath().parent_path());
        }

        return paths;
    }

    static std::vector<std::fs::path> getConfigPaths() {
        #if defined(OS_WINDOWS)
            return getDataPaths();
        #elif defined(OS_MACOS)
            return getDataPaths();
        #elif defined(OS_LINUX)
            return {xdg::ConfigHomeDir() / "imhex"};
        #endif
    }

    std::vector<std::fs::path> appendPath(std::vector<std::fs::path> paths, const std::fs::path &folder) {
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
                result = appendPath(getDataPaths(), "logs");
                break;
            case ImHexPath::Plugins:
                result = appendPath(getPluginPaths(), "plugins");
                break;
            case ImHexPath::Libraries:
                result = appendPath(getPluginPaths(), "lib");
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
            case ImHexPath::Nodes:
                result = appendPath(getDefaultPaths(ImHexPath::Scripts), "nodes");
                break;
            case ImHexPath::Themes:
                result = appendPath(getDataPaths(), "themes");
                break;
            case ImHexPath::Layouts:
                result = appendPath(getDataPaths(), "layouts");
                break;
        }

        if (!listNonExisting) {
            result.erase(std::remove_if(result.begin(), result.end(), [](const auto &path) {
                return !wolv::io::fs::isDirectory(path);
            }), result.end());
        }

        return result;
    }

    bool isPathWritable(const std::fs::path &path) {
        constexpr static auto TestFileName = "__imhex__tmp__";
        {
            wolv::io::File file(path / TestFileName, wolv::io::File::Mode::Read);
            if (file.isValid()) {
                if (!file.remove())
                    return false;
            }
        }

        wolv::io::File file(path / TestFileName, wolv::io::File::Mode::Create);
        bool result = file.isValid();
        if (!file.remove())
            return false;

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
