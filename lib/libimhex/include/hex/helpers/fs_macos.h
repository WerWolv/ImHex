#pragma once

#if defined(OS_MACOS)
    #include <hex/helpers/fs.hpp>

    extern "C" std::string getMacExecutableDirectoryPath();
    extern "C" std::string getMacApplicationSupportDirectoryPath();
#endif
