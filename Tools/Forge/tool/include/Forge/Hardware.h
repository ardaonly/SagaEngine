/// @file Hardware.h
/// @brief Hardware resource detection (CPU, RAM) for build scheduling.

#pragma once

#include <cstdint>

namespace Forge
{

/// Static hardware information about the host machine.
struct HardwareReport
{
    uint32_t cpuCores      = 1;
    uint64_t totalRamBytes = 0;
};

class Hardware
{
public:
    Hardware()                           = delete;
    Hardware(const Hardware&)            = delete;
    Hardware& operator=(const Hardware&) = delete;

    /// Detect host CPU core count and total physical RAM.
    [[nodiscard]] static HardwareReport GetReport();
};

} // namespace Forge
