#pragma once

#if defined(OS_MACOS)
    #include <hex/helpers/paths.hpp>

    namespace hex {
        std::string getMacExecutableDirectoryPath();
        std::string getMacApplicationSupportDirectoryPath();
    }
#endif
