#pragma once

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

    struct ItemFilter {
        // Human-friendly name
        std::string name;
        // Extensions that constitute this filter
        std::string spec;
    };

    void setFileBrowserErrorCallback(const std::function<void(const std::string&)> &callback);
    bool openFileBrowser(DialogMode mode, const std::vector<ItemFilter> &validExtensions, const std::function<void(std::fs::path)> &callback, const std::string &defaultPath = {}, bool multiple = false);

    void openFileExternal(const std::fs::path &filePath);
    void openFolderExternal(const std::fs::path &dirPath);
    void openFolderWithSelectionExternal(const std::fs::path &selectedFilePath);


    bool isPathWritable(const std::fs::path &path);

}