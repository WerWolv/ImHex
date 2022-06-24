#pragma once

#if defined(OS_MACOS)

    #include <hex/helpers/fs.hpp>

    extern "C" const char * getMacExecutableDirectoryPath();
    extern "C" const char * getMacApplicationSupportDirectoryPath();

    extern "C" void macFree(void *ptr);

#endif
