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

    std::vector<std::string> getPath(ImHexPath path);

}