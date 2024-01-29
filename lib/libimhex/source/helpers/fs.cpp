#include <hex/helpers/fs.hpp>

#include <hex/api/imhex_api.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils_linux.hpp>

#include <xdg.hpp>

#if defined(OS_WINDOWS)
    #include <windows.h>
    #include <shlobj.h>
    #include <shellapi.h>
#elif defined(OS_LINUX) || defined(OS_WEB)
    #include <xdg.hpp>
    #include <limits.h>
#endif

#if defined(OS_WEB)
    #include <emscripten.h>
#else
    #include <nfd.hpp>
#endif

#include <algorithm>
#include <filesystem>

#include <wolv/io/file.hpp>
#include <wolv/io/fs.hpp>
#include <wolv/utils/string.hpp>

namespace hex::fs {

    static std::function<void(const std::string&)> s_fileBrowserErrorCallback;
    void setFileBrowserErrorCallback(const std::function<void(const std::string&)> &callback) {
        s_fileBrowserErrorCallback = callback;
    }

    // With help from https://github.com/owncloud/client/blob/cba22aa34b3677406e0499aadd126ce1d94637a2/src/gui/openfilemanager.cpp
    void openFileExternal(const std::fs::path &filePath) {
        // Make sure the file exists before trying to open it
        if (!wolv::io::fs::exists(filePath))
            return;

        #if defined(OS_WINDOWS)
            hex::unused(
                ShellExecute(nullptr, "open", wolv::util::toUTF8String(filePath).c_str(), nullptr, nullptr, SW_SHOWNORMAL)
            );
        #elif defined(OS_MACOS)
            hex::unused(system(
                hex::format("open {}", wolv::util::toUTF8String(filePath)).c_str()
            ));
        #elif defined(OS_LINUX)
            executeCmd({"xdg-open", wolv::util::toUTF8String(filePath)});
        #endif
    }

    void openFolderExternal(const std::fs::path &dirPath) {
        // Make sure the folder exists before trying to open it
        if (!wolv::io::fs::exists(dirPath))
            return;

        #if defined(OS_WINDOWS)
            hex::unused(system(
                hex::format("explorer.exe {}", wolv::util::toUTF8String(dirPath)).c_str()
            ));
        #elif defined(OS_MACOS)
            hex::unused(system(
                hex::format("open {}", wolv::util::toUTF8String(dirPath)).c_str()
            ));
        #elif defined(OS_LINUX)
            executeCmd({"xdg-open", wolv::util::toUTF8String(dirPath)});
        #endif
    }

    void openFolderWithSelectionExternal(const std::fs::path &selectedFilePath) {
        // Make sure the file exists before trying to open it
        if (!wolv::io::fs::exists(selectedFilePath))
            return;

        #if defined(OS_WINDOWS)
            hex::unused(system(
                hex::format(R"(explorer.exe /select,"{}")", wolv::util::toUTF8String(selectedFilePath)).c_str()
            ));
        #elif defined(OS_MACOS)
            hex::unused(system(
                hex::format(
                    R"(osascript -e 'tell application "Finder" to reveal POSIX file "{}"')",
                    wolv::util::toUTF8String(selectedFilePath)
                ).c_str()
            ));
            system(R"(osascript -e 'tell application "Finder" to activate')");
        #elif defined(OS_LINUX)
            // Fallback to only opening the folder for now
            // TODO actually select the file
            executeCmd({"xdg-open", wolv::util::toUTF8String(selectedFilePath.parent_path())});
        #endif
    }

    #if defined(OS_WEB)

        std::function<void(std::fs::path)> currentCallback;

        EMSCRIPTEN_KEEPALIVE
        extern "C" void fileBrowserCallback(char* path) {
            currentCallback(path);
        }

        EM_JS(int, callJs_saveFile, (const char *rawFilename), {
            let filename = UTF8ToString(rawFilename) || "file.bin";
            FS.createPath("/", "savedFiles");

            if (FS.analyzePath(filename).exists) {
                FS.unlink(filename);
            }

            // Call callback that will write the file
            Module._fileBrowserCallback(stringToNewUTF8("/savedFiles/" + filename));

            let data = FS.readFile("/savedFiles/" + filename);

            const reader = Object.assign(new FileReader(), {
                onload: () => {

                    // Show popup to user to download
                    let saver = document.createElement('a');
                    saver.href = reader.result;
                    saver.download = filename;
                    saver.style = "display: none";

                    saver.click();

                },
                onerror: () => {
                    throw new Error(reader.error);
                },
            });
            reader.readAsDataURL(new File([data], "", { type: "application/octet-stream" }));

        });

        EM_JS(int, callJs_openFile, (bool multiple), {
            let selector = document.createElement("input");
            selector.type = "file";
            selector.style = "display: none";
            if (multiple) {
                selector.multiple = true;
            }
            selector.onchange = () => {
                if (selector.files.length == 0) return;

                FS.createPath("/", "openedFiles");
                for (let file of selector.files) {
                    const fr = new FileReader();
                    fr.onload = () => {
                        let folder = "/openedFiles/"+Math.random().toString(36).substring(2)+"/";
                        FS.createPath("/", folder);
                        if (FS.analyzePath(folder+file.name).exists) {
                            console.log(`Error: ${folder+file.name} already exist`);
                        } else {
                            FS.createDataFile(folder, file.name, fr.result, true, true);
                            Module._fileBrowserCallback(stringToNewUTF8(folder+file.name));
                        }
                    };

                    fr.readAsBinaryString(file);
                }
            };
            selector.click();
        });

        bool openFileBrowser(DialogMode mode, const std::vector<ItemFilter> &validExtensions, const std::function<void(std::fs::path)> &callback, const std::string &defaultPath, bool multiple) {
            switch (mode) {
                case DialogMode::Open: {
                    currentCallback = callback;
                    callJs_openFile(multiple);
                    break;
                }
                case DialogMode::Save: {
                    currentCallback = callback;
                    std::fs::path path;

                    if (!defaultPath.empty())
                        path = std::fs::path(defaultPath).filename();
                    else if (!validExtensions.empty())
                        path = "file." + validExtensions[0].spec;

                    callJs_saveFile(path.filename().string().c_str());
                    break;
                }
                case DialogMode::Folder: {
                    throw std::logic_error("Selecting a folder is not implemented");
                    return false;
                }
                default:
                    std::unreachable();
            }
            return true;
        }

    #else

        bool openFileBrowser(DialogMode mode, const std::vector<ItemFilter> &validExtensions, const std::function<void(std::fs::path)> &callback, const std::string &defaultPath, bool multiple) {
            // Turn the content of the ItemFilter objects into something NFD understands
            std::vector<nfdfilteritem_t> validExtensionsNfd;
            for (const auto &extension : validExtensions) {
                validExtensionsNfd.emplace_back(nfdfilteritem_t{ extension.name.c_str(), extension.spec.c_str() });
            }

            // Clear errors from previous runs
            NFD::ClearError();

            // Try to initialize NFD
            if (NFD::Init() != NFD_OKAY) {
                // Handle errors if initialization failed
                log::error("NFD init returned an error: {}", NFD::GetError());
                if (s_fileBrowserErrorCallback != nullptr) {
                    auto error = NFD::GetError();
                    s_fileBrowserErrorCallback(error != nullptr ? error : "No details");
                }

                return false;
            }

            NFD::UniquePathU8 outPath;
            NFD::UniquePathSet outPaths;
            nfdresult_t result = NFD_ERROR;

            // Open the correct file dialog based on the mode
            switch (mode) {
                case DialogMode::Open:
                    if (multiple)
                        result = NFD::OpenDialogMultiple(outPaths, validExtensionsNfd.data(), validExtensionsNfd.size(), defaultPath.empty() ? nullptr : defaultPath.c_str());
                    else
                        result = NFD::OpenDialog(outPath, validExtensionsNfd.data(), validExtensionsNfd.size(), defaultPath.empty() ? nullptr : defaultPath.c_str());
                    break;
                case DialogMode::Save:
                    result = NFD::SaveDialog(outPath, validExtensionsNfd.data(), validExtensionsNfd.size(), defaultPath.empty() ? nullptr : defaultPath.c_str());
                    break;
                case DialogMode::Folder:
                    result = NFD::PickFolder(outPath, defaultPath.empty() ? nullptr : defaultPath.c_str());
                    break;
            }

            if (result == NFD_OKAY){
                // Handle the path if the dialog was opened in single mode
                if (outPath != nullptr) {
                    // Call the provided callback with the path
                    callback(outPath.get());
                }

                // Handle multiple paths if the dialog was opened in multiple mode
                if (outPaths != nullptr) {
                    nfdpathsetsize_t numPaths = 0;
                    if (NFD::PathSet::Count(outPaths, numPaths) == NFD_OKAY) {
                        // Loop over all returned paths and call the callback with each of them
                        for (size_t i = 0; i < numPaths; i++) {
                            NFD::UniquePathSetPath path;
                            if (NFD::PathSet::GetPath(outPaths, i, path) == NFD_OKAY)
                                callback(path.get());
                        }
                    }
                }
            } else if (result == NFD_ERROR) {
                // Handle errors that occurred during the file dialog call

                log::error("Requested file dialog returned an error: {}", NFD::GetError());

                if (s_fileBrowserErrorCallback != nullptr) {
                    auto error = NFD::GetError();
                    s_fileBrowserErrorCallback(error != nullptr ? error : "No details");
                }
            }

            NFD::Quit();

            return result == NFD_OKAY;
        }

    #endif

    std::vector<std::fs::path> getDataPaths() {
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

            paths.push_back(wolv::io::fs::getApplicationSupportDirectoryPath());

        #elif defined(OS_LINUX) || defined(OS_WEB)

            paths.push_back(xdg::DataHomeDir());

            auto dataDirs = xdg::DataDirs();
            std::copy(dataDirs.begin(), dataDirs.end(), std::back_inserter(paths));

        #endif

        #if defined(OS_MACOS)

            if (auto executablePath = wolv::io::fs::getExecutablePath(); executablePath.has_value())
                paths.push_back(*executablePath);

        #else

            for (auto &path : paths)
                path = path / "imhex";

            if (auto executablePath = wolv::io::fs::getExecutablePath(); executablePath.has_value())
                paths.push_back(executablePath->parent_path());

        #endif


        // Add additional data directories to the path
        auto additionalDirs = ImHexApi::System::getAdditionalFolderPaths();
        std::copy(additionalDirs.begin(), additionalDirs.end(), std::back_inserter(paths));

        // Add the project file directory to the path, if one is loaded
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
        #elif defined(OS_LINUX) || defined(OS_WEB)
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

        // Add the system plugin directory to the path if one was provided at compile time
        #if defined(OS_LINUX) && defined(SYSTEM_PLUGINS_LOCATION)
            paths.push_back(SYSTEM_PLUGINS_LOCATION);
        #endif

        return paths;
    }


    std::vector<std::fs::path> getDefaultPaths(ImHexPath path, bool listNonExisting) {
        std::vector<std::fs::path> result;

        // Return the correct path based on the ImHexPath enum
        switch (path) {
            case ImHexPath::END:
                return { };
            case ImHexPath::Constants:
                result = appendPath(getDataPaths(), "constants");
                break;
            case ImHexPath::Config:
                result = appendPath(getConfigPaths(), "config");
                break;
            case ImHexPath::Backups:
                result = appendPath(getDataPaths(), "backups");
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
            case ImHexPath::Workspaces:
                result = appendPath(getDataPaths(), "workspaces");
                break;
        }

        // Remove all paths that don't exist if requested
        if (!listNonExisting) {
            std::erase_if(result, [](const auto &entryPath) {
                return !wolv::io::fs::isDirectory(entryPath);
            });
        }

        return result;
    }

    bool isPathWritable(const std::fs::path &path) {
        constexpr static auto TestFileName = "__imhex__tmp__";

        // Try to open the __imhex__tmp__ file in the given path
        // If one does exist already, try to delete it
        {
            wolv::io::File file(path / TestFileName, wolv::io::File::Mode::Read);
            if (file.isValid()) {
                if (!file.remove())
                    return false;
            }
        }

        // Try to create a new file in the given path
        // If that fails, or the file cannot be deleted anymore afterward; the path is not writable
        wolv::io::File file(path / TestFileName, wolv::io::File::Mode::Create);
        bool result = file.isValid();
        if (!file.remove())
            return false;

        return result;
    }

    std::fs::path toShortPath(const std::fs::path &path) {
        #if defined(OS_WINDOWS)
            // Get the size of the short path
            size_t size = GetShortPathNameW(path.c_str(), nullptr, 0);
            if (size == 0)
                return path;

            // Get the short path
            std::wstring newPath(size, 0x00);
            GetShortPathNameW(path.c_str(), newPath.data(), newPath.size());
            newPath.pop_back();

            return newPath;
        #else
            // Other supported platforms don't have short paths
            return path;
        #endif
    }


}
