#include <hex/plugin.hpp>

#if defined(OS_WINDOWS)
    #include <windows.h>
    #include <psapi.h>
#endif

namespace hex::plugin::builtin {

    void addFooterItems() {

        #if defined(OS_WINDOWS)
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

                ImGui::TextUnformatted(hex::format("{0} / {1}", hex::toByteString(usedMem), hex::toByteString(totalMem)).c_str());
            });
        #endif
    }

}