#include <hex/plugin.hpp>

#include <windows.h>
#include <psapi.h>

namespace hex::plugin::windows {

    static ULONGLONG subtractTimes(const FILETIME &left, const FILETIME &right) {
        LARGE_INTEGER a, b;

        a.LowPart = left.dwLowDateTime;
        a.HighPart = left.dwHighDateTime;

        b.LowPart = right.dwLowDateTime;
        b.HighPart = right.dwHighDateTime;

        return a.QuadPart - b.QuadPart;
    }

    void addFooterItems() {

        ContentRegistry::Interface::addFooterItem([] {
            static MEMORYSTATUSEX memInfo;
            static PROCESS_MEMORY_COUNTERS_EX pmc;

            if (ImGui::GetFrameCount() % 60 == 0) {
                memInfo.dwLength = sizeof(MEMORYSTATUSEX);
                GlobalMemoryStatusEx(&memInfo);
                GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc));
            }

            auto totalMem = memInfo.ullTotalPhys;
            auto usedMem = pmc.PrivateUsage;

            ImGui::TextUnformatted(hex::format(ICON_FA_MICROCHIP " {0} / {1}", hex::toByteString(usedMem), hex::toByteString(totalMem)).c_str());
        });

    }

}