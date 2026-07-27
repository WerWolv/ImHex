#include <hex/helpers/fs.hpp>

#include <hex/helpers/logger.hpp>
#include <hex/helpers/fmt.hpp>
#include <hex/helpers/utils_linux.hpp>
#include <hex/helpers/auto_reset.hpp>

#if defined(OS_WINDOWS)
    #include <windows.h>
    #include <shlobj.h>
    #include <shellapi.h>
    #if !defined(GLFW_EXPOSE_NATIVE_WIN32)
        #define GLFW_EXPOSE_NATIVE_WIN32
    #endif
#elif defined(OS_MACOS)
    #if !defined(GLFW_EXPOSE_NATIVE_COCOA)
        #define GLFW_EXPOSE_NATIVE_COCOA
    #endif
#elif defined(OS_LINUX)
    #include <xdg.hpp>
    #if defined(OS_FREEBSD)
        #include <sys/syslimits.h>
    #endif
    #if !defined(GLFW_EXPOSE_NATIVE_X11)
        #define GLFW_EXPOSE_NATIVE_X11
    #endif
#endif

#if defined(OS_WEB)
    #include <emscripten.h>
#else
    #include <GLFW/glfw3.h>
    #include <GLFW/glfw3native.h>
    #include <libdlgmod/libdlgmod.h>
#endif

#include <cstdlib>
#include <filesystem>
#include <sstream>

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
    void openFileExternal(std::fs::path filePath) {
        filePath.make_preferred();

        // Make sure the file exists before trying to open it
        if (!wolv::io::fs::exists(filePath)) {
            return;
        }

        #if defined(OS_WINDOWS)
            std::ignore = ShellExecuteW(nullptr, L"open", filePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        #elif defined(OS_MACOS)
            std::ignore = system(
                fmt::format("open {}", wolv::util::toUTF8String(filePath)).c_str()
            );
        #elif defined(OS_LINUX)
            executeCmd({"xdg-open", wolv::util::toUTF8String(filePath)});
        #endif
    }

    void openFolderExternal(std::fs::path dirPath) {
        dirPath.make_preferred();

        // Make sure the folder exists before trying to open it
        if (!wolv::io::fs::exists(dirPath)) {
            return;
        }

        #if defined(OS_WINDOWS)
            auto args = fmt::format(L"\"{}\"", dirPath.c_str());
            ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
        #elif defined(OS_MACOS)
            std::ignore = system(
                fmt::format("open {}", wolv::util::toUTF8String(dirPath)).c_str()
            );
        #elif defined(OS_LINUX)
            executeCmd({"xdg-open", wolv::util::toUTF8String(dirPath)});
        #endif
    }

    void openFolderWithSelectionExternal(std::fs::path selectedFilePath) {
        selectedFilePath.make_preferred();

        // Make sure the file exists before trying to open it
        if (!wolv::io::fs::exists(selectedFilePath)) {
            return;
        }

        #if defined(OS_WINDOWS)
            auto args = fmt::format(L"/select,\"{}\"", selectedFilePath.c_str());
            ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
        #elif defined(OS_MACOS)
            std::ignore = system(
                fmt::format(
                    R"(osascript -e 'tell application "Finder" to reveal POSIX file "{}"')",
                    wolv::util::toUTF8String(selectedFilePath)
                ).c_str()
            );
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

            let data;
            try {
                data = FS.readFile("/savedFiles/" + filename);
            } catch (e) {
                console.log(e);
                return;
            }

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

                    std::fs::create_directory("/savedFiles");
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
            std::string fileFilter, firstSpec, outPath;
            unsigned long long nativeWindow = 0;

#if defined(OS_WINDOWS)
            nativeWindow = (unsigned long long)(void *)glfwGetWin32Window(ImHexApi::System::getMainWindowHandle());
#elif defined(OS_MACOS)
            nativeWindow = (unsigned long long)(void *)glfwGetCocoaWindow(ImHexApi::System::getMainWindowHandle());
#elif defined(OS_LINUX) && defined(GLFW_PLATFORM) && defined(GLFW_PLATFORM_X11)
            nativeWindow = (unsigned long long)glfwGetX11Window(ImHexApi::System::getMainWindowHandle());
#endif

            widget_set_owner(std::to_string(nativeWindow).c_str());

            bool initFirstSpec = false;
            size_t validExtensionsSize = validExtensions.size();
            for (size_t i = 0; i < validExtensionsSize; i++) {
                if (!initFirstSpec) {
                    firstSpec = validExtensions[i].spec;
                    initFirstSpec = true;
                }
                fileFilter += validExtensions[i].name + " (*." + validExtensions[i].spec + ")|*." + validExtensions[i].spec;
                if (i < validExtensionsSize - 1) {
                    fileFilter += "|";
                }
            }

            // Open the correct file dialog based on the mode
            switch (mode) {
                case DialogMode::Open:
                    if (multiple)
                        outPath = get_open_filenames_ext(fileFilter.c_str(), "", defaultPath.c_str(), "Open one or more files...");
                    else
                        outPath = get_open_filename_ext(fileFilter.c_str(), "", defaultPath.c_str(), "Open a file...");
                    break;
                case DialogMode::Save:
                    outPath = get_save_filename_ext(fileFilter.c_str(), ((!firstSpec.empty()) ? ("Untitled." + firstSpec).c_str() : "Untitled"), defaultPath.c_str(), "Save a file...");
                    break;
                case DialogMode::Folder:
                    outPath = get_directory_alt("Select a folder...", defaultPath.c_str());
                    break;
            }

            if (!outPath.empty()){
                // Handle the path if the dialog was opened in single mode
                if (outPath.find('\n') == std::string::npos) {
                    // Call the provided callback with the path
                    callback(outPath);
                }

                // Handle multiple paths if the dialog was opened in multiple mode
                if (outPath.find('\n') != std::string::npos) {
                    std::vector<std::string> outPaths = wolv::util::splitString(outPath, "\n", true);
                    // Loop over all returned paths and call the callback with each of them
                    for (size_t i = 0; i < outPaths.size(); i++) {
                        callback(outPaths[i]);
                    }
                }
            }

            return true;
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
