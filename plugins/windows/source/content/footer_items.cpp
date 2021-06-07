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
            static float cpuUsage = 0.0F;

            if (ImGui::HasSecondPassed()) {
                static ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;
                static u32 numProcessors;
                static HANDLE self = GetCurrentProcess();

                FILETIME ftime, fsys, fuser;
                ULARGE_INTEGER now, sys, user;

                GetSystemTimeAsFileTime(&ftime);
                memcpy(&now, &ftime, sizeof(FILETIME));

                GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
                memcpy(&sys, &fsys, sizeof(FILETIME));
                memcpy(&user, &fuser, sizeof(FILETIME));

                if (lastCPU.QuadPart == 0) {
                    SYSTEM_INFO sysInfo;
                    FILETIME ftime, fsys, fuser;

                    GetSystemInfo(&sysInfo);
                    numProcessors = sysInfo.dwNumberOfProcessors;

                    GetSystemTimeAsFileTime(&ftime);
                    memcpy(&lastCPU, &ftime, sizeof(FILETIME));

                    self = GetCurrentProcess();
                    GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
                    memcpy(&lastSysCPU, &fsys, sizeof(FILETIME));
                    memcpy(&lastUserCPU, &fuser, sizeof(FILETIME));
                } else {
                    cpuUsage = (sys.QuadPart - lastSysCPU.QuadPart) +
                               (user.QuadPart - lastUserCPU.QuadPart);
                    cpuUsage /= (now.QuadPart - lastCPU.QuadPart);
                    cpuUsage /= numProcessors;
                    lastCPU = now;
                    lastUserCPU = user;
                    lastSysCPU = sys;
                }
            }

            ImGui::TextUnformatted(hex::format(ICON_FA_TACHOMETER_ALT " {0:.2f}%", cpuUsage * 100).c_str());
        });

        ContentRegistry::Interface::addFooterItem([] {
            static MEMORYSTATUSEX memInfo;
            static PROCESS_MEMORY_COUNTERS_EX pmc;

            if (ImGui::HasSecondPassed()) {
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