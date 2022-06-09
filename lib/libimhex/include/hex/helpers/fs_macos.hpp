#pragma once

#if defined(OS_MACOS)

    #include <hex/helpers/fs.hpp>
    #include <string>

    extern "C" void getMacExecutableDirectoryPath(std::string &result);
    extern "C" void getMacApplicationSupportDirectoryPath(std::string &result);

#endif
