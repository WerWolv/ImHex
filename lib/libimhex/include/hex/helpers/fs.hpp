#pragma once

#include <hex.hpp>

#include <optional>
#include <string>
#include <vector>
#include <filesystem>
#include <functional>

#include <wolv/io/fs.hpp>

namespace hex::fs {

    enum class DialogMode {
        Open,
        Save,
        Folder
    };

    struct itemfilter {
        // human-friendly name
        const std::string name;
        // extensions that constitute this filter
        const std::string spec;
    };

    void setFileBrowserErrorCallback(const std::function<void(const std::string&)> &callback);
    bool openFileBrowser(DialogMode mode, const std::vector<itemfilter> &validExtensions, const std::function<void(std::fs::path)> &callback, const std::string &defaultPath = {}, bool multiple = false);

    void openFileExternal(const std::fs::path &filePath);
    void openFolderExternal(const std::fs::path &dirPath);
    void openFolderWithSelectionExternal(const std::fs::path &selectedFilePath);

    enum class ImHexPath : u32 {
        Patterns = 0,
        PatternsInclude,
        Magic,
        Plugins,
        Yara,
        Config,
        Resources,
        Constants,
        Encodings,
        Logs,
        Recent,
        Scripts,
        Inspectors,
        Themes,
        Libraries,
        Nodes,
        Layouts,

        END
    };

    bool isPathWritable(const std::fs::path &path);

    std::vector<std::fs::path> getDefaultPaths(ImHexPath path, bool listNonExisting = false);

    // temporarily expose these for the migration function
    std::vector<std::fs::path> getDataPaths();
    std::vector<std::fs::path> appendPath(std::vector<std::fs::path> paths, const std::fs::path &folder);
}