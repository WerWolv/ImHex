#include <hex/api/content_registry.hpp>

#include <hex/helpers/utils.hpp>

#include <windows.h>
#include <psapi.h>

#include <imgui.h>
#include <hex/ui/imgui_imhex_extensions.h>

#include <fonts/fontawesome_font.h>
#include <fonts/codicons_font.h>

namespace hex::plugin::windows {

    void addTitleBarButtons() {
#if defined(DEBUG)
        ContentRegistry::Interface::addTitleBarButton(ICON_VS_DEBUG, "hex.windows.title_bar_button.debug_build", []{
            if (ImGui::GetIO().KeyCtrl) {
                // Explicitly trigger a segfault by writing to an invalid memory location
                // Used for debugging crashes
                *reinterpret_cast<u8 *>(0x10) = 0x10;
            } else if (ImGui::GetIO().KeyShift) {
                // Explicitly trigger an abort by throwing an uncaught exception
                // Used for debugging exception errors
                throw std::runtime_error("Debug Error");
            } else {
                hex::openWebpage("https://imhex.werwolv.net/debug");
            }
        });
#endif

        ContentRegistry::Interface::addTitleBarButton(ICON_VS_SMILEY, "hex.windows.title_bar_button.feedback", []{
            hex::openWebpage("mailto://hey@werwolv.net");
        });

    }

    void addFooterItems() {

        ContentRegistry::Interface::addFooterItem([] {
            static float cpuUsage = 0.0F;

            if (ImGui::HasSecondPassed()) {
                static ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;
                static u32 numProcessors;
                static HANDLE self = GetCurrentProcess();

                ULARGE_INTEGER now, sys, user;
                {
                    FILETIME ftime, fsys, fuser;

                    GetSystemTimeAsFileTime(&ftime);
                    memcpy(&now, &ftime, sizeof(FILETIME));

                    GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
                    memcpy(&sys, &fsys, sizeof(FILETIME));
                    memcpy(&user, &fuser, sizeof(FILETIME));
                }

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
                    lastCPU     = now;
                    lastUserCPU = user;
                    lastSysCPU  = sys;
                }

                cpuUsage *= 100;
            }

            ImGui::TextFormatted(ICON_FA_TACHOMETER_ALT " {0:2}.{1:02}", u32(cpuUsage), u32(cpuUsage * 100) % 100);
        });

        ContentRegistry::Interface::addFooterItem([] {
            static MEMORYSTATUSEX memInfo;
            static PROCESS_MEMORY_COUNTERS_EX pmc;

            if (ImGui::HasSecondPassed()) {
                memInfo.dwLength = sizeof(MEMORYSTATUSEX);
                GlobalMemoryStatusEx(&memInfo);
                GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&pmc), sizeof(pmc));
            }

            auto totalMem = memInfo.ullTotalPhys;
            auto usedMem  = pmc.PrivateUsage;

            ImGui::TextFormatted(ICON_FA_MICROCHIP " {0} / {1}", hex::toByteString(usedMem), hex::toByteString(totalMem));
        });
    }

}