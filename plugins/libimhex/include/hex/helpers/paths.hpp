#pragma once

#include <string>
#include <vector>

namespace hex {

    enum class ImHexPath {
        Patterns,
        PatternsInclude,
        Magic,
        Python,
        Plugins,
        Yara,
        Config,
        Resources,
        Constants
    };

    std::string getExecutablePath();

    std::vector<std::string> getPath(ImHexPath path, bool listNonExisting = false);

}