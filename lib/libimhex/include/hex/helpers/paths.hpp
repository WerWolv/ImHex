#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace hex {

    namespace fs = std::filesystem;

    enum class ImHexPath {
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

    std::string getExecutablePath();

    std::vector<fs::path> getPath(ImHexPath path, bool listNonExisting = false);

}