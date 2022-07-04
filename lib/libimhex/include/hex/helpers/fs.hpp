#pragma once

#include <optional>
#include <string>
#include <vector>
#include <filesystem>
#include <functional>

#include <nfd.hpp>

namespace std::fs {
    using namespace std::filesystem;
}

namespace hex::fs {

    [[maybe_unused]]
    static inline bool exists(const std::fs::path &path) {
        std::error_code error;
        return std::filesystem::exists(path, error) && !error;
    }

    [[maybe_unused]]
    static inline bool createDirectories(const std::fs::path &path) {
        std::error_code error;
        return std::filesystem::create_directories(path, error) && !error;
    }

    [[maybe_unused]]
    static inline bool isRegularFile(const std::fs::path &path) {
        std::error_code error;
        return std::filesystem::is_regular_file(path, error) && !error;
    }

    [[maybe_unused]]
    static inline bool copyFile(const std::fs::path &from, const std::fs::path &to, std::fs::copy_options = std::fs::copy_options::none) {
        std::error_code error;
        return std::filesystem::copy_file(from, to, error) && !error;
    }

    [[maybe_unused]]
    static inline bool isDirectory(const std::fs::path &path) {
        std::error_code error;
        return std::filesystem::is_directory(path, error) && !error;
    }

    [[maybe_unused]]
    static inline bool remove(const std::fs::path &path) {
        std::error_code error;
        return std::filesystem::remove(path, error) && !error;
    }

    [[maybe_unused]]
    static inline bool removeAll(const std::fs::path &path) {
        std::error_code error;
        return std::filesystem::remove_all(path, error) && !error;
    }

    [[maybe_unused]]
    static inline uintmax_t getFileSize(const std::fs::path &path) {
        std::error_code error;
        auto size = std::filesystem::file_size(path, error);

        if (error) return 0;
        else return size;
    }

    bool isPathWritable(const std::fs::path &path);

    std::fs::path toShortPath(const std::fs::path &path);

    enum class DialogMode
    {
        Open,
        Save,
        Folder
    };

    bool openFileBrowser(DialogMode mode, const std::vector<nfdfilteritem_t> &validExtensions, const std::function<void(std::fs::path)> &callback, const std::string &defaultPath = {});

    enum class ImHexPath
    {
        Patterns,
        PatternsInclude,
        Magic,
        Python,
        Plugins,
        Yara,
        Config,
        Resources,
        Constants,
        Encodings,
        Logs
    };

    std::optional<std::fs::path> getExecutablePath();

    std::vector<std::fs::path> getDefaultPaths(ImHexPath path, bool listNonExisting = false);

}