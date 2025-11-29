#include <hex/helpers/ram.hpp>

#if defined(OS_WINDOWS)
    #include <windows.h>
#elif defined(OS_MACOS)
    #include <sys/types.h>
    #include <sys/sysctl.h>
#elif defined(OS_LINUX)
    #include <fstream>
#endif

namespace hex {
    u64 getPhysicalRAM() {
        #if defined(OS_WINDOWS)
            MEMORYSTATUSEX status;
            status.dwLength = sizeof(status);
            if (GlobalMemoryStatusEx(&status)) {
                return status.ullTotalPhys;
            }
            return 0;

        #elif defined(OS_MACOS)
            int64_t mem;
            size_t len = sizeof(mem);
            if (sysctlbyname("hw.memsize", &mem, &len, nullptr, 0) == 0) {
                return mem;
            }
            return 0;

        #elif defined(OS_LINUX)
            std::ifstream meminfo("/proc/meminfo");
            std::string line;
            while (std::getline(meminfo, line)) {
                if (line.starts_with("MemTotal:")) {
                    std::string value = line.substr(9);
                    return std::stoll(value) * 1024; // kB to bytes
                }
            }
            return 0;

        #else
            return 0;
        #endif
    }
}
