/// @file Hardware.cpp
/// @brief Cross-platform hardware resource detection.

#include "Forge/Hardware.h"

#include <thread>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#elif defined(__linux__)
    #include <unistd.h>
    #include <fstream>
    #include <string>
#elif defined(__APPLE__)
    #include <sys/types.h>
    #include <sys/sysctl.h>
#endif

namespace Forge
{

// ─── Hardware Detection ───────────────────────────────────────────────────────

HardwareReport Hardware::GetReport()
{
    HardwareReport report;

    // CPU Cores
    report.cpuCores = std::thread::hardware_concurrency();
    if (report.cpuCores == 0) report.cpuCores = 1;

    // RAM Detection
#if defined(_WIN32)
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status))
    {
        report.totalRamBytes = status.ullTotalPhys;
    }
#elif defined(__linux__)
    // Try sysconf first (standard POSIX-ish)
    const long pages = sysconf(_SC_PHYS_PAGES);
    const long pageSize = sysconf(_SC_PAGESIZE);
    if (pages > 0 && pageSize > 0)
    {
        report.totalRamBytes = static_cast<uint64_t>(pages) * static_cast<uint64_t>(pageSize);
    }
    else
    {
        // Fallback to /proc/meminfo
        std::ifstream meminfo("/proc/meminfo");
        std::string line;
        while (std::getline(meminfo, line))
        {
            if (line.compare(0, 9, "MemTotal:") == 0)
            {
                // Format is: MemTotal:       32711624 kB
                size_t start = line.find_first_of("0123456789");
                size_t end = line.find_last_of("0123456789");
                if (start != std::string::npos && end != std::string::npos)
                {
                    uint64_t kb = std::stoull(line.substr(start, end - start + 1));
                    report.totalRamBytes = kb * 1024;
                }
                break;
            }
        }
    }
#elif defined(__APPLE__)
    int64_t mem = 0;
    size_t len = sizeof(mem);
    if (sysctlbyname("hw.memsize", &mem, &len, nullptr, 0) == 0)
    {
        report.totalRamBytes = static_cast<uint64_t>(mem);
    }
#endif

    return report;
}

} // namespace Forge
