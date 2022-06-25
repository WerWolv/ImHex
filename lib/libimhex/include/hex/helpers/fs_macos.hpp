#pragma once

#if defined(OS_MACOS)

    #include <hex/helpers/fs.hpp>

    extern "C" char * getMacExecutableDirectoryPath();
    extern "C" char * getMacApplicationSupportDirectoryPath();

    extern "C" void macFree(void *ptr);

#endif
