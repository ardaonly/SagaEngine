/// @file BuildScheduler.cpp
/// @brief Implementation of resource-aware job scaling.

#include "Forge/BuildScheduler.h"
#include <algorithm>
#include <cmath>

namespace Forge
{

namespace
{

/// Conservative estimate of RAM required per C++ compilation job (in GB).
constexpr double kRamPerJobSafe       = 2.5;
constexpr double kRamPerJobAggressive = 1.0;

} // namespace

// ─── BuildScheduler ───────────────────────────────────────────────────────────

uint32_t BuildScheduler::CalculateJobs(SchedulingPolicy policy, uint32_t userOverride)
{
    if (userOverride > 0) return userOverride;

    const auto report = Hardware::GetReport();
    const double totalRamGb = BytesToGB(report.totalRamBytes);

    uint32_t jobs = 1;

    switch (policy)
    {
        case SchedulingPolicy::Safe:
        {
            // Leave one core for the OS, and respect RAM limits (2.5GB per job).
            const uint32_t coreLimit = std::max(1u, report.cpuCores > 1 ? report.cpuCores - 1 : 1u);
            const uint32_t ramLimit  = std::max(1u, static_cast<uint32_t>(std::floor(totalRamGb / kRamPerJobSafe)));
            jobs = std::min(coreLimit, ramLimit);
            // Absolute safety cap for "Safe" mode to prevent unexpected spikes on high-core machines.
            jobs = std::min(jobs, 16u);
            break;
        }
        case SchedulingPolicy::MaxPerformance:
        {
            // Use all cores but still respect basic RAM limits (1.5GB per job).
            const uint32_t coreLimit = report.cpuCores;
            const uint32_t ramLimit  = std::max(1u, static_cast<uint32_t>(std::floor(totalRamGb / 1.5)));
            jobs = std::min(coreLimit, ramLimit);
            break;
        }
        case SchedulingPolicy::Aggressive:
        {
            // Current behavior: just use all cores.
            jobs = report.cpuCores;
            break;
        }
    }

    return std::max(1u, jobs);
}

double BuildScheduler::BytesToGB(uint64_t bytes)
{
    return static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
}

} // namespace Forge
