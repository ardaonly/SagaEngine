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
constexpr double kRamPerJobSafe       = 4.0;
constexpr double kRamPerJobAggressive = 1.0;
constexpr uint32_t kSafeJobCap        = 4;
constexpr uint32_t kDefaultSafeJobs   = 2;
constexpr uint32_t kHeavyJobCap       = 2;

} // namespace

// ─── BuildScheduler ───────────────────────────────────────────────────────────

JobPlan BuildScheduler::PlanJobs(const JobRequest& request)
{
    return PlanJobsForReport(Hardware::GetReport(), request);
}

JobPlan BuildScheduler::PlanJobsForReport(const HardwareReport& report, const JobRequest& request)
{
    const double totalRamGb = BytesToGB(report.totalRamBytes);

    uint32_t detectedJobs = 1;

    switch (request.policy)
    {
        case SchedulingPolicy::Safe:
        {
            // Leave room for the OS and linker spikes. Engine-scale C++ builds can
            // exhaust RAM before CPU, so Safe mode is intentionally conservative.
            const uint32_t coreLimit = std::max(1u, report.cpuCores > 2 ? report.cpuCores - 2 : 1u);
            const uint32_t ramLimit  = report.totalRamBytes == 0
                ? 1u
                : std::max(1u, static_cast<uint32_t>(std::floor(totalRamGb / kRamPerJobSafe)));
            detectedJobs = std::min(coreLimit, ramLimit);
            detectedJobs = std::min(detectedJobs, kSafeJobCap);
            break;
        }
        case SchedulingPolicy::MaxPerformance:
        {
            // Use all cores but still respect basic RAM limits (1.5GB per job).
            const uint32_t coreLimit = report.cpuCores;
            const uint32_t ramLimit  = std::max(1u, static_cast<uint32_t>(std::floor(totalRamGb / 1.5)));
            detectedJobs = std::min(coreLimit, ramLimit);
            break;
        }
        case SchedulingPolicy::Aggressive:
        {
            const uint32_t coreLimit = report.cpuCores;
            const uint32_t ramLimit  = std::max(1u, static_cast<uint32_t>(std::floor(totalRamGb / kRamPerJobAggressive)));
            detectedJobs = std::min(coreLimit, ramLimit);
            break;
        }
    }

    detectedJobs = std::max(1u, detectedJobs);

    JobPlan plan;
    plan.hardware        = report;
    plan.requestedJobs   = request.requestedJobs;
    plan.detectedJobs    = detectedJobs;
    plan.hardSafetyCap   = std::max(1u, std::min(report.cpuCores / 2u, kSafeJobCap));
    plan.safetyLimit     = std::min(plan.detectedJobs, plan.hardSafetyCap);
    plan.forceUnsafeJobs = request.forceUnsafeJobs;
    plan.heavyDependency = request.heavyDependency;

    if (plan.heavyDependency)
        plan.safetyLimit = std::min(plan.safetyLimit, kHeavyJobCap);

    if (plan.requestedJobs > 0)
    {
        plan.finalJobs = plan.forceUnsafeJobs
            ? plan.requestedJobs
            : std::min(plan.requestedJobs, plan.safetyLimit);
        plan.clamped = !plan.forceUnsafeJobs && plan.finalJobs < plan.requestedJobs;
    }
    else
    {
        plan.finalJobs = std::min(kDefaultSafeJobs, plan.safetyLimit);
    }

    plan.finalJobs = std::max(1u, plan.finalJobs);
    return plan;
}

uint32_t BuildScheduler::CalculateJobs(SchedulingPolicy policy, uint32_t userOverride)
{
    return PlanJobs({policy, userOverride}).finalJobs;
}

double BuildScheduler::BytesToGB(uint64_t bytes)
{
    return static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
}

} // namespace Forge
