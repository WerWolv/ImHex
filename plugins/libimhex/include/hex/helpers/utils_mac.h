#pragma once

#if defined(OS_MACOS)
    #include <hex/helpers/utils.hpp>

    namespace hex {
        std::string getPathForMac(ImHexPath path);
    }
#endif
