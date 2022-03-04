#pragma once

#if defined(OS_MACOS)
    #include <hex/helpers/fs.hpp>

namespace hex {
    std::string getMacExecutableDirectoryPath();
    std::string getMacApplicationSupportDirectoryPath();
}
#endif
