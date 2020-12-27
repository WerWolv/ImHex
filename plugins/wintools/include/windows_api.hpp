#pragma once

#include <hex.hpp>
#include <windows.h>

#include <string>
#include <vector>

namespace hex::plugin::win {

    struct Icon {
        std::vector<u8> data;
        u32 width, height;
    };

    std::vector<DWORD> getProcessIDs();
    std::string getProcessName(DWORD pid);
    Icon getProcessIcon(DWORD pid);

}