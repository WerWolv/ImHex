#include <hex/helpers/fs.hpp>

#include <hex/api/imhex_api.hpp>
#include <hex/api/project_file_manager.hpp>
#include <hex/helpers/logger.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils_linux.hpp>
#include <hex/helpers/auto_reset.hpp>

#if defined(OS_WINDOWS)
    #include <windows.h>
    #include <shlobj.h>
    #include <shellapi.h>
#elif defined(OS_LINUX) || defined(OS_WEB)
    #include <xdg.hpp>
# if defined(OS_FREEBSD)
    #include <sys/syslimits.h>
# else
    #include <limits.h>
# endif
#endif

#if defined(OS_WEB)
    #include <emscripten.h>
#else
    #include <nfd.hpp>
    #if defined(OS_WINDOWS)
        #define GLFW_EXPOSE_NATIVE_WIN32
    #endif
    #if defined(OS_MACOS)
        // macOS platform headers can't be compiled with gcc
        #define GLFW_NATIVE_INCLUDE_NONE
        typedef uint32_t CGDirectDisplayID;
        typedef void *id;
        typedef void NSWindow;
        #define GLFW_EXPOSE_NATIVE_COCOA
    #endif
    #if defined(OS_LINUX) && defined(GLFW_BACKEND_X11)
        #define GLFW_EXPOSE_NATIVE_X11
    #endif
    #if defined(OS_LINUX) && defined(GLFW_BACKEND_WAYLAND)
        #define GLFW_EXPOSE_NATIVE_WAYLAND
    #endif
    #include <nfd_glfw3.h>
    #if defined(OS_LINUX) && defined(GLFW_BACKEND_WAYLAND)
        #undef GLFW_EXPOSE_NATIVE_WAYLAND
    #endif
    #if defined(OS_LINUX) && defined(GLFW_BACKEND_X11)
        #undef GLFW_EXPOSE_NATIVE_X11
    #endif
    #if defined(OS_MACOS)
        #undef GLFW_EXPOSE_NATIVE_COCOA
        #undef GLFW_NATIVE_INCLUDE_NONE
    #endif
    #if defined(OS_WINDOWS)
        #undef GLFW_EXPOSE_NATIVE_WIN32
    #endif
#endif

#include <filesystem>

#include <wolv/io/file.hpp>
#include <wolv/io/fs.hpp>
#include <wolv/utils/string.hpp>

#include <fmt/format.h>
#include <fmt/xchar.h>

namespace hex::fs {

    static AutoReset<std::function<void(const std::string&)>> s_fileBrowserErrorCallback;
    void setFileBrowserErrorCallback(const std::function<void(const std::string&)> &callback) {
        s_fileBrowserErrorCallback = callback;
    }

    // With help from https://github.com/owncloud/client/blob/cba22aa34b3677406e0499aadd126ce1d94637a2/src/gui/openfilemanager.cpp
    void openFileExternal(const std::fs::path &filePath) {
        // Make sure the file exists before trying to open it
        if (!wolv::io::fs::exists(filePath)) {
            return;
        }

        #if defined(OS_WINDOWS)
            hex::unused(
                ShellExecuteW(nullptr, L"open", filePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL)
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
        if (!wolv::io::fs::exists(dirPath)) {
            return;
        }

        #if defined(OS_WINDOWS)
            auto args = fmt::format(L"\"{}\"", dirPath.c_str());
            ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
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
        if (!wolv::io::fs::exists(selectedFilePath)) {
            return;
        }

        #if defined(OS_WINDOWS)
            auto args = fmt::format(L"/select,\"{}\"", selectedFilePath.c_str());
            ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
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
            validExtensionsNfd.reserve(validExtensions.size());
            for (const auto &extension : validExtensions) {
                validExtensionsNfd.emplace_back(nfdfilteritem_t{ extension.name.c_str(), extension.spec.c_str() });
            }

            // Clear errors from previous runs
            NFD::ClearError();

            // Try to initialize NFD
            if (NFD::Init() != NFD_OKAY) {
                // Handle errors if initialization failed
                log::error("NFD init returned an error: {}", NFD::GetError());
                if (*s_fileBrowserErrorCallback != nullptr) {
                    const auto error = NFD::GetError();
                    (*s_fileBrowserErrorCallback)(error != nullptr ? error : "No details");
                }

                return false;
            }

            NFD::UniquePathU8 outPath;
            NFD::UniquePathSet outPaths;
            nfdresult_t result = NFD_ERROR;
            nfdwindowhandle_t nativeWindow{};
            NFD_GetNativeWindowFromGLFWWindow(ImHexApi::System::getMainWindowHandle(), &nativeWindow);

            // Open the correct file dialog based on the mode
            switch (mode) {
                case DialogMode::Open:
                    if (multiple)
                        result = NFD::OpenDialogMultiple(outPaths, validExtensionsNfd.data(), validExtensionsNfd.size(), defaultPath.empty() ? nullptr : defaultPath.c_str(), nativeWindow);
                    else
                        result = NFD::OpenDialog(outPath, validExtensionsNfd.data(), validExtensionsNfd.size(), defaultPath.empty() ? nullptr : defaultPath.c_str(), nativeWindow);
                    break;
                case DialogMode::Save:
                    result = NFD::SaveDialog(outPath, validExtensionsNfd.data(), validExtensionsNfd.size(), defaultPath.empty() ? nullptr : defaultPath.c_str(), nullptr, nativeWindow);
                    break;
                case DialogMode::Folder:
                    result = NFD::PickFolder(outPath, defaultPath.empty() ? nullptr : defaultPath.c_str(), nativeWindow);
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

                if (*s_fileBrowserErrorCallback != nullptr) {
                    const auto error = NFD::GetError();
                    (*s_fileBrowserErrorCallback)(error != nullptr ? error : "No details");
                }
            }

            NFD::Quit();

            return result == NFD_OKAY;
        }

    #endif

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
        const bool result = file.isValid();
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
